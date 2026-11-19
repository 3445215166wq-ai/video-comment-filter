#pragma once

#include <string>
#include <cstddef>

// Internal utilities (not part of public utils.h API)
static inline bool is_valid_utf8(const char* data, size_t len)
{
    const unsigned char* bytes = (const unsigned char*)data;
    size_t i = 0;
    while (i < len) {
        unsigned char c = bytes[i];
        if (c <= 0x7F) { // ASCII
            i++;
            continue;
        }
        else if ((c >> 5) == 0x6) {
            if (i + 1 >= len) return false;
            unsigned char c1 = bytes[i + 1];
            if ((c1 >> 6) != 0x2) return false;
            i += 2;
        }
        else if ((c >> 4) == 0xE) {
            if (i + 2 >= len) return false;
            unsigned char c1 = bytes[i + 1], c2 = bytes[i + 2];
            if ((c1 >> 6) != 0x2 || (c2 >> 6) != 0x2) return false;
            i += 3;
        }
        else if ((c >> 3) == 0x1E) {
            if (i + 3 >= len) return false;
            unsigned char c1 = bytes[i + 1], c2 = bytes[i + 2], c3 = bytes[i + 3];
            if ((c1 >> 6) != 0x2 || (c2 >> 6) != 0x2 || (c3 >> 6) != 0x2) return false;
            i += 4;
        }
        else {
            return false;
        }
    }
    return true;
}

static inline bool is_valid_utf8(const std::string& s)
{
    return is_valid_utf8(s.c_str(), s.size());
}
