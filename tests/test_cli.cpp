#include <gtest/gtest.h>

#include "cli/hasten.hpp"

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

class Cli : public ::testing::Test
{
   protected:
    std::filesystem::path temp_dir;

    void SetUp() override
    {
        temp_dir = std::filesystem::temp_directory_path() / "hasten_tests";
        std::filesystem::create_directories(temp_dir);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    std::filesystem::path WriteFile(const std::string& name, const std::string& content)
    {
        auto path = temp_dir / name;
        std::ofstream f(path);
        f << content;
        return path;
    }
};

TEST_F(Cli, TestRunOutputWithHelp)
{
    testing::internal::CaptureStdout();

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--help")};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 0);

    // clang-format off
    std::string expectation =
        "Usage: hasten <Options>:\n"
        "Options:\n"
        "  -h [ --help ]            Show help message\n"
        "  -i [ --input-file ] FILE Hasten IDL input file. This should be root module \n"
        "                           file. Imports are resolved relative to this file.\n"
        "  -o [ --output-dir ] DIR  Output directory. If not specified, use the same \n"
        "                           directory as input file.\n"
        "  -c [ --check-only ]      Only check the input IDL for errors\n"
        "  -a [ --print-ast ]       Emit parsed AST as JSON\n"
        "  -u [ --assign-uids ]     Assign unique IDs to AST nodes\n\n";
    // clang-format on

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, expectation);

    // to assist with mismatch debugging
    if (output != expectation) {
        auto mismatch = std::mismatch(expectation.begin(), expectation.end(), output.begin(), output.end());
        std::cout << (int)*mismatch.first << " " << (int)*mismatch.second << "\n";
        std::cout << std::distance(expectation.begin(), mismatch.first) << " "
                  << std::distance(output.begin(), mismatch.second) << "\n";
        std::cout << expectation.size() << " " << output.size() << "\n";
    }
}

TEST_F(Cli, TestRunOutputWithNoInputFile)
{
    testing::internal::CaptureStdout();

    int argc = 1;
    char* argv[] = {const_cast<char*>("hasten")};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 1);

    std::string output = testing::internal::GetCapturedStdout();

    // The spdlog error contains the timestamp, so we can't compare it.
    // Search for the expected message as substring instead.
    std::string expectation =
        "[error] Failed to parse command line: the option '--input-file' is required but missing\n";
    EXPECT_TRUE(output.find(expectation) != std::string::npos);
}

TEST_F(Cli, TestRunWithInputFile)
{
    testing::internal::CaptureStdout();

    auto idl = R"IDL(
        module test;
        interface foo {
            rpc bar(1: i32 x) -> bool;
        };
    )IDL";

    auto idl_path = WriteFile("foo.idl", idl);

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>(idl_path.c_str())};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 0);

    std::string expectation_one =
        fmt::format("Hasten v{}.{}.{}\n", HASTEN_VERSION_MAJOR, HASTEN_VERSION_MINOR, HASTEN_VERSION_PATCH);

    std::string expectation_two = "Parsed program with 1 files\n\n";

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find(expectation_one), std::string::npos);
    EXPECT_NE(output.find(expectation_two), std::string::npos);
}

TEST_F(Cli, TestRunOutputWithInvalidInputFile)
{
    testing::internal::CaptureStdout();

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--input-file=invalid.idl")};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 1);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("[error] Failed to parse program:"), std::string::npos);
    EXPECT_NE(output.find("Failed to open file: invalid.idl"), std::string::npos);
}

TEST_F(Cli, TestRunOutputWithDuplicateFieldIds) {
    testing::internal::CaptureStdout();

    auto idl = R"IDL(
        module test;
        struct foo {
            1: i32 x;
            1: i32 y;
        };
    )IDL";

    auto idl_path = WriteFile("foo.idl", idl);

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>(idl_path.c_str())};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 1);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("[error] Semantic analysis failed:"), std::string::npos);
    EXPECT_NE(output.find("Duplicate field id '1' in struct 'foo'"), std::string::npos);
}


