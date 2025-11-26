#include <gtest/gtest.h>

#include "frontend/frontend.hpp"

#include <filesystem>
#include <fstream>

class FrontendDetail : public ::testing::Test
{
   protected:
    std::filesystem::path tempDir;

    void SetUp() override
    {
        tempDir = std::filesystem::temp_directory_path() / "hasten_tests";
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
    }

    std::filesystem::path WriteFile(const std::string& name, const std::string& content)
    {
        auto path = tempDir / name;
        std::ofstream f(path);
        f << content;
        return path;
    }
};

TEST_F(FrontendDetail, ReadFile)
{
    auto idl = R"IDL(
        module test;
        interface foo {
            rpc bar(1: i32 x) -> bool;
        };
    )IDL";

    auto idlPath = WriteFile("foo.idl", idl);

    auto result = hasten::frontend::detail::read_file(idlPath.string());
    ASSERT_TRUE(result) << result.error();
    EXPECT_EQ(result.value(), idl);
}

TEST_F(FrontendDetail, ReadEmptyFile)
{
    auto idlPath = WriteFile("empty.idl", "");

    auto result = hasten::frontend::detail::read_file(idlPath.string());
    ASSERT_TRUE(result) << result.error();
    EXPECT_EQ(result.value(), "");
}

TEST_F(FrontendDetail, ReadNonExistentFile)
{
    auto result = hasten::frontend::detail::read_file("non_existent.idl");
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().starts_with("Failed to open file: non_existent.idl"));
}

TEST_F(FrontendDetail, ParseFileContentSuccess)
{
    auto idl = R"IDL(
        module detail;
        interface foo {
            rpc bar(1: i32 x) -> bool;
        };
    )IDL";

    std::expected<std::string, std::string> content = std::string(idl);

    auto result = hasten::frontend::detail::parse_file_content(std::move(content));
    ASSERT_TRUE(result) << result.error();
    EXPECT_EQ(result->content, idl);
    EXPECT_TRUE(result->path.empty());
    ASSERT_EQ(result->module.name.parts.size(), 1);
    EXPECT_EQ(result->module.name.parts.at(0), "detail");
}

TEST_F(FrontendDetail, ParseFileContentPropagatesError)
{
    auto result = hasten::frontend::detail::parse_file_content(std::unexpected<std::string>("parse failure"));
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), "parse failure");
}

TEST_F(FrontendDetail, ParseSingleFileSuccess)
{
    auto idl = R"IDL(
        module single;
        interface foo {
            rpc bar(1: i32 x) -> bool;
        };
    )IDL";

    auto path = WriteFile("single.idl", idl);

    auto result = hasten::frontend::detail::parse_single_file(path.string());
    ASSERT_TRUE(result) << result.error();
    EXPECT_EQ(result->path, path.string());
    EXPECT_EQ(result->content, idl);
    ASSERT_EQ(result->module.name.parts.size(), 1);
    EXPECT_EQ(result->module.name.parts.at(0), "single");
}

TEST_F(FrontendDetail, ParseSingleFileMissingFile)
{
    auto path = (tempDir / "does_not_exist.idl").string();

    auto result = hasten::frontend::detail::parse_single_file(path);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().starts_with("Failed to open file: " + path));
}

TEST_F(FrontendDetail, ParseImportsParsesDependencies)
{
    auto root = R"IDL(
        module root;
        import "second.idl";
        interface root_if {
            rpc bar(1: i32 x) -> bool;
        };
    )IDL";

    auto second = R"IDL(
        module second;
        import "third.idl";
        interface second_if {
            rpc baz(1: i32 x) -> bool;
        };
    )IDL";

    auto third = R"IDL(
        module third;
        interface third_if {
            rpc qux(1: i32 x) -> bool;
        };
    )IDL";

    auto rootPath = WriteFile("root.idl", root);
    auto secondPath = WriteFile("second.idl", second);
    auto thirdPath = WriteFile("third.idl", third);

    hasten::frontend::Program::Files files;
    auto result = hasten::frontend::detail::parse_imports(rootPath.string(), files);
    ASSERT_TRUE(result) << result.error();

    ASSERT_EQ(files.size(), 3);
    EXPECT_TRUE(files.contains(rootPath.string()));
    EXPECT_TRUE(files.contains(secondPath.string()));
    EXPECT_TRUE(files.contains(thirdPath.string()));

    EXPECT_EQ(files.at(rootPath.string()).module.imports.size(), 1);
    EXPECT_EQ(files.at(secondPath.string()).module.imports.size(), 1);
    EXPECT_EQ(files.at(thirdPath.string()).module.imports.size(), 0);
}

TEST_F(FrontendDetail, ParseImportsDetectsDuplicate)
{
    {
        auto idl = R"IDL(
            module duplicate;
            interface foo {
                rpc bar(1: i32 x) -> bool;
            };
        )IDL";

        auto path = WriteFile("duplicate.idl", idl);

        hasten::frontend::Program::Files files;
        auto first = hasten::frontend::detail::parse_imports(path.string(), files);
        ASSERT_TRUE(first) << first.error();

        auto second = hasten::frontend::detail::parse_imports(path.string(), files);
        ASSERT_FALSE(second);
        EXPECT_EQ(second.error(), "Duplicate import: " + path.string());
    }
}

TEST_F(FrontendDetail, ParseImportsSkipsAlreadyParsed)
{
    auto first_idl = R"IDL(
        module first;
        import "second.idl";
        interface foo {
            rpc bar(1: i32 x) -> bool;
        };
    )IDL";

    auto second_idl = R"IDL(
        module second;
        import "first.idl";
        interface foo {
            rpc bar(1: i32 x) -> bool;
        };
    )IDL";

    auto firstPath = WriteFile("first.idl", first_idl);
    auto secondPath = WriteFile("second.idl", second_idl);

    hasten::frontend::Program::Files files;
    auto result = hasten::frontend::detail::parse_imports(firstPath.string(), files);
    ASSERT_TRUE(result) << result.error();

    ASSERT_EQ(files.size(), 2);
    EXPECT_TRUE(files.contains(firstPath.string()));
    EXPECT_TRUE(files.contains(secondPath.string()));
}
