#pragma once

#include <cstddef>

namespace WiFiPasswordValidation
{
template <typename Text>
bool isValid(const Text &password)
{
    const std::size_t length = password.length();
    if (length == 0) return true;
    if (length >= 8 && length <= 63) return true;
    if (length != 64) return false;

    for (std::size_t index = 0; index < length; ++index) {
        const char value = password[index];
        const bool hexadecimal =
            (value >= '0' && value <= '9') ||
            (value >= 'a' && value <= 'f') ||
            (value >= 'A' && value <= 'F');
        if (!hexadecimal) return false;
    }
    return true;
}
}
