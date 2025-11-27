#pragma once

#include <algorithm>
#include <string>
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
    void clear();

    const std::vector<Diagnostic>& diagnostics() const;

private:
    std::vector<Diagnostic> _diagnostics;
};

}  // namespace hasten::frontend
