#pragma once

#include "semantic_context.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hasten::frontend::semantic
{

template <typename Node>
void check_unique_names(Context& context, const std::vector<Node>& nodes, const SourceFile& file,
                        const std::string& owner_label, const std::string& element_kind)
{
    std::unordered_set<std::string> names;
    for (const auto& node : nodes) {
        auto [_, inserted] = names.insert(node.name);
        if (!inserted) {
            context.report_error(file, node,
                                 "Duplicate " + element_kind + " name '" + node.name + "' in " + owner_label);
        }
    }
}

template <typename Node>
void check_id_bounds(Context& context, const Node& node, const SourceFile& file,
                     const std::string& element_kind, const std::string& owner_label)
{
    constexpr std::uint64_t kMaxId = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    auto id = node.id;
    if (id == 0) {
        context.report_error(
            file, node, "Invalid " + element_kind + " id '0' in " + owner_label + "; ids must start at 1");
        return;
    }
    if (id > kMaxId) {
        context.report_error(file, node,
                             "Invalid " + element_kind + " id '" + std::to_string(id) + "' in " +
                                 owner_label + "; maximum allowed value is " + std::to_string(kMaxId));
    }
}

template <typename Node>
void check_id_collection(Context& context, const std::vector<Node>& nodes, const SourceFile& file,
                         const std::string& owner_label, const std::string& element_kind)
{
    std::unordered_map<std::uint64_t, const Node*> seen;
    std::vector<const Node*> ordered;
    ordered.reserve(nodes.size());
    for (const auto& node : nodes) {
        ordered.push_back(&node);
        check_id_bounds(context, node, file, element_kind, owner_label);
        auto [it, inserted] = seen.emplace(node.id, &node);
        if (!inserted) {
            context.report_error(
                file, node,
                "Duplicate " + element_kind + " id '" + std::to_string(node.id) + "' in " + owner_label);
        }
    }

    std::sort(ordered.begin(), ordered.end(), [](const Node* lhs, const Node* rhs) {
        return lhs->id < rhs->id;
    });

    for (size_t i = 1; i < ordered.size(); ++i) {
        auto prev = ordered[i - 1]->id;
        auto current = ordered[i]->id;
        if (current > prev + 1) {
            context.report_note(file, *ordered[i],
                                "Gap detected between " + std::to_string(prev) + " and " +
                                    std::to_string(current) + " for " + element_kind + " ids in " +
                                    owner_label);
        }
    }
}

}  // namespace hasten::frontend::semantic
