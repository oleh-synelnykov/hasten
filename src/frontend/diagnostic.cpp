#include "frontend/diagnostic.hpp"
#include "idl/config.hpp"

namespace hasten::frontend
{

void DiagnosticSink::report(Severity severity, SourceLocation location, std::string message)
{
    _diagnostics.emplace_back(severity, location, std::move(message));
}

bool DiagnosticSink::has_errors() const
{
    auto is_error = [](const auto& d) {
        return d.severity == Severity::Error;
    };
    return std::any_of(_diagnostics.begin(), _diagnostics.end(), is_error);
}

bool DiagnosticSink::has_warnings() const
{
    auto is_warning = [](const auto& d) {
        return d.severity == Severity::Warning;
    };
    return std::any_of(_diagnostics.begin(), _diagnostics.end(), is_warning);
}

bool DiagnosticSink::has_notes() const
{
    auto is_note = [](const auto& d) {
        return d.severity == Severity::Note;
    };
    return std::any_of(_diagnostics.begin(), _diagnostics.end(), is_note);
}

void DiagnosticSink::clear()
{
    _diagnostics.clear();
}

const std::vector<Diagnostic>& DiagnosticSink::diagnostics() const {
    return _diagnostics;
}


/**
 * @brief Get the location of a node in the source file.
 *
 * @tparam Node Type of the AST node.
 * @param node AST node to get the location of.
 * @param file File name.
 * @param cache Position cache.
 * @return Location of the node.
 */
template <typename Node>
SourceLocation locate(const Node& node, const std::string& file,
                      const idl::parser::position_cache_type& cache)
{
    auto pos = cache.position_of(node.id());
    return {
        .file = file,
        .line = static_cast<size_t>(pos.line),
        .column = static_cast<size_t>(pos.column)};
}

}  // namespace hasten::frontend