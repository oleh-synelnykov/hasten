#include <gtest/gtest.h>

#include "frontend/diagnostic.hpp"
#include "frontend/semantic/validator.hpp"
#include "idl/parser.hpp"

#include <algorithm>
#include <expected>
#include <string>
#include <utility>
#include <vector>

using namespace hasten;

namespace
{

std::expected<frontend::SourceFile, std::string> make_source_file(const std::string& path,
                                                                  const std::string& content)
{
    auto parsed = idl::parser::parse_file(content);
    if (!parsed) {
        ADD_FAILURE() << "Failed to parse test file '" << path << "': " << parsed.error();
        return std::unexpected("Failed to parse test file '" + path + "'");
    }

    return frontend::SourceFile{
        path,
        content,
        std::move(parsed->module),
        std::move(parsed->position_cache),
    };
}

std::expected<frontend::Program, std::string> make_program(
    const std::vector<std::pair<std::string, std::string>>& files)
{
    frontend::Program program;
    for (const auto& [path, content] : files) {
        auto source_file = make_source_file(path, content);
        if (!source_file) {
            return std::unexpected(source_file.error());
        }
        program.files.emplace(path, *source_file);
    }
    return program;
}

std::vector<frontend::Diagnostic> run_validator(frontend::Program& program)
{
    frontend::DiagnosticSink sink;
    frontend::semantic::Validator validator{program, sink};
    validator.run();
    return sink.diagnostics();
}

bool contains_message(const std::vector<frontend::Diagnostic>& diagnostics, frontend::Severity severity,
                      const std::string& needle)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const auto& diagnostic) {
        return diagnostic.severity == severity && diagnostic.message.find(needle) != std::string::npos;
    });
}

}  // namespace

