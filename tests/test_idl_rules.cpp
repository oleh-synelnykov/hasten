#include <gtest/gtest.h>
#include <string>
#include <boost/spirit/home/x3.hpp>
#include <boost/variant/get.hpp>
#include <vector>

#include "idl/ast.hpp"
#include "idl/config.hpp"
#include "idl/rules.hpp"

using namespace hasten::idl;
using namespace hasten::idl::parser;

template <typename Rule, typename Attr>
testing::AssertionResult parse_rule(const std::string& input,
                                       Rule const& rule,
                                       Attr& out)
{
    auto first = input.begin();
    auto last = input.end();

    // error handling
    error_handler_type err_handler(first, last, std::cerr);
    auto const with_err = x3::with<x3::error_handler_tag>(std::ref(err_handler))[ rule ];

    bool ok = phrase_parse(first, last, with_err, skipper(), out);

    if (!ok) {
        return testing::AssertionFailure()
            << "parse failed for input: `" << input << "`";
    }
    if (first != last) {
        return testing::AssertionFailure()
            << "parse did not consume full input. Remaining: `"
            << std::string(first, last) << "`";
    }
    return testing::AssertionSuccess();
}

TEST(IdlRule, ParseBoolLiteral) {
    {
        bool b = false;
        ASSERT_TRUE(parse_rule("true", boolean_literal(), b));
        EXPECT_TRUE(b);
    }
    {
        bool b = true;
        ASSERT_TRUE(parse_rule("false", boolean_literal(), b));
        EXPECT_FALSE(b);
    }
}

TEST(IdlRule, ParseIntLiteralDecimal) {
    int64_t i{};
    ASSERT_TRUE(parse_rule("123", integer_literal(), i));
    EXPECT_EQ(i, 123);
    ASSERT_TRUE(parse_rule("-321", integer_literal(), i));
    EXPECT_EQ(i, -321);
    ASSERT_TRUE(parse_rule("+456", integer_literal(), i));
    EXPECT_EQ(i, 456);
}

TEST(IdlRule, ParseIntLiteralHex) {
    {
        // upper case
        int64_t i{};
        ASSERT_TRUE(parse_rule("0xFF", integer_literal(), i));
        EXPECT_EQ(i, 255);
    }
    {
        // lowercase
        int64_t i{};
        ASSERT_TRUE(parse_rule("0xab", integer_literal(), i));
        EXPECT_EQ(i, 171);
    }
    {
        // mixed case
        int64_t i{};
        ASSERT_TRUE(parse_rule("0xAf", integer_literal(), i));
        EXPECT_EQ(i, 175);
    }
    {
        // invalid
        int64_t i{};
        ASSERT_FALSE(parse_rule("0xGG", integer_literal(), i));
    }
}

TEST(IdlRule, ParseIntLiteralBinary) {
    int64_t i{};
    ASSERT_TRUE(parse_rule("0b1010", integer_literal(), i));
    EXPECT_EQ(i, 10);
}

TEST(IdlRule, ParseIntLiteralOctal) {
    int64_t i{};
    ASSERT_TRUE(parse_rule("0o17", integer_literal(), i));
    EXPECT_EQ(i, 15);
}

TEST(IdlRule, ParseFloatLiteral) {
    {
        double d = 0.0;
        ASSERT_TRUE(parse_rule("123.45", float_literal(), d));
        EXPECT_DOUBLE_EQ(d, 123.45);
    }
    {
        double d = 0.0;
        ASSERT_TRUE(parse_rule("123.45e6", float_literal(), d));
        EXPECT_DOUBLE_EQ(d, 123.45e6);
    }
    {
        double d = 0.0;
        ASSERT_TRUE(parse_rule("123.45e-6", float_literal(), d));
        EXPECT_DOUBLE_EQ(d, 123.45e-6);
    }
    {
        double d = 0.0;
        ASSERT_TRUE(parse_rule("123.45e+6", float_literal(), d));
        EXPECT_DOUBLE_EQ(d, 123.45e+6);
    }
}

TEST(IdlRule, ParseBytesLiteral) {
    {
        // upper case
        ast::Bytes b;
        ASSERT_TRUE(parse_rule("b\"DE AD BE EF\"", bytes_literal(), b));
        ASSERT_EQ(b.size(), 4);
        EXPECT_EQ(b[0], 0xDE);
        EXPECT_EQ(b[1], 0xAD);
        EXPECT_EQ(b[2], 0xBE);
        EXPECT_EQ(b[3], 0xEF);
    }
    {
        // lowercase
        ast::Bytes b;
        ASSERT_TRUE(parse_rule("b\"de ad be ef\"", bytes_literal(), b));
        ASSERT_EQ(b.size(), 4);
        EXPECT_EQ(b[0], 0xDE);
        EXPECT_EQ(b[1], 0xAD);
        EXPECT_EQ(b[2], 0xBE);
        EXPECT_EQ(b[3], 0xEF);
    }
    {
        // mixed case
        ast::Bytes b;
        ASSERT_TRUE(parse_rule("b\"De Ad Be Ef\"", bytes_literal(), b));
        ASSERT_EQ(b.size(), 4);
        EXPECT_EQ(b[0], 0xDE);
        EXPECT_EQ(b[1], 0xAD);
        EXPECT_EQ(b[2], 0xBE);
        EXPECT_EQ(b[3], 0xEF);
    }
    {
        // extra whitespace
        ast::Bytes b;
        ASSERT_TRUE(parse_rule("b\" DE AD  BE  \"", bytes_literal(), b));
        ASSERT_EQ(b.size(), 3);
        EXPECT_EQ(b[0], 0xDE);
        EXPECT_EQ(b[1], 0xAD);
        EXPECT_EQ(b[2], 0xBE);
    }
    {
        // invalid
        ast::Bytes b;
        ASSERT_FALSE(parse_rule("b\"DE AD GG\"", bytes_literal(), b));
    }
}

TEST(IdlRule, ParsePrimitiveTypes) {
    using PrimitiveKindExpectation = std::pair<std::string, ast::PrimitiveKind>;

    // clang-format off
    const std::vector<PrimitiveKindExpectation> tests {
        { "bool", ast::PrimitiveKind::Bool },
        { "i8", ast::PrimitiveKind::I8 },
        { "i16", ast::PrimitiveKind::I16 },
        { "i32", ast::PrimitiveKind::I32 },
        { "i64", ast::PrimitiveKind::I64 },
        { "u8", ast::PrimitiveKind::U8 },
        { "u16", ast::PrimitiveKind::U16 },
        { "u32", ast::PrimitiveKind::U32 },
        { "u64", ast::PrimitiveKind::U64 },
        { "f32", ast::PrimitiveKind::F32 },
        { "f64", ast::PrimitiveKind::F64 },
        { "string", ast::PrimitiveKind::String },
        { "bytes", ast::PrimitiveKind::Bytes },
    };
    // clang-format on

    for (const PrimitiveKindExpectation& e : tests) {
        const auto& [name, kind] = e;

        ast::Primitive t;
        ASSERT_TRUE(parse_rule(name, primitive_type(), t));
        EXPECT_EQ(t.kind, kind);
    }
}

