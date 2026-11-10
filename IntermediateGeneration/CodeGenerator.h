#ifndef CODEGENERATOR_H
#define CODEGENERATOR_H

#include "IntermediateCode.h"
#include "AST.hpp"

#include <string>

// Generador de código intermedio TAC. Recorre el AST en un único pase
// sin modificarlo . Hereda de ASTVisitor e implementa los 16 visit .
class CodeGenerator : public ASTVisitor {
private:
    CodeBuffer buffer;
    std::string lastTemp;

    // Mapea un operador binario textual a su OpCode TAC.
    OpCode mapStringToOpCode(const std::string& op);

    // Mapea un operador unario textual a su OpCode TAC.
    OpCode mapStringToUnaryOpCode(const std::string& op);

public:
    CodeGenerator() = default;

    // Acceso al búfer de instrucciones generadas.
    CodeBuffer& getBuffer();
    const CodeBuffer& getBuffer() const;

    // --- Métodos visit (16) ---
    void visit(const ProgramNode& node) override;
    void visit(const FunctionDecl& node) override;
    void visit(const StructDecl& node) override;
    void visit(const VarDecl& node) override;
    void visit(const BlockStmt& node) override;
    void visit(const IfStmt& node) override;
    void visit(const WhileStmt& node) override;
    void visit(const ForStmt& node) override;
    void visit(const ReturnStmt& node) override;
    void visit(const ExprStmt& node) override;
    void visit(const LiteralExpr& node) override;
    void visit(const IdentifierExpr& node) override;
    void visit(const UnaryExpr& node) override;
    void visit(const BinaryExpr& node) override;
    void visit(const CallExpr& node) override;
    void visit(const MemberExpr& node) override;
};

#endif // CODEGENERATOR_H