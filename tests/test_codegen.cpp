#include <gtest/gtest.h>

#include "codegen/generator.hpp"
#include "frontend/frontend.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

class Codegen : public ::testing::Test
{
protected:
    std::filesystem::path temp_dir;

    void SetUp() override
    {
        auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        auto unique = std::string("hasten_codegen-") + std::to_string(stamp);
        temp_dir = std::filesystem::temp_directory_path() / unique;
        std::filesystem::create_directories(temp_dir);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    std::filesystem::path write_file(const std::string& name, const std::string& content)
    {
        auto path = temp_dir / name;
        std::ofstream out(path);
        out << content;
        return path;
    }

    static std::string read_file(const std::filesystem::path& path)
    {
        std::ifstream in(path);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
};

TEST_F(Codegen, GeneratesBasicModule)
{
    auto idl = R"IDL(
        module sample.core;

        struct Payload {
            1: string message;
        };

        interface Echo {
            rpc Ping(1: Payload payload) -> (1: Payload reply);
        };
    )IDL";

    auto idl_path = write_file("echo.hidl", idl);
    auto program = hasten::frontend::parse_program(idl_path.string());
    ASSERT_TRUE(program) << program.error();

    auto output_dir = temp_dir / "out";
    hasten::codegen::GenerationOptions options;
    options.output_dir = output_dir;

    hasten::codegen::Generator generator{program.value(), options};
    auto result = generator.run();
    ASSERT_TRUE(result) << result.error();

    auto header = output_dir / "sample" / "core" / "sample_core.gen.hpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    auto client_src = output_dir / "sample" / "core" / "sample_core_Echo_client.gen.cpp";
    auto server_src = output_dir / "sample" / "core" / "sample_core_Echo_server.gen.cpp";
    ASSERT_TRUE(std::filesystem::exists(client_src));
    ASSERT_TRUE(std::filesystem::exists(server_src));

    std::string contents = read_file(header);
    EXPECT_NE(contents.find("class EchoClient"), std::string::npos);
    EXPECT_NE(contents.find("struct Payload"), std::string::npos);

    auto manifest = output_dir / "generated.cmake";
    ASSERT_TRUE(std::filesystem::exists(manifest));
    std::string manifest_contents = read_file(manifest);
    EXPECT_NE(manifest_contents.find("add_library(hasten_sample_core_common INTERFACE)"), std::string::npos);
    EXPECT_NE(manifest_contents.find("add_library(hasten_sample_core_Echo_client OBJECT"), std::string::npos);
    EXPECT_NE(manifest_contents.find("add_library(hasten_sample_core_Echo_server OBJECT"), std::string::npos);
}

TEST_F(Codegen, GeneratesModuleWithEnumsAndMethodVariants)
{
    auto idl = R"IDL(
        module features.testing;

        enum Status {
            Ok = 0,
            Failed = 1,
            Pending = 2,
        };

        struct Settings {
            1: optional<string> mode = "auto";
            2: i32 level = 42;
        };

        interface Multi {
            rpc GetStatus(1: optional<i32> code = 5) -> Status;
            oneway Reset(1: Settings payload);
            stream Watch(1: Settings request) -> (1: Settings update, 2: string note);
            notify Alarm(1: i32 code);
        };
    )IDL";

    auto idl_path = write_file("features.hidl", idl);
    auto program = hasten::frontend::parse_program(idl_path.string());
    ASSERT_TRUE(program) << program.error();

    auto output_dir = temp_dir / "features_out";
    hasten::codegen::GenerationOptions options;
    options.output_dir = output_dir;

    hasten::codegen::Generator generator{program.value(), options};
    auto result = generator.run();
    ASSERT_TRUE(result) << result.error();

    auto header = output_dir / "features" / "testing" / "features_testing.gen.hpp";
    auto client_src = output_dir / "features" / "testing" / "features_testing_Multi_client.gen.cpp";
    auto server_src = output_dir / "features" / "testing" / "features_testing_Multi_server.gen.cpp";
    ASSERT_TRUE(std::filesystem::exists(header));
    ASSERT_TRUE(std::filesystem::exists(client_src));
    ASSERT_TRUE(std::filesystem::exists(server_src));

#define ASSERT_FIND_BEGIN(contents, str)             \
    std::string::size_type pos = contents.find(str); \
    ASSERT_NE(pos, std::string::npos);

#define ASSERT_FIND(contents, str)     \
    pos = contents.find(str, pos);     \
    ASSERT_NE(pos, std::string::npos);

    // Header file

    const std::string header_contents = read_file(header);

    {
        ASSERT_FIND_BEGIN(header_contents, "namespace features::testing")
        ASSERT_FIND(header_contents, "{")
    }

