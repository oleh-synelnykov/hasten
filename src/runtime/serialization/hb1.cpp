#include "hasten/runtime/serialization/hb1.hpp"

#include <array>
#include <unordered_map>

namespace hasten::runtime::hb1
{
namespace
{

constexpr std::size_t kMaxVarintBytes = 10;

Result<void> append_bytes(PayloadSink& sink, const std::uint8_t* data, std::size_t size)
{
    return sink.append(std::span<const std::uint8_t>(data, size));
}

std::size_t encode_varint_u64(std::uint64_t value, std::uint8_t* out)
{
    std::size_t index = 0;
    while (value >= 0x80) {
        out[index++] = static_cast<std::uint8_t>(value | 0x80);
        value >>= 7;
    }
    out[index++] = static_cast<std::uint8_t>(value);
    return index;
}

}  // namespace

Writer::Writer(PayloadSink& sink)
    : sink_(&sink)
{
}

Result<void> Writer::write_varint(std::uint64_t value)
{
    std::uint8_t buffer[kMaxVarintBytes];
    auto size = encode_varint_u64(value, buffer);
    return append_bytes(*sink_, buffer, size);
}

Result<void> Writer::write_zigzag(std::int64_t value)
{
    std::uint64_t zigzag = (static_cast<std::uint64_t>(value) << 1) ^ static_cast<std::uint64_t>(value >> 63);
    return write_varint(zigzag);
}

Result<void> Writer::write_tag(std::uint32_t tag, WireType type)
{
    if (auto res = write_varint(tag); !res) {
        return res;
    }
    std::uint8_t byte = static_cast<std::uint8_t>(type);
    return sink_->append(std::span<const std::uint8_t>(&byte, 1));
}

Result<void> Writer::write_field_varint(std::uint32_t tag, std::uint64_t value)
{
    if (auto res = write_tag(tag, WireType::Varint); !res) {
        return res;
    }
    return write_varint(value);
}

Result<void> Writer::write_field_svarint(std::uint32_t tag, std::int64_t value)
{
    if (auto res = write_tag(tag, WireType::ZigZagVarint); !res) {
        return res;
    }
    return write_zigzag(value);
}

Result<void> Writer::write_field_fixed32(std::uint32_t tag, std::uint32_t value)
{
    if (auto res = write_tag(tag, WireType::Fixed32); !res) {
        return res;
    }
    std::array<std::uint8_t, 4> buf{};
    buf[0] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    buf[1] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    buf[2] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    buf[3] = static_cast<std::uint8_t>(value & 0xFF);
    return sink_->append(buf);
}

Result<void> Writer::write_field_fixed64(std::uint32_t tag, std::uint64_t value)
{
    if (auto res = write_tag(tag, WireType::Fixed64); !res) {
        return res;
    }
    std::array<std::uint8_t, 8> buf{};
    for (int i = 0; i < 8; ++i) {
        buf[i] = static_cast<std::uint8_t>((value >> (56 - i * 8)) & 0xFF);
    }
    return sink_->append(buf);
}

Result<void> Writer::write_field_bytes(std::uint32_t tag, std::span<const std::uint8_t> bytes)
{
    if (auto res = write_tag(tag, WireType::LengthDelimited); !res) {
        return res;
    }
    if (auto res = write_varint(bytes.size()); !res) {
        return res;
    }
    return sink_->append(bytes);
}

Result<void> Writer::write_field_string(std::uint32_t tag, std::string_view value)
{
    return write_field_bytes(tag, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(value.data()), value.size()));
}

Reader::Reader(PayloadSource& source)
    : source_(&source)
{
}

namespace
{

Result<std::uint64_t> read_varint(PayloadSource& source)
{
    std::uint64_t result = 0;
    int shift = 0;
    for (int i = 0; i < static_cast<int>(kMaxVarintBytes); ++i) {
        if (source.empty()) {
            return unexpected_result<std::uint64_t>(ErrorCode::TransportError, "unexpected end of payload");
        }
        auto byte_span = source.read(1);
        if (!byte_span) {
            return std::unexpected(byte_span.error());
        }
        std::uint8_t byte = (*byte_span)[0];
        result |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            return result;
        }
        shift += 7;
    }
    return unexpected_result<std::uint64_t>(ErrorCode::TransportError, "varint too long");
}

}  // namespace

