#pragma once

#include "hasten/runtime/error.hpp"
#include "hasten/runtime/result.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace hasten::runtime
{

constexpr std::size_t FrameHeaderSize = 24;
constexpr std::array<char, 8> PrefaceMagic{'H', 'A', 'S', 'T', 'E', 'N', '/', '1'};

enum class FrameType : std::uint8_t {
    Data = 0x00,
    Settings = 0x01,
    Goodbye = 0x02,
    Ping = 0x03,
    Cancel = 0x04,
    Error = 0x05,
};

inline std::string_view to_string(FrameType type)
{
    switch (type) {
        case FrameType::Data:
            return "DATA";
        case FrameType::Settings:
            return "SETTINGS";
        case FrameType::Goodbye:
            return "GOODBYE";
        case FrameType::Ping:
            return "PING";
        case FrameType::Cancel:
            return "CANCEL";
        case FrameType::Error:
            return "ERROR";
    }
    return "UNKNOWN";
}

enum FrameFlags : std::uint8_t { FrameFlagEndStream = 0x01 };

struct FrameHeader {
    static constexpr std::uint32_t Magic = 0x48425331;  // HBS1
    static constexpr std::uint16_t Version = 0x0001;

    std::uint32_t magic = Magic;
    std::uint16_t version = Version;
    FrameType type = FrameType::Data;
    std::uint8_t flags = 0;
    std::uint32_t length = 0;
    std::uint64_t stream_id = 0;
    std::uint32_t header_crc = 0;
};

struct Frame {
    FrameHeader header;
    std::vector<std::uint8_t> payload;
};

Result<FrameHeader> decode_header(std::span<const std::uint8_t, FrameHeaderSize> buffer);
Result<void> encode_header(const FrameHeader& header, std::span<std::uint8_t, FrameHeaderSize> out);

}  // namespace hasten::runtime