TEST(SemanticValidatorTest, ValidProgramProducesNoDiagnostics)
{
    auto program = make_program({{"valid.hidl",
                                  R"IDL(
                                      module sample;
                                      enum Mode { ON, OFF };
                                      struct Data {
                                          1: i32 id;
                                          2: optional<string> label;
                                      };
                                      interface Api {
                                          rpc Ping(1: Data data, 2: i32 tries = 1) -> (1: Data reply);
                                      };
                                  )IDL"}});

    ASSERT_TRUE(program) << program.error();
    auto diags = run_validator(program.value());
    EXPECT_TRUE(diags.empty());
}

TEST(SemanticValidatorTest, DuplicateModulesProduceErrors)
{
    auto program = make_program({
        {"first.hidl", "module clash; struct A { 1: i32 id; };"},
        {"second.hidl", "module clash; struct B { 1: i32 id; };"},
    });

    ASSERT_TRUE(program) << program.error();
    auto diags = run_validator(program.value());
    EXPECT_TRUE(contains_message(diags, frontend::Severity::Error, "Module 'clash' already defined"));
}

TEST(SemanticValidatorTest, DuplicateDeclarationsProduceErrors)
{
    auto program = make_program({{"dup.hidl",
                                  R"IDL(
                                      module foo;
                                      struct Shape { 1: i32 id; };
                                      struct Shape { 1: i32 other; };
                                  )IDL"}});

    ASSERT_TRUE(program) << program.error();
    auto diags = run_validator(program.value());
    EXPECT_TRUE(
        contains_message(diags, frontend::Severity::Error, "Declaration 'foo.Shape' already defined"));
}

TEST(SemanticValidatorTest, StructFieldNameAndIdValidation)
{
    auto program = make_program({{"structs.hidl",
                                  R"IDL(
                                      module foo;
                                      struct Bag {
                                          0: i32 unset;
                                          2: i32 weight;
                                          2: string weight;
                                          5: string desc;
                                      };
                                  )IDL"}});

    ASSERT_TRUE(program) << program.error();
    auto diags = run_validator(program.value());
    EXPECT_TRUE(contains_message(diags, frontend::Severity::Error, "Invalid field id '0'"));
    EXPECT_TRUE(contains_message(diags, frontend::Severity::Error, "Duplicate field id '2'"));
    EXPECT_TRUE(contains_message(diags, frontend::Severity::Note, "Gap detected"));
}

TEST(SemanticValidatorTest, InterfaceParameterValidation)
{
    auto program = make_program({{"iface.hidl",
                                  R"IDL(
                                      module foo;
                                      struct Payload { 1: i32 id; };
                                      interface Api {
                                          rpc Call(0: Payload data, 0: Payload again) -> bool;
                                      };
                                  )IDL"}});

    ASSERT_TRUE(program) << program.error();
    auto diags = run_validator(program.value());
    EXPECT_TRUE(contains_message(diags, frontend::Severity::Error, "Invalid parameter id '0'"));
    EXPECT_TRUE(contains_message(diags, frontend::Severity::Error, "Duplicate parameter id '0'"));
}

TEST(SemanticValidatorTest, ResultFieldValidation)
{
    auto program = make_program({{"result.hidl",
                                  R"IDL(
                                      module foo;
                                      struct Entry { 1: i32 id; };
                                      interface Api {
                                          rpc Fetch(1: i32 id) -> (2: Entry entry, 2: Entry again);
                                      };
                                  )IDL"}});

    ASSERT_TRUE(program) << program.error();
    auto diags = run_validator(program.value());
    EXPECT_TRUE(contains_message(diags, frontend::Severity::Error, "Duplicate result field id '2'"));
}

TEST(SemanticValidatorTest, EnumMustHaveUniqueEnumerators)
{
    auto program = make_program({{"enum.hidl",
                                  R"IDL(
                                      module foo;
                                      enum Mode { ON, ON };
                                  )IDL"}});

    ASSERT_TRUE(program) << program.error();
    auto diags = run_validator(program.value());
    EXPECT_TRUE(
        contains_message(diags, frontend::Severity::Error, "Duplicate enumerator name 'ON' in enum 'Mode'"));
}

TEST(SemanticValidatorTest, UnknownTypeReferenceIsReported)
{
    auto program = make_program({{"types.hidl",
                                  R"IDL(
                                      module foo;
                                      struct Bag {
                                          1: Missing nope;
                                      };
                                  )IDL"}});

    ASSERT_TRUE(program) << program.error();
    auto diags = run_validator(program.value());
    EXPECT_TRUE(contains_message(diags, frontend::Severity::Error,
                                 "Unknown type 'Missing' referenced in field 'nope'"));
}

TEST(SemanticValidatorTest, MapKeyMustBePrimitiveOrEnum)
{
    auto program = make_program({{"maps.hidl",
                                  R"IDL(
                                      module foo;
                                      struct Key { 1: i32 id; };
                                      struct Box {
                                          1: map<Key, string> bad_map;
                                      };
                                  )IDL"}});

    ASSERT_TRUE(program) << program.error();
    auto diags = run_validator(program.value());
    EXPECT_TRUE(
        contains_message(diags, frontend::Severity::Error,
                         "Map key in field 'bad_map' of struct 'Box' must be a primitive or enum type"));
}

TEST(SemanticValidatorTest, NestedOptionalsAreRejected)
{
    auto program = make_program({{"optional.hidl",
                                  R"IDL(
                                      module foo;
                                      struct Data {
                                          1: optional<optional<i32>> weird;
                                      };
                                  )IDL"}});

    ASSERT_TRUE(program) << program.error();
    auto diags = run_validator(program.value());
    EXPECT_TRUE(contains_message(diags, frontend::Severity::Error,
                                 "Nested optional types are not allowed in field 'weird'"));
}

TEST(SemanticValidatorTest, IdOverflowIsDetected)
{
    auto program = make_program({{"overflow.hidl",
                                  R"IDL(
                                      module foo;
                                      struct Data {
                                          2147483648: i32 huge;
                                      };
                                  )IDL"}});

    ASSERT_TRUE(program) << program.error();
    auto diags = run_validator(program.value());
    EXPECT_TRUE(contains_message(
        diags, frontend::Severity::Error,
        "Invalid field id '2147483648' in struct 'Data'; maximum allowed value is 2147483647"));
}