Result<bool> Reader::next(FieldView& out)
{
    if (source_->empty()) {
        return false;
    }

    auto tag = read_varint(*source_);
    if (!tag) {
        return std::unexpected(tag.error());
    }
    auto type_data = source_->read(1);
    if (!type_data) {
        return std::unexpected(type_data.error());
    }

    auto wire_type = static_cast<WireType>((*type_data)[0]);
    std::span<const std::uint8_t> value;
    out.scratch.clear();

    switch (wire_type) {
        case WireType::Varint:
        case WireType::ZigZagVarint: {
            std::uint8_t buffer[kMaxVarintBytes];
            std::size_t count = 0;
            while (count < kMaxVarintBytes) {
                auto byte_span = source_->read(1);
                if (!byte_span) {
                    return std::unexpected(byte_span.error());
                }
                buffer[count++] = (*byte_span)[0];
                if ((buffer[count - 1] & 0x80) == 0) {
                    break;
                }
            }
            if ((buffer[count - 1] & 0x80) != 0) {
                return unexpected_result<bool>(ErrorCode::TransportError, "unterminated varint");
            }
            out.scratch.assign(buffer, buffer + count);
            value = std::span<const std::uint8_t>(out.scratch.data(), out.scratch.size());
            break;
        }
        case WireType::Fixed32: {
            auto bytes = source_->read(4);
            if (!bytes) {
                return std::unexpected(bytes.error());
            }
            value = *bytes;
            break;
        }
        case WireType::Fixed64: {
            auto bytes = source_->read(8);
            if (!bytes) {
                return std::unexpected(bytes.error());
            }
            value = *bytes;
            break;
        }
        case WireType::LengthDelimited: {
            auto len = read_varint(*source_);
            if (!len) {
                return std::unexpected(len.error());
            }
            auto bytes = source_->read(static_cast<std::size_t>(*len));
            if (!bytes) {
                return std::unexpected(bytes.error());
            }
            value = *bytes;
            break;
        }
        case WireType::Capability: {
            auto len = read_varint(*source_);
            if (!len) {
                return std::unexpected(len.error());
            }
            auto bytes = source_->read(static_cast<std::size_t>(*len));
            if (!bytes) {
                return std::unexpected(bytes.error());
            }
            value = *bytes;
            break;
        }
    }

    out.id = static_cast<std::uint32_t>(*tag);
    out.wire_type = wire_type;
    out.data = value;
    return true;
}

Result<std::uint64_t> decode_varint(std::span<const std::uint8_t> data)
{
    std::uint64_t result = 0;
    int shift = 0;
    for (std::size_t i = 0; i < data.size(); ++i) {
        std::uint8_t byte = data[i];
        result |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            return result;
        }
        shift += 7;
    }
    return unexpected_result<std::uint64_t>(ErrorCode::TransportError, "unterminated varint payload");
}

Result<std::int64_t> decode_zigzag(std::span<const std::uint8_t> data)
{
    auto varint = decode_varint(data);
    if (!varint) {
        return std::unexpected(varint.error());
    }
    std::uint64_t value = *varint;
    std::int64_t decoded = static_cast<std::int64_t>((value >> 1) ^ -static_cast<std::int64_t>(value & 1));
    return decoded;
}

