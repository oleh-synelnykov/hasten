#include <gtest/gtest.h>
#include <boost/spirit/home/x3.hpp>
#include <boost/variant/get.hpp>

#include "idl/ast.hpp"
#include "idl/parser.hpp"

using namespace hasten::idl;
using namespace hasten::idl::parser;

TEST(Parser, ParseModule) {
    ast::Module m;
    std::string error;
    ASSERT_TRUE(parse_file("module foo;", m, &error)) << error;
    ASSERT_EQ(m.name.parts.size(), 1);
    EXPECT_EQ(m.name.parts[0], "foo");
}

TEST(Parser, ParseModuleMultiPartName) {
    ast::Module m;
    std::string error;
    ASSERT_TRUE(parse_file("module foo.bar.v2;", m, &error)) << error;
    ASSERT_EQ(m.name.parts.size(), 3);
    EXPECT_EQ(m.name.parts[0], "foo");
    EXPECT_EQ(m.name.parts[1], "bar");
    EXPECT_EQ(m.name.parts[2], "v2");
}

TEST(Parser, ParseModuleWithImports) {
    const std::string input =
        R"(module foo.bar;
           import "std/base.hidl";
           import "std/math.hidl";)";

    ast::Module m;
    std::string error;
    ASSERT_TRUE(parse_file(input, m, &error)) << error;
    ASSERT_EQ(m.imports.size(), 2);
    EXPECT_EQ(m.imports[0].path, "std/base.hidl");
    EXPECT_EQ(m.imports[1].path, "std/math.hidl");
}

TEST(Parser, ParseModuleWithStructAndDefaults) {
    const std::string input =
        R"(module data;
           struct User {
             1:u64 id;
             2:optional<string> name = "anon";
             3:vector<i32> scores;
           };)";

    ast::Module m;
    std::string error;
    ASSERT_TRUE(parse_file(input, m, &error)) << error;
    ASSERT_EQ(m.decls.size(), 1);

    auto* s = boost::get<ast::Struct>(&m.decls[0]);
    ASSERT_NE(s, nullptr);
    ASSERT_EQ(s->fields.size(), 3);
    EXPECT_EQ(s->fields[0].name, "id");

    const auto* opt = boost::get<ast::Optional>(&s->fields[1].type);
    ASSERT_NE(opt, nullptr);
    const auto* inner_prim = boost::get<ast::Primitive>(&opt->inner);
    ASSERT_NE(inner_prim, nullptr);
    EXPECT_EQ(inner_prim->kind, ast::PrimitiveKind::String);
    ASSERT_TRUE(s->fields[1].default_value.has_value());
    EXPECT_EQ(boost::get<std::string>(s->fields[1].default_value.value()), "anon");

    const auto* vec = boost::get<ast::Vector>(&s->fields[2].type);
    ASSERT_NE(vec, nullptr);
    const auto* vec_prim = boost::get<ast::Primitive>(&vec->element);
    ASSERT_NE(vec_prim, nullptr);
    EXPECT_EQ(vec_prim->kind, ast::PrimitiveKind::I32);
}

TEST(Parser, ParseModuleWithInterfaceAndResults) {
    const std::string input =
        R"(module svc;
           interface Echo {
             rpc Ping(1:string msg) -> (1:string reply);
             oneway Fire(1:i32 code);
           };)";

    ast::Module m;
    std::string error;
    ASSERT_TRUE(parse_file(input, m, &error)) << error;
    ASSERT_EQ(m.decls.size(), 1);
    auto* iface = boost::get<ast::Interface>(&m.decls[0]);
    ASSERT_NE(iface, nullptr);
    ASSERT_EQ(iface->methods.size(), 2);

    const auto& ping = iface->methods[0];
    EXPECT_EQ(ping.name, "Ping");
    ASSERT_EQ(ping.params.size(), 1);
    ASSERT_TRUE(ping.result.has_value());
    const auto* ping_tuple = boost::get<std::vector<ast::Field>>(&ping.result.value());
    ASSERT_NE(ping_tuple, nullptr);
    ASSERT_EQ(ping_tuple->size(), 1);
    EXPECT_EQ((*ping_tuple)[0].name, "reply");

    const auto& fire = iface->methods[1];
    EXPECT_EQ(fire.kind, ast::MethodKind::Oneway);
    ASSERT_EQ(fire.params.size(), 1);
    EXPECT_FALSE(fire.result.has_value());
}

TEST(Parser, ParseModuleWithConstAndEnum) {
    const std::string input =
        R"(module config;
           const i32 MaxRetries = 5;
           enum State { Ready = 1, Busy = 2, };)";

    ast::Module m;
    std::string error;
    ASSERT_TRUE(parse_file(input, m, &error)) << error;
    ASSERT_EQ(m.decls.size(), 2);

    auto* c = boost::get<ast::ConstantDeclaration>(&m.decls[0]);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->name, "MaxRetries");
    EXPECT_EQ(boost::get<std::int64_t>(c->value), 5);

    auto* e = boost::get<ast::Enum>(&m.decls[1]);
    ASSERT_NE(e, nullptr);
    ASSERT_EQ(e->items.size(), 2);
    EXPECT_EQ(e->items[0].name, "Ready");
    ASSERT_TRUE(e->items[0].value.has_value());
    EXPECT_EQ(e->items[0].value.value(), 1);
}

