#include "Validation.h"

#include <cctype>
#include <cmath>

namespace {
bool isAllowedNamePunctuation(char character) {
    return character == ' ' || character == '-' || character == '\'' ||
           character == '.' || character == ',' || character == '&' || character == '(' ||
           character == ')' || character == '/';
}

bool isAsciiLetterOrDigit(char character) {
    const unsigned char value = static_cast<unsigned char>(character);
    return std::isalpha(value) != 0 || std::isdigit(value) != 0;
}
}

std::string Validation::trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

std::string Validation::collapseSpaces(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    bool previousSpace = false;
    for (const char character : value) {
        const bool space = std::isspace(static_cast<unsigned char>(character)) != 0;
        if (space) {
            if (!previousSpace && !result.empty()) result += ' ';
        } else {
            result += character;
        }
        previousSpace = space;
    }
    if (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

std::string Validation::lowerAscii(const std::string& value) {
    std::string result = value;
    for (char& character : result) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return result;
}

std::string Validation::normalizedName(const std::string& value) {
    return lowerAscii(collapseSpaces(trim(value)));
}

bool Validation::equalsIgnoreCase(const std::string& left, const std::string& right) {
    return normalizedName(left) == normalizedName(right);
}

bool Validation::containsControlCharacters(const std::string& value) {
    for (const unsigned char character : value) {
        if (character < 32U || character == 127U) return true;
    }
    return false;
}

bool Validation::isSafePersistedText(
    const std::string& value,
    std::size_t maximumLength
) {
    const std::string cleaned = trim(value);
    if (cleaned.empty() || cleaned.size() > maximumLength || containsControlCharacters(cleaned)) {
        return false;
    }
    return cleaned.find('\t') == std::string::npos &&
           cleaned.find('\n') == std::string::npos &&
           cleaned.find('\r') == std::string::npos;
}

bool Validation::isIdentifier(
    const std::string& value,
    std::size_t minimumLength,
    std::size_t maximumLength
) {
    if (value.size() < minimumLength || value.size() > maximumLength) return false;
    for (const char character : value) {
        if (!isAsciiLetterOrDigit(character) && character != '-' && character != '_') return false;
    }
    return true;
}

bool Validation::isPersonName(const std::string& value) {
    const std::string cleaned = collapseSpaces(trim(value));
    if (cleaned.size() < 2 || cleaned.size() > 50 || containsControlCharacters(cleaned)) return false;
    bool hasLetter = false;
    for (const char character : cleaned) {
        if (std::isalpha(static_cast<unsigned char>(character))) hasLetter = true;
        else if (character != ' ' && character != '-' && character != '\'') return false;
    }
    return hasLetter;
}

bool Validation::isPhoneNumber(const std::string& value) {
    if (value.size() < 10 || value.size() > 18) return false;
    int digitCount = 0;
    for (const char character : value) {
        if (std::isdigit(static_cast<unsigned char>(character))) ++digitCount;
        else if (character != '+' && character != '-' && character != ' ' && character != '(' && character != ')') return false;
    }
    return digitCount >= 10;
}

bool Validation::isVehicleName(const std::string& value) {
    const std::string cleaned = collapseSpaces(trim(value));
    return cleaned == "Bike" || cleaned == "Car" || cleaned == "Van" || cleaned == "Pickup";
}

bool Validation::isVehicleRegistration(const std::string& value) {
    if (value.size() < 4 || value.size() > 20) return false;
    bool hasLetter = false;
    bool hasDigitValue = false;
    for (const char character : value) {
        if (std::isalpha(static_cast<unsigned char>(character))) hasLetter = true;
        else if (std::isdigit(static_cast<unsigned char>(character))) hasDigitValue = true;
        else if (character != '-' && character != ' ') return false;
    }
    return hasLetter && hasDigitValue;
}

bool Validation::isLocationName(const std::string& value) {
    const std::string cleaned = collapseSpaces(trim(value));
    if (cleaned.size() < 2 || cleaned.size() > 60 || containsControlCharacters(cleaned)) return false;
    bool meaningful = false;
    for (const char character : cleaned) {
        if (isAsciiLetterOrDigit(character)) meaningful = true;
        else if (!isAllowedNamePunctuation(character)) return false;
    }
    return meaningful;
}

bool Validation::isLocationType(const std::string& value) {
    const std::string cleaned = collapseSpaces(trim(value));
    if (cleaned.size() < 2 || cleaned.size() > 30) return false;
    for (const char character : cleaned) {
        if (!std::isalpha(static_cast<unsigned char>(character)) && character != ' ' && character != '-') return false;
    }
    return true;
}

bool Validation::isRouteName(const std::string& value) {
    const std::string cleaned = collapseSpaces(trim(value));
    if (cleaned.size() < 2 || cleaned.size() > 60 || containsControlCharacters(cleaned)) return false;
    bool meaningful = false;
    for (const char character : cleaned) {
        if (isAsciiLetterOrDigit(character)) meaningful = true;
        else if (!isAllowedNamePunctuation(character) && character != '_') return false;
    }
    return meaningful;
}

bool Validation::isMapCoordinate(int x, int y) {
    return x >= 70 && x <= 1130 && y >= 60 && y <= 650;
}

bool Validation::isDistance(double value) {
    return std::isfinite(value) && value >= 0.1 && value <= 100.0;
}

bool Validation::isTravelTime(int minutes) {
    return minutes >= 1 && minutes <= 180;
}

bool Validation::isMoney(double value, double maximum) {
    return std::isfinite(value) && value >= 0.0 && value <= maximum;
}

bool Validation::isPasswordLengthValid(const std::string& password) {
    return password.size() >= 8 && password.size() <= 72 && !containsControlCharacters(password);
}

bool Validation::hasUppercase(const std::string& value) {
    for (const unsigned char character : value) if (std::isupper(character)) return true;
    return false;
}

bool Validation::hasLowercase(const std::string& value) {
    for (const unsigned char character : value) if (std::islower(character)) return true;
    return false;
}

bool Validation::hasDigit(const std::string& value) {
    for (const unsigned char character : value) if (std::isdigit(character)) return true;
    return false;
}

bool Validation::hasSymbol(const std::string& value) {
    for (const char character : value) {
        if (!std::isalnum(static_cast<unsigned char>(character))) return true;
    }
    return false;
}

std::string Validation::passwordError(const std::string& password) {
    if (!isPasswordLengthValid(password)) return "Password must contain 8 to 72 characters.";
    if (!hasUppercase(password)) return "Password must include an uppercase letter.";
    if (!hasLowercase(password)) return "Password must include a lowercase letter.";
    if (!hasDigit(password)) return "Password must include a number.";
    if (!hasSymbol(password)) return "Password must include a symbol.";
    return {};
}
