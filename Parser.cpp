#ifndef PARSER_CPP
#define PARSER_CPP

#include "Parser.hpp"
#include <iostream>

Parser::Parser(std::vector<Token> tokens)
    : tokens(std::move(tokens)), currentPosition(0) {}

const Token& Parser::peek() const {
    if (currentPosition >= tokens.size()) {
        return tokens.back();
    }
    return tokens[currentPosition];
}

const Token& Parser::previous() const {
    if (currentPosition == 0) return tokens[0];
    return tokens[currentPosition - 1];
}

bool Parser::isAtEnd() const {
    return peek().tipo == TipoToken::FIN_ARCHIVO;
}

bool Parser::check(TipoToken type) const {
    if (isAtEnd()) return false;
    return peek().tipo == type;
}

bool Parser::checkLexeme(const std::string& lexeme) const {
    if (isAtEnd()) return false;
    return peek().lexema == lexeme;
}

bool Parser::match(TipoToken type) {
    if (check(type)) {
        currentPosition++;
        return true;
    }
    return false;
}

bool Parser::matchLexeme(const std::string& lexeme) {
    if (checkLexeme(lexeme)) {
        currentPosition++;
        return true;
    }
    return false;
}

const Token& Parser::consume(TipoToken type, const std::string& errorMessage) {
    if (check(type)) {
        const Token& t = peek();
        currentPosition++;
        return t;
    }
    error(peek(), errorMessage);
    throw ParseError(errorMessage);
}

const Token& Parser::consumeLexeme(const std::string& lexeme, const std::string& errorMessage) {
    if (checkLexeme(lexeme)) {
        const Token& t = peek();
        currentPosition++;
        return t;
    }
    error(peek(), errorMessage);
    throw ParseError(errorMessage);
}

void Parser::error(const Token& token, const std::string& message) {
    std::cerr << "[Error Sintáctico] Línea " << token.ubicacion.linea 
              << ", Columna " << token.ubicacion.columna << ": " << message 
              << " (Encontrado: '" << token.lexema << "')\n";
}

void Parser::synchronize() {
    currentPosition++;
    while (!isAtEnd()) {
        if (previous().lexema == ";") return;
        switch (peek().tipo) {
            case TipoToken::PALABRA_RESERVADA:
                if (peek().lexema == "int" || peek().lexema == "float" || 
                    peek().lexema == "if" || peek().lexema == "return" || 
                    peek().lexema == "while" || peek().lexema == "for" || 
                    peek().lexema == "struct") {
                    return;
                }
                break;
            default:
                break;
        }
        currentPosition++;
    }
}

std::unique_ptr<ProgramNode> Parser::parse() {
    Ubicacion loc = tokens.empty() ? Ubicacion{1, 1} : tokens[0].ubicacion;
    auto program = std::make_unique<ProgramNode>(loc);
    while (!isAtEnd()) {
        try {
            program->declarations.push_back(parseDeclaration());
        } catch (const ParseError&) {
            synchronize();
        }
    }
    return program;
}

StmtPtr Parser::parseDeclaration() {
    if (matchLexeme("struct")) {
        return parseStructDecl();
    }
    if (check(TipoToken::PALABRA_RESERVADA) && 
        (peek().lexema == "int" || peek().lexema == "float" || peek().lexema == "double" || peek().lexema == "void")) {
        return parseFunctionOrVarDecl();
    }
    return parseStatement();
}

DeclPtr Parser::parseStructDecl() {
    Token structToken = previous();
    Token nameToken = consume(TipoToken::IDENTIFICADOR, "Se esperaba el nombre de la estructura.");
    std::string name = nameToken.lexema;

    consumeLexeme("{", "Se esperaba '{' para abrir el cuerpo de la estructura.");
    std::vector<std::unique_ptr<VarDecl>> fields;
    while (!checkLexeme("}") && !isAtEnd()) {
        Token typeToken = consume(TipoToken::PALABRA_RESERVADA, "Se esperaba un tipo para el campo de la estructura.");
        std::string type = typeToken.lexema;
        Token fieldNameToken = consume(TipoToken::IDENTIFICADOR, "Se esperaba un identificador para el campo.");
        std::string fieldName = fieldNameToken.lexema;

        ExprPtr initializer = nullptr;
        if (matchLexeme("=")) {
            initializer = parseExpression();
        }
        consumeLexeme(";", "Se esperaba ';' al final del campo de la estructura.");
        fields.push_back(std::make_unique<VarDecl>(typeToken.ubicacion, type, fieldName, std::move(initializer)));
    }
    consumeLexeme("}", "Se esperaba '}' al cerrar el cuerpo de la estructura.");
    consumeLexeme(";", "Se esperaba ';' al final de la declaración de la estructura.");

    return std::make_unique<StructDecl>(structToken.ubicacion, name, std::move(fields));
}

DeclPtr Parser::parseFunctionOrVarDecl() {
    Token typeToken = peek();
    std::string type = typeToken.lexema;
    currentPosition++; 

    Token nameToken = consume(TipoToken::IDENTIFICADOR, "Se esperaba un identificador después del tipo.");
    std::string name = nameToken.lexema;

    // Análisis de Función
    if (matchLexeme("(")) {
        std::vector<Param> params;
        if (!checkLexeme(")")) {
            do {
                Token pType = consume(TipoToken::PALABRA_RESERVADA, "Se esperaba el tipo del parámetro.");
                Token pName = consume(TipoToken::IDENTIFICADOR, "Se esperaba el nombre del parámetro.");
                params.emplace_back(pType.lexema, pName.lexema, pType.ubicacion);
            } while (matchLexeme(","));
        }
        consumeLexeme(")", "Se esperaba ')' después de los parámetros de la función.");
        
        // Soporte de prototipos 
        if (matchLexeme(";")) {
            return std::make_unique<FunctionDecl>(typeToken.ubicacion, type, name, std::move(params), nullptr);
        }

        consumeLexeme("{", "Se esperaba '{' para abrir el cuerpo de la función.");
        std::vector<StmtPtr> bodyStmts;
        while (!checkLexeme("}") && !isAtEnd()) {
            bodyStmts.push_back(parseDeclaration());
        }
        consumeLexeme("}", "Se esperaba '}' al cerrar el bloque de la función.");
        
        auto bodyBlock = std::make_unique<BlockStmt>(typeToken.ubicacion, std::move(bodyStmts));
        return std::make_unique<FunctionDecl>(typeToken.ubicacion, type, name, std::move(params), std::move(bodyBlock));
    }

    // Análisis de Variable
    ExprPtr initializer = nullptr;
    if (matchLexeme("=")) {
        initializer = parseExpression();
    }
    consumeLexeme(";", "Se esperaba ';' al final de la declaración de variable.");
    return std::make_unique<VarDecl>(typeToken.ubicacion, type, name, std::move(initializer));
}

StmtPtr Parser::parseStatement() {
    if (matchLexeme("{")) return parseBlockStatement();
    if (matchLexeme("if")) return parseIfStatement();
    if (matchLexeme("while")) return parseWhileStatement(); 
    if (matchLexeme("for")) return parseForStatement();     
    if (matchLexeme("return")) return parseReturnStatement();
    return parseExpressionStatement();
}

StmtPtr Parser::parseBlockStatement() {
    Token blockToken = previous();
    std::vector<StmtPtr> statements;
    while (!checkLexeme("}") && !isAtEnd()) {
        statements.push_back(parseDeclaration());
    }
    consumeLexeme("}", "Se esperaba '}' al cerrar el bloque.");
    return std::make_unique<BlockStmt>(blockToken.ubicacion, std::move(statements));
}

