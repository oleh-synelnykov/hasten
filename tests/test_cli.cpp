#include <gtest/gtest.h>

#include "cli/hasten.hpp"

#include <fmt/core.h>
#include <algorithm>


TEST(Cli, TestRunOutputWithHelp)
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

TEST(Cli, TestRunOutputWithNoInputFile)
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

TEST(Cli, TestRunWithInputFile)
{
    testing::internal::CaptureStdout();

    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--input-file=test.idl")};
    int result = hasten::run(argc, argv);
    EXPECT_EQ(result, 0);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, fmt::format("Hasten v{}.{}.{}\n", HASTEN_VERSION_MAJOR, HASTEN_VERSION_MINOR,
                                  HASTEN_VERSION_PATCH));
}

// For future tests:
// TEST(Cli, TestRunOutputWithInvalidInputFile)
// {
//     testing::internal::CaptureStdout();

//     int argc = 2;
//     char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--input-file=invalid.idl")};
//     int result = hasten::run(argc, argv);
//     EXPECT_EQ(result, 1);

//     std::string output = testing::internal::GetCapturedStdout();

//     // The spdlog error contains the timestamp, so we can't compare it.
//     // Search for the expected message as substring instead.
//     std::string expectation =
//         "[error] Failed to parse command line: the option '--input-file' is required but missing\n";
//     EXPECT_TRUE(output.find(expectation) != std::string::npos);
// }