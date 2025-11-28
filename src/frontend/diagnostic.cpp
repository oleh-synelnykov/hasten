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

}  // namespace hasten::frontend