TEST(IdlRule, ParseConstValue) {
    {
        ast::ConstantValue v;
        ASSERT_TRUE(parse_rule("null", const_value(), v));
        auto* nil = boost::get<ast::Null>(&v);
        ASSERT_NE(nil, nullptr);
    }
    {
        ast::ConstantValue v;
        ASSERT_TRUE(parse_rule("true", const_value(), v));
        auto* b = boost::get<bool>(&v);
        ASSERT_NE(b, nullptr);
        EXPECT_TRUE(*b);
    }
    {
        ast::ConstantValue v;
        ASSERT_TRUE(parse_rule("false", const_value(), v));
        auto* b = boost::get<bool>(&v);
        ASSERT_NE(b, nullptr);
        EXPECT_FALSE(*b);
    }
    {
        ast::ConstantValue v;
        ASSERT_TRUE(parse_rule("123", const_value(), v));
        auto* i = boost::get<int64_t>(&v);
        ASSERT_NE(i, nullptr);
        EXPECT_EQ(*i, 123);
    }
    {
        ast::ConstantValue v;
        ASSERT_TRUE(parse_rule("123.45", const_value(), v));
        auto* d = boost::get<double>(&v);
        ASSERT_NE(d, nullptr);
        EXPECT_EQ(*d, 123.45);
    }
    {
        ast::ConstantValue v;
        ASSERT_TRUE(parse_rule("\"hello\"", const_value(), v));
        auto* s = boost::get<std::string>(&v);
        ASSERT_NE(s, nullptr);
        EXPECT_EQ(*s, "hello");
    }
    {
        ast::ConstantValue v;
        ASSERT_TRUE(parse_rule("a.b", const_value(), v));
        auto* q = boost::get<ast::QualifiedIdentifier>(&v);
        ASSERT_NE(q, nullptr);
        EXPECT_EQ(q->parts.size(), 2);
        EXPECT_EQ(q->parts[0], "a");
        EXPECT_EQ(q->parts[1], "b");
    }
    {
        ast::ConstantValue v;
        ASSERT_TRUE(parse_rule("b\"DE AD BE EF\"", const_value(), v));
        auto* b = boost::get<ast::Bytes>(&v);
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(b->size(), 4);
        EXPECT_EQ((*b)[0], 0xDE);
        EXPECT_EQ((*b)[1], 0xAD);
        EXPECT_EQ((*b)[2], 0xBE);
        EXPECT_EQ((*b)[3], 0xEF);
    }
}

TEST(IdlRule, ParseIdentifier) {
    std::string s;
    ASSERT_TRUE(parse_rule("a", identifier(), s));
    EXPECT_EQ(s, "a");

    // identifier includes keywords
    EXPECT_TRUE(parse_rule("vector", identifier(), s));
    EXPECT_TRUE(parse_rule("bool", identifier(), s));
}

TEST(IdlRule, ParseQualifiedIdentifier) {
    ast::QualifiedIdentifier s;
    ASSERT_TRUE(parse_rule("a.b", qualified_identifier(), s));
    ASSERT_EQ(s.parts.size(), 2);
    EXPECT_EQ(s.parts[0], std::string("a"));
    EXPECT_EQ(s.parts[1], std::string("b"));
}

TEST(IdlRule, ParseUserType) {
    ast::UserType s;
    ASSERT_TRUE(parse_rule("a.b", user_type(), s));
    ASSERT_EQ(s.name.parts.size(), 2);
    EXPECT_EQ(s.name.parts[0], std::string("a"));
    EXPECT_EQ(s.name.parts[1], std::string("b"));
}

TEST(IdlRule, ParseVectorType) {
    using VectorTypeExpectation = std::pair<std::string, ast::PrimitiveKind>;

    // clang-format off
    const std::vector<VectorTypeExpectation> tests {
        { "vector<bool>", ast::PrimitiveKind::Bool },
        { "vector<i8>", ast::PrimitiveKind::I8 },
        { "vector<i16>", ast::PrimitiveKind::I16 },
        { "vector<i32>", ast::PrimitiveKind::I32 },
        { "vector<i64>", ast::PrimitiveKind::I64 },
        { "vector<u8>", ast::PrimitiveKind::U8 },
        { "vector<u16>", ast::PrimitiveKind::U16 },
        { "vector<u32>", ast::PrimitiveKind::U32 },
        { "vector<u64>", ast::PrimitiveKind::U64 },
        { "vector<f32>", ast::PrimitiveKind::F32 },
        { "vector<f64>", ast::PrimitiveKind::F64 },
        { "vector<string>", ast::PrimitiveKind::String },
        { "vector<bytes>", ast::PrimitiveKind::Bytes },
    };
    // clang-format on

    for (const VectorTypeExpectation& e : tests) {
        const auto& [name, kind] = e;

        ast::Vector t;
        ASSERT_TRUE(parse_rule(name, vector_type(), t));
        auto* inner = boost::get<ast::Primitive>(&t.element);
        ASSERT_NE(inner, nullptr);
        EXPECT_EQ(inner->kind, kind);
    }
}

TEST(IdlRule, ParseMapType) {
    using MapTypeExpectation = std::tuple<std::string, ast::PrimitiveKind, ast::PrimitiveKind>;

    // clang-format off
    const std::vector<MapTypeExpectation> tests {
        { "map<bool, bool>", ast::PrimitiveKind::Bool, ast::PrimitiveKind::Bool },
        { "map<i8, i8>", ast::PrimitiveKind::I8, ast::PrimitiveKind::I8 },
        { "map<i16, i16>", ast::PrimitiveKind::I16, ast::PrimitiveKind::I16 },
        { "map<i32, i32>", ast::PrimitiveKind::I32, ast::PrimitiveKind::I32 },
        { "map<i64, i64>", ast::PrimitiveKind::I64, ast::PrimitiveKind::I64 },
        { "map<u8, u8>", ast::PrimitiveKind::U8, ast::PrimitiveKind::U8 },
        { "map<u16, u16>", ast::PrimitiveKind::U16, ast::PrimitiveKind::U16 },
        { "map<u32, u32>", ast::PrimitiveKind::U32, ast::PrimitiveKind::U32 },
        { "map<u64, u64>", ast::PrimitiveKind::U64, ast::PrimitiveKind::U64 },
        { "map<f32, f32>", ast::PrimitiveKind::F32, ast::PrimitiveKind::F32 },
        { "map<f64, f64>", ast::PrimitiveKind::F64, ast::PrimitiveKind::F64 },
        { "map<string, string>", ast::PrimitiveKind::String, ast::PrimitiveKind::String },
        { "map<bytes, bytes>", ast::PrimitiveKind::Bytes, ast::PrimitiveKind::Bytes },
        { "map<bool, bytes>", ast::PrimitiveKind::Bool, ast::PrimitiveKind::Bytes },
        { "map<i8, string>", ast::PrimitiveKind::I8, ast::PrimitiveKind::String },
        { "map<i16, f64>", ast::PrimitiveKind::I16, ast::PrimitiveKind::F64 },
        { "map<i32, f32>", ast::PrimitiveKind::I32, ast::PrimitiveKind::F32 },
        { "map<i64, u64>", ast::PrimitiveKind::I64, ast::PrimitiveKind::U64 },
        { "map<u8, u32>", ast::PrimitiveKind::U8, ast::PrimitiveKind::U32 },
        { "map<u16, u8>", ast::PrimitiveKind::U16, ast::PrimitiveKind::U8 },
        { "map<u32, u16>", ast::PrimitiveKind::U32, ast::PrimitiveKind::U16 },
        { "map<u64, i16>", ast::PrimitiveKind::U64, ast::PrimitiveKind::I16 },
        { "map<f32, i32>", ast::PrimitiveKind::F32, ast::PrimitiveKind::I32 },
        { "map<f64, i16>", ast::PrimitiveKind::F64, ast::PrimitiveKind::I16 },
        { "map<string, i8>", ast::PrimitiveKind::String, ast::PrimitiveKind::I8 },
        { "map<bytes, bool>", ast::PrimitiveKind::Bytes, ast::PrimitiveKind::Bool },
    };
    // clang-format on

    for (const MapTypeExpectation& e : tests) {
        const auto& [name, key, value] = e;

        ast::Map t;
        ASSERT_TRUE(parse_rule(name, map_type(), t));
        auto* innerKey = boost::get<ast::Primitive>(&t.key);
        auto* innerValue = boost::get<ast::Primitive>(&t.value);
        ASSERT_NE(innerKey, nullptr);
        ASSERT_NE(innerValue, nullptr);
        EXPECT_EQ(innerKey->kind, key);
        EXPECT_EQ(innerValue->kind, value);
    }
}

TEST(IdlRule, ParseOptionalType) {
    using OptionalTypeExpectation = std::pair<std::string, ast::PrimitiveKind>;

    // clang-format off
    const std::vector<OptionalTypeExpectation> tests {
        { "optional<bool>", ast::PrimitiveKind::Bool },
        { "optional<i8>", ast::PrimitiveKind::I8 },
        { "optional<i16>", ast::PrimitiveKind::I16 },
        { "optional<i32>", ast::PrimitiveKind::I32 },
        { "optional<i64>", ast::PrimitiveKind::I64 },
        { "optional<u8>", ast::PrimitiveKind::U8 },
        { "optional<u16>", ast::PrimitiveKind::U16 },
        { "optional<u32>", ast::PrimitiveKind::U32 },
        { "optional<u64>", ast::PrimitiveKind::U64 },
        { "optional<f32>", ast::PrimitiveKind::F32 },
        { "optional<f64>", ast::PrimitiveKind::F64 },
        { "optional<string>", ast::PrimitiveKind::String },
        { "optional<bytes>", ast::PrimitiveKind::Bytes },
    };
    // clang-format on

    for (const OptionalTypeExpectation& e : tests) {
        const auto& [name, kind] = e;

        ast::Optional t;
        ASSERT_TRUE(parse_rule(name, optional_type(), t));
        auto* inner = boost::get<ast::Primitive>(&t.inner);
        ASSERT_NE(inner, nullptr);
        EXPECT_EQ(inner->kind, kind);
    }
}

TEST(IdlRule, ParseType) {
    {
        ast::Type t;
        ASSERT_TRUE(parse_rule("bool", type(), t));
        auto* inner = boost::get<ast::Primitive>(&t);
        ASSERT_NE(inner, nullptr);
        EXPECT_EQ(inner->kind, ast::PrimitiveKind::Bool);
    }
    {
        ast::Type t;
        ASSERT_TRUE(parse_rule("vector<i8>", type(), t));
        auto* inner = boost::get<ast::Vector>(&t);
        ASSERT_NE(inner, nullptr);
        auto* innerInner = boost::get<ast::Primitive>(&inner->element);
        ASSERT_NE(innerInner, nullptr);
        EXPECT_EQ(innerInner->kind, ast::PrimitiveKind::I8);
    }
    {
        ast::Type t;
        ASSERT_TRUE(parse_rule("map<i8, i16>", type(), t));
        auto* inner = boost::get<ast::Map>(&t);
        ASSERT_NE(inner, nullptr);
        auto* innerKey = boost::get<ast::Primitive>(&inner->key);
        auto* innerValue = boost::get<ast::Primitive>(&inner->value);
        ASSERT_NE(innerKey, nullptr);
        ASSERT_NE(innerValue, nullptr);
        EXPECT_EQ(innerKey->kind, ast::PrimitiveKind::I8);
        EXPECT_EQ(innerValue->kind, ast::PrimitiveKind::I16);
    }
    {
        ast::Type t;
        ASSERT_TRUE(parse_rule("optional<bool>", type(), t));
        auto* inner = boost::get<ast::Optional>(&t);
        ASSERT_NE(inner, nullptr);
        auto* innerInner = boost::get<ast::Primitive>(&inner->inner);
        ASSERT_NE(innerInner, nullptr);
        EXPECT_EQ(innerInner->kind, ast::PrimitiveKind::Bool);
    }
    {
        ast::Type t;
        ASSERT_TRUE(parse_rule("user.type", type(), t));
        auto* inner = boost::get<ast::UserType>(&t);
        ASSERT_NE(inner, nullptr);
        ASSERT_EQ(inner->name.parts.size(), 2);
        EXPECT_EQ(inner->name.parts[0], "user");
        EXPECT_EQ(inner->name.parts[1], "type");
    }
}

