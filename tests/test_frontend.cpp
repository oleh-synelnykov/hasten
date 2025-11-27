#include <gtest/gtest.h>

#include "frontend/frontend.hpp"
#include "idl/visit.hpp"

#include <boost/core/demangle.hpp>

#include <filesystem>
#include <fstream>

class Frontend : public ::testing::Test
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

TEST_F(Frontend, ParseProgramSingleFile)
{
    auto idl = R"IDL(
        module test;
        interface foo {
            rpc bar(1: i32 x) -> bool;
        };
    )IDL";

    auto idl_path = WriteFile("foo.idl", idl);

    auto result = hasten::frontend::parse_program(idl_path.string());
    ASSERT_TRUE(result) << result.error();

    const auto& files = result->files;
    ASSERT_EQ(files.size(), 1);
    ASSERT_TRUE(files.contains(idl_path.string()));

    const auto& module = files.at(idl_path.string()).module;
    ASSERT_EQ(module.name.parts.size(), 1);
    EXPECT_EQ(module.name.parts.at(0), "test");

    const auto& declarations = module.decls;
    ASSERT_EQ(declarations.size(), 1);

    const auto* interface = boost::get<hasten::idl::ast::Interface>(&declarations.at(0));
    ASSERT_NE(interface, nullptr);
    EXPECT_EQ(interface->name, "foo");

    const auto& methods = interface->methods;

    ASSERT_EQ(methods.size(), 1);
    const auto& method = methods.at(0);
    EXPECT_EQ(method.name, "bar");
    EXPECT_EQ(method.kind, hasten::idl::ast::MethodKind::Rpc);

    ASSERT_EQ(method.params.size(), 1);
    const auto& param = method.params.at(0);
    EXPECT_EQ(param.name, "x");
    EXPECT_EQ(param.type.which(), 0);
    const auto* type = boost::get<hasten::idl::ast::Primitive>(&param.type);
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->kind, hasten::idl::ast::PrimitiveKind::I32);

    ASSERT_TRUE(method.result.has_value());
    {
        const auto* result = boost::get<hasten::idl::ast::Type>(&method.result.value());
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->which(), 0);
        const auto* type = boost::get<hasten::idl::ast::Primitive>(result);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, hasten::idl::ast::PrimitiveKind::Bool);
    }
}

TEST_F(Frontend, ParseProgramMultipleFiles)
{
    auto idl = R"IDL(
        module test;
        import "second.idl";
        interface foo {
            rpc bar(1: i32 x) -> bool;
        };
    )IDL";

    auto second_idl = R"IDL(
        module test2;
        import "third.idl";
        import "fourth.idl";
        interface foo2 {
            rpc bar2(1: i32 x2) -> bool;
        };
    )IDL";

    auto third_idl = R"IDL(
        module test3;
        interface foo3 {
            rpc bar3(1: i32 x3) -> bool;
        };
    )IDL";

    auto fourth_idl = R"IDL(
        module test4;
        interface foo4 {
            rpc bar4(1: i32 x4) -> bool;
        };
    )IDL";

    auto idl_path = WriteFile("foo.idl", idl);
    auto second_idl_path = WriteFile("second.idl", second_idl);
    auto third_idl_path = WriteFile("third.idl", third_idl);
    auto fourth_idl_path = WriteFile("fourth.idl", fourth_idl);
    auto out_dir = temp_dir / "out";

    auto result = hasten::frontend::parse_program(idl_path.string());
    ASSERT_TRUE(result) << result.error();

    const auto& files = result->files;
    ASSERT_EQ(files.size(), 4);
    ASSERT_TRUE(files.contains(idl_path.string()));
    ASSERT_TRUE(files.contains(second_idl_path.string()));
    ASSERT_TRUE(files.contains(third_idl_path.string()));
    ASSERT_TRUE(files.contains(fourth_idl_path.string()));

    {
        // first idl
        const auto& module = files.at(idl_path.string()).module;
        ASSERT_EQ(module.name.parts.size(), 1);
        EXPECT_EQ(module.name.parts.at(0), "test");

        const auto& declarations = module.decls;
        ASSERT_EQ(declarations.size(), 1);

        const auto* interface = boost::get<hasten::idl::ast::Interface>(&declarations.at(0));
        ASSERT_NE(interface, nullptr);
        EXPECT_EQ(interface->name, "foo");

        const auto& methods = interface->methods;

        ASSERT_EQ(methods.size(), 1);
        const auto& method = methods.at(0);
        EXPECT_EQ(method.name, "bar");
        EXPECT_EQ(method.kind, hasten::idl::ast::MethodKind::Rpc);

        ASSERT_EQ(method.params.size(), 1);
        const auto& param = method.params.at(0);
        EXPECT_EQ(param.name, "x");
        EXPECT_EQ(param.type.which(), 0);
        const auto* type = boost::get<hasten::idl::ast::Primitive>(&param.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, hasten::idl::ast::PrimitiveKind::I32);

        ASSERT_TRUE(method.result.has_value());
        {
            const auto* result = boost::get<hasten::idl::ast::Type>(&method.result.value());
            ASSERT_NE(result, nullptr);
            EXPECT_EQ(result->which(), 0);
            const auto* type = boost::get<hasten::idl::ast::Primitive>(result);
            ASSERT_NE(type, nullptr);
            EXPECT_EQ(type->kind, hasten::idl::ast::PrimitiveKind::Bool);
        }
    }

    {
        // second idl
        const auto& module = files.at(second_idl_path.string()).module;
        ASSERT_EQ(module.name.parts.size(), 1);
        EXPECT_EQ(module.name.parts.at(0), "test2");

        const auto& declarations = module.decls;
        ASSERT_EQ(declarations.size(), 1);

        const auto* interface = boost::get<hasten::idl::ast::Interface>(&declarations.at(0));
        ASSERT_NE(interface, nullptr);
        EXPECT_EQ(interface->name, "foo2");

        const auto& methods = interface->methods;

        ASSERT_EQ(methods.size(), 1);
        const auto& method = methods.at(0);
        EXPECT_EQ(method.name, "bar2");
        EXPECT_EQ(method.kind, hasten::idl::ast::MethodKind::Rpc);

        ASSERT_EQ(method.params.size(), 1);
        const auto& param = method.params.at(0);
        EXPECT_EQ(param.name, "x2");
        EXPECT_EQ(param.type.which(), 0);
        const auto* type = boost::get<hasten::idl::ast::Primitive>(&param.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, hasten::idl::ast::PrimitiveKind::I32);

        ASSERT_TRUE(method.result.has_value());
        {
            const auto* result = boost::get<hasten::idl::ast::Type>(&method.result.value());
            ASSERT_NE(result, nullptr);
            EXPECT_EQ(result->which(), 0);
            const auto* type = boost::get<hasten::idl::ast::Primitive>(result);
            ASSERT_NE(type, nullptr);
            EXPECT_EQ(type->kind, hasten::idl::ast::PrimitiveKind::Bool);
        }
    }

    {
        // third idl
        const auto& module = files.at(third_idl_path.string()).module;
        ASSERT_EQ(module.name.parts.size(), 1);
        EXPECT_EQ(module.name.parts.at(0), "test3");

        const auto& declarations = module.decls;
        ASSERT_EQ(declarations.size(), 1);

        const auto* interface = boost::get<hasten::idl::ast::Interface>(&declarations.at(0));
        ASSERT_NE(interface, nullptr);
        EXPECT_EQ(interface->name, "foo3");

        const auto& methods = interface->methods;

        ASSERT_EQ(methods.size(), 1);
        const auto& method = methods.at(0);
        EXPECT_EQ(method.name, "bar3");
        EXPECT_EQ(method.kind, hasten::idl::ast::MethodKind::Rpc);

        ASSERT_EQ(method.params.size(), 1);
        const auto& param = method.params.at(0);
        EXPECT_EQ(param.name, "x3");
        EXPECT_EQ(param.type.which(), 0);
        const auto* type = boost::get<hasten::idl::ast::Primitive>(&param.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, hasten::idl::ast::PrimitiveKind::I32);

        ASSERT_TRUE(method.result.has_value());
        {
            const auto* result = boost::get<hasten::idl::ast::Type>(&method.result.value());
            ASSERT_NE(result, nullptr);
            EXPECT_EQ(result->which(), 0);
            const auto* type = boost::get<hasten::idl::ast::Primitive>(result);
            ASSERT_NE(type, nullptr);
            EXPECT_EQ(type->kind, hasten::idl::ast::PrimitiveKind::Bool);
        }
    }

    {
        // fourth idl
        const auto& module = files.at(fourth_idl_path.string()).module;
        ASSERT_EQ(module.name.parts.size(), 1);
        EXPECT_EQ(module.name.parts.at(0), "test4");

        const auto& declarations = module.decls;
        ASSERT_EQ(declarations.size(), 1);

        const auto* interface = boost::get<hasten::idl::ast::Interface>(&declarations.at(0));
        ASSERT_NE(interface, nullptr);
        EXPECT_EQ(interface->name, "foo4");

        const auto& methods = interface->methods;

        ASSERT_EQ(methods.size(), 1);
        const auto& method = methods.at(0);
        EXPECT_EQ(method.name, "bar4");
        EXPECT_EQ(method.kind, hasten::idl::ast::MethodKind::Rpc);

        ASSERT_EQ(method.params.size(), 1);
        const auto& param = method.params.at(0);
        EXPECT_EQ(param.name, "x4");
        EXPECT_EQ(param.type.which(), 0);
        const auto* type = boost::get<hasten::idl::ast::Primitive>(&param.type);
        ASSERT_NE(type, nullptr);
        EXPECT_EQ(type->kind, hasten::idl::ast::PrimitiveKind::I32);

        ASSERT_TRUE(method.result.has_value());
        {
            const auto* result = boost::get<hasten::idl::ast::Type>(&method.result.value());
            ASSERT_NE(result, nullptr);
            EXPECT_EQ(result->which(), 0);
            const auto* type = boost::get<hasten::idl::ast::Primitive>(result);
            ASSERT_NE(type, nullptr);
            EXPECT_EQ(type->kind, hasten::idl::ast::PrimitiveKind::Bool);
        }
    }
}

