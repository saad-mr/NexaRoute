#include "AuthService.h"

#include "Security.h"
#include "Validation.h"

#include <chrono>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {
constexpr int PASSWORD_ROUNDS = 2048;
constexpr int MAXIMUM_FAILED_ATTEMPTS = 5;
constexpr long long LOCK_SECONDS = 60;
constexpr long long SESSION_SECONDS = 8 * 60 * 60;

std::string uppercaseAscii(const std::string& value) {
    std::string result = value;
    for (char& character : result) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    return result;
}

AccountRole parseRole(const std::string& value) {
    return value == "Admin" ? AccountRole::Admin : AccountRole::Rider;
}
}

AuthService::AuthService()
    : accounts_(),
      accountIndex_(101),
      sessions_(),
      sessionIndex_(211),
      auditEntries_(),
      runtimeDirectory_(),
      persistenceEnabled_(false),
      ready_(false) {
}

bool AuthService::initialize(
    const std::filesystem::path& runtimeDirectory,
    bool persistenceEnabled
) {
    accounts_.clear();
    accountIndex_.clear();
    sessions_.clear();
    sessionIndex_.clear();
    auditEntries_.clear();
    runtimeDirectory_ = runtimeDirectory;
    persistenceEnabled_ = persistenceEnabled && !runtimeDirectory_.empty();
    ready_ = false;

    if (persistenceEnabled_) {
        std::error_code error;
        std::filesystem::create_directories(runtimeDirectory_, error);
        if (error) persistenceEnabled_ = false;
    }

    if (persistenceEnabled_) loadAccounts();
    if (!ensureAccount(defaultAdminUsername(), AccountRole::Admin, defaultAdminPassword())) {
        return false;
    }
    ready_ = true;
    saveAccounts();
    recordAudit(defaultAdminUsername(), AccountRole::Admin, "service_start", true, "Authentication service initialized");
    return true;
}

bool AuthService::ready() const {
    return ready_;
}

AuthResult AuthService::loginRider(
    const std::string& riderId,
    const std::string& password
) {
    return login(riderId, password, AccountRole::Rider);
}

AuthResult AuthService::loginAdmin(
    const std::string& username,
    const std::string& password
) {
    return login(username, password, AccountRole::Admin);
}

AuthResult AuthService::login(
    const std::string& accountId,
    const std::string& password,
    AccountRole role
) {
    AuthResult result;
    result.role = role;
    result.accountId = canonicalAccountId(accountId, role);
    if (!ready_) {
        result.error = "Authentication service is unavailable.";
        return result;
    }

    expireOldSessions();
    AuthAccount* account = findAccount(result.accountId, role);
    if (account == nullptr || !account->enabled) {
        result.error = role == AccountRole::Admin
            ? "Admin username or password is incorrect."
            : "Rider ID or password is incorrect.";
        recordAudit(result.accountId, role, "login", false, "Unknown or disabled account");
        return result;
    }

    const long long now = currentEpochSeconds();
    if (account->lockedUntilEpoch > now) {
        const long long remaining = account->lockedUntilEpoch - now;
        result.error = "Account is temporarily locked. Try again in " + std::to_string(remaining) + " seconds.";
        recordAudit(account->accountId, role, "login", false, "Account temporarily locked");
        return result;
    }

    const std::string candidate = Security::derivePasswordHash(password, account->passwordSalt, PASSWORD_ROUNDS);
    if (!Security::constantTimeEquals(candidate, account->passwordHash)) {
        ++account->failedAttempts;
        if (account->failedAttempts >= MAXIMUM_FAILED_ATTEMPTS) {
            account->lockedUntilEpoch = now + LOCK_SECONDS;
            account->failedAttempts = 0;
        }
        saveAccounts();
        result.error = role == AccountRole::Admin
            ? "Admin username or password is incorrect."
            : "Rider ID or password is incorrect.";
        recordAudit(account->accountId, role, "login", false, "Password verification failed");
        return result;
    }

    account->failedAttempts = 0;
    account->lockedUntilEpoch = 0;
    account->lastLoginEpoch = now;
    saveAccounts();
    result = issueSession(*account);
    recordAudit(account->accountId, role, "login", true, "Session issued");
    return result;
}

