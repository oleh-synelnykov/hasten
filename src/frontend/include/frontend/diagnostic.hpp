#pragma once

#include "frontend/source_file.hpp"

#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>

#include <string>
#include <type_traits>
#include <vector>

namespace hasten::frontend
{

enum class Severity {
    Note,
    Warning,
    Error,
};

struct SourceLocation {
    std::string file;
    size_t line = 0;
    size_t column = 0;
};

struct Diagnostic {
    Severity severity;
    SourceLocation location;
    std::string message;
};

/**
 * @brief Sink for diagnostics.
 *
 * This class is used to collect diagnostics from the frontend.
 */
class DiagnosticSink
{
public:
    void report(Severity severity, SourceLocation location, std::string message);
    bool has_errors() const;
    bool has_warnings() const;
    bool has_notes() const;
    void clear();

    const std::vector<Diagnostic>& diagnostics() const;

private:
    std::vector<Diagnostic> _diagnostics;
};

template <typename Node>
SourceLocation locate(const Node& node, const SourceFile& file)
{
    using boost::spirit::x3::position_tagged;

    if constexpr (std::is_base_of_v<position_tagged, Node>) {
        const auto& tagged = static_cast<const position_tagged&>(node);
        if (tagged.id_first < 0 || tagged.id_last < 0) {
            return {file.path, 0, 0};
        }

        auto range = file.position_cache.position_of(node);
        auto begin_iter = file.position_cache.first();
        auto current_iter = range.begin();

        size_t line = 1;
        size_t column = 1;
        for (auto it = begin_iter; it != current_iter; ++it) {
            if (*it == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }

        return {file.path, line, column};
    } else {
        return {file.path, 0, 0};
    }
}

}  // namespace hasten::frontend
