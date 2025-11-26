#include <gtest/gtest.h>

#include "cli/options.hpp"

#include <algorithm>

TEST(CliOptions, ParseHelpOption)
{
    int argc = 2;
    char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--help")};
    auto opts = hasten::parse_command_line(argc, argv);
    ASSERT_TRUE(opts);
    ASSERT_TRUE(opts->help_message.has_value());

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

    auto help = opts->help_message.value();
    EXPECT_EQ(help, expectation);

    // to assist with mismatch debugging
    if (help != expectation) {
        auto mismatch = std::mismatch(expectation.begin(), expectation.end(), help.begin(), help.end());
        std::cout << (int)*mismatch.first << " " << (int)*mismatch.second << "\n";
        std::cout << std::distance(expectation.begin(), mismatch.first) << " "
                  << std::distance(help.begin(), mismatch.second) << "\n";
        std::cout << expectation.size() << " " << help.size() << "\n";
    }
}

TEST(CliOptions, ParseInputFileOption)
{
    // input file is required
    auto opts = hasten::parse_command_line(0, nullptr);
    ASSERT_FALSE(opts);
    EXPECT_EQ(opts.error(), "the option '--input-file' is required but missing");

    {
        // System usually passes argv[0] as the program name
        // but still input file is required
        int argc = 1;
        char* argv[] = {const_cast<char*>("hasten")};
        opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "the option '--input-file' is required but missing");
    }

    {
        // input file provided as positional argument
        int argc = 2;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("input.idl")};
        opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_FALSE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // error case - --input-file option is provided without value
        int argc = 2;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--input-file")};
        opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "the required argument for option '--input-file' is missing");
    }

    {
        // error case - -i option is provided without value
        int argc = 2;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-i")};
        opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "the required argument for option '--input-file' is missing");
    }

    {
        // input file provided with explicit --input-file option
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--input-file"),
                        const_cast<char*>("input.idl")};
        opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_FALSE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // input file specified with long option and equal sign
        int argc = 2;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--input-file=input.idl")};
        opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_FALSE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // input file provided with short -i option
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-i"), const_cast<char*>("input.idl")};
        opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_FALSE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // error case - input file provided with short -i option and positional argument
        int argc = 4;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-i"), const_cast<char*>("input.idl"),
                        const_cast<char*>("input2.idl")};
        opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "option '--input-file' cannot be specified more than once");
    }

    {
        // error case - input file provided more than once
        int argc = 5;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-i"), const_cast<char*>("input.idl"),
                        const_cast<char*>("-i"), const_cast<char*>("input2.idl")};
        opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "option '--input-file' cannot be specified more than once");
    }
}

TEST(CliOptions, ParseOutputDirOption)
{
    {
        // error case - --output-dir option is provided without value
        int argc = 2;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--output-dir")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "the required argument for option '--output-dir' is missing");
    }

    {
        // error case - -o option is provided without value
        int argc = 2;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-o")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "the required argument for option '--output-dir' is missing");
    }

    {
        // output dir provided with explicit --output-dir option, not no input file
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--output-dir"),
                        const_cast<char*>("output_dir")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "the option '--input-file' is required but missing");
    }

    {
        // output dir provided with short -o option
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-o"),
                        const_cast<char*>("output_dir")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "the option '--input-file' is required but missing");
    }

    {
        // output dir provided with short -o option, as well as input file (as positional argument)
        int argc = 4;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-o"), const_cast<char*>("output_dir"),
                        const_cast<char*>("input.idl")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->output_dir, "output_dir");
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // error case - output dir specified more than once
        int argc = 5;
        char* argv[] = {const_cast<char*>("hasten"),      const_cast<char*>("-o"),
                        const_cast<char*>("output_dir"),  const_cast<char*>("-o"),
                        const_cast<char*>("output_dir2"), const_cast<char*>("input.idl")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "option '--output-dir' cannot be specified more than once");
    }
}