bool AuthService::registerRiderCredential(
    const std::string& riderId,
    const std::string& password,
    std::string& error
) {
    const std::string canonical = canonicalAccountId(riderId, AccountRole::Rider);
    if (!ready_) {
        error = "Authentication service is unavailable.";
        return false;
    }
    if (!Validation::isIdentifier(canonical, 4, 20)) {
        error = "Rider ID format is invalid.";
        return false;
    }
    error = passwordError(password);
    if (!error.empty()) return false;
    if (credentialExists(canonical, AccountRole::Rider)) {
        error = "A login already exists for this rider ID.";
        return false;
    }
    if (createAccount(canonical, AccountRole::Rider, password) == nullptr) {
        error = "The rider login could not be created.";
        return false;
    }
    saveAccounts();
    recordAudit(canonical, AccountRole::Rider, "register", true, "Rider credential created");
    return true;
}

bool AuthService::removeRiderCredential(const std::string& riderId) {
    AuthAccount* account = findAccount(riderId, AccountRole::Rider);
    if (account == nullptr) return false;
    account->enabled = false;
    saveAccounts();
    recordAudit(account->accountId, AccountRole::Rider, "disable", true, "Credential disabled");
    return true;
}

bool AuthService::changePassword(
    const std::string& token,
    const std::string& currentPassword,
    const std::string& newPassword,
    std::string& error
) {
    AuthSession* const* storedSession = sessionIndex_.find(token);
    AuthSession* activeSession = storedSession == nullptr ? nullptr : *storedSession;
    if (activeSession == nullptr || !activeSession->active ||
        activeSession->expiresAtEpoch <= currentEpochSeconds()) {
        error = "Your session has expired. Sign in again.";
        return false;
    }
    AuthAccount* account = findAccount(activeSession->accountId, activeSession->role);
    if (account == nullptr) {
        error = "Account was not found.";
        return false;
    }
    const std::string currentHash = Security::derivePasswordHash(
        currentPassword,
        account->passwordSalt,
        PASSWORD_ROUNDS
    );
    if (!Security::constantTimeEquals(currentHash, account->passwordHash)) {
        error = "Current password is incorrect.";
        recordAudit(account->accountId, account->role, "password_change", false, "Current password failed");
        return false;
    }
    error = passwordError(newPassword);
    if (!error.empty()) return false;
    if (currentPassword == newPassword) {
        error = "New password must be different from the current password.";
        return false;
    }

    account->passwordSalt = Security::randomHex(16);
    account->passwordHash = Security::derivePasswordHash(newPassword, account->passwordSalt, PASSWORD_ROUNDS);
    saveAccounts();
    recordAudit(account->accountId, account->role, "password_change", true, "Password updated");
    return true;
}

bool AuthService::logout(const std::string& token) {
    AuthSession** stored = sessionIndex_.find(token);
    if (stored == nullptr || *stored == nullptr || !(*stored)->active) return false;
    AuthSession* activeSession = *stored;
    activeSession->active = false;
    recordAudit(activeSession->accountId, activeSession->role, "logout", true, "Session closed");
    return true;
}

bool AuthService::validateSession(
    const std::string& token,
    AccountRole requiredRole,
    const std::string& requiredAccountId
) {
    if (!ready_ || token.empty()) return false;
    AuthSession** stored = sessionIndex_.find(token);
    if (stored == nullptr || *stored == nullptr) return false;
    AuthSession* activeSession = *stored;
    if (!activeSession->active || activeSession->expiresAtEpoch <= currentEpochSeconds()) {
        activeSession->active = false;
        return false;
    }
    if (activeSession->role != requiredRole) return false;
    return requiredAccountId.empty() ||
           activeSession->accountId == canonicalAccountId(requiredAccountId, requiredRole);
}

