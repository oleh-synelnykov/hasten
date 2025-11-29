#include "hasten/runtime/serialization/hb1.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace hasten::runtime;
using namespace hasten::runtime::hb1;

TEST(Serialization, EncodesAndDecodesPrimitiveFields)
{
    std::vector<std::uint8_t> buffer;
    VectorSink sink{buffer};
    Writer writer{sink};

    ASSERT_TRUE(writer.write_field_varint(1, 123));
    ASSERT_TRUE(writer.write_field_svarint(2, -42));
    ASSERT_TRUE(writer.write_field_string(3, "hello"));

    SpanSource source{buffer};
    Reader reader{source};

    FieldView view;
    ASSERT_TRUE(reader.next(view));
    EXPECT_EQ(view.id, 1u);
    auto varint = decode_varint(view.data);
    ASSERT_TRUE(varint);
    EXPECT_EQ(*varint, 123u);

    ASSERT_TRUE(reader.next(view));
    EXPECT_EQ(view.id, 2u);
    auto signed_value = decode_zigzag(view.data);
    ASSERT_TRUE(signed_value);
    EXPECT_EQ(*signed_value, -42);

    ASSERT_TRUE(reader.next(view));
    EXPECT_EQ(view.id, 3u);
    auto text = decode_string(view.data);
    ASSERT_TRUE(text);
    EXPECT_EQ(*text, "hello");

    auto eof = reader.next(view);
    ASSERT_TRUE(eof);
    EXPECT_FALSE(*eof);
}

TEST(Serialization, ValidatesRequiredFields)
{
    FieldDescriptor fields[]{
        FieldDescriptor{1, WireType::Varint, false},
        FieldDescriptor{3, WireType::LengthDelimited, true},
    };
    MessageDescriptor descriptor{fields};

    std::vector<std::uint8_t> buffer;
    VectorSink sink{buffer};
    Writer writer{sink};
    ASSERT_TRUE(writer.write_field_varint(1, 7));

    SpanSource source{buffer};
    Reader reader{source};
    std::vector<FieldView> views;
    FieldView view;
    while (true) {
        auto next = reader.next(view);
        ASSERT_TRUE(next);
        if (!*next) {
            break;
        }
        views.push_back(view);
    }

    EXPECT_TRUE(validate_fields(descriptor, views));
}

TEST(Serialization, EncodeDecodeRoundTrip)
{
    FieldDescriptor fields[]{
        FieldDescriptor{1, WireType::Varint, false},
        FieldDescriptor{2, WireType::ZigZagVarint, false},
        FieldDescriptor{3, WireType::LengthDelimited, true, ValueKind::String},
    };
    MessageDescriptor descriptor{fields};

    FieldValue values[]{
        FieldValue{1, WireType::Varint, Value::make_unsigned(17)},
        FieldValue{2, WireType::ZigZagVarint, Value::make_signed(-9)},
        FieldValue{3, WireType::LengthDelimited, Value::make_string("payload")},
    };

    std::vector<std::uint8_t> buffer;
    VectorSink sink{buffer};
    Writer writer{sink};
    ASSERT_TRUE(encode_message(descriptor, values, writer));

    SpanSource source{buffer};
    Reader reader{source};
    auto decoded = decode_message(descriptor, reader);
    ASSERT_TRUE(decoded);
    ASSERT_EQ(decoded->size(), 3u);
    EXPECT_EQ((*decoded)[0].value.unsigned_value, 17u);
    EXPECT_EQ((*decoded)[1].value.signed_value, -9);
    EXPECT_EQ((*decoded)[2].value.text, "payload");
}
