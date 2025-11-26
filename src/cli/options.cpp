#include "cli/options.hpp"

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <boost/program_options.hpp>

namespace hasten
{

std::expected<Options, std::string> parse_command_line(int argc, char* argv[])
{
    namespace po = boost::program_options;

    Options opts;
    std::string output_dir_value;

    // clang-format off
    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Show help message")
        ("input-file,i", po::value<std::string>(&opts.input_file)->value_name("FILE")->required(),
            "Hasten IDL input file. This should be root module file. Imports are resolved relative to this file.")
        ("output-dir,o", po::value<std::string>(&output_dir_value)->value_name("DIR"),
            "Output directory. If not specified, use the same directory as input file.")
        ("check-only,c", po::bool_switch(&opts.check_only), "Only check the input IDL for errors")
        ("print-ast,a", po::bool_switch(&opts.print_ast), "Emit parsed AST as JSON")
        ("assign-uids,u", po::bool_switch(&opts.assign_uids), "Assign unique IDs to AST nodes");
    // clang-format on

    po::positional_options_description positional;
    positional.add("input-file", 1);

    po::variables_map vm;

    try {
        auto parser = po::command_line_parser(argc, argv).options(desc).positional(positional).run();
        po::store(parser, vm);

        if (vm.count("help")) {
            // if help is specified, return the options object with help set to true, ignore other options
            std::string prog_name = argc > 0 ? argv[0] : "hasten";
            opts.help_message = fmt::format("Usage: {} <Options>:\n{}\n", prog_name, fmt::streamed(desc));
            return opts;
        }

        po::notify(vm);

        if (!output_dir_value.empty()) {
            opts.output_dir = std::move(output_dir_value);
        }
        return opts;
    } catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

}  // namespace hasten