Result<std::string> decode_string(std::span<const std::uint8_t> data)
{
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

Value Value::make_unsigned(std::uint64_t v)
{
    Value out;
    out.kind = ValueKind::Unsigned;
    out.unsigned_value = v;
    return out;
}

Value Value::make_signed(std::int64_t v)
{
    Value out;
    out.kind = ValueKind::Signed;
    out.signed_value = v;
    return out;
}

Value Value::make_string(std::string v)
{
    Value out;
    out.kind = ValueKind::String;
    out.text = std::move(v);
    return out;
}

Value Value::make_bytes(std::vector<std::uint8_t> v)
{
    Value out;
    out.kind = ValueKind::Bytes;
    out.bytes = std::move(v);
    return out;
}

namespace
{

const FieldDescriptor* find_field(const MessageDescriptor& descriptor, std::uint32_t id)
{
    for (const auto& field : descriptor.fields) {
        if (field.id == id) {
            return &field;
        }
    }
    return nullptr;
}

Result<void> encode_value(const FieldValue& value, Writer& writer)
{
    switch (value.wire_type) {
        case WireType::Varint:
            if (value.value.kind != ValueKind::Unsigned) {
                return unexpected_result(ErrorCode::InternalError, "value kind mismatch");
            }
            return writer.write_field_varint(value.id, value.value.unsigned_value);
        case WireType::ZigZagVarint:
            if (value.value.kind != ValueKind::Signed) {
                return unexpected_result(ErrorCode::InternalError, "value kind mismatch");
            }
            return writer.write_field_svarint(value.id, value.value.signed_value);
        case WireType::Fixed32:
            if (value.value.kind != ValueKind::Unsigned) {
                return unexpected_result(ErrorCode::InternalError, "value kind mismatch");
            }
            return writer.write_field_fixed32(value.id, static_cast<std::uint32_t>(value.value.unsigned_value));
        case WireType::Fixed64:
            if (value.value.kind != ValueKind::Unsigned) {
                return unexpected_result(ErrorCode::InternalError, "value kind mismatch");
            }
            return writer.write_field_fixed64(value.id, value.value.unsigned_value);
        case WireType::LengthDelimited:
            if (value.value.kind == ValueKind::String) {
                return writer.write_field_string(value.id, value.value.text);
            }
            if (value.value.kind == ValueKind::Bytes) {
                return writer.write_field_bytes(value.id, value.value.bytes);
            }
            return unexpected_result(ErrorCode::InternalError, "length-delimited field requires string/bytes");
        case WireType::Capability:
            return unexpected_result(ErrorCode::Unimplemented, "capability encoding not implemented");
    }
    return unexpected_result(ErrorCode::InternalError, "unknown wire type");
}

}  // namespace

Result<void> encode_message(const MessageDescriptor& descriptor,
                            std::span<const FieldValue> values,
                            Writer& writer)
{
    for (const auto& value : values) {
        auto* desc = find_field(descriptor, value.id);
        if (!desc) {
            return unexpected_result(ErrorCode::InternalError, "unknown field id in encode_message");
        }
        if (desc->wire_type != value.wire_type) {
            return unexpected_result(ErrorCode::InternalError, "wire type mismatch in encode_message");
        }
        if (desc->wire_type == WireType::LengthDelimited) {
            if (desc->preferred_kind == ValueKind::String && value.value.kind != ValueKind::String) {
                return unexpected_result(ErrorCode::InternalError, "length-delimited field expects string");
            }
            if (desc->preferred_kind == ValueKind::Bytes && value.value.kind != ValueKind::Bytes) {
                return unexpected_result(ErrorCode::InternalError, "length-delimited field expects bytes");
            }
        }
        if (auto res = encode_value(value, writer); !res) {
            return res;
        }
    }
    return {};
}

Result<std::vector<FieldValue>> decode_message(const MessageDescriptor& descriptor, Reader& reader)
{
    std::vector<FieldView> views;
    FieldView view;
    while (true) {
        auto next = reader.next(view);
        if (!next) {
            return std::unexpected(next.error());
        }
        if (!*next) {
            break;
        }
        views.push_back(view);
    }

    std::vector<FieldValue> values;
    for (const auto& field : views) {
        auto* desc = find_field(descriptor, field.id);
        if (!desc) {
            continue;  // unknown fields ignored
        }
        FieldValue value{};
        value.id = field.id;
        value.wire_type = field.wire_type;
        switch (field.wire_type) {
            case WireType::Varint: {
                auto decoded = decode_varint(field.data);
                if (!decoded) {
                    return std::unexpected(decoded.error());
                }
                value.value = Value::make_unsigned(*decoded);
                break;
            }
            case WireType::ZigZagVarint: {
                auto decoded = decode_zigzag(field.data);
                if (!decoded) {
                    return std::unexpected(decoded.error());
                }
                value.value = Value::make_signed(*decoded);
                break;
            }
            case WireType::Fixed32: {
                if (field.data.size() != 4) {
                    return unexpected_result<std::vector<FieldValue>>(ErrorCode::TransportError, "fixed32 length mismatch");
                }
                std::uint32_t v = (static_cast<std::uint32_t>(field.data[0]) << 24) |
                                  (static_cast<std::uint32_t>(field.data[1]) << 16) |
                                  (static_cast<std::uint32_t>(field.data[2]) << 8) |
                                  static_cast<std::uint32_t>(field.data[3]);
                value.value = Value::make_unsigned(v);
                break;
            }
            case WireType::Fixed64: {
                if (field.data.size() != 8) {
                    return unexpected_result<std::vector<FieldValue>>(ErrorCode::TransportError, "fixed64 length mismatch");
                }
                std::uint64_t v = 0;
                for (auto byte : field.data) {
                    v = (v << 8) | byte;
                }
                value.value = Value::make_unsigned(v);
                break;
            }
            case WireType::LengthDelimited: {
                if (desc->preferred_kind == ValueKind::String) {
                    std::string text(reinterpret_cast<const char*>(field.data.data()), field.data.size());
                    value.value = Value::make_string(std::move(text));
                } else {
                    value.value = Value::make_bytes(std::vector<std::uint8_t>(field.data.begin(), field.data.end()));
                }
                break;
            }
            case WireType::Capability:
                return unexpected_result<std::vector<FieldValue>>(ErrorCode::Unimplemented, "capability decoding not implemented");
        }
        values.push_back(std::move(value));
    }

    // Validate required fields.
    for (const auto& field : descriptor.fields) {
        if (field.optional) {
            continue;
        }
        bool present = false;
        for (const auto& value : values) {
            if (value.id == field.id) {
                present = true;
                break;
            }
        }
        if (!present) {
            return unexpected_result<std::vector<FieldValue>>(ErrorCode::TransportError, "missing required field");
        }
    }

    return values;
}

Result<void> validate_fields(const MessageDescriptor& descriptor, std::span<const FieldView> fields)
{
    std::unordered_map<std::uint32_t, bool> seen;
    for (const auto& field : fields) {
        auto* desc = find_field(descriptor, field.id);
        if (!desc) {
            continue;
        }
        if (desc->wire_type != field.wire_type) {
            return unexpected_result(ErrorCode::TransportError, "wire type mismatch during validation");
        }
        seen[field.id] = true;
    }

    for (const auto& desc : descriptor.fields) {
        if (!desc.optional) {
            auto it = seen.find(desc.id);
            if (it == seen.end()) {
                return unexpected_result(ErrorCode::TransportError, "missing required field");
            }
        }
    }
    return {};
}

}  // namespace hasten::runtime::hb1
