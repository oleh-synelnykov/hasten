#include "cli/hasten.hpp"

#include "idl/visit.hpp"
#include "cli/options.hpp"
#include "frontend/diagnostic.hpp"
#include "frontend/frontend.hpp"
#include "frontend/semantics.hpp"

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

namespace hasten
{

int run(int argc, char* argv[])
{
    auto opts = parse_command_line(argc, argv);
    if (!opts) {
        spdlog::error("Failed to parse command line: {}", opts.error());
        return 1;
    }

    if (opts->help_message) {
        fmt::print("{}", opts->help_message.value());
        return 0;
    }

    fmt::print("Hasten v{}.{}.{}\n", HASTEN_VERSION_MAJOR, HASTEN_VERSION_MINOR, HASTEN_VERSION_PATCH);

    auto maybe_program = frontend::parse_program(opts->input_file);
    if (!maybe_program) {
        spdlog::error("Failed to parse program: {}", maybe_program.error());
        return 1;
    }

    frontend::Program& program = maybe_program.value();
    frontend::DiagnosticSink diagnostics;

    frontend::pass::CheckDuplicateIds pass{program, diagnostics};
    for (const auto& [_, f] : program.files) {
        idl::ast::visit(f.module, pass);
    }

    if (diagnostics.has_errors()) {
        spdlog::error("Semantic analysis failed:");
        for (const auto& diagnostic : diagnostics.diagnostics()) {
            spdlog::error("{}:{}:{}: {}", diagnostic.location.file, diagnostic.location.line,
                          diagnostic.location.column, diagnostic.message);
        }
        return 1;
    }

    spdlog::info("Parsed program with {} files\n", program.files.size());
    return 0;
}

}  // namespace hasten