StmtPtr Parser::parseIfStatement() {
    Token ifToken = previous();
    consumeLexeme("(", "Se esperaba '(' después de 'if'.");
    auto condition = parseExpression();
    consumeLexeme(")", "Se esperaba ')' después de la condición del 'if'.");

    consumeLexeme("{", "Se esperaba '{' para el cuerpo del 'if'.");
    std::vector<StmtPtr> thenStmts;
    while (!checkLexeme("}") && !isAtEnd()) {
        thenStmts.push_back(parseDeclaration());
    }
    consumeLexeme("}", "Se esperaba '}' al cerrar el bloque del 'if'.");
    StmtPtr thenBranch = std::make_unique<BlockStmt>(ifToken.ubicacion, std::move(thenStmts));

    StmtPtr elseBranch = nullptr;
    if (matchLexeme("else")) {
        consumeLexeme("{", "Se esperaba '{' para el cuerpo del 'else'.");
        std::vector<StmtPtr> elseStmts;
        while (!checkLexeme("}") && !isAtEnd()) {
            elseStmts.push_back(parseDeclaration());
        }
        consumeLexeme("}", "Se esperaba '}' al cerrar el bloque del 'else'.");
        elseBranch = std::make_unique<BlockStmt>(ifToken.ubicacion, std::move(elseStmts));
    }

    return std::make_unique<IfStmt>(ifToken.ubicacion, std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

StmtPtr Parser::parseWhileStatement() {
    Token whileToken = previous();
    consumeLexeme("(", "Se esperaba '(' después de 'while'.");
    auto condition = parseExpression();
    consumeLexeme(")", "Se esperaba ')' después de la condición del 'while'.");

    consumeLexeme("{", "Se esperaba '{' para el cuerpo del bucle 'while'.");
    std::vector<StmtPtr> bodyStmts;
    while (!checkLexeme("}") && !isAtEnd()) {
        bodyStmts.push_back(parseDeclaration());
    }
    consumeLexeme("}", "Se esperaba '}' al cerrar el bloque del 'while'.");
    StmtPtr body = std::make_unique<BlockStmt>(whileToken.ubicacion, std::move(bodyStmts));

    return std::make_unique<WhileStmt>(whileToken.ubicacion, std::move(condition), std::move(body));
}

StmtPtr Parser::parseForStatement() {
    Token forToken = previous();
    consumeLexeme("(", "Se esperaba '(' después de 'for'.");

    // Inicialización 
    StmtPtr init = nullptr;
    if (matchLexeme(";")) {
        init = nullptr;
    } else if (check(TipoToken::PALABRA_RESERVADA) && 
              (peek().lexema == "int" || peek().lexema == "float" || peek().lexema == "double" || peek().lexema == "void")) {
        init = parseFunctionOrVarDecl();
    } else {
        auto expr = parseExpression();
        consumeLexeme(";", "Se esperaba ';' después de la inicialización del 'for'.");
        init = std::make_unique<ExprStmt>(expr->ubicacion, std::move(expr));
    }

    // Condición
    ExprPtr condition = nullptr;
    if (!checkLexeme(";")) {
        condition = parseExpression();
    }
    consumeLexeme(";", "Se esperaba ';' después de la condición del 'for'.");

    // Incremento
    ExprPtr increment = nullptr;
    if (!checkLexeme(")")) {
        increment = parseExpression();
    }
    consumeLexeme(")", "Se esperaba ')' después de las cláusulas del 'for'.");

    // Cuerpo
    consumeLexeme("{", "Se esperaba '{' para el cuerpo del bucle 'for'.");
    std::vector<StmtPtr> bodyStmts;
    while (!checkLexeme("}") && !isAtEnd()) {
        bodyStmts.push_back(parseDeclaration());
    }
    consumeLexeme("}", "Se esperaba '}' al cerrar el bloque del 'for'.");
    StmtPtr body = std::make_unique<BlockStmt>(forToken.ubicacion, std::move(bodyStmts));

    return std::make_unique<ForStmt>(forToken.ubicacion, std::move(init), std::move(condition), std::move(increment), std::move(body));
}

StmtPtr Parser::parseReturnStatement() {
    Token retToken = previous();
    ExprPtr value = nullptr;
    if (!checkLexeme(";")) {
        value = parseExpression();
    }
    consumeLexeme(";", "Se esperaba ';' después de la instrucción 'return'.");
    return std::make_unique<ReturnStmt>(retToken.ubicacion, std::move(value));
}

StmtPtr Parser::parseExpressionStatement() {
    Token t = peek();
    auto expr = parseExpression();
    consumeLexeme(";", "Se esperaba ';' al final de la expresión.");
    return std::make_unique<ExprStmt>(t.ubicacion, std::move(expr));
}

// --- Precedencia de Expresiones ---

ExprPtr Parser::parseExpression() {
    return parseAssignment();
}

ExprPtr Parser::parseAssignment() {
    auto expr = parseEquality();
    if (matchLexeme("=")) {
        Token equals = previous();
        auto value = parseAssignment();
        if (dynamic_cast<IdentifierExpr*>(expr.get())) {
            return std::make_unique<BinaryExpr>(equals.ubicacion, std::move(expr), "=", std::move(value));
        }
        error(equals, "Destino de asignación inválido.");
    }
    return expr;
}

ExprPtr Parser::parseEquality() {
    auto expr = parseComparison();
    while (matchLexeme("==") || matchLexeme("!=")) {
        Token opToken = previous();
        auto right = parseComparison();
        expr = std::make_unique<BinaryExpr>(opToken.ubicacion, std::move(expr), opToken.lexema, std::move(right));
    }
    return expr;
}

ExprPtr Parser::parseComparison() {
    auto expr = parseTerm();
    while (matchLexeme(">") || matchLexeme(">=") || matchLexeme("<") || matchLexeme("<=")) {
        Token opToken = previous();
        auto right = parseTerm();
        expr = std::make_unique<BinaryExpr>(opToken.ubicacion, std::move(expr), opToken.lexema, std::move(right));
    }
    return expr;
}

ExprPtr Parser::parseTerm() {
    auto expr = parseFactor();
    while (matchLexeme("+") || matchLexeme("-")) {
        Token opToken = previous();
        auto right = parseFactor();
        expr = std::make_unique<BinaryExpr>(opToken.ubicacion, std::move(expr), opToken.lexema, std::move(right));
    }
    return expr;
}

ExprPtr Parser::parseFactor() {
    auto expr = parseUnary(); // Conecta con operadores unarios
    while (matchLexeme("*") || matchLexeme("/")) {
        Token opToken = previous();
        auto right = parseUnary();
        expr = std::make_unique<BinaryExpr>(opToken.ubicacion, std::move(expr), opToken.lexema, std::move(right));
    }
    return expr;
}

// NUEVO: Soporte para expresiones unarias 
ExprPtr Parser::parseUnary() {
    if (matchLexeme("!") || matchLexeme("-") || matchLexeme("+") || matchLexeme("++") || matchLexeme("--")) {
        Token opToken = previous();
        auto operand = parseUnary(); // Permite unarios anidados 
        return std::make_unique<UnaryExpr>(opToken.ubicacion, opToken.lexema, std::move(operand), true);
    }

    auto expr = parsePrimary();

    // Soporte para sufijos unarios 
    if (matchLexeme("++") || matchLexeme("--")) {
        Token opToken = previous();
        return std::make_unique<UnaryExpr>(opToken.ubicacion, opToken.lexema, std::move(expr), false);
    }

    return expr;
}

ExprPtr Parser::parsePrimary() {
    if (match(TipoToken::NUMERO) || match(TipoToken::CADENA) || match(TipoToken::CARACTER)) {
        Token t = previous();
        return std::make_unique<LiteralExpr>(t.ubicacion, t.lexema, t.tipo);
    }
    
    if (match(TipoToken::IDENTIFICADOR)) {
        Token t = previous();
        ExprPtr expr = std::make_unique<IdentifierExpr>(t.ubicacion, t.lexema);
        
        // Bucle para permitir encadenamientos
        while (true) {
            if (matchLexeme("(")) {
                
                std::vector<ExprPtr> args;
                if (!checkLexeme(")")) {
                    do {
                        args.push_back(parseExpression());
                    } while (matchLexeme(","));
                }
                consumeLexeme(")", "Se esperaba ')' después de los argumentos.");
                expr = std::make_unique<CallExpr>(t.ubicacion, std::move(expr), std::move(args));
            } 
            else if (matchLexeme(".") || matchLexeme("->")) {
              
                std::string op = previous().lexema;
                Token memberToken = consume(TipoToken::IDENTIFICADOR, "Se esperaba un identificador de miembro tras '.' o '->'.");
                expr = std::make_unique<MemberExpr>(t.ubicacion, std::move(expr), memberToken.lexema, op == "->");
            } 
            else {
                break;
            }
        }
        return expr;
    }
    
    if (matchLexeme("(")) {
        auto expr = parseExpression();
        consumeLexeme(")", "Se esperaba ')' después de la expresión agrupada.");
        return expr;
    }

    error(peek(), "Expresión esperada.");
    throw ParseError("Expresión esperada.");
}

#endif