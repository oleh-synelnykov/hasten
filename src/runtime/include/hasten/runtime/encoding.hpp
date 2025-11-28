#pragma once

#include <string_view>

namespace hasten::runtime
{

enum class Encoding { Hb1, MessagePack };

inline std::string_view to_string(Encoding encoding)
{
    switch (encoding) {
        case Encoding::Hb1:
            return "HB1";
        case Encoding::MessagePack:
            return "MessagePack";
    }
    return "Unknown";
}

}  // namespace hasten::runtime
