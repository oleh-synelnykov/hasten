#pragma once

#include <string_view>

namespace hasten::runtime
{

enum class Encoding { Hb1 };

inline std::string_view to_string(Encoding encoding)
{
    switch (encoding) {
        case Encoding::Hb1:
            return "HB1";
    }
    return "Unknown";
}

}  // namespace hasten::runtime