TEST(IdlRule, ParseName) {
    std::string id;
    EXPECT_TRUE(parse_rule("foo_bar123", name(), id));
    EXPECT_EQ(id, "foo_bar123");

    // Check that reserved keywords are not allowed
    EXPECT_FALSE(parse_rule("module", name(), id));
    EXPECT_FALSE(parse_rule("import", name(), id));
    EXPECT_FALSE(parse_rule("interface", name(), id));
    EXPECT_FALSE(parse_rule("struct", name(), id));
    EXPECT_FALSE(parse_rule("enum", name(), id));
    EXPECT_FALSE(parse_rule("const", name(), id));
    EXPECT_FALSE(parse_rule("rpc", name(), id));
    EXPECT_FALSE(parse_rule("oneway", name(), id));
    EXPECT_FALSE(parse_rule("stream", name(), id));
    EXPECT_FALSE(parse_rule("notify", name(), id));
    EXPECT_FALSE(parse_rule("vector", name(), id));
    EXPECT_FALSE(parse_rule("map", name(), id));
    EXPECT_FALSE(parse_rule("optional", name(), id));
    EXPECT_FALSE(parse_rule("null", name(), id));
    EXPECT_FALSE(parse_rule("bool", name(), id));
    EXPECT_FALSE(parse_rule("i8", name(), id));
    EXPECT_FALSE(parse_rule("i16", name(), id));
    EXPECT_FALSE(parse_rule("i32", name(), id));
    EXPECT_FALSE(parse_rule("i64", name(), id));
    EXPECT_FALSE(parse_rule("u8", name(), id));
    EXPECT_FALSE(parse_rule("u16", name(), id));
    EXPECT_FALSE(parse_rule("u32", name(), id));
    EXPECT_FALSE(parse_rule("u64", name(), id));
    EXPECT_FALSE(parse_rule("f32", name(), id));
    EXPECT_FALSE(parse_rule("f64", name(), id));
    EXPECT_FALSE(parse_rule("string", name(), id));
    EXPECT_FALSE(parse_rule("bytes", name(), id));
}

TEST(IdlRule, ParseAttribute) {
    ast::Attribute t;
    ASSERT_TRUE(parse_rule("a", attribute(), t));
    EXPECT_EQ(t.name, "a");
    EXPECT_FALSE(t.value.has_value());
}

TEST(IdlRule, ParseAttributeWithValue) {
    {
        ast::Attribute t;
        ASSERT_TRUE(parse_rule("a=true", attribute(), t));
        EXPECT_EQ(t.name, "a");
        ASSERT_TRUE(t.value.has_value());
        bool* b = boost::get<bool>(&t.value.value());
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(*b, true);
    }
    {
        // spaces
        ast::Attribute t;
        ASSERT_TRUE(parse_rule("a = true", attribute(), t));
        EXPECT_EQ(t.name, "a");
        ASSERT_TRUE(t.value.has_value());
        bool* b = boost::get<bool>(&t.value.value());
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(*b, true);
    }
    {
        ast::Attribute t;
        ASSERT_TRUE(parse_rule("a=123", attribute(), t));
        EXPECT_EQ(t.name, "a");
        ASSERT_TRUE(t.value.has_value());
        int64_t* b = boost::get<int64_t>(&t.value.value());
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(*b, 123);
    }
    {
        ast::Attribute t;
        ASSERT_TRUE(parse_rule("a=\"123\"", attribute(), t));
        EXPECT_EQ(t.name, "a");
        ASSERT_TRUE(t.value.has_value());
        std::string* b = boost::get<std::string>(&t.value.value());
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(*b, "123");
    }
    {
        ast::Attribute t;
        ASSERT_TRUE(parse_rule("a=b\"DEADBEEF\"", attribute(), t));
        EXPECT_EQ(t.name, "a");
        ASSERT_TRUE(t.value.has_value());
        ast::Bytes* b = boost::get<ast::Bytes>(&t.value.value());
        ASSERT_NE(b, nullptr);
        ast::Bytes& val = *b;
        EXPECT_EQ(val.size(), 4);
        EXPECT_EQ(val[0], 0xDE);
        EXPECT_EQ(val[1], 0xAD);
        EXPECT_EQ(val[2], 0xBE);
        EXPECT_EQ(val[3], 0xEF);
    }
}

TEST(IdlRule, ParseAttributeList) {
    ast::AttributeList t;
    ASSERT_TRUE(parse_rule("[a=true, b=123, c=\"123\", d=b\"DEADBEEF\"]", attribute_list(), t));
    ASSERT_EQ(t.size(), 4);

    EXPECT_EQ(t[0].name, "a");
    ASSERT_TRUE(t[0].value.has_value());
    bool* b = boost::get<bool>(&t[0].value.value());
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(*b, true);

    EXPECT_EQ(t[1].name, "b");
    ASSERT_TRUE(t[1].value.has_value());
    int64_t* b2 = boost::get<int64_t>(&t[1].value.value());
    ASSERT_NE(b2, nullptr);
    EXPECT_EQ(*b2, 123);

    EXPECT_EQ(t[2].name, "c");
    ASSERT_TRUE(t[2].value.has_value());
    std::string* b3 = boost::get<std::string>(&t[2].value.value());
    ASSERT_NE(b3, nullptr);
    EXPECT_EQ(*b3, "123");

    EXPECT_EQ(t[3].name, "d");
    ASSERT_TRUE(t[3].value.has_value());
    ast::Bytes* b4 = boost::get<ast::Bytes>(&t[3].value.value());
    ASSERT_NE(b4, nullptr);
    ast::Bytes& val = *b4;
    ASSERT_EQ(val.size(), 4);
    EXPECT_EQ(val[0], 0xDE);
    EXPECT_EQ(val[1], 0xAD);
    EXPECT_EQ(val[2], 0xBE);
    EXPECT_EQ(val[3], 0xEF);
}

