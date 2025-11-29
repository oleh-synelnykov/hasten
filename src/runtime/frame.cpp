#include "hasten/runtime/frame.hpp"

#include <array>
#include <cstring>

namespace hasten::runtime
{
namespace
{

constexpr std::uint32_t crc32_for_byte(std::uint32_t r)
{
    for (int k = 0; k < 8; ++k) {
        r = (r & 1U) ? (0xEDB88320U ^ (r >> 1U)) : (r >> 1U);
    }
    return r ^ 0xFF000000U;
}

constexpr auto make_crc_table()
{
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < table.size(); ++i) {
        table[i] = crc32_for_byte(i);
    }
    return table;
}

inline std::uint32_t crc32(std::span<const std::uint8_t> data)
{
    static constexpr auto table = make_crc_table();
    std::uint32_t crc = 0;
    for (std::uint8_t byte : data) {
        crc = table[(crc ^ byte) & 0xFFU] ^ (crc >> 8U);
    }
    return crc;
}

constexpr bool valid_frame_type(std::uint8_t value)
{
    switch (static_cast<FrameType>(value)) {
        case FrameType::Data:
        case FrameType::Settings:
        case FrameType::Goodbye:
        case FrameType::Ping:
        case FrameType::Cancel:
        case FrameType::Error:
            return true;
    }
    return false;
}

inline void write_be16(std::uint8_t* dst, std::uint16_t value)
{
    dst[0] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    dst[1] = static_cast<std::uint8_t>(value & 0xFFU);
}

inline void write_be32(std::uint8_t* dst, std::uint32_t value)
{
    dst[0] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
    dst[1] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    dst[2] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    dst[3] = static_cast<std::uint8_t>(value & 0xFFU);
}

inline void write_be64(std::uint8_t* dst, std::uint64_t value)
{
    for (int i = 0; i < 8; ++i) {
        dst[i] = static_cast<std::uint8_t>((value >> (56U - (i * 8U))) & 0xFFU);
    }
}

inline std::uint16_t read_be16(const std::uint8_t* src)
{
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(src[0]) << 8U | src[1]);
}

inline std::uint32_t read_be32(const std::uint8_t* src)
{
    return (static_cast<std::uint32_t>(src[0]) << 24U) | (static_cast<std::uint32_t>(src[1]) << 16U) |
           (static_cast<std::uint32_t>(src[2]) << 8U) | static_cast<std::uint32_t>(src[3]);
}

inline std::uint64_t read_be64(const std::uint8_t* src)
{
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8U) | static_cast<std::uint64_t>(src[i]);
    }
    return value;
}

}  // namespace

Result<void> encode_header(const FrameHeader& header, std::span<std::uint8_t, FrameHeaderSize> out)
{
    FrameHeader tmp = header;
    tmp.length = header.length;
    tmp.magic = FrameHeader::Magic;
    tmp.version = FrameHeader::Version;

    write_be32(out.data(), tmp.magic);
    write_be16(out.data() + 4, tmp.version);
    out[6] = static_cast<std::uint8_t>(tmp.type);
    out[7] = tmp.flags;
    write_be32(out.data() + 8, tmp.length);
    write_be64(out.data() + 12, tmp.stream_id);
    write_be32(out.data() + 20, 0);

    std::span<const std::uint8_t> crc_span(out.data(), FrameHeaderSize - sizeof(std::uint32_t));
    tmp.header_crc = crc32(crc_span);
    write_be32(out.data() + 20, tmp.header_crc);

    return {};
}

Result<FrameHeader> decode_header(std::span<const std::uint8_t, FrameHeaderSize> buffer)
{
    FrameHeader header;
    header.magic = read_be32(buffer.data());
    if (header.magic != FrameHeader::Magic) {
        return unexpected_result<FrameHeader>(ErrorCode::TransportError, "invalid frame magic");
    }
    header.version = read_be16(buffer.data() + 4);
    if (header.version != FrameHeader::Version) {
        return unexpected_result<FrameHeader>(ErrorCode::TransportError, "unsupported frame version");
    }

    std::uint8_t type_byte = buffer[6];
    if (!valid_frame_type(type_byte)) {
        return unexpected_result<FrameHeader>(ErrorCode::TransportError, "unknown frame type");
    }
    header.type = static_cast<FrameType>(type_byte);
    header.flags = buffer[7];
    header.length = read_be32(buffer.data() + 8);
    header.stream_id = read_be64(buffer.data() + 12);
    header.header_crc = read_be32(buffer.data() + 20);

    std::span<const std::uint8_t> crc_span(buffer.data(), FrameHeaderSize - sizeof(std::uint32_t));
    auto computed_crc = crc32(crc_span);
    if (computed_crc != header.header_crc) {
        return unexpected_result<FrameHeader>(ErrorCode::TransportError, "frame header crc mismatch");
    }

    return header;
}

}  // namespace hasten::runtime