TEST_F(Cli, TestRunOutputWithDuplicateParameterIds) {
    testing::internal::CaptureStdout();

    auto idl = R"IDL(
        module test;
        interface foo {
            rpc bar(1: i32 x, 1: i32 y) -> bool;
        };
    )IDL";

    auto idl_path = WriteFile("foo.idl", idl);

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>(idl_path.c_str())};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 1);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("[error] Semantic analysis failed:"), std::string::npos);
    EXPECT_NE(output.find("Duplicate parameter id '1' in method 'bar'"), std::string::npos);
}

TEST_F(Cli, TestRunOutputWithDuplicateResultIds) {
    testing::internal::CaptureStdout();

    auto idl = R"IDL(
        module test;
        interface foo {
            rpc baz(1: i32 x) -> (1: i32 y, 1: i32 z);
        };
    )IDL";

    auto idl_path = WriteFile("foo.idl", idl);

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>(idl_path.c_str())};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 1);

    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("[error] Semantic analysis failed:"), std::string::npos);
    EXPECT_NE(output.find("Duplicate result field id '1' in method 'baz'"), std::string::npos);
}

TEST_F(Cli, TestRunWithUnknownUserType)
{
    testing::internal::CaptureStdout();

    auto idl = R"IDL(
        module sample;
        struct Foo {
            1: MissingType value;
        };
    )IDL";

    auto idl_path = WriteFile("unknown.idl", idl);

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>(idl_path.c_str())};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 1);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Unknown type 'MissingType' referenced in field 'value' of struct 'Foo'"),
              std::string::npos);
}

TEST_F(Cli, TestRunWithDuplicateModules)
{
    testing::internal::CaptureStdout();

    auto main_idl = R"IDL(
        module sample;
        import "other.idl";
        struct Foo { 1: i32 id; };
    )IDL";

    auto other_idl = R"IDL(
        module sample;
        struct Bar { 1: i32 id; };
    )IDL";

    auto main_path = WriteFile("main.idl", main_idl);
    WriteFile("other.idl", other_idl);

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>(main_path.c_str())};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 1);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Module 'sample' already defined"), std::string::npos);
}

TEST_F(Cli, TestRunWithInvalidMapKey)
{
    testing::internal::CaptureStdout();

    auto idl = R"IDL(
        module sample;
        struct Foo {
            1: map<vector<i32>, string> data;
        };
    )IDL";

    auto idl_path = WriteFile("map.idl", idl);

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>(idl_path.c_str())};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 1);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Map key in field 'data' of struct 'Foo' must be a primitive or enum type"),
              std::string::npos);
}

TEST_F(Cli, TestRunWithNestedOptional)
{
    testing::internal::CaptureStdout();

    auto idl = R"IDL(
        module sample;
        struct Foo {
            1: optional<optional<i64>> value;
        };
    )IDL";

    auto idl_path = WriteFile("optional.idl", idl);

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>(idl_path.c_str())};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 1);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Nested optional types are not allowed in field 'value' of struct 'Foo'"),
              std::string::npos);
}

TEST_F(Cli, TestWarningForFieldIdGaps)
{
    testing::internal::CaptureStdout();

    auto idl = R"IDL(
        module sample;
        struct Foo {
            1: i32 a;
            3: i32 c;
        };
    )IDL";

    auto idl_path = WriteFile("gap.idl", idl);

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>(idl_path.c_str())};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 0);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Gap detected between 1 and 3 for field ids in struct 'Foo'"), std::string::npos);
    EXPECT_NE(output.find("Semantic analysis diagnostics"), std::string::npos);
}