struct TraverseAll {
    template <typename T>
    void operator()(const T& node)
    {
        // It is ugly as hell to use in unit tests. If this doesn't work with your
        // compiler - feel free to disable VisitWholeProgram test (e.g. rename it to
        // DISABLED_VisitWholeProgram)
        std::cout << "Visiting " << boost::core::demangle(typeid(node).name()) << "\n";
    }
};

TEST_F(Frontend, VisitWholeProgram)
{
    auto idl = R"IDL(
        module test;
        struct foo {
           1: i32 x;
           2: i32 y;
        };
        interface bar {
            rpc baz(1: i32 x) -> i32;
        };
    )IDL";

    auto idl_path = WriteFile("foo.idl", idl);

    testing::internal::CaptureStdout();

    auto maybe_program = hasten::frontend::parse_program(idl_path.string());
    ASSERT_TRUE(maybe_program) << maybe_program.error();

    hasten::frontend::Program& program = maybe_program.value();

    TraverseAll pass;
    for (const auto& [_, f] : program.files) {
        hasten::idl::ast::visit(f.module, pass);
    }

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectation =
        "Visiting hasten::idl::ast::Module\n"
        "Visiting hasten::idl::ast::Struct\n"
        "Visiting hasten::idl::ast::Field\n"
        "Visiting hasten::idl::ast::Primitive\n"
        "Visiting hasten::idl::ast::Field\n"
        "Visiting hasten::idl::ast::Primitive\n"
        "Visiting hasten::idl::ast::Interface\n"
        "Visiting hasten::idl::ast::Method\n"
        "Visiting hasten::idl::ast::Parameter\n"
        "Visiting hasten::idl::ast::Primitive\n";

    EXPECT_EQ(output, expectation);
}