    {
        ASSERT_FIND_BEGIN(header_contents, "enum class Status")
        ASSERT_FIND(header_contents, "{")
        ASSERT_FIND(header_contents, "Ok = 0,")
        ASSERT_FIND(header_contents, "Failed = 1,")
        ASSERT_FIND(header_contents, "Pending = 2")
        ASSERT_FIND(header_contents, "};")
    }

    {
        ASSERT_FIND_BEGIN(header_contents, "struct Settings")
        ASSERT_FIND(header_contents, "{")
        ASSERT_FIND(header_contents, "std::optional<std::string> mode;")
        ASSERT_FIND(header_contents, "int32_t level;")
        ASSERT_FIND(header_contents, "};")
    }

    {
        ASSERT_FIND_BEGIN(header_contents, "struct MultiWatchResult")
        ASSERT_FIND(header_contents, "{")
        ASSERT_FIND(header_contents, "Settings update;")
        ASSERT_FIND(header_contents, "std::string note;")
        ASSERT_FIND(header_contents, "};")
    }

    {
        // Client class
        ASSERT_FIND_BEGIN(header_contents, "class MultiClient")
        ASSERT_FIND(header_contents, "{")
        ASSERT_FIND(header_contents, "public:")
        // Constructor
        ASSERT_FIND(header_contents, "MultiClient(")
        ASSERT_FIND(header_contents, "std::shared_ptr<hasten::runtime::Channel> channel")
        ASSERT_FIND(header_contents, ",")
        ASSERT_FIND(header_contents, "std::shared_ptr<hasten::runtime::Dispatcher> dispatcher")
        ASSERT_FIND(header_contents, ");")

        // Methods
        // GetStatus
        ASSERT_FIND(header_contents, "void GetStatus(")
        ASSERT_FIND(header_contents, "const std::optional<std::int32_t>& code");
        ASSERT_FIND(header_contents, ",");
        ASSERT_FIND(header_contents, "std::function<void(hasten::runtime::Result<Status>)> callback")
        ASSERT_FIND(header_contents, ") const;");
        ASSERT_FIND(header_contents, "std::future<hasten::runtime::Result<Status>> GetStatus_async(");
        ASSERT_FIND(header_contents, "const std::optional<std::int32_t>& code");
        ASSERT_FIND(header_contents, ") const;");
        ASSERT_FIND(header_contents, "hasten::runtime::Result<Status> GetStatus_sync(");
        ASSERT_FIND(header_contents, "const std::optional<std::int32_t>& code");
        ASSERT_FIND(header_contents, ") const;");

        // Reset
        ASSERT_FIND(header_contents, "void Reset(");
        ASSERT_FIND(header_contents, "const Settings& payload");
        ASSERT_FIND(header_contents, ",");
        ASSERT_FIND(header_contents, "std::function<void(hasten::runtime::Result<void>)> callback");
        ASSERT_FIND(header_contents, ") const;");
        ASSERT_FIND(header_contents, "std::future<hasten::runtime::Result<void>> Reset_async(");
        ASSERT_FIND(header_contents, "const Settings& payload");
        ASSERT_FIND(header_contents, ") const;");
        ASSERT_FIND(header_contents, "hasten::runtime::Result<void> Reset_sync(");
        ASSERT_FIND(header_contents, "const Settings& payload");
        ASSERT_FIND(header_contents, ") const;");

        // Watch
        ASSERT_FIND(header_contents, "void Watch(");
        ASSERT_FIND(header_contents, "const Settings& request");
        ASSERT_FIND(header_contents, ",");
        ASSERT_FIND(header_contents,
                    "std::function<void(hasten::runtime::Result<MultiWatchResult>)> callback");
        ASSERT_FIND(header_contents, ") const;");
        ASSERT_FIND(header_contents, "std::future<hasten::runtime::Result<MultiWatchResult>> Watch_async(");
        ASSERT_FIND(header_contents, "const Settings& request");
        ASSERT_FIND(header_contents, ") const;");
        ASSERT_FIND(header_contents, "hasten::runtime::Result<MultiWatchResult> Watch_sync(");
        ASSERT_FIND(header_contents, "const Settings& request");
        ASSERT_FIND(header_contents, ") const;");

        // Alarm
        ASSERT_FIND(header_contents, "void Alarm(");
        ASSERT_FIND(header_contents, "std::int32_t code");
        ASSERT_FIND(header_contents, ",");
        ASSERT_FIND(header_contents, "std::function<void(hasten::runtime::Result<void>)> callback");
        ASSERT_FIND(header_contents, ") const;");
        ASSERT_FIND(header_contents, "std::future<hasten::runtime::Result<void>> Alarm_async(");
        ASSERT_FIND(header_contents, "std::int32_t code");
        ASSERT_FIND(header_contents, ") const;");
        ASSERT_FIND(header_contents, "hasten::runtime::Result<void> Alarm_sync(");
        ASSERT_FIND(header_contents, "std::int32_t code");
        ASSERT_FIND(header_contents, ") const;");

        // Fields
        ASSERT_FIND(header_contents, "private:")
        ASSERT_FIND(header_contents, "std::shared_ptr<hasten::runtime::Channel> channel_;")
        ASSERT_FIND(header_contents, "std::shared_ptr<hasten::runtime::Dispatcher> dispatcher_;")

        ASSERT_FIND(header_contents, "};")
    }