const AuthSession* AuthService::session(const std::string& token) const {
    AuthSession* const* stored = sessionIndex_.find(token);
    if (stored == nullptr || *stored == nullptr) return nullptr;
    const AuthSession* activeSession = *stored;
    if (!activeSession->active || activeSession->expiresAtEpoch <= currentEpochSeconds()) return nullptr;
    return activeSession;
}

bool AuthService::credentialExists(
    const std::string& accountId,
    AccountRole role
) const {
    return findAccount(accountId, role) != nullptr;
}

std::size_t AuthService::accountCount() const {
    return accounts_.size();
}

std::size_t AuthService::activeSessionCount() const {
    std::size_t count = 0;
    sessions_.forEach([&count](const AuthSession& value) {
        if (value.active && value.expiresAtEpoch > currentEpochSeconds()) ++count;
    });
    return count;
}

const char* AuthService::roleName(AccountRole role) {
    return role == AccountRole::Admin ? "Admin" : "Rider";
}

std::string AuthService::passwordError(const std::string& password) {
    return Validation::passwordError(password);
}

const char* AuthService::defaultAdminUsername() {
    return "admin";
}

const char* AuthService::defaultAdminPassword() {
    return "Nexa@2026";
}

AuthAccount* AuthService::findAccount(
    const std::string& accountId,
    AccountRole role
) {
    AuthAccount** stored = accountIndex_.find(accountKey(accountId, role));
    return stored == nullptr ? nullptr : *stored;
}

const AuthAccount* AuthService::findAccount(
    const std::string& accountId,
    AccountRole role
) const {
    AuthAccount* const* stored = accountIndex_.find(accountKey(accountId, role));
    return stored == nullptr ? nullptr : *stored;
}

AuthAccount* AuthService::createAccount(
    const std::string& accountId,
    AccountRole role,
    const std::string& password
) {
    const std::string canonical = canonicalAccountId(accountId, role);
    if (canonical.empty() || credentialExists(canonical, role)) return nullptr;
    AuthAccount account;
    account.accountId = canonical;
    account.role = role;
    account.passwordSalt = Security::randomHex(16);
    account.passwordHash = Security::derivePasswordHash(password, account.passwordSalt, PASSWORD_ROUNDS);
    account.createdAtEpoch = currentEpochSeconds();
    AuthAccount* stored = accounts_.insertBack(std::move(account));
    if (stored == nullptr || !accountIndex_.insert(accountKey(canonical, role), stored)) return nullptr;
    return stored;
}

bool AuthService::ensureAccount(
    const std::string& accountId,
    AccountRole role,
    const std::string& initialPassword
) {
    if (credentialExists(accountId, role)) return true;
    return createAccount(accountId, role, initialPassword) != nullptr;
}

AuthResult AuthService::issueSession(AuthAccount& account) {
    AuthResult result;
    result.success = true;
    result.accountId = account.accountId;
    result.role = account.role;

    AuthSession sessionValue;
    do {
        sessionValue.token = Security::randomHex(32);
    } while (sessionIndex_.find(sessionValue.token) != nullptr);
    sessionValue.accountId = account.accountId;
    sessionValue.role = account.role;
    sessionValue.createdAtEpoch = currentEpochSeconds();
    sessionValue.expiresAtEpoch = sessionValue.createdAtEpoch + SESSION_SECONDS;
    result.token = sessionValue.token;
    result.expiresAtEpoch = sessionValue.expiresAtEpoch;

    AuthSession* stored = sessions_.insertBack(std::move(sessionValue));
    if (stored == nullptr || !sessionIndex_.insert(stored->token, stored)) {
        result.success = false;
        result.error = "A secure session could not be created.";
        result.token.clear();
        return result;
    }
    return result;
}

void AuthService::expireOldSessions() {
    const long long now = currentEpochSeconds();
    sessions_.forEach([now](AuthSession& value) {
        if (value.active && value.expiresAtEpoch <= now) value.active = false;
    });
}

