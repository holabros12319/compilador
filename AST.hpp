#ifndef AST_HPP
#define AST_HPP

#include "Tokens.hpp"
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Declaraciones adelantadas
class ProgramNode;
class FunctionDecl;
class StructDecl;
class VarDecl;
class BlockStmt;
class IfStmt;
class WhileStmt;
class ForStmt;
class ReturnStmt;
class ExprStmt;
class LiteralExpr;
class IdentifierExpr;
class UnaryExpr;
class BinaryExpr;
class CallExpr;
class MemberExpr; // Acceso a miembros de estructuras 

// Patrón Visitor para recorrer el árbol limpiamente
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void visit(const ProgramNode&)    = 0;
    virtual void visit(const FunctionDecl&)   = 0;
    virtual void visit(const StructDecl&)     = 0;
    virtual void visit(const VarDecl&)        = 0;
    virtual void visit(const BlockStmt&)      = 0;
    virtual void visit(const IfStmt&)         = 0;
    virtual void visit(const WhileStmt&)      = 0;
    virtual void visit(const ForStmt&)        = 0;
    virtual void visit(const ReturnStmt&)     = 0;
    virtual void visit(const ExprStmt&)       = 0;
    virtual void visit(const LiteralExpr&)    = 0;
    virtual void visit(const IdentifierExpr&) = 0;
    virtual void visit(const UnaryExpr&)      = 0;
    virtual void visit(const BinaryExpr&)     = 0;
    virtual void visit(const CallExpr&)       = 0;
    virtual void visit(const MemberExpr&)     = 0; //Visita para MemberExpr
};

// Identificadores de nodos para depuración
enum class NodeKind {
    Program,
    FunctionDecl, StructDecl, VarDecl,
    BlockStmt, IfStmt, WhileStmt, ForStmt, ReturnStmt, ExprStmt,
    LiteralExpr, IdentifierExpr, UnaryExpr, BinaryExpr, CallExpr, MemberExpr
};

std::string nodeKindToString(NodeKind kind);

// Clase base para todos los nodos del AST (guarda la ubicación exacta)
class ASTNode {
public:
    const Ubicacion ubicacion;

    explicit ASTNode(Ubicacion u) : ubicacion(u) {}
    virtual ~ASTNode() = default;

    ASTNode(const ASTNode&)            = delete;
    ASTNode& operator=(const ASTNode&) = delete;

    int line()   const { return ubicacion.linea; }
    int column() const { return ubicacion.columna; }

    virtual NodeKind kind() const = 0;
    virtual void accept(ASTVisitor& visitor) const = 0;
    virtual std::string print(int indent) const = 0;
    std::string toString() const { return print(0); }

protected:
    static std::string pad(int indent);
};

class Expr : public ASTNode { public: using ASTNode::ASTNode; };
class Stmt : public ASTNode { public: using ASTNode::ASTNode; };
class Decl : public Stmt {
public:
    std::string name;
    Decl(Ubicacion u, std::string n) : Stmt(u), name(std::move(n)) {}
};

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using DeclPtr = std::unique_ptr<Decl>;

// --- EXPRESIONES ---

class LiteralExpr : public Expr {
public:
    std::string value;
    TipoToken   literalType;

    LiteralExpr(Ubicacion u, std::string v, TipoToken t)
        : Expr(u), value(std::move(v)), literalType(t) {}

