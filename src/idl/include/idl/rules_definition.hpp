#pragma once

#include "idl/ast.hpp"
#include "rules.hpp"

#include <type_traits>

namespace boost::spirit::x3::traits
{

template <typename T>
struct is_substitute<std::optional<T>, boost::optional<T>> : std::true_type {
};

template <typename T>
struct is_substitute<boost::optional<T>, std::optional<T>> : std::true_type {
};

}  // namespace boost::spirit::x3::traits

/**
 * @file rules_definition.hpp
 * @brief Spirit rules definitions
 *
 * This file contains Spirit rules definitions for the IDL parser.
 *
 * @note This file is included in exactly one module, hence the NOLINT.
 *
 */
// NOLINTBEGIN(misc-definitions-in-headers)

namespace hasten::idl::parser
{

namespace rule
{

namespace x3 = boost::spirit::x3;

// -------------- helpers --------------
inline auto make_kw(const char* s)
{
    return x3::lexeme[x3::lit(s) >> !(x3::alnum | x3::char_('_'))];
}

// clang-format off
// ... to make the rules more readable

const auto kw_module   = make_kw("module");
const auto kw_import   = make_kw("import");
const auto kw_interface= make_kw("interface");
const auto kw_struct   = make_kw("struct");
const auto kw_enum     = make_kw("enum");
const auto kw_const    = make_kw("const");
const auto kw_rpc      = make_kw("rpc");
const auto kw_oneway   = make_kw("oneway");
const auto kw_stream   = make_kw("stream");
const auto kw_notify   = make_kw("notify");
const auto kw_vector   = make_kw("vector");
const auto kw_map      = make_kw("map");
const auto kw_optional = make_kw("optional");
const auto kw_null     = make_kw("null");

// primitives
const auto kw_bool   = make_kw("bool");
const auto kw_i8     = make_kw("i8");
const auto kw_i16    = make_kw("i16");
const auto kw_i32    = make_kw("i32");
const auto kw_i64    = make_kw("i64");
const auto kw_u8     = make_kw("u8");
const auto kw_u16    = make_kw("u16");
const auto kw_u32    = make_kw("u32");
const auto kw_u64    = make_kw("u64");
const auto kw_f32    = make_kw("f32");
const auto kw_f64    = make_kw("f64");
const auto kw_string = make_kw("string");
const auto kw_bytes  = make_kw("bytes");

// boolean constants
const auto kw_true   = make_kw("true");
const auto kw_false  = make_kw("false");

// Handle reserved keywords. TODO: Different from keywords above?
x3::symbols<> reserved_identifiers;
struct reserved_init {
  reserved_init() {
    reserved_identifiers.add
      ("module")
      ("import")
      ("interface")
      ("struct")
      ("enum")
      ("const")
      ("rpc")
      ("oneway")
      ("stream")
      ("notify")
      ("vector")
      ("map")
      ("optional")
      ("null")
      // primitives
      ("bool")
      ("i8")
      ("i16")
      ("i32")
      ("i64")
      ("u8")
      ("u16")
      ("u32")
      ("u64")
      ("f32")
      ("f64")
      ("string")
      ("bytes");
  }
} reserved_init_instance;

// skipper
LineComment line_comment = "line_comment";
BlockComment block_comment = "block_comment";
Comment comment = "comment";
Skipper skipper = "skipper";

const auto line_comment_def = x3::lexeme["//" >> *(x3::char_ - x3::eol) >> (x3::eol | x3::eoi)];
const auto block_comment_def = x3::lexeme["/*" >> *(x3::char_ - "*/") >> "*/"];
const auto comment_def = line_comment | block_comment;
const auto skipper_def = comment | x3::space;

// tokens
Identifier identifier = "ident";
Name name = "name";
QualifiedIdentifier qualified_identifier = "qident";
StringLiteral string_lit = "string_lit";
BooleanLiteral bool_lit = "bool_lit";
IntegerLiteral int_lit = "int_lit";
FloatLiteral float_lit = "float_lit";
BytesLiteral bytes_lit = "bytes_lit";
ConstantValue const_value = "const_value";

// identifiers
const auto identifier_def =
    x3::lexeme[
        (x3::alpha | x3::char_('_'))
        >> *(x3::alnum | x3::char_('_'))
    ];

const auto name_def =
    x3::lexeme[ !reserved_identifiers
        >> (x3::alpha | x3::char_('_'))
        >> *(x3::alnum | x3::char_('_'))
    ];

const auto qualified_identifier_def =
    (identifier % '.')
    [([](auto& ctx){
        _val(ctx) = ast::QualifiedIdentifier{
            .parts = std::move(_attr(ctx))
        };
    })];

// strings: capture raw range with quotes, then strip quotes; keep escapes as-is
const auto string_lit_def =
    x3::raw['"' >> *('\\' >> x3::char_ | ~x3::char_('"')) >> '"']
    [([](auto& ctx){
        auto const& rng = _attr(ctx);           // iterator_range<It>
        std::string s(rng.begin()+1, rng.end()-1); // drop surrounding quotes
        _val(ctx) = std::move(s);
    })];

const auto bool_lit_def  = (kw_true  >> x3::attr(true))
                         | (kw_false >> x3::attr(false));

// integers: decimal (int64), hex 0x..., bin 0b..., oct 0o...
const auto int_lit_def =
    (x3::int64 >> !(x3::char_('x') | x3::char_('b') | x3::char_('o') | x3::char_('.')))
        [([](auto& ctx){
            _val(ctx) = _attr(ctx);      // explicit assignment is a must here
        })]
  | x3::raw["0x" >> +x3::xdigit]
        [([](auto& ctx){
            auto const& s = _attr(ctx);
            std::int64_t v = 0;
            for (auto it = std::next(s.begin(), 2); it != s.end(); ++it) {
                char c = *it;
                v <<= 4;
                if ('0'<=c && c<='9') v |= (c - '0');
                else if ('a'<=c && c<='f') v |= 10 + (c - 'a');
                else if ('A'<=c && c<='F') v |= 10 + (c - 'A');
            }
            _val(ctx) = v;
        })]
  | x3::raw["0b" >> +x3::char_('0','1')]
        [([](auto& ctx){
            auto const& s = _attr(ctx);
            std::int64_t v = 0;
            for (auto it = std::next(s.begin(), 2); it != s.end(); ++it) {
                v = (v<<1) | (*it=='1');
            }
            _val(ctx) = v;
        })]
  | x3::raw["0o" >> +x3::char_('0','7')]
        [([](auto& ctx){
            auto const& s = _attr(ctx);
            std::int64_t v = 0;
            for (auto it = std::next(s.begin(), 2); it != s.end(); ++it) {
                v = (v*8) + (*it - '0');
            }
            _val(ctx) = v;
        })];

// floats
const auto float_lit_def =
    x3::raw[
        +x3::digit >>
        -('.' >> *x3::digit) >>
        -((x3::char_('e')|x3::char_('E')) >> -(x3::char_('+')|x3::char_('-')) >> +x3::digit)
    ]
    [([](auto& ctx){
        auto const& s = _attr(ctx);
        _val(ctx) = std::stod(std::string(s.begin(), s.end()));
    })];

// bytes literal: b"DE AD BE EF" (spaces allowed) -> vector<uint8_t> {0xDE,0xAD,0xBE,0xEF}
const auto bytes_lit_def =
    x3::raw[x3::char_('b') >> '"' >>
            *(
                // allow whitespace inside
                x3::space
                | (x3::xdigit >> x3::xdigit)
              )
              >> '"'
            ]
    [([](auto &ctx) {
        ast::Bytes out;

        auto hex_value = [](char c) -> int {
            if ('0' <= c && c <= '9') return c - '0';
            if ('a' <= c && c <= 'f') return 10 + (c - 'a');
            if ('A' <= c && c <= 'F') return 10 + (c - 'A');
            return -1;
        };

        const auto &raw_string = _attr(ctx);

        int hi = -1;

        // skip b" and the end quote
        for (auto it = raw_string.begin() + 2; it != raw_string.end() - 1; ++it) {
            char c = *it;
            if (std::isspace(static_cast<unsigned char>(c))) continue;
            int value = hex_value(c);
            if (value < 0) {
                // Should never happen, since the rule matches xdigits
                continue;
            }

            if (hi < 0) {
                hi = value;
            } else {
                out.push_back(static_cast<std::uint8_t>((hi<<4) | value));
                hi = -1;
            }
        }
        _val(ctx) = std::move(out);
    })];

// const value: null | bool | int | float | string | qident | bytes
const auto const_value_def =
    (kw_null    >> x3::attr(ast::ConstantValue{ast::Null{}}))
    | bool_lit
    | int_lit      // must be before float_lit
    | float_lit
    | bytes_lit    // must be before string_lit
    | string_lit
    | qualified_identifier;

// -------------- type rules --------------
Type type = "type";
PrimitiveType prim_type = "prim_type";
UserType user_type = "user_type";
VectorType vec_type = "vec_type";
MapType map_type = "map_type";
OptionalType opt_type = "opt_type";

const auto prim_type_def =
    (kw_bool   >> x3::attr(ast::Primitive{ast::PrimitiveKind::Bool}))
    | (kw_i8     >> x3::attr(ast::Primitive{ast::PrimitiveKind::I8}))
    | (kw_i16    >> x3::attr(ast::Primitive{ast::PrimitiveKind::I16}))
    | (kw_i32    >> x3::attr(ast::Primitive{ast::PrimitiveKind::I32}))
    | (kw_i64    >> x3::attr(ast::Primitive{ast::PrimitiveKind::I64}))
    | (kw_u8     >> x3::attr(ast::Primitive{ast::PrimitiveKind::U8}))
    | (kw_u16    >> x3::attr(ast::Primitive{ast::PrimitiveKind::U16}))
    | (kw_u32    >> x3::attr(ast::Primitive{ast::PrimitiveKind::U32}))
    | (kw_u64    >> x3::attr(ast::Primitive{ast::PrimitiveKind::U64}))
    | (kw_f32    >> x3::attr(ast::Primitive{ast::PrimitiveKind::F32}))
    | (kw_f64    >> x3::attr(ast::Primitive{ast::PrimitiveKind::F64}))
    | (kw_string >> x3::attr(ast::Primitive{ast::PrimitiveKind::String}))
    | (kw_bytes  >> x3::attr(ast::Primitive{ast::PrimitiveKind::Bytes}));

const auto user_type_def = qualified_identifier;

// vec_type: attribute of the sequence is just the inner `Type`
// because tokens/keywords contribute no attributes.
const auto vec_type_def = kw_vector >> '<' >> type >> '>';

// map_type: attribute of the sequence is a Fusion sequence (Type, Type)
const auto map_type_def = kw_map >> '<' >> type >> ',' >> type >> '>';

// opt_type: attribute is the inner `Type`
const auto opt_type_def = kw_optional >> '<' >> type >> '>';

const auto type_def =
    prim_type
    | opt_type
    | vec_type
    | map_type
    | user_type;

// -------------- attributes --------------
Attribute attribute = "attribute";
AttributeList attribute_list = "attribute_list";

const auto attribute_def =
    identifier >> -('=' >> const_value);

const auto attribute_list_def =
    '[' >> (attribute % ',') >> ']';

const auto attribute_list_or_empty =
    attribute_list | x3::attr(ast::AttributeList{});

// -------------- fields / params / results --------------
Field field = "field";
Parameter param = "param";
Result result = "result";
ReturnField ret_field = "ret_field";
ReturnFields ret_fields = "ret_fields";

const auto field_def =
    int_lit >> ':' >> type >> name >> -('=' >> const_value) >> attribute_list_or_empty >> ';';

const auto param_def =
    int_lit >> ':' >> type >> name >> -('=' >> const_value) >> attribute_list_or_empty;

const auto params_or_empty = (param % ',') | x3::attr(std::vector<ast::Parameter>{});

const auto result_def =
    ( type [([](auto& ctx){ _val(ctx) = ast::Result{ .is_tuple = false, .single = std::move(_attr(ctx)) }; })] )
    | ( ret_fields [([](auto& ctx){ _val(ctx) = ast::Result{ .is_tuple = true, .tuple_fields = std::move(_attr(ctx)) }; })] );

const auto ret_field_def =
    int_lit
    >> ':'
    >> type
    >> name
    >> x3::attr(std::optional<ast::ConstantValue>{})
    >> attribute_list_or_empty
    >> -x3::lit(';');

const auto ret_fields_def =
    ('(' >> ret_field % ',' >> ')');

// -------------- declarations --------------
Constant const_decl = "const_decl";
EnumItem enum_item  = "enum_item";
Enum enum_decl  = "enum_decl";
Struct struct_decl= "struct_decl";
Method method     = "method";
MethodKind method_kind = "method_kind";
Interface interface_decl = "interface_decl";
Declaration decl       = "decl";

const auto const_decl_def = kw_const >> type >> name >> '=' >> const_value >> ';';

const auto enum_item_def = identifier >> -('=' >> int_lit) >> attribute_list_or_empty;

const auto enum_decl_def =
    kw_enum >> identifier >> '{' >> (enum_item % ',') >> -x3::lit(',') >> '}' >> -x3::lit(';');

const auto struct_decl_def = kw_struct >> name >> '{' >> *field >> '}' >> -x3::lit(';');

const auto method_kind_def =
    (kw_rpc    >> x3::attr(ast::MethodKind::Rpc))
    | (kw_oneway >> x3::attr(ast::MethodKind::Oneway))
    | (kw_stream >> x3::attr(ast::MethodKind::Stream))
    | (kw_notify >> x3::attr(ast::MethodKind::Notify));

const auto method_def =
    method_kind >> name >> '(' >> params_or_empty >> ')' >> -("->" >> result) >> attribute_list_or_empty >> ';';

const auto interface_decl_def = kw_interface >> identifier >> '{' >> *method >> '}' >> -x3::lit(';');

const auto decl_def =
    const_decl
    | enum_decl
    | struct_decl
    | interface_decl;

// -------------- imports / module / file --------------
Import import = "import";
Module module = "module";
ModuleDeclaration module_decl = "module_decl";

const auto import_def = kw_import >> string_lit >> ';';

const auto module_decl_def =
    (kw_module >> qualified_identifier >> ';');

const auto module_def = module_decl >> *import >> *decl;

// clang-format on

// clang-format off
BOOST_SPIRIT_DEFINE(
    line_comment,
    block_comment,
    comment,
    skipper,
    identifier,
    name,
    qualified_identifier,
    string_lit,
    bool_lit,
    int_lit,
    float_lit,
    bytes_lit,
    const_value,
    prim_type,
    user_type,
    vec_type,
    map_type,
    opt_type,
    type,
    attribute,
    attribute_list,
    field,
    param,
    result,
    ret_field,
    ret_fields,
    const_decl,
    enum_item,
    enum_decl,
    struct_decl,
    method,
    method_kind,
    interface_decl,
    decl,
    import,
    module_decl,
    module
);
// clang-format on

}  // namespace rule

rule::LineComment line_comment()
{
    return rule::line_comment;
}

rule::BlockComment block_comment()
{
    return rule::block_comment;
}

rule::Comment comment()
{
    return rule::comment;
}

rule::Skipper skipper()
{
    return rule::skipper;
}

rule::Identifier identifier()
{
    return rule::identifier;
}

rule::Name name()
{
    return rule::name;
}

rule::QualifiedIdentifier qualified_identifier()
{
    return rule::qualified_identifier;
}

rule::StringLiteral string_literal()
{
    return rule::string_lit;
}

rule::BooleanLiteral boolean_literal()
{
    return rule::bool_lit;
}

rule::IntegerLiteral integer_literal()
{
    return rule::int_lit;
}

rule::FloatLiteral float_literal()
{
    return rule::float_lit;
}

rule::BytesLiteral bytes_literal()
{
    return rule::bytes_lit;
}

rule::ConstantValue const_value()
{
    return rule::const_value;
}

rule::PrimitiveType primitive_type()
{
    return rule::prim_type;
}

rule::UserType user_type()
{
    return rule::user_type;
}

rule::VectorType vector_type()
{
    return rule::vec_type;
}

rule::MapType map_type()
{
    return rule::map_type;
}

rule::OptionalType optional_type()
{
    return rule::opt_type;
}

rule::Type type()
{
    return rule::type;
}

rule::Attribute attribute()
{
    return rule::attribute;
}

rule::AttributeList attribute_list()
{
    return rule::attribute_list;
}

rule::Field field()
{
    return rule::field;
}

rule::Parameter param()
{
    return rule::param;
}

rule::Result result()
{
    return rule::result;
}

rule::ReturnField ret_field()
{
    return rule::ret_field;
}

rule::ReturnFields ret_fields()
{
    return rule::ret_fields;
}

rule::Constant const_decl()
{
    return rule::const_decl;
}

rule::EnumItem enum_item()
{
    return rule::enum_item;
}

rule::Enum enum_decl()
{
    return rule::enum_decl;
}

rule::Struct struct_decl()
{
    return rule::struct_decl;
}

rule::Method method()
{
    return rule::method;
}

rule::MethodKind method_kind()
{
    return rule::method_kind;
}

rule::Interface interface_decl()
{
    return rule::interface_decl;
}

rule::Declaration declaration()
{
    return rule::decl;
}

rule::Import import()
{
    return rule::import;
}

rule::Module module()
{
    return rule::module;
}

rule::ModuleDeclaration module_decl()
{
    return rule::module_decl;
}

}  // namespace hasten::idl::parser

// NOLINTEND(misc-definitions-in-headers)