void AuthService::recordAudit(
    const std::string& accountId,
    AccountRole role,
    const std::string& action,
    bool success,
    const std::string& detail
) {
    AuthAuditEntry entry;
    entry.timestampEpoch = currentEpochSeconds();
    entry.accountId = canonicalAccountId(accountId, role);
    entry.role = role;
    entry.action = action;
    entry.success = success;
    entry.detail = detail;
    AuthAuditEntry* stored = auditEntries_.insertBack(entry);
    if (stored != nullptr) appendAudit(*stored);
}

bool AuthService::loadAccounts() {
    std::ifstream input(runtimeDirectory_ / "accounts.db");
    if (!input) return false;
    std::string line;
    while (std::getline(input, line)) {
        std::stringstream stream(line);
        std::string fields[9];
        bool complete = true;
        for (int index = 0; index < 9; ++index) {
            if (!std::getline(stream, fields[index], '\t')) {
                complete = false;
                break;
            }
        }
        if (!complete) continue;
        try {
            AuthAccount account;
            account.role = parseRole(fields[0]);
            account.accountId = canonicalAccountId(fields[1], account.role);
            account.passwordSalt = fields[2];
            account.passwordHash = fields[3];
            account.enabled = fields[4] == "1";
            account.failedAttempts = std::stoi(fields[5]);
            account.lockedUntilEpoch = std::stoll(fields[6]);
            account.createdAtEpoch = std::stoll(fields[7]);
            account.lastLoginEpoch = std::stoll(fields[8]);
            if (account.accountId.empty() || account.passwordSalt.empty() || account.passwordHash.size() != 64) continue;
            AuthAccount* stored = accounts_.insertBack(std::move(account));
            if (stored != nullptr) accountIndex_.insert(accountKey(stored->accountId, stored->role), stored);
        } catch (...) {
            // Ignore malformed credential rows while retaining valid accounts.
        }
    }
    return !accounts_.empty();
}

void AuthService::saveAccounts() const {
    if (!persistenceEnabled_) return;
    std::ofstream output(runtimeDirectory_ / "accounts.db", std::ios::trunc);
    if (!output) return;
    accounts_.forEach([&output](const AuthAccount& account) {
        output << roleName(account.role) << '\t'
               << safeField(account.accountId) << '\t'
               << account.passwordSalt << '\t'
               << account.passwordHash << '\t'
               << (account.enabled ? 1 : 0) << '\t'
               << account.failedAttempts << '\t'
               << account.lockedUntilEpoch << '\t'
               << account.createdAtEpoch << '\t'
               << account.lastLoginEpoch << '\n';
    });
}

void AuthService::appendAudit(const AuthAuditEntry& entry) const {
    if (!persistenceEnabled_) return;
    std::ofstream output(runtimeDirectory_ / "auth_audit.db", std::ios::app);
    if (!output) return;
    output << entry.timestampEpoch << '\t'
           << roleName(entry.role) << '\t'
           << safeField(entry.accountId) << '\t'
           << safeField(entry.action) << '\t'
           << (entry.success ? 1 : 0) << '\t'
           << safeField(entry.detail) << '\n';
}

std::string AuthService::accountKey(
    const std::string& accountId,
    AccountRole role
) {
    return std::string(role == AccountRole::Admin ? "admin:" : "rider:") +
           canonicalAccountId(accountId, role);
}

std::string AuthService::canonicalAccountId(
    const std::string& accountId,
    AccountRole role
) {
    const std::string cleaned = Validation::collapseSpaces(Validation::trim(accountId));
    return role == AccountRole::Admin
        ? Validation::lowerAscii(cleaned)
        : uppercaseAscii(cleaned);
}

std::string AuthService::safeField(const std::string& value) {
    std::string safe = value;
    for (char& character : safe) {
        if (character == '\t' || character == '\r' || character == '\n') character = ' ';
    }
    return safe;
}

long long AuthService::currentEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