TEST_F(Cli, TestPrintAstOutputsJson)
{
    testing::internal::CaptureStdout();

    auto shared_idl = R"IDL(
        module sample.shared;

        struct SharedData {
            1: string tag [deprecated];
            2: i32 version [deprecated=false];
        };
    )IDL";

    auto main_idl = R"IDL(
        module sample;

        import "shared.idl";

        struct User {
            1: i32 id;
            2: string name;
            3: optional<vector<u64>> tokens;
            4: map<string, i64> metadata;
        };

        interface Foo {
            rpc ping(1: string msg) -> (1: string reply);
            rpc status(1: u32 code) -> bool;
        };
    )IDL";

    auto shared_path = WriteFile("shared.idl", shared_idl);
    auto main_path = WriteFile("sample.idl", main_idl);
    auto shared_path_str = shared_path.string();
    auto main_path_str = main_path.string();

    int argc = 3;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>(main_path_str.c_str()),
                    const_cast<char*>("--print-ast")};

    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 0);

    std::string output = testing::internal::GetCapturedStdout();
    auto json_start = output.find('{');
    ASSERT_NE(json_start, std::string::npos);

    std::string json_payload = output.substr(json_start);
    auto parsed = nlohmann::json::parse(json_payload);

    ASSERT_TRUE(parsed.contains("files"));
    ASSERT_EQ(parsed["files"].size(), 2);

    auto find_file = [&](const std::string& path) -> const nlohmann::json& {
        for (const auto& file_json : parsed["files"]) {
            if (file_json.at("path") == path) {
                return file_json;
            }
        }
        ADD_FAILURE() << "File path not found in AST JSON: " << path;
        return parsed["files"][0];
    };

    const auto& main_file = find_file(main_path_str);
    const auto& shared_file = find_file(shared_path_str);

    // Validate main module basics
    const auto& main_module = main_file.at("module");
    EXPECT_EQ(main_module.at("name"), "sample");
    ASSERT_TRUE(main_module.contains("imports"));
    ASSERT_EQ(main_module.at("imports").size(), 1);
    EXPECT_EQ(main_module.at("imports")[0].at("path"), "shared.idl");

    // Validate User struct with diverse field types
    const auto& main_decls = main_module.at("declarations");
    auto user_decl_it = std::find_if(main_decls.begin(), main_decls.end(), [](const nlohmann::json& decl) {
        return decl.at("kind") == "struct" && decl.at("name") == "User";
    });
    ASSERT_NE(user_decl_it, main_decls.end());
    const auto& user_struct = *user_decl_it;
    ASSERT_EQ(user_struct.at("fields").size(), 4);
    EXPECT_EQ(user_struct.at("fields")[0].at("type").at("name"), "i32");
    EXPECT_EQ(user_struct.at("fields")[2].at("type").at("kind"), "optional");
    EXPECT_EQ(user_struct.at("fields")[2].at("type").at("inner").at("kind"), "vector");
    EXPECT_EQ(user_struct.at("fields")[2].at("type").at("inner").at("element").at("name"), "u64");
    EXPECT_EQ(user_struct.at("fields")[3].at("type").at("kind"), "map");
    EXPECT_EQ(user_struct.at("fields")[3].at("type").at("key").at("name"), "string");

    // Validate Foo interface methods and result forms
    auto foo_decl_it = std::find_if(main_decls.begin(), main_decls.end(), [](const nlohmann::json& decl) {
        return decl.at("kind") == "interface" && decl.at("name") == "Foo";
    });
    ASSERT_NE(foo_decl_it, main_decls.end());
    const auto& foo_iface = *foo_decl_it;
    ASSERT_EQ(foo_iface.at("methods").size(), 2);
    const auto& ping_method = foo_iface.at("methods")[0];
    ASSERT_TRUE(ping_method.contains("result"));
    EXPECT_EQ(ping_method.at("result").at("kind"), "tuple");
    const auto& status_method = foo_iface.at("methods")[1];
    ASSERT_TRUE(status_method.contains("result"));
    EXPECT_EQ(status_method.at("result").at("kind"), "type");
    EXPECT_EQ(status_method.at("result").at("type").at("name"), "bool");

    // Validate imported file attributes
    const auto& shared_module_json = shared_file.at("module");
    EXPECT_EQ(shared_module_json.at("name"), "sample.shared");
    const auto& shared_decl = shared_module_json.at("declarations")[0];
    ASSERT_EQ(shared_decl.at("kind"), "struct");
    const auto& shared_fields = shared_decl.at("fields");
    ASSERT_EQ(shared_fields.size(), 2);
    ASSERT_EQ(shared_fields[0].at("attributes").size(), 1);
    EXPECT_EQ(shared_fields[0].at("attributes")[0].at("name"), "deprecated");
    ASSERT_EQ(shared_fields[1].at("attributes").size(), 1);
    EXPECT_EQ(shared_fields[1].at("attributes")[0].at("value"), false);
}
