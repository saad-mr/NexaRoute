#pragma once

#include <cstddef>
#include <string>

namespace Validation {
std::string trim(const std::string& value);
std::string collapseSpaces(const std::string& value);
std::string lowerAscii(const std::string& value);
std::string normalizedName(const std::string& value);
bool equalsIgnoreCase(const std::string& left, const std::string& right);
bool containsControlCharacters(const std::string& value);
bool isSafePersistedText(const std::string& value, std::size_t maximumLength);
bool isIdentifier(const std::string& value, std::size_t minimumLength, std::size_t maximumLength);
bool isPersonName(const std::string& value);
bool isPhoneNumber(const std::string& value);
bool isVehicleName(const std::string& value);
bool isVehicleRegistration(const std::string& value);
bool isLocationName(const std::string& value);
bool isLocationType(const std::string& value);
bool isRouteName(const std::string& value);
bool isMapCoordinate(int x, int y);
bool isDistance(double value);
bool isTravelTime(int minutes);
bool isMoney(double value, double maximum);
bool isPasswordLengthValid(const std::string& password);
bool hasUppercase(const std::string& value);
bool hasLowercase(const std::string& value);
bool hasDigit(const std::string& value);
bool hasSymbol(const std::string& value);
std::string passwordError(const std::string& password);
}