TEST(Parser, ParseModuleWithContainerTypes) {
    const std::string input =
        R"(module storage;
           struct Bag {
             1:map<string,i32> counts;
             2:vector<vector<u8>> blobs;
           };
           interface BagService {
             rpc Get(1:u64 id) -> (1:Bag bag);
           };)";

    ast::Module m;
    std::string error;
    ASSERT_TRUE(parse_file(input, m, &error)) << error;
    ASSERT_EQ(m.decls.size(), 2);

    auto* bag = boost::get<ast::Struct>(&m.decls[0]);
    ASSERT_NE(bag, nullptr);
    const auto* map = boost::get<ast::Map>(&bag->fields[0].type);
    ASSERT_NE(map, nullptr);
    const auto* key_prim = boost::get<ast::Primitive>(&map->key);
    ASSERT_NE(key_prim, nullptr);
    EXPECT_EQ(key_prim->kind, ast::PrimitiveKind::String);

    const auto* nested_vec = boost::get<ast::Vector>(&bag->fields[1].type);
    ASSERT_NE(nested_vec, nullptr);
    const auto* inner_vec = boost::get<ast::Vector>(&nested_vec->element);
    ASSERT_NE(inner_vec, nullptr);
    const auto* byte_prim = boost::get<ast::Primitive>(&inner_vec->element);
    ASSERT_NE(byte_prim, nullptr);
    EXPECT_EQ(byte_prim->kind, ast::PrimitiveKind::U8);

    auto* svc = boost::get<ast::Interface>(&m.decls[1]);
    ASSERT_NE(svc, nullptr);
    ASSERT_EQ(svc->methods.size(), 1);
    const auto& get = svc->methods[0];
    ASSERT_TRUE(get.result.has_value());
    const auto* get_tuple = boost::get<std::vector<ast::Field>>(&get.result.value());
    ASSERT_NE(get_tuple, nullptr);
    ASSERT_EQ(get_tuple->size(), 1);
    const auto& ret_field = (*get_tuple)[0];
    EXPECT_EQ(ret_field.name, "bag");
    const auto* user_type = boost::get<ast::UserType>(&ret_field.type);
    ASSERT_NE(user_type, nullptr);
    ASSERT_EQ(user_type->name.parts.size(), 1);
    EXPECT_EQ(user_type->name.parts[0], "Bag");
}

TEST(Parser, ParseModuleWithAttributes) {
    const std::string input =
        R"(module annotated;
           struct Document {
             1:string id [deprecated];
             2:string contents [deprecated, format="utf8"];
           };)";

    ast::Module m;
    std::string error;
    ASSERT_TRUE(parse_file(input, m, &error)) << error;
    ASSERT_EQ(m.decls.size(), 1);
    auto* doc = boost::get<ast::Struct>(&m.decls[0]);
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(doc->fields.size(), 2);

    ASSERT_EQ(doc->fields[0].attrs.size(), 1);
    EXPECT_EQ(doc->fields[0].attrs[0].name, "deprecated");
    EXPECT_FALSE(doc->fields[0].attrs[0].value.has_value());

    ASSERT_EQ(doc->fields[1].attrs.size(), 2);
    EXPECT_EQ(doc->fields[1].attrs[1].name, "format");
    ASSERT_TRUE(doc->fields[1].attrs[1].value.has_value());
    EXPECT_EQ(boost::get<std::string>(*doc->fields[1].attrs[1].value), "utf8");
}

TEST(Parser, ParseModuleFailureReportsError) {
    ast::Module m;
    std::string error;
    {
        EXPECT_FALSE(parse_file("module missing_semicolon", m, &error));
        ASSERT_FALSE(error.empty());
        EXPECT_NE(error.find("Expected ';'"), std::string::npos);
    }
    {
        EXPECT_FALSE(parse_file("module;", m, &error));
        ASSERT_FALSE(error.empty());
        EXPECT_NE(error.find("Expected qualified identifier"), std::string::npos);
    }
    {
        // identifier cannot start with number
        EXPECT_FALSE(parse_file("module 123;", m, &error));
        ASSERT_FALSE(error.empty());
        EXPECT_NE(error.find("Expected qualified identifier"), std::string::npos);
    }
}

TEST(Parser, ParseInterfaceWithErrors) {
    ast::Module m;
    std::string error;
    {
        // interface with no name
        EXPECT_FALSE(parse_file(R"(
            module test; // module declaration is a must
            interface {
                rpc bar() -> (1:string);
            };)", m, &error));
        ASSERT_FALSE(error.empty());
        EXPECT_NE(error.find("Expected identifier"), std::string::npos) << error;
    }
    {
        // interface with no body
        EXPECT_FALSE(parse_file(R"(
            module test;
            interface foo;
        )", m, &error));
        ASSERT_FALSE(error.empty());
        EXPECT_NE(error.find("Expected '{'"), std::string::npos) << error;
    }
    {
        // interface with parentheses instead of braces
        EXPECT_FALSE(parse_file(R"(
            module test;
            interface foo ( rpc bar() -> (1:string) );
        )", m, &error));
        ASSERT_FALSE(error.empty());
        EXPECT_NE(error.find("Expected '{'"), std::string::npos) << error;
    }
    {
        // interface body unclosed
        EXPECT_FALSE(parse_file(R"(
            module test;
            interface foo {
                rpc bar() -> (1:string);
        )", m, &error));
        ASSERT_FALSE(error.empty());
        EXPECT_NE(error.find("Expected '}'"), std::string::npos) << error;
    }
}