TEST(IdlRule, ParseField) {
    {
        ast::Field t;
        ASSERT_TRUE(parse_rule("5: bool x;", field(), t));
        EXPECT_EQ(t.id, 5);

        auto* type = boost::get<ast::Primitive>(&t.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(t.name, "x");
        EXPECT_FALSE(t.default_value.has_value());
        EXPECT_TRUE(t.attrs.empty());
    }
    {
        ast::Field t;
        ASSERT_TRUE(parse_rule("5: bool x = true;", field(), t));
        EXPECT_EQ(t.id, 5);

        auto* type = boost::get<ast::Primitive>(&t.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(t.name, "x");
        ASSERT_TRUE(t.default_value.has_value());
        bool* b = boost::get<bool>(&t.default_value.value());
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(*b, true);
        EXPECT_TRUE(t.attrs.empty());
    }
    {
        // with attribute list
        ast::Field t;
        ASSERT_TRUE(parse_rule("5: bool x = true [a=true, b=123, c=\"123\", d=b\"DEADBEEF\"];", field(), t));
        EXPECT_EQ(t.id, 5);

        auto* type = boost::get<ast::Primitive>(&t.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(t.name, "x");
        ASSERT_TRUE(t.default_value.has_value());
        bool* b = boost::get<bool>(&t.default_value.value());
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(*b, true);
        EXPECT_EQ(t.attrs.size(), 4);
        EXPECT_EQ(t.attrs[0].name, "a");
        EXPECT_EQ(t.attrs[1].name, "b");
        EXPECT_EQ(t.attrs[2].name, "c");
        EXPECT_EQ(t.attrs[3].name, "d");
    }
}

TEST(IdlRule, ParseParam) {
    {
        ast::Parameter t;
        ASSERT_TRUE(parse_rule("5: bool x", param(), t));
        EXPECT_EQ(t.id, 5);

        auto* type = boost::get<ast::Primitive>(&t.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(t.name, "x");
        EXPECT_FALSE(t.default_value.has_value());
        EXPECT_TRUE(t.attrs.empty());
    }
    {
        ast::Parameter t;
        ASSERT_TRUE(parse_rule("5: bool x = true", param(), t));
        EXPECT_EQ(t.id, 5);

        auto* type = boost::get<ast::Primitive>(&t.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(t.name, "x");
        ASSERT_TRUE(t.default_value.has_value());
        bool* b = boost::get<bool>(&t.default_value.value());
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(*b, true);
        EXPECT_TRUE(t.attrs.empty());
    }
    {
        // with attribute list
        ast::Parameter t;
        ASSERT_TRUE(parse_rule("5: bool x = true [a=true, b=123, c=\"123\", d=b\"DEADBEEF\"]", param(), t));
        EXPECT_EQ(t.id, 5);

        auto* type = boost::get<ast::Primitive>(&t.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(t.name, "x");
        ASSERT_TRUE(t.default_value.has_value());
        bool* b = boost::get<bool>(&t.default_value.value());
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(*b, true);
        EXPECT_EQ(t.attrs.size(), 4);
        EXPECT_EQ(t.attrs[0].name, "a");
        EXPECT_EQ(t.attrs[1].name, "b");
        EXPECT_EQ(t.attrs[2].name, "c");
        EXPECT_EQ(t.attrs[3].name, "d");
    }
}

TEST(IdlRule, ParseRetField) {
    {
        ast::Field t;
        ASSERT_TRUE(parse_rule("1: bool x", ret_field(), t));
        EXPECT_EQ(t.id, 1);

        auto* type = boost::get<ast::Primitive>(&t.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(t.name, "x");
        EXPECT_FALSE(t.default_value.has_value());
        EXPECT_TRUE(t.attrs.empty());
    }
}

TEST(IdlRule, ParseRetFields) {
    {
        std::vector<ast::Field> t;
        ASSERT_TRUE(parse_rule("(1: bool x)", ret_fields(), t));
        ASSERT_EQ(t.size(), 1);
        EXPECT_EQ(t[0].id, 1);

        auto* type = boost::get<ast::Primitive>(&t[0].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(t[0].name, "x");
        EXPECT_FALSE(t[0].default_value.has_value());
        EXPECT_TRUE(t[0].attrs.empty());
    }

    {
        std::vector<ast::Field> t;
        ASSERT_TRUE(parse_rule("(1: bool x, 2: i64 y)", ret_fields(), t));
        ASSERT_EQ(t.size(), 2);
        EXPECT_EQ(t[0].id, 1);
        EXPECT_EQ(t[1].id, 2);

        auto* type = boost::get<ast::Primitive>(&t[0].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(t[0].name, "x");
        EXPECT_FALSE(t[0].default_value.has_value());
        EXPECT_TRUE(t[0].attrs.empty());

        type = boost::get<ast::Primitive>(&t[1].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::I64);

        EXPECT_EQ(t[1].name, "y");
        EXPECT_FALSE(t[1].default_value.has_value());
        EXPECT_TRUE(t[1].attrs.empty());
    }
}


TEST(IdlRule, ParseResult) {
    {
        ast::Result t;
        ASSERT_TRUE(parse_rule("bool", result(), t));
        EXPECT_EQ(t.which(), 0);
        auto* type = boost::get<ast::Type>(&t);
        auto* primitive = boost::get<ast::Primitive>(type);
        ASSERT_NE(primitive, nullptr);
        EXPECT_EQ(primitive->kind, ast::PrimitiveKind::Bool);
    }
    {
        ast::Result t;
        ASSERT_TRUE(parse_rule("(1: bool x, 2: i64 y)", result(), t));
        EXPECT_EQ(t.which(), 1);
        auto* fields = boost::get<std::vector<ast::Field>>(&t);
        ASSERT_EQ(fields->size(), 2);

        EXPECT_EQ(fields->at(0).id, 1);
        EXPECT_EQ(fields->at(0).name, "x");
        auto* type = boost::get<ast::Primitive>(&fields->at(0).type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(fields->at(1).id, 2);
        EXPECT_EQ(fields->at(1).name, "y");
        type = boost::get<ast::Primitive>(&fields->at(1).type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::I64);
    }
}


TEST(IdlRule, ParseConstDecl) {
    {
        ast::ConstantDeclaration t;
        ASSERT_TRUE(parse_rule("const bool x = true;", const_decl(), t));

        auto* type = boost::get<ast::Primitive>(&t.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(t.name, "x");

        ASSERT_FALSE(t.value.empty());
        bool* b = boost::get<bool>(&t.value);
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(*b, true);
    }
    {
        ast::ConstantDeclaration t;
        ASSERT_TRUE(parse_rule("const string str = \"hello\";", const_decl(), t));

        auto* type = boost::get<ast::Primitive>(&t.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::String);

        EXPECT_EQ(t.name, "str");

        ASSERT_FALSE(t.value.empty());
        std::string* s = boost::get<std::string>(&t.value);
        ASSERT_NE(s, nullptr);
        EXPECT_EQ(*s, "hello");
    }
}


TEST(IdlRule, ParseEnumItem) {
    {
        ast::Enumerator t;
        ASSERT_TRUE(parse_rule("A", enum_item(), t));
        EXPECT_EQ(t.name, "A");
    }
    {
        ast::Enumerator t;
        ASSERT_TRUE(parse_rule("A = 123", enum_item(), t));
        EXPECT_EQ(t.name, "A");
        EXPECT_EQ(t.value, 123);
    }
}


TEST(IdlRule, ParseEnumDecl) {
    {
        ast::Enum t;
        ASSERT_TRUE(parse_rule("enum E { A }", enum_decl(), t));
        EXPECT_EQ(t.name, "E");
        ASSERT_EQ(t.items.size(), 1);
        EXPECT_EQ(t.items[0].name, "A");
    }
    {
        ast::Enum t;
        ASSERT_TRUE(parse_rule("enum E { A, B, C }", enum_decl(), t));
        EXPECT_EQ(t.name, "E");
        ASSERT_EQ(t.items.size(), 3);
        EXPECT_EQ(t.items[0].name, "A");
        EXPECT_EQ(t.items[1].name, "B");
        EXPECT_EQ(t.items[2].name, "C");
    }
    {
        ast::Enum t;
        ASSERT_TRUE(parse_rule("enum E { A = 123, B = 456, C = 789 }", enum_decl(), t));
        EXPECT_EQ(t.name, "E");
        ASSERT_EQ(t.items.size(), 3);
        EXPECT_EQ(t.items[0].name, "A");
        EXPECT_EQ(t.items[0].value, 123);
        EXPECT_EQ(t.items[1].name, "B");
        EXPECT_EQ(t.items[1].value, 456);
        EXPECT_EQ(t.items[2].name, "C");
        EXPECT_EQ(t.items[2].value, 789);
    }
    {
        ast::Enum t;
        ASSERT_TRUE(parse_rule(R"(enum E {
            A = 123, // no attributes for this one
            B = 456 [a=false, b=456, c="456", d=b"DEADBEEF"],
            C = 789 [a=true, b=789, c="789", d=b"DEADBEEF"], // even with the trailing comma
        })", enum_decl(), t));
        EXPECT_EQ(t.name, "E");
        ASSERT_EQ(t.items.size(), 3);
        EXPECT_EQ(t.items[0].name, "A");
        EXPECT_EQ(t.items[0].value, 123);
        EXPECT_EQ(t.items[1].name, "B");
        EXPECT_EQ(t.items[1].value, 456);
        EXPECT_EQ(t.items[2].name, "C");
        EXPECT_EQ(t.items[2].value, 789);
    }
}

TEST(IdlRule, ParseStructDecl) {
    {
        // no semicolon at the end
        ast::Struct t;
        ASSERT_TRUE(parse_rule(R"(struct a { 1: bool x; })", struct_decl(), t));
        EXPECT_EQ(t.name, "a");
        ASSERT_EQ(t.fields.size(), 1);
        EXPECT_EQ(t.fields[0].id, 1);
        EXPECT_EQ(t.fields[0].name, "x");
        auto* type = boost::get<ast::Primitive>(&t.fields[0].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);
    }
    {
        // semicolon at the end
        ast::Struct t;
        ASSERT_TRUE(parse_rule(R"(struct a { 1: bool x; 2: i64 y; };)", struct_decl(), t));
        EXPECT_EQ(t.name, "a");
        ASSERT_EQ(t.fields.size(), 2);
        EXPECT_EQ(t.fields[0].id, 1);
        EXPECT_EQ(t.fields[0].name, "x");
        auto* type = boost::get<ast::Primitive>(&t.fields[0].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);
        EXPECT_EQ(t.fields[1].id, 2);
        EXPECT_EQ(t.fields[1].name, "y");
        type = boost::get<ast::Primitive>(&t.fields[1].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::I64);
    }

    {
        ast::Struct t;
        ASSERT_TRUE(parse_rule(R"(
            struct S {
                1: bool a [a=true];
                2: i64 b; // comment
            };
        )", struct_decl(), t));
        EXPECT_EQ(t.name, "S");
        ASSERT_EQ(t.fields.size(), 2);

        EXPECT_EQ(t.fields[0].id, 1);
        EXPECT_EQ(t.fields[0].name, "a");
        auto* type = boost::get<ast::Primitive>(&t.fields[0].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);

        EXPECT_EQ(t.fields[1].id, 2);
        EXPECT_EQ(t.fields[1].name, "b");
        type = boost::get<ast::Primitive>(&t.fields[1].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::I64);
    }
}

TEST(IdlRule, ParseMethodKind) {
    {
        ast::MethodKind t;
        ASSERT_TRUE(parse_rule("rpc", method_kind(), t));
        EXPECT_EQ(t, ast::MethodKind::Rpc);
    }
    {
        ast::MethodKind t;
        ASSERT_TRUE(parse_rule("oneway", method_kind(), t));
        EXPECT_EQ(t, ast::MethodKind::Oneway);
    }
    {
        ast::MethodKind t;
        ASSERT_TRUE(parse_rule("stream", method_kind(), t));
        EXPECT_EQ(t, ast::MethodKind::Stream);
    }
    {
        ast::MethodKind t;
        ASSERT_TRUE(parse_rule("notify", method_kind(), t));
        EXPECT_EQ(t, ast::MethodKind::Notify);
    }
}


TEST(IdlRule, ParseMethod) {
    {
        ast::Method t;
        ASSERT_TRUE(parse_rule(R"(
            rpc func (1: bool x [a=true]) -> bool;
        )", method(), t));
        EXPECT_EQ(t.kind, ast::MethodKind::Rpc);
        EXPECT_EQ(t.name, "func");

        ASSERT_EQ(t.params.size(), 1);

        EXPECT_EQ(t.params[0].id, 1);
        EXPECT_EQ(t.params[0].name, "x");
        auto* type = boost::get<ast::Primitive>(&t.params[0].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);
        EXPECT_FALSE(t.params[0].default_value.has_value());
        ASSERT_EQ(t.params[0].attrs.size(), 1);
        EXPECT_EQ(t.params[0].attrs[0].name, "a");
        EXPECT_TRUE(t.params[0].attrs[0].value.has_value());
        bool* attr_value = boost::get<bool>(&t.params[0].attrs[0].value.value());
        ASSERT_NE(attr_value, nullptr);
        EXPECT_EQ(*attr_value, true);

        EXPECT_TRUE(t.result.has_value());
        EXPECT_EQ(t.result->which(), 0);
        auto* result_type = boost::get<ast::Type>(&t.result.value());
        ASSERT_NE(result_type, nullptr);
        auto* primitive = boost::get<ast::Primitive>(result_type);
        ASSERT_NE(primitive, nullptr);
        EXPECT_EQ(primitive->kind, ast::PrimitiveKind::Bool);
    }
    {
        // oneway, default param value and no result
        ast::Method t;
        ASSERT_TRUE(parse_rule(R"(
            oneway func (1: bool x = false [a=true]);
        )", method(), t));
        EXPECT_EQ(t.kind, ast::MethodKind::Oneway);
        EXPECT_EQ(t.name, "func");

        ASSERT_EQ(t.params.size(), 1);

        EXPECT_EQ(t.params[0].id, 1);
        EXPECT_EQ(t.params[0].name, "x");
        auto* type = boost::get<ast::Primitive>(&t.params[0].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);
        EXPECT_TRUE(t.params[0].default_value.has_value());
        bool* default_value = boost::get<bool>(&t.params[0].default_value.value());
        ASSERT_NE(default_value, nullptr);
        EXPECT_EQ(*default_value, false);
        ASSERT_EQ(t.params[0].attrs.size(), 1);
        EXPECT_EQ(t.params[0].attrs[0].name, "a");
        EXPECT_TRUE(t.params[0].attrs[0].value.has_value());
        bool* attr_value = boost::get<bool>(&t.params[0].attrs[0].value.value());
        ASSERT_NE(attr_value, nullptr);
        EXPECT_EQ(*attr_value, true);

        EXPECT_FALSE(t.result.has_value());
    }
}


TEST(IdlRule, ParseInterfaceDecl) {
    {
        ast::Interface t;
        ASSERT_TRUE(parse_rule(R"(
            interface MyInterface {
                rpc    method1 (1: bool x [a=true]) -> bool;
                oneway method2 ();
                stream method3 ( 1: i32 x ) -> ( 1: i32 y, 2: f64 z );
                notify method4 ( 1: bool b, 2: i64 y );
            };
        )", interface_decl(), t));
        EXPECT_EQ(t.name, "MyInterface");
        ASSERT_EQ(t.methods.size(), 4);

        {
            ast::Method& method1 = t.methods[0];
            EXPECT_EQ(method1.name, "method1");
            EXPECT_EQ(method1.kind, ast::MethodKind::Rpc);

            ASSERT_EQ(method1.params.size(), 1);
            EXPECT_EQ(method1.params[0].id, 1);
            EXPECT_EQ(method1.params[0].name, "x");
            auto* type = boost::get<ast::Primitive>(&method1.params[0].type);
            ASSERT_NE(type, nullptr);
            EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);
            EXPECT_FALSE(method1.params[0].default_value.has_value());
            ASSERT_EQ(method1.params[0].attrs.size(), 1);
            EXPECT_EQ(method1.params[0].attrs[0].name, "a");
            EXPECT_TRUE(method1.params[0].attrs[0].value.has_value());
            bool* attr_value = boost::get<bool>(&method1.params[0].attrs[0].value.value());
            ASSERT_NE(attr_value, nullptr);
            EXPECT_EQ(*attr_value, true);

            ASSERT_TRUE(method1.result.has_value());
            EXPECT_EQ(method1.result->which(), 0);
            auto* result_type = boost::get<ast::Type>(&method1.result.value());
            ASSERT_NE(result_type, nullptr);
            auto* primitive = boost::get<ast::Primitive>(result_type);
            ASSERT_NE(primitive, nullptr);
            EXPECT_EQ(primitive->kind, ast::PrimitiveKind::Bool);
        }

        {
            ast::Method& method2 = t.methods[1];
            EXPECT_EQ(method2.name, "method2");
            EXPECT_EQ(method2.kind, ast::MethodKind::Oneway);
            EXPECT_EQ(method2.params.size(), 0);
            EXPECT_FALSE(method2.result.has_value());
        }

        {
            ast::Method& method3 = t.methods[2];
            EXPECT_EQ(method3.name, "method3");
            EXPECT_EQ(method3.kind, ast::MethodKind::Stream);
            ASSERT_EQ(method3.params.size(), 1);
            EXPECT_EQ(method3.params[0].id, 1);
            EXPECT_EQ(method3.params[0].name, "x");
            auto* type = boost::get<ast::Primitive>(&method3.params[0].type);
            ASSERT_NE(type, nullptr);
            EXPECT_EQ(type->kind, ast::PrimitiveKind::I32);
            EXPECT_FALSE(method3.params[0].default_value.has_value());
            EXPECT_TRUE(method3.params[0].attrs.empty());

            ASSERT_TRUE(method3.result.has_value());
            EXPECT_EQ(method3.result->which(), 1);
            auto* tuple_result = boost::get<std::vector<ast::Field>>(&method3.result.value());
            ASSERT_EQ(tuple_result->size(), 2);
            EXPECT_EQ(tuple_result->at(0).id, 1);
            EXPECT_EQ(tuple_result->at(0).name, "y");
            type = boost::get<ast::Primitive>(&tuple_result->at(0).type);
            ASSERT_NE(type, nullptr);
            EXPECT_EQ(type->kind, ast::PrimitiveKind::I32);
            EXPECT_FALSE(tuple_result->at(0).default_value.has_value());
            EXPECT_TRUE(tuple_result->at(0).attrs.empty());

            EXPECT_EQ(tuple_result->at(1).id, 2);
            EXPECT_EQ(tuple_result->at(1).name, "z");
            type = boost::get<ast::Primitive>(&tuple_result->at(1).type);
            ASSERT_NE(type, nullptr);
            EXPECT_EQ(type->kind, ast::PrimitiveKind::F64);
            EXPECT_FALSE(tuple_result->at(1).default_value.has_value());
            EXPECT_TRUE(tuple_result->at(1).attrs.empty());
        }

        {
            ast::Method& method4 = t.methods[3];
            EXPECT_EQ(method4.name, "method4");
            EXPECT_EQ(method4.kind, ast::MethodKind::Notify);
            EXPECT_EQ(method4.params.size(), 2);
            EXPECT_EQ(method4.params[0].id, 1);
            EXPECT_EQ(method4.params[0].name, "b");
            auto* type = boost::get<ast::Primitive>(&method4.params[0].type);
            ASSERT_NE(type, nullptr);
            EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);
            EXPECT_FALSE(method4.params[0].default_value.has_value());
            ASSERT_EQ(method4.params[0].attrs.size(), 0);
            EXPECT_EQ(method4.params[1].id, 2);
            EXPECT_EQ(method4.params[1].name, "y");
            type = boost::get<ast::Primitive>(&method4.params[1].type);
            ASSERT_NE(type, nullptr);
            EXPECT_EQ(type->kind, ast::PrimitiveKind::I64);
            EXPECT_FALSE(method4.params[1].default_value.has_value());
            EXPECT_TRUE(method4.params[1].attrs.empty());
        }
    }
}

TEST(IdlRule, ParseModuleDecl) {
    {
        ast::QualifiedIdentifier t;
        ASSERT_TRUE(parse_rule(R"(
            module MyModule;
        )", module_decl(), t));
        EXPECT_EQ(t.to_string(), "MyModule");
        ASSERT_EQ(t.parts.size(), 1);
        EXPECT_EQ(t.parts[0], "MyModule");
    }
    {
        ast::QualifiedIdentifier t;
        ASSERT_TRUE(parse_rule(R"(
            module MyModule.v2; // comment
        )", module_decl(), t));
        EXPECT_EQ(t.to_string(), "MyModule.v2");
        ASSERT_EQ(t.parts.size(), 2);
        EXPECT_EQ(t.parts[0], "MyModule");
        EXPECT_EQ(t.parts[1], "v2");
    }
}

TEST(IdlRule, ParseImportDecl) {
    {
        ast::Import t;
        ASSERT_TRUE(parse_rule(R"(
            import "path/to/file.idl";
        )", import(), t));
        EXPECT_EQ(t.path, "path/to/file.idl");
    }
}

TEST(IdlRule, ParseDecl) {
    {
        ast::Declaration t;
        ASSERT_TRUE(parse_rule(R"(
            const bool x = true;
        )", declaration(), t));
        auto* const_decl = boost::get<ast::ConstantDeclaration>(&t);
        ASSERT_NE(const_decl, nullptr);
        EXPECT_EQ(const_decl->name, "x");
        auto* type = boost::get<ast::Primitive>(&const_decl->type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);
        auto* value = boost::get<bool>(&const_decl->value);
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, true);
    }
    {
        ast::Declaration t;
        ASSERT_TRUE(parse_rule(R"(
            enum E { A, B, C };
        )", declaration(), t));
        auto* enum_decl = boost::get<ast::Enum>(&t);
        ASSERT_NE(enum_decl, nullptr);
        EXPECT_EQ(enum_decl->name, "E");
        ASSERT_EQ(enum_decl->items.size(), 3);
        EXPECT_EQ(enum_decl->items[0].name, "A");
        EXPECT_EQ(enum_decl->items[1].name, "B");
        EXPECT_EQ(enum_decl->items[2].name, "C");
    }
    {
        ast::Declaration t;
        ASSERT_TRUE(parse_rule(R"(
            struct S { 1: i32 x; };
        )", declaration(), t));
        auto* struct_decl = boost::get<ast::Struct>(&t);
        ASSERT_NE(struct_decl, nullptr);
        EXPECT_EQ(struct_decl->name, "S");
        ASSERT_EQ(struct_decl->fields.size(), 1);
        EXPECT_EQ(struct_decl->fields[0].id, 1);
        EXPECT_EQ(struct_decl->fields[0].name, "x");
        auto* type = boost::get<ast::Primitive>(&struct_decl->fields[0].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::I32);
    }
    {
        ast::Declaration t;
        ASSERT_TRUE(parse_rule(R"(
            interface I { rpc method (1: i32 x) -> i32; };
        )", declaration(), t));
        auto* interface_decl = boost::get<ast::Interface>(&t);
        ASSERT_NE(interface_decl, nullptr);
        EXPECT_EQ(interface_decl->name, "I");
        ASSERT_EQ(interface_decl->methods.size(), 1);
        EXPECT_EQ(interface_decl->methods[0].name, "method");
        EXPECT_EQ(interface_decl->methods[0].kind, ast::MethodKind::Rpc);
        ASSERT_EQ(interface_decl->methods[0].params.size(), 1);
        EXPECT_EQ(interface_decl->methods[0].params[0].id, 1);
        EXPECT_EQ(interface_decl->methods[0].params[0].name, "x");
        auto* type = boost::get<ast::Primitive>(&interface_decl->methods[0].params[0].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::I32);
        ASSERT_TRUE(interface_decl->methods[0].result.has_value());
        ast::Result& result = interface_decl->methods[0].result.value();
        EXPECT_EQ(result.which(), 0);
        auto* result_type = boost::get<ast::Type>(&result);
        ASSERT_NE(result_type, nullptr);
        auto* primitive = boost::get<ast::Primitive>(result_type);
        ASSERT_NE(primitive, nullptr);
        EXPECT_EQ(primitive->kind, ast::PrimitiveKind::I32);
    }
}


TEST(IdlRule, ParseModule) {
    {
        ast::Module t;
        ASSERT_TRUE(parse_rule(R"(
            module MyModule;
            import "path/to/file.idl";
            const bool x = true;
            enum E { A, B, C };
            struct S { 1: i32 x; };
            interface I { rpc method (1: i32 x) -> i32; }
        )", module(), t));
        EXPECT_EQ(t.name.to_string(), "MyModule");
        ASSERT_EQ(t.imports.size(), 1);
        EXPECT_EQ(t.imports[0].path, "path/to/file.idl");
        ASSERT_EQ(t.decls.size(), 4);
        auto* const_decl = boost::get<ast::ConstantDeclaration>(&t.decls[0]);
        ASSERT_NE(const_decl, nullptr);
        EXPECT_EQ(const_decl->name, "x");
        auto* type = boost::get<ast::Primitive>(&const_decl->type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::Bool);
        auto* value = boost::get<bool>(&const_decl->value);
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, true);
        auto* enum_decl = boost::get<ast::Enum>(&t.decls[1]);
        ASSERT_NE(enum_decl, nullptr);
        EXPECT_EQ(enum_decl->name, "E");
        ASSERT_EQ(enum_decl->items.size(), 3);
        EXPECT_EQ(enum_decl->items[0].name, "A");
        EXPECT_EQ(enum_decl->items[1].name, "B");
        EXPECT_EQ(enum_decl->items[2].name, "C");
        auto* struct_decl = boost::get<ast::Struct>(&t.decls[2]);
        ASSERT_NE(struct_decl, nullptr);
        EXPECT_EQ(struct_decl->name, "S");
        ASSERT_EQ(struct_decl->fields.size(), 1);
        EXPECT_EQ(struct_decl->fields[0].id, 1);
        EXPECT_EQ(struct_decl->fields[0].name, "x");
        type = boost::get<ast::Primitive>(&struct_decl->fields[0].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::I32);
        auto* interface_decl = boost::get<ast::Interface>(&t.decls[3]);
        ASSERT_NE(interface_decl, nullptr);
        EXPECT_EQ(interface_decl->name, "I");
        ASSERT_EQ(interface_decl->methods.size(), 1);
        EXPECT_EQ(interface_decl->methods[0].name, "method");
        EXPECT_EQ(interface_decl->methods[0].kind, ast::MethodKind::Rpc);
        ASSERT_EQ(interface_decl->methods[0].params.size(), 1);
        EXPECT_EQ(interface_decl->methods[0].params[0].id, 1);
        EXPECT_EQ(interface_decl->methods[0].params[0].name, "x");
        type = boost::get<ast::Primitive>(&interface_decl->methods[0].params[0].type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, ast::PrimitiveKind::I32);
    }
}
