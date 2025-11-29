#pragma once

#include "hasten/runtime/result.hpp"
#include "hasten/runtime/serialization/payload.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hasten::runtime::hb1
{

enum class WireType : std::uint8_t {
    Varint = 0,
    ZigZagVarint = 1,
    Fixed32 = 2,
    Fixed64 = 3,
    LengthDelimited = 4,
    Capability = 5,
};

struct FieldView {
    std::uint32_t id = 0;
    WireType wire_type = WireType::Varint;
    std::span<const std::uint8_t> data;
    std::vector<std::uint8_t> scratch;
};

class Writer
{
public:
    explicit Writer(PayloadSink& sink);

    result<void> write_varint(std::uint64_t value);
    result<void> write_zigzag(std::int64_t value);
    result<void> write_field_varint(std::uint32_t tag, std::uint64_t value);
    result<void> write_field_svarint(std::uint32_t tag, std::int64_t value);
    result<void> write_field_fixed32(std::uint32_t tag, std::uint32_t value);
    result<void> write_field_fixed64(std::uint32_t tag, std::uint64_t value);
    result<void> write_field_bytes(std::uint32_t tag, std::span<const std::uint8_t> bytes);
    result<void> write_field_string(std::uint32_t tag, std::string_view value);

private:
    result<void> write_tag(std::uint32_t tag, WireType type);
    PayloadSink* sink_;
};

class Reader
{
public:
    explicit Reader(PayloadSource& source);

    // Returns false on EOF.
    result<bool> next(FieldView& out);

private:
    PayloadSource* source_;
};

result<std::uint64_t> decode_varint(std::span<const std::uint8_t> data);
result<std::int64_t> decode_zigzag(std::span<const std::uint8_t> data);
result<std::string> decode_string(std::span<const std::uint8_t> data);

enum class ValueKind { Unsigned, Signed, String, Bytes };

struct Value {
    ValueKind kind = ValueKind::Unsigned;
    std::uint64_t unsigned_value = 0;
    std::int64_t signed_value = 0;
    std::string text;
    std::vector<std::uint8_t> bytes;

    static Value make_unsigned(std::uint64_t v);
    static Value make_signed(std::int64_t v);
    static Value make_string(std::string v);
    static Value make_bytes(std::vector<std::uint8_t> v);
};

struct FieldValue {
    std::uint32_t id = 0;
    WireType wire_type = WireType::Varint;
    Value value;
};

struct FieldDescriptor {
    std::uint32_t id = 0;
    WireType wire_type = WireType::Varint;
    bool optional = false;
    ValueKind preferred_kind = ValueKind::Unsigned;
};

struct MessageDescriptor {
    std::span<const FieldDescriptor> fields;
};

result<void> encode_message(const MessageDescriptor& descriptor,
                            std::span<const FieldValue> values,
                            Writer& writer);

result<std::vector<FieldValue>> decode_message(const MessageDescriptor& descriptor, Reader& reader);

result<void> validate_fields(const MessageDescriptor& descriptor, std::span<const FieldView> fields);

}  // namespace hasten::runtime::hb1
