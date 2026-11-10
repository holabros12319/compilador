#ifndef PARSER_HPP
#define PARSER_HPP

#include "Tokens.hpp"
#include "AST.hpp" 
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <unordered_set>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::unique_ptr<ProgramNode> parse();

    // Verdadero si parse() reporto al menos un error sintactico. Permite
    // distinguir un AST completo de uno parcial (docs.md D-13).
    bool huboErroresSintacticos() const;

private:
    std::vector<Token> tokens;
    std::size_t currentPosition;
    bool errorSintactico;
    std::unordered_set<std::string> knownTypes;

    // Métodos seguros de navegación 
    const Token& peek() const;
    const Token& previous() const;
    bool isAtEnd() const;
    bool check(TipoToken type) const;
    bool checkLexeme(const std::string& lexeme) const;
    bool match(TipoToken type);
    bool matchLexeme(const std::string& lexeme);
    const Token& consume(TipoToken type, const std::string& errorMessage);
    const Token& consumeLexeme(const std::string& lexeme, const std::string& errorMessage);

    // Panic Mode Recovery
    void synchronize();
    void error(const Token& token, const std::string& message);
    class ParseError : public std::runtime_error {
    public:
        explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
    };

    // Gramática de Declaraciones
    StmtPtr parseDeclaration();
    DeclPtr parseFunctionOrVarDecl();
    DeclPtr parseStructDecl();
    std::string parseType();

    // Gramática de Sentencias
    StmtPtr parseStatement();
    StmtPtr parseBlockStatement();
    StmtPtr parseIfStatement();
    StmtPtr parseWhileStatement();
    StmtPtr parseForStatement();    
    StmtPtr parseReturnStatement();
    StmtPtr parseExpressionStatement();

    // Escalada de Precedencia (Precedence Climbing CFG)
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseTerm();
    ExprPtr parseShift();
    ExprPtr parseFactor();
    ExprPtr parseUnary();
    ExprPtr parsePrimary();
};

#endif // PARSER_HPP