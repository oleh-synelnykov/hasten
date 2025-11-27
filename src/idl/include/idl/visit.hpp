#pragma once

#include "idl/ast.hpp"

#include <boost/variant.hpp>

namespace hasten::idl::ast
{

/**
 * @brief Generic visit entry point: pre-order traversal
 */
template <typename Node, typename Visitor>
void visit(const Node& node, Visitor&& v);

/**
 * @brief Forward-declared helpers
 */
template <typename Node, typename Visitor>
void visit_children(const Node& node, Visitor&& v);

// ------------------------------
// Default implementations
// ------------------------------

/**
 * @brief Generic case: call visitor on the node, then visit its children
 */
template <typename Node, typename Visitor>
void visit(const Node& node, Visitor&& v)
{
    v(node);                  // pre-order hook
    visit_children(node, v);  // recurse into children
}

/**
 * @brief Default: leaf nodes have no children
 */
template <typename Node, typename Visitor>
void visit_children(const Node&, Visitor&&)
{
    // nothing
}

// ------------------------------
// Specializations for variants
// ------------------------------

template <typename Visitor>
void visit(const Type& t, Visitor&& v);

template <typename Visitor>
void visit(const Declaration& d, Visitor&& v);

template <typename Visitor>
void visit(const Result& r, Visitor&& v);

// Implementations using boost::apply_visitor
template <typename Visitor>
void visit(const Type& t, Visitor&& v)
{
    boost::apply_visitor(
        [&](auto const& inner) {
            visit(inner, v);
        },
        t);
}

template <typename Visitor>
void visit(const Declaration& d, Visitor&& v)
{
    boost::apply_visitor(
        [&](auto const& inner) {
            visit(inner, v);
        },
        d);
}

template <typename Visitor>
void visit(const Result& r, Visitor&& v)
{
    boost::apply_visitor(
        [&](auto const& inner) {
            visit(inner, v);
        },
        r);
}

// ------------------------------
// Children for composite nodes
// ------------------------------

// Module: imports + decls
template <typename Visitor>
void visit_children(const Module& m, Visitor&& v)
{
    for (auto const& imp : m.imports) {
        visit(imp, v);
    }
    for (auto const& decl : m.decls) {
        visit(decl, v);  // Declaration variant
    }
}

// Import: leaf (no children) – default is enough

// Struct: iterate fields
template <typename Visitor>
void visit_children(const Struct& s, Visitor&& v)
{
    for (auto const& f : s.fields) {
        visit(f, v);
    }
}

// Field: its type + attributes
template <typename Visitor>
void visit_children(const Field& f, Visitor&& v)
{
    visit(f.type, v);  // Type variant
    for (auto const& a : f.attrs) {
        visit(a, v);
    }
}

// Enum: items
template <typename Visitor>
void visit_children(const Enum& e, Visitor&& v)
{
    for (auto const& item : e.items) {
        visit(item, v);
    }
}

// Interface: methods + attrs
template <typename Visitor>
void visit_children(const Interface& iface, Visitor&& v)
{
    for (auto const& attr : iface.attrs) {
        visit(attr, v);
    }
    for (auto const& m : iface.methods) {
        visit(m, v);
    }
}

// Method: params + result + attrs
template <typename Visitor>
void visit_children(const Method& m, Visitor&& v)
{
    for (auto const& p : m.params) {
        visit(p, v);
    }
    if (m.result) {
        visit(*m.result, v);  // Result variant (Type or vector<Field>)
    }
    for (auto const& a : m.attrs) {
        visit(a, v);
    }
}

// Parameter, Attribute, Primitive, UserType, Enumerator, ConstantDeclaration
// can all use the default visit_children = no children.

template <typename Visitor>
void visit_children(const Vector& vct, Visitor&& v)
{
    visit(vct.element, v);
}

template <typename Visitor>
void visit_children(const Map& m, Visitor&& v)
{
    visit(m.key, v);
    visit(m.value, v);
}

template <typename Visitor>
void visit_children(const Optional& o, Visitor&& v)
{
    visit(o.inner, v);
}

}  // namespace hasten::idl::ast