    {
        // Server interface
        ASSERT_FIND_BEGIN(header_contents, "class Multi")
        ASSERT_FIND(header_contents, "{")
        ASSERT_FIND(header_contents, "public:")
        ASSERT_FIND(header_contents, "virtual ~Multi() = default;")
        ASSERT_FIND(
            header_contents,
            "virtual hasten::runtime::Result<Status> GetStatus(const std::optional<std::int32_t>& code) = 0;")
        ASSERT_FIND(header_contents,
                    "virtual hasten::runtime::Result<void> Reset(const Settings& payload) = 0;")
        ASSERT_FIND(header_contents,
                    "virtual hasten::runtime::Result<MultiWatchResult> Watch(const Settings& request) = 0;")
        ASSERT_FIND(header_contents, "virtual hasten::runtime::Result<void> Alarm(std::int32_t code) = 0;")
        ASSERT_FIND(header_contents, "};")
    }

    {
        // bind_Multi
        ASSERT_FIND_BEGIN(header_contents, "void")
        ASSERT_FIND(header_contents, "bind_Multi(")
        ASSERT_FIND(header_contents, "hasten::runtime::Dispatcher& dispatcher")
        ASSERT_FIND(header_contents, ",");
        ASSERT_FIND(header_contents, "std::shared_ptr<Multi> implementation")
        ASSERT_FIND(header_contents, ",");
        ASSERT_FIND(header_contents, "std::shared_ptr<hasten::runtime::Executor> executor = nullptr");
        ASSERT_FIND(header_contents, ");")
    }

    {
        // make_Multi_client
        ASSERT_FIND_BEGIN(header_contents, "std::shared_ptr<MultiClient>")
        ASSERT_FIND(header_contents, "make_Multi_client(");
        ASSERT_FIND(header_contents, "std::shared_ptr<hasten::runtime::Channel> channel");
        ASSERT_FIND(header_contents, ",");
        ASSERT_FIND(header_contents, "std::shared_ptr<hasten::runtime::Dispatcher> dispatcher");
        ASSERT_FIND(header_contents, ");");
    }

    {
        // make_Multi_client_uds
        ASSERT_FIND_BEGIN(header_contents, "inline");
        ASSERT_FIND(header_contents, "hasten::runtime::Result<std::shared_ptr<MultiClient>>");
        ASSERT_FIND(header_contents, "make_Multi_client_uds(");
        ASSERT_FIND(header_contents, "const std::string& path");
        ASSERT_FIND(header_contents, ")");
        ASSERT_FIND(header_contents, "{");
        ASSERT_FIND(header_contents, "auto channel_result = hasten::runtime::uds::connect(path);");
        ASSERT_FIND(header_contents, "if (!channel_result) {");
        ASSERT_FIND(header_contents, "return std::unexpected(channel_result.error());");
        ASSERT_FIND(header_contents, "}");
        ASSERT_FIND(header_contents, "auto dispatcher = hasten::runtime::uds::make_dispatcher();");
        ASSERT_FIND(header_contents, "return make_Multi_client(std::move(channel_result.value()), dispatcher);");
        ASSERT_FIND(header_contents, "}");
    }

    {
        ASSERT_FIND_BEGIN(header_contents, "}")
        ASSERT_FIND(header_contents, "//")
        ASSERT_FIND(header_contents, "namespace features::testing")
    }

    // Client file

    const std::string client_contents = read_file(client_src);

    {
        ASSERT_FIND_BEGIN(client_contents, "#include")
        ASSERT_FIND(client_contents, "features_testing.gen.hpp")
    }

    {
        ASSERT_FIND_BEGIN(client_contents, "namespace features::testing")
        ASSERT_FIND(client_contents, "{")
    }

    {
        // Constructor definition
        ASSERT_FIND_BEGIN(client_contents, "MultiClient::MultiClient(")
        ASSERT_FIND(client_contents, "std::shared_ptr<hasten::runtime::Channel> channel")
        ASSERT_FIND(client_contents, ",")
        ASSERT_FIND(client_contents, "std::shared_ptr<hasten::runtime::Dispatcher> dispatcher")
        ASSERT_FIND(client_contents, ")")

        ASSERT_FIND(client_contents, ":")
        ASSERT_FIND(client_contents, "channel_(std::move(channel))")
        ASSERT_FIND(client_contents, ",")
        ASSERT_FIND(client_contents, "dispatcher_(std::move(dispatcher))")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // GetStatus definition
        ASSERT_FIND_BEGIN(client_contents, "void")
        ASSERT_FIND(client_contents, "MultiClient::GetStatus(")
        ASSERT_FIND(client_contents, "const std::optional<std::int32_t>& code")
        ASSERT_FIND(client_contents, "std::function<void(hasten::runtime::Result<Status>)> callback")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // GetStatus_async definition
        ASSERT_FIND_BEGIN(client_contents, "std::future<hasten::runtime::Result<Status>>")
        ASSERT_FIND(client_contents, "MultiClient::GetStatus_async(")
        ASSERT_FIND(client_contents, "const std::optional<std::int32_t>& code")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "return")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // GetStatus_sync definition
        ASSERT_FIND_BEGIN(client_contents, "hasten::runtime::Result<Status>")
        ASSERT_FIND(client_contents, "MultiClient::GetStatus_sync(")
        ASSERT_FIND(client_contents, "const std::optional<std::int32_t>& code")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "return")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // Reset
        ASSERT_FIND_BEGIN(client_contents, "void")
        ASSERT_FIND(client_contents, "MultiClient::Reset(")
        ASSERT_FIND(client_contents, "const Settings& payload")
        ASSERT_FIND(client_contents, ",")
        ASSERT_FIND(client_contents, "std::function<void(hasten::runtime::Result<void>)> callback")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // Reset_async
        ASSERT_FIND_BEGIN(client_contents, "std::future<hasten::runtime::Result<void>>")
        ASSERT_FIND(client_contents, "MultiClient::Reset_async(")
        ASSERT_FIND(client_contents, "const Settings& payload")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "return")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // Reset_sync
        ASSERT_FIND_BEGIN(client_contents, "hasten::runtime::Result<void>")
        ASSERT_FIND(client_contents, "MultiClient::Reset_sync(")
        ASSERT_FIND(client_contents, "const Settings& payload")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "return")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // Watch
        ASSERT_FIND_BEGIN(client_contents, "void")
        ASSERT_FIND(client_contents, "MultiClient::Watch(")
        ASSERT_FIND(client_contents, "const Settings& request")
        ASSERT_FIND(client_contents, ",")
        ASSERT_FIND(client_contents, "std::function<void(hasten::runtime::Result<MultiWatchResult>)> callback")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // Watch_async
        ASSERT_FIND_BEGIN(client_contents, "std::future<hasten::runtime::Result<MultiWatchResult>>")
        ASSERT_FIND(client_contents, "MultiClient::Watch_async(")
        ASSERT_FIND(client_contents, "const Settings& request")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "return")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // Watch_sync
        ASSERT_FIND_BEGIN(client_contents, "hasten::runtime::Result<MultiWatchResult>")
        ASSERT_FIND(client_contents, "MultiClient::Watch_sync(")
        ASSERT_FIND(client_contents, "const Settings& request")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "return")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // Alarm
        ASSERT_FIND_BEGIN(client_contents, "void")
        ASSERT_FIND(client_contents, "MultiClient::Alarm(")
        ASSERT_FIND(client_contents, "std::int32_t code")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // Alarm_async
        ASSERT_FIND_BEGIN(client_contents, "std::future<hasten::runtime::Result<void>>")
        ASSERT_FIND(client_contents, "MultiClient::Alarm_async(")
        ASSERT_FIND(client_contents, "std::int32_t code")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "return")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // Alarm_sync
        ASSERT_FIND_BEGIN(client_contents, "hasten::runtime::Result<void>")
        ASSERT_FIND(client_contents, "MultiClient::Alarm_sync(")
        ASSERT_FIND(client_contents, "std::int32_t code")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "const")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "return")
        ASSERT_FIND(client_contents, "}")
    }

    {
        // make_Multi_client
        ASSERT_FIND_BEGIN(client_contents, "std::shared_ptr<MultiClient>")
        ASSERT_FIND(client_contents, "make_Multi_client(")
        ASSERT_FIND(client_contents, "std::shared_ptr<hasten::runtime::Channel> channel")
        ASSERT_FIND(client_contents, "std::shared_ptr<hasten::runtime::Dispatcher> dispatcher")
        ASSERT_FIND(client_contents, ")")
        ASSERT_FIND(client_contents, "{")
        ASSERT_FIND(client_contents, "return")
        ASSERT_FIND(client_contents, "}")
    }
}
