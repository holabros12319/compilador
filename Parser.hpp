#ifndef PARSER_HPP
#define PARSER_HPP

#include "Tokens.hpp"
#include "AST.hpp" 
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::unique_ptr<ProgramNode> parse();

private:
    std::vector<Token> tokens;
    std::size_t currentPosition;

    // Métodos de navegación y control de tokens
    const Token& peek() const;
    const Token& previous() const;
    bool isAtEnd() const;
    bool check(TipoToken type) const;
    bool checkLexeme(const std::string& lexeme) const;
    bool match(TipoToken type);
    bool matchLexeme(const std::string& lexeme);
    const Token& consume(TipoToken type, const std::string& errorMessage);
    const Token& consumeLexeme(const std::string& lexeme, const std::string& errorMessage);

    // Manejo de errores y sincronización
    void synchronize();
    void error(const Token& token, const std::string& message);
    class ParseError : public std::runtime_error {
    public:
        explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
    };

    // Reglas gramaticales del Parser
    StmtPtr parseDeclaration();
    DeclPtr parseFunctionOrVarDecl();
    DeclPtr parseStructDecl();       // NUEVO: Soporte para estructuras 
    StmtPtr parseStatement();
    StmtPtr parseBlockStatement();
    StmtPtr parseIfStatement();
    StmtPtr parseWhileStatement();   // NUEVO: Soporte para bucles while
    StmtPtr parseForStatement();    
    StmtPtr parseReturnStatement();
    StmtPtr parseExpressionStatement();

    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseTerm();
    ExprPtr parseFactor();
    ExprPtr parseUnary();            // NUEVO: Soporte para operadores unarios 
    ExprPtr parsePrimary();
};

#endif // PARSER_HPP