TEST(CliOptions, ParseCheckOnlyOption)
{
    {
        // error case - check-only provided with explicit --check-only option, no input file
        int argc = 2;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--check-only")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "the option '--input-file' is required but missing");
    }

    {
        // check-only provided with explicit --check-only option, input file provided
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--check-only"),
                        const_cast<char*>("input.idl")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_TRUE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // check-only provided with short -c option, input file provided
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-c"), const_cast<char*>("input.idl")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_TRUE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }
}

TEST(CliOptions, ParsePrintAstOption)
{
    {
        // error case - print-ast provided with explicit --print-ast option, no input file
        int argc = 2;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--print-ast")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "the option '--input-file' is required but missing");
    }

    {
        // print-ast provided with explicit --print-ast option, input file provided
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--print-ast"),
                        const_cast<char*>("input.idl")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_FALSE(opts->check_only);
        EXPECT_TRUE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // print-ast provided with short -a option, input file provided
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-a"), const_cast<char*>("input.idl")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_FALSE(opts->check_only);
        EXPECT_TRUE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }
}

TEST(CliOptions, ParseAssignUidsOption)
{
    {
        // error case - assign-uids provided with explicit --assign-uids option, no input file
        int argc = 2;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--assign-uids")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_FALSE(opts);
        EXPECT_EQ(opts.error(), "the option '--input-file' is required but missing");
    }

    {
        // assign-uids provided with explicit --assign-uids option, input file provided
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("--assign-uids"),
                        const_cast<char*>("input.idl")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_FALSE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_TRUE(opts->assign_uids);
    }

    {
        // assign-uids provided with short -u option, input file provided
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-u"), const_cast<char*>("input.idl")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_FALSE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_TRUE(opts->assign_uids);
    }
}

TEST(CliOptions, ParseAllOptionCombinations)
{
    {
        // check-only
        int argc = 6;
        char* argv[] = {const_cast<char*>("hasten"),     const_cast<char*>("--input-file"),
                        const_cast<char*>("input.idl"),  const_cast<char*>("--output-dir"),
                        const_cast<char*>("output_dir"), const_cast<char*>("--check-only")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_EQ(opts->output_dir.value(), "output_dir");
        EXPECT_TRUE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // print-ast
        int argc = 6;
        char* argv[] = {const_cast<char*>("hasten"),     const_cast<char*>("--input-file"),
                        const_cast<char*>("input.idl"),  const_cast<char*>("--output-dir"),
                        const_cast<char*>("output_dir"), const_cast<char*>("--print-ast")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_EQ(opts->output_dir.value(), "output_dir");
        EXPECT_FALSE(opts->check_only);
        EXPECT_TRUE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // assign-uids
        int argc = 6;
        char* argv[] = {const_cast<char*>("hasten"),     const_cast<char*>("--input-file"),
                        const_cast<char*>("input.idl"),  const_cast<char*>("--output-dir"),
                        const_cast<char*>("output_dir"), const_cast<char*>("--assign-uids")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_EQ(opts->output_dir.value(), "output_dir");
        EXPECT_FALSE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_TRUE(opts->assign_uids);
    }

    {
        // check-only + print-ast
        int argc = 7;
        char* argv[] = {const_cast<char*>("hasten"),     const_cast<char*>("--input-file"),
                        const_cast<char*>("input.idl"),  const_cast<char*>("--output-dir"),
                        const_cast<char*>("output_dir"), const_cast<char*>("--check-only"),
                        const_cast<char*>("--print-ast")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_EQ(opts->output_dir.value(), "output_dir");
        EXPECT_TRUE(opts->check_only);
        EXPECT_TRUE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // check-only + assign-uids
        int argc = 7;
        char* argv[] = {const_cast<char*>("hasten"),       const_cast<char*>("--input-file"),
                        const_cast<char*>("input.idl"),    const_cast<char*>("--output-dir"),
                        const_cast<char*>("output_dir"),   const_cast<char*>("--check-only"),
                        const_cast<char*>("--assign-uids")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_EQ(opts->output_dir.value(), "output_dir");
        EXPECT_TRUE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_TRUE(opts->assign_uids);
    }

    {
        // print-ast + assign-uids
        int argc = 7;
        char* argv[] = {const_cast<char*>("hasten"),       const_cast<char*>("--input-file"),
                        const_cast<char*>("input.idl"),    const_cast<char*>("--output-dir"),
                        const_cast<char*>("output_dir"),   const_cast<char*>("--print-ast"),
                        const_cast<char*>("--assign-uids")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_EQ(opts->output_dir.value(), "output_dir");
        EXPECT_FALSE(opts->check_only);
        EXPECT_TRUE(opts->print_ast);
        EXPECT_TRUE(opts->assign_uids);
    }

    {
        // check-only + print-ast + assign-uids
        int argc = 8;
        char* argv[] = {const_cast<char*>("hasten"),      const_cast<char*>("--input-file"),
                        const_cast<char*>("input.idl"),   const_cast<char*>("--output-dir"),
                        const_cast<char*>("output_dir"),  const_cast<char*>("--check-only"),
                        const_cast<char*>("--print-ast"), const_cast<char*>("--assign-uids")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_EQ(opts->output_dir.value(), "output_dir");
        EXPECT_TRUE(opts->check_only);
        EXPECT_TRUE(opts->print_ast);
        EXPECT_TRUE(opts->assign_uids);
    }

    // short options, same combinations as above
    {
        // check-only
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-c"), const_cast<char*>("input.idl")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_TRUE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // print-ast
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-a"), const_cast<char*>("input.idl")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_FALSE(opts->check_only);
        EXPECT_TRUE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // assign-uids
        int argc = 3;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-u"), const_cast<char*>("input.idl")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_FALSE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_TRUE(opts->assign_uids);
    }

    {
        // check-only + print-ast
        int argc = 4;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-c"), const_cast<char*>("input.idl"),
                        const_cast<char*>("-a")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_TRUE(opts->check_only);
        EXPECT_TRUE(opts->print_ast);
        EXPECT_FALSE(opts->assign_uids);
    }

    {
        // check-only + assign-uids
        int argc = 4;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-c"), const_cast<char*>("input.idl"),
                        const_cast<char*>("-u")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_TRUE(opts->check_only);
        EXPECT_FALSE(opts->print_ast);
        EXPECT_TRUE(opts->assign_uids);
    }

    {
        // print-ast + assign-uids
        int argc = 4;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-a"), const_cast<char*>("input.idl"),
                        const_cast<char*>("-u")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_FALSE(opts->check_only);
        EXPECT_TRUE(opts->print_ast);
        EXPECT_TRUE(opts->assign_uids);
    }

    {
        // check-only + print-ast + assign-uids
        int argc = 5;
        char* argv[] = {const_cast<char*>("hasten"), const_cast<char*>("-c"), const_cast<char*>("input.idl"),
                        const_cast<char*>("-a"), const_cast<char*>("-u")};
        auto opts = hasten::parse_command_line(argc, argv);
        ASSERT_TRUE(opts);
        EXPECT_EQ(opts->input_file, "input.idl");
        EXPECT_FALSE(opts->output_dir.has_value());
        EXPECT_TRUE(opts->check_only);
        EXPECT_TRUE(opts->print_ast);
        EXPECT_TRUE(opts->assign_uids);
    }
}
