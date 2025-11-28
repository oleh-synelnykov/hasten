#pragma once

#include "frontend/diagnostic.hpp"
#include "frontend/program.hpp"
#include "idl/ast.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hasten::frontend {

namespace ast = idl::ast;


/**
 * @brief Validates the semantic correctness of the program.
 *
 * Using class with internal state to collect all errors during validation,
 * allowing reporting of multiple errors instead of stopping at the first.
 */
class ValidateSemanticsImpl
{
public:
    enum class DeclKind { Struct, Enum, Interface };

    struct DeclInfo {
        DeclKind kind;
        const SourceFile* file;
    };

    /**
     * @brief Constructs the validator with the given program and diagnostic sink.
     * @param program The program to validate.
     * @param sink The diagnostic sink to report errors to.
     */
    ValidateSemanticsImpl(Program& program, DiagnosticSink& sink)
        : _program(program)
        , _sink(sink)
    {
    }

    /**
     * @brief Runs the semantic validation on the entire program.
     * @post All semantic errors are reported via the diagnostic sink.
     */
    void run();

private:
    /**
     * @brief Build indices for efficient lookup of declarations by name and module.
     */
    void build_indices();

    /**
     * @brief Register a declaration for later lookup.
     * @param module_name The name of the module where the declaration is defined.
     * @param name The name of the declaration.
     * @param file The source file where the declaration is defined.
     * @param kind The kind of declaration (struct, enum, interface).
     */
    void register_declaration(const std::string& module_name, const std::string& name, const SourceFile& file,
                              DeclKind kind);

    void validate_file(const SourceFile& file);
    void validate_struct(const ast::Struct& s, const SourceFile& file, const std::string& module_name);
    void validate_enum(const ast::Enum& e, const SourceFile& file, const std::string&);
    void validate_interface(const ast::Interface& iface, const SourceFile& file,
                            const std::string& module_name);
    void validate_type(const ast::Type& type, const SourceFile& file, const std::string& module_name,
                       const std::string& usage);
    void validate_map_key(const ast::Type& key, const SourceFile& file, const std::string& module_name,
                          const std::string& usage);

    const DeclInfo* resolve_user_type(const ast::UserType& user_type, const std::string& module_name,
                                      const SourceFile& file, const std::string& usage);

    void report(Severity severity, const SourceFile& file, const std::string& message);
    void report_error(const SourceFile& file, const std::string& message);
    void report_note(const SourceFile& file, const std::string& message);

    void check_id_bounds(std::uint64_t id, const SourceFile& file, const std::string& element_kind,
                         const std::string& owner_label);

    template <typename Node>
    void check_unique_names(const std::vector<Node>& nodes, const SourceFile& file,
                            const std::string& owner_label, const std::string& element_kind);

    template <typename Node>
    void check_id_collection(const std::vector<Node>& nodes, const SourceFile& file,
                             const std::string& owner_label, const std::string& element_kind);

    Program& _program;
    DiagnosticSink& _sink;
    std::unordered_map<std::string, const SourceFile*> _module_index;
    std::unordered_map<std::string, DeclInfo> _declarations;
};


template <typename Node>
inline void ValidateSemanticsImpl::check_unique_names(const std::vector<Node>& nodes, const SourceFile& file,
                                              const std::string& owner_label, const std::string& element_kind)
{
    std::unordered_set<std::string> names;
    for (const auto& node : nodes) {
        auto [it, inserted] = names.insert(node.name);
        if (!inserted) {
            report_error(file,
                         "Duplicate " + element_kind + " name '" + node.name + "' in " + owner_label);
        }
    }
}

template <typename Node>
inline void ValidateSemanticsImpl::check_id_collection(const std::vector<Node>& nodes, const SourceFile& file,
                                               const std::string& owner_label, const std::string& element_kind)
{
    std::unordered_map<std::uint64_t, const Node*> seen;
    std::vector<const Node*> ordered;
    ordered.reserve(nodes.size());
    for (const auto& node : nodes) {
        ordered.push_back(&node);
        check_id_bounds(node.id, file, element_kind, owner_label);
        auto [it, inserted] = seen.emplace(node.id, &node);
        if (!inserted) {
            report_error(file, "Duplicate " + element_kind + " id '" + std::to_string(node.id) + "' in " +
                                   owner_label);
        }
    }

    std::sort(ordered.begin(), ordered.end(), [](const Node* lhs, const Node* rhs) {
        return lhs->id < rhs->id;
    });
    for (size_t i = 1; i < ordered.size(); ++i) {
        auto prev = ordered[i - 1]->id;
        auto current = ordered[i]->id;
        if (current > prev + 1) {
            report_note(file, "Gap detected between " + std::to_string(prev) + " and " +
                                      std::to_string(current) + " for " + element_kind + " ids in " +
                                      owner_label);
        }
    }
}

}  // namespace hasten::frontend