    NodeKind kind() const override { return NodeKind::LiteralExpr; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class IdentifierExpr : public Expr {
public:
    std::string name;

    IdentifierExpr(Ubicacion u, std::string n) : Expr(u), name(std::move(n)) {}

    NodeKind kind() const override { return NodeKind::IdentifierExpr; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class UnaryExpr : public Expr {
public:
    std::string op;
    ExprPtr     operand;
    bool        isPrefix;

    UnaryExpr(Ubicacion u, std::string o, ExprPtr e, bool prefix = true)
        : Expr(u), op(std::move(o)), operand(std::move(e)), isPrefix(prefix) {}

    NodeKind kind() const override { return NodeKind::UnaryExpr; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class BinaryExpr : public Expr {
public:
    ExprPtr     left;
    std::string op;
    ExprPtr     right;

    BinaryExpr(Ubicacion u, ExprPtr l, std::string o, ExprPtr r)
        : Expr(u), left(std::move(l)), op(std::move(o)), right(std::move(r)) {}

    NodeKind kind() const override { return NodeKind::BinaryExpr; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class CallExpr : public Expr {
public:
    ExprPtr              callee;
    std::vector<ExprPtr> arguments;

    CallExpr(Ubicacion u, ExprPtr c, std::vector<ExprPtr> args)
        : Expr(u), callee(std::move(c)), arguments(std::move(args)) {}

    NodeKind kind() const override { return NodeKind::CallExpr; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

// NUEVO: Nodo para acceder a propiedades o campos de un struct (ej. obj.campo o ptr->campo)
class MemberExpr : public Expr {
public:
    ExprPtr     object;
    std::string member;
    bool        isPointer; // true si usa '->', false si usa '.'

    MemberExpr(Ubicacion u, ExprPtr obj, std::string mem, bool ptr = false)
        : Expr(u), object(std::move(obj)), member(std::move(mem)), isPointer(ptr) {}

    NodeKind kind() const override { return NodeKind::MemberExpr; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

// --- SENTENCIAS Y DECLARACIONES (Sin cambios estructurales previos, solo integradas) ---
class BlockStmt : public Stmt {
public:
    std::vector<StmtPtr> statements;
    explicit BlockStmt(Ubicacion u, std::vector<StmtPtr> stmts = {})
        : Stmt(u), statements(std::move(stmts)) {}
    NodeKind kind() const override { return NodeKind::BlockStmt; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class ExprStmt : public Stmt {
public:
    ExprPtr expression;
    ExprStmt(Ubicacion u, ExprPtr e) : Stmt(u), expression(std::move(e)) {}
    NodeKind kind() const override { return NodeKind::ExprStmt; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class IfStmt : public Stmt {
public:
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;
    IfStmt(Ubicacion u, ExprPtr cond, StmtPtr thenB, StmtPtr elseB = nullptr)
        : Stmt(u), condition(std::move(cond)), thenBranch(std::move(thenB)), elseBranch(std::move(elseB)) {}
    NodeKind kind() const override { return NodeKind::IfStmt; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class WhileStmt : public Stmt {
public:
    ExprPtr condition;
    StmtPtr body;
    WhileStmt(Ubicacion u, ExprPtr cond, StmtPtr b)
        : Stmt(u), condition(std::move(cond)), body(std::move(b)) {}
    NodeKind kind() const override { return NodeKind::WhileStmt; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class ForStmt : public Stmt {
public:
    StmtPtr init;
    ExprPtr condition;
    ExprPtr increment;
    StmtPtr body;
    ForStmt(Ubicacion u, StmtPtr i, ExprPtr cond, ExprPtr inc, StmtPtr b)
        : Stmt(u), init(std::move(i)), condition(std::move(cond)), increment(std::move(inc)), body(std::move(b)) {}
    NodeKind kind() const override { return NodeKind::ForStmt; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class ReturnStmt : public Stmt {
public:
    ExprPtr value;
    ReturnStmt(Ubicacion u, ExprPtr v = nullptr) : Stmt(u), value(std::move(v)) {}
    NodeKind kind() const override { return NodeKind::ReturnStmt; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class VarDecl : public Decl {
public:
    std::string type;
    ExprPtr     initializer;
    VarDecl(Ubicacion u, std::string t, std::string n, ExprPtr init = nullptr)
        : Decl(u, std::move(n)), type(std::move(t)), initializer(std::move(init)) {}
    NodeKind kind() const override { return NodeKind::VarDecl; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

struct Param {
    std::string type;
    std::string name;
    Ubicacion   ubicacion;
    Param(std::string t, std::string n, Ubicacion u) : type(std::move(t)), name(std::move(n)), ubicacion(u) {}
};

class FunctionDecl : public Decl {
public:
    std::string returnType;
    std::vector<Param> parameters;
    std::unique_ptr<BlockStmt> body;
    FunctionDecl(Ubicacion u, std::string retType, std::string n, std::vector<Param> params, std::unique_ptr<BlockStmt> b = nullptr)
        : Decl(u, std::move(n)), returnType(std::move(retType)), parameters(std::move(params)), body(std::move(b)) {}
    bool isPrototype() const { return body == nullptr; }
    NodeKind kind() const override { return NodeKind::FunctionDecl; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class StructDecl : public Decl {
public:
    std::vector<std::unique_ptr<VarDecl>> fields;
    StructDecl(Ubicacion u, std::string n, std::vector<std::unique_ptr<VarDecl>> f = {})
        : Decl(u, std::move(n)), fields(std::move(f)) {}
    NodeKind kind() const override { return NodeKind::StructDecl; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

class ProgramNode : public ASTNode {
public:
    std::vector<StmtPtr> declarations;
    explicit ProgramNode(Ubicacion u = Ubicacion{1, 1}) : ASTNode(u) {}
    NodeKind kind() const override { return NodeKind::Program; }
    void accept(ASTVisitor& v) const override { v.visit(*this); }
    std::string print(int indent) const override;
};

#endif // AST_HPP
