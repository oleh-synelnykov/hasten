#include "cli/hasten.hpp"

#include "cli/options.hpp"
#include "codegen/generator.hpp"
#include "idl/json_dump.hpp"
#include "frontend/diagnostic.hpp"
#include "frontend/frontend.hpp"
#include "frontend/semantic/validator.hpp"

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>

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

    // Parse
    auto maybe_program = frontend::parse_program(opts->input_file);
    if (!maybe_program) {
        spdlog::error("Failed to parse program: {}", maybe_program.error());
        return 1;
    }

    // Validate
    frontend::Program& program = maybe_program.value();
    frontend::DiagnosticSink diagnostics;
    frontend::semantic::Validator validator{program, diagnostics};
    validator.run();

    const bool has_any_diagnostics = !diagnostics.diagnostics().empty();
    const bool has_errors = diagnostics.has_errors();
    const bool has_warnings = diagnostics.has_warnings();

    if (has_errors) {
        spdlog::error("Semantic analysis failed:");
    } else if (has_warnings) {
        spdlog::warn("Semantic analysis warnings:");
    } else if (has_any_diagnostics) {
        spdlog::info("Semantic analysis diagnostics:");
    }

    if (has_any_diagnostics) {
        for (const auto& diagnostic : diagnostics.diagnostics()) {
            auto log_message = fmt::format("{}:{}:{}: {}", diagnostic.location.file, diagnostic.location.line,
                                           diagnostic.location.column, diagnostic.message);
            switch (diagnostic.severity) {
                case frontend::Severity::Error:
                    spdlog::error("{}", log_message);
                    break;
                case frontend::Severity::Warning:
                    spdlog::warn("{}", log_message);
                    break;
                case frontend::Severity::Note:
                    spdlog::info("{}", log_message);
                    break;
            }
        }
    }

    if (has_errors) {
        return 1;
    }

    spdlog::info("Parsed program with {} files", program.files.size());

    if (opts->print_ast) {
        nlohmann::json files = nlohmann::json::array();
        for (const auto& [path, file] : program.files) {
            nlohmann::json file_json;
            file_json["path"] = path;
            file_json["module"] = idl::ast::to_json(file.module);
            files.push_back(std::move(file_json));
        }

        nlohmann::json program_json;
        program_json["files"] = std::move(files);
        fmt::print("{}\n", program_json.dump(2));
    }

    if (!opts->check_only && !opts->print_ast) {
        namespace fs = std::filesystem;
        fs::path output_dir;
        if (opts->output_dir) {
            output_dir = fs::path(*opts->output_dir);
        } else {
            fs::path input_path(opts->input_file);
            output_dir = input_path.parent_path();
            if (output_dir.empty()) {
                output_dir = fs::current_path();
            }
        }

        codegen::GenerationOptions gen_opts;
        gen_opts.output_dir = output_dir;

        codegen::Generator generator{program, std::move(gen_opts)};
        if (auto result = generator.run(); !result) {
            spdlog::error("Code generation failed: {}", result.error());
            return 1;
        }

        spdlog::info("Generated sources under {}", fs::absolute(output_dir).string());
    }

    return 0;
}

}  // namespace hasten
