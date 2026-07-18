#pragma once

#include "HashTable.h"
#include "SinglyLinkedList.h"

#include <filesystem>
#include <string>

enum class AccountRole {
    Rider,
    Admin
};

struct AuthAccount {
    std::string accountId;
    AccountRole role = AccountRole::Rider;
    std::string passwordSalt;
    std::string passwordHash;
    bool enabled = true;
    int failedAttempts = 0;
    long long lockedUntilEpoch = 0;
    long long createdAtEpoch = 0;
    long long lastLoginEpoch = 0;
};

struct AuthSession {
    std::string token;
    std::string accountId;
    AccountRole role = AccountRole::Rider;
    long long createdAtEpoch = 0;
    long long expiresAtEpoch = 0;
    bool active = true;
};

struct AuthResult {
    bool success = false;
    std::string error;
    std::string token;
    std::string accountId;
    AccountRole role = AccountRole::Rider;
    long long expiresAtEpoch = 0;
};

struct AuthAuditEntry {
    long long timestampEpoch = 0;
    std::string accountId;
    AccountRole role = AccountRole::Rider;
    std::string action;
    bool success = false;
    std::string detail;
};

class AuthService {
public:
    AuthService();

    bool initialize(
        const std::filesystem::path& runtimeDirectory,
        bool persistenceEnabled
    );
    bool ready() const;

    AuthResult loginRider(const std::string& riderId, const std::string& password);
    AuthResult loginAdmin(const std::string& username, const std::string& password);
    bool registerRiderCredential(
        const std::string& riderId,
        const std::string& password,
        std::string& error
    );
    bool removeRiderCredential(const std::string& riderId);
    bool changePassword(
        const std::string& token,
        const std::string& currentPassword,
        const std::string& newPassword,
        std::string& error
    );
    bool logout(const std::string& token);

    bool validateSession(
        const std::string& token,
        AccountRole requiredRole,
        const std::string& requiredAccountId = {}
    );
    const AuthSession* session(const std::string& token) const;
    bool credentialExists(const std::string& accountId, AccountRole role) const;
    std::size_t accountCount() const;
    std::size_t activeSessionCount() const;

    static const char* roleName(AccountRole role);
    static std::string passwordError(const std::string& password);
    static const char* defaultAdminUsername();
    static const char* defaultAdminPassword();

private:
    SinglyLinkedList<AuthAccount> accounts_;
    HashTable<AuthAccount*> accountIndex_;
    SinglyLinkedList<AuthSession> sessions_;
    HashTable<AuthSession*> sessionIndex_;
    SinglyLinkedList<AuthAuditEntry> auditEntries_;
    std::filesystem::path runtimeDirectory_;
    bool persistenceEnabled_;
    bool ready_;

    AuthResult login(
        const std::string& accountId,
        const std::string& password,
        AccountRole role
    );
    AuthAccount* findAccount(const std::string& accountId, AccountRole role);
    const AuthAccount* findAccount(const std::string& accountId, AccountRole role) const;
    AuthAccount* createAccount(
        const std::string& accountId,
        AccountRole role,
        const std::string& password
    );
    bool ensureAccount(
        const std::string& accountId,
        AccountRole role,
        const std::string& initialPassword
    );
    AuthResult issueSession(AuthAccount& account);
    void expireOldSessions();
    void recordAudit(
        const std::string& accountId,
        AccountRole role,
        const std::string& action,
        bool success,
        const std::string& detail
    );
    bool loadAccounts();
    void saveAccounts() const;
    void appendAudit(const AuthAuditEntry& entry) const;
    static std::string accountKey(const std::string& accountId, AccountRole role);
    static std::string canonicalAccountId(const std::string& accountId, AccountRole role);
    static std::string safeField(const std::string& value);
    static long long currentEpochSeconds();
};
