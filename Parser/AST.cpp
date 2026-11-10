
#ifndef AST_CPP
#define AST_CPP


#include "AST.hpp"
#include "cstddef"

namespace {
    std::string exprOrNull(const Expr* e) {
        return e ? e->print(0) : "null";
    }

    std::string stmtOrNull(const Stmt* s, int indent) {
        return s ? s->print(indent) : std::string(static_cast<std::size_t>(indent) * 2, ' ') + "null";
    }
}

std::string nodeKindToString(NodeKind kind) {
    switch (kind) {
    case NodeKind::Program:        return "Program";
    case NodeKind::FunctionDecl:   return "FunctionDecl";
    case NodeKind::StructDecl:     return "StructDecl";
    case NodeKind::VarDecl:        return "VarDecl";
    case NodeKind::BlockStmt:      return "BlockStmt";
    case NodeKind::IfStmt:         return "IfStmt";
    case NodeKind::WhileStmt:      return "WhileStmt";
    case NodeKind::ForStmt:        return "ForStmt";
    case NodeKind::ReturnStmt:     return "ReturnStmt";
    case NodeKind::ExprStmt:       return "ExprStmt";
    case NodeKind::LiteralExpr:    return "LiteralExpr";
    case NodeKind::IdentifierExpr: return "IdentifierExpr";
    case NodeKind::UnaryExpr:      return "UnaryExpr";
    case NodeKind::BinaryExpr:     return "BinaryExpr";
    case NodeKind::CallExpr:       return "CallExpr";
    case NodeKind::MemberExpr:     return "MemberExpr"; // NUEVO
    }
    return "Desconocido";
}

std::string ASTNode::pad(int indent) {
    return std::string(static_cast<std::size_t>(indent < 0 ? 0 : indent) * 2, ' ');
}

// Impresión de Expresiones
std::string LiteralExpr::print(int) const { return "(Literal " + value + ")"; }
std::string IdentifierExpr::print(int) const { return "(Id " + name + ")"; }
std::string UnaryExpr::print(int) const {
    return std::string("(Unary ") + (isPrefix ? "prefix " : "postfix ") + op + " " + exprOrNull(operand.get()) + ")";
}
std::string BinaryExpr::print(int) const {
    return "(Binary " + exprOrNull(left.get()) + " " + op + " " + exprOrNull(right.get()) + ")";
}
std::string CallExpr::print(int) const {
    std::string s = "(Call " + exprOrNull(callee.get());
    for (const auto& arg : arguments) s += " " + exprOrNull(arg.get());
    return s + ")";
}
// NUEVO: Método print para MemberExpr
std::string MemberExpr::print(int) const {
    std::string op = isPointer ? "->" : ".";
    return "(Member " + exprOrNull(object.get()) + " " + op + " " + member + ")";
}

// Impresión de Sentencias y Declaraciones
std::string BlockStmt::print(int indent) const {
    std::string s = pad(indent) + "(Block\n";
    for (const auto& stmt : statements) s += stmtOrNull(stmt.get(), indent + 1) + "\n";
    return s + pad(indent) + ")";
}

std::string ExprStmt::print(int indent) const {
    return pad(indent) + "(ExprStmt " + exprOrNull(expression.get()) + ")";
}

std::string IfStmt::print(int indent) const {
    std::string s = pad(indent) + "(If " + exprOrNull(condition.get()) + "\n";
    s += stmtOrNull(thenBranch.get(), indent + 1);
    if (elseBranch) s += "\n" + pad(indent + 1) + "else:\n" + elseBranch->print(indent + 2);
    return s + "\n" + pad(indent) + ")";
}

std::string WhileStmt::print(int indent) const {
    return pad(indent) + "(While " + exprOrNull(condition.get()) + "\n" + stmtOrNull(body.get(), indent + 1) + "\n" + pad(indent) + ")";
}

std::string ForStmt::print(int indent) const {
    std::string s = pad(indent) + "(For\n";
    s += pad(indent + 1) + "init: " + (init ? init->print(0) : "null") + "\n";
    s += pad(indent + 1) + "cond: " + exprOrNull(condition.get()) + "\n";
    s += pad(indent + 1) + "incr: " + exprOrNull(increment.get()) + "\n";
    s += stmtOrNull(body.get(), indent + 1) + "\n";
    return s + pad(indent) + ")";
}

std::string ReturnStmt::print(int indent) const {
    return pad(indent) + "(Return " + (value ? value->print(0) : "") + ")";
}

std::string VarDecl::print(int indent) const {
    return pad(indent) + "(VarDecl " + type + " " + name + (initializer ? " = " + initializer->print(0) : "") + ")";
}

std::string FunctionDecl::print(int indent) const {
    std::string s = pad(indent) + "(Function " + returnType + " " + name + " (";
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) s += ", ";
        s += parameters[i].type + " " + parameters[i].name;
    }
    s += ")";
    if (body) s += "\n" + body->print(indent + 1) + "\n" + pad(indent) + ")";
    else s += " prototype)";
    return s;
}

std::string StructDecl::print(int indent) const {
    std::string s = pad(indent) + "(Struct " + name + "\n";
    for (const auto& field : fields) s += field->print(indent + 1) + "\n";
    return s + pad(indent) + ")";
}

std::string ProgramNode::print(int indent) const {
    std::string s = pad(indent) + "(Program\n";
    for (const auto& decl : declarations) s += stmtOrNull(decl.get(), indent + 1) + "\n";
    return s + pad(indent) + ")";
}

#endif 