#ifndef PARSER_CPP
#define PARSER_CPP

#include "Parser.hpp"
#include <iostream>

Parser::Parser(std::vector<Token> tokens)
    : tokens(std::move(tokens)), currentPosition(0), errorSintactico(false) {
    knownTypes = {"int", "float", "double", "char", "bool", "string", "void"};
}

bool Parser::huboErroresSintacticos() const {
    return errorSintactico;
}


const Token& Parser::peek() const {
    if (tokens.empty()) {
        static const Token eofToken(TipoToken::FIN_ARCHIVO, "", 1, 1);
        return eofToken;
    }
    if (currentPosition >= tokens.size()) {
        return tokens.back();
    }
    return tokens[currentPosition];
}

const Token& Parser::previous() const {
    if (currentPosition == 0 || tokens.empty()) return peek();
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
    errorSintactico = true;
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
                    peek().lexema == "string" || peek().lexema == "if" || peek().lexema == "return" || 
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
    // La segunda condicion evita un bucle infinito si el flujo de tokens no
    // termina en FIN_ARCHIVO (docs.md C-01): peek() repetiria tokens.back().
    while (!isAtEnd() && currentPosition < tokens.size()) {
        try {
            program->declarations.push_back(parseDeclaration());
        } catch (const ParseError&) {
            synchronize();
        }
    }
    return program;
}

StmtPtr Parser::parseDeclaration() {
    if (matchLexeme("struct")) return parseStructDecl();
    if ((peek().tipo == TipoToken::PALABRA_RESERVADA || peek().tipo == TipoToken::IDENTIFICADOR) &&
        knownTypes.count(peek().lexema) > 0) {
        return parseFunctionOrVarDecl();
    }
    return parseStatement();
}

DeclPtr Parser::parseStructDecl() {
    Token structToken = previous();
    Token nameToken = consume(TipoToken::IDENTIFICADOR, "Se esperaba el nombre de la estructura.");
    knownTypes.insert(nameToken.lexema);
    consumeLexeme("{", "Se esperaba '{' para abrir la estructura.");
    
    std::vector<std::unique_ptr<VarDecl>> fields;
    while (!checkLexeme("}") && !isAtEnd()) {
        Token typeToken = peek();
        if (typeToken.tipo != TipoToken::PALABRA_RESERVADA && typeToken.tipo != TipoToken::IDENTIFICADOR) {
            error(typeToken, "Se esperaba un tipo.");
            throw ParseError("Se esperaba un tipo.");
        }
        currentPosition++;
        Token fieldNameToken = consume(TipoToken::IDENTIFICADOR, "Se esperaba un identificador.");
        ExprPtr initializer = matchLexeme("=") ? parseExpression() : nullptr;
        consumeLexeme(";", "Se esperaba ';'.");
        fields.push_back(std::make_unique<VarDecl>(typeToken.ubicacion, typeToken.lexema, fieldNameToken.lexema, std::move(initializer)));
    }
    consumeLexeme("}", "Se esperaba '}'.");
    consumeLexeme(";", "Se esperaba ';'.");

    return std::make_unique<StructDecl>(structToken.ubicacion, nameToken.lexema, std::move(fields));
}

std::string Parser::parseType() {
    if (!isAtEnd() && (peek().tipo == TipoToken::PALABRA_RESERVADA || peek().tipo == TipoToken::IDENTIFICADOR) &&
        knownTypes.count(peek().lexema) > 0) {
        std::string type = peek().lexema;
        currentPosition++;
        return type;
    }
    error(peek(), "Se esperaba un tipo válido.");
    throw ParseError("Se esperaba un tipo válido.");
}

DeclPtr Parser::parseFunctionOrVarDecl() {
    Token typeToken = peek();
    currentPosition++; 
    Token nameToken = consume(TipoToken::IDENTIFICADOR, "Se esperaba un identificador.");

    if (matchLexeme("(")) {
        std::vector<Param> params;
        if (!checkLexeme(")")) {
            do {
                Token pType = peek();
                if (pType.tipo != TipoToken::PALABRA_RESERVADA && pType.tipo != TipoToken::IDENTIFICADOR) {
                    error(pType, "Se esperaba el tipo.");
                    throw ParseError("Se esperaba el tipo.");
                }
                currentPosition++;
                Token pName = consume(TipoToken::IDENTIFICADOR, "Se esperaba el parámetro.");
                params.emplace_back(pType.lexema, pName.lexema, pType.ubicacion);
            } while (matchLexeme(","));
        }
        consumeLexeme(")", "Se esperaba ')'.");
        
        if (matchLexeme(";")) {
            return std::make_unique<FunctionDecl>(typeToken.ubicacion, typeToken.lexema, nameToken.lexema, std::move(params), nullptr);
        }

        consumeLexeme("{", "Se esperaba '{'.");
        std::vector<StmtPtr> bodyStmts;
        while (!checkLexeme("}") && !isAtEnd()) {
            bodyStmts.push_back(parseDeclaration());
        }
        consumeLexeme("}", "Se esperaba '}'.");
        
        auto bodyBlock = std::make_unique<BlockStmt>(typeToken.ubicacion, std::move(bodyStmts));
        return std::make_unique<FunctionDecl>(typeToken.ubicacion, typeToken.lexema, nameToken.lexema, std::move(params), std::move(bodyBlock));
    }

    ExprPtr initializer = matchLexeme("=") ? parseExpression() : nullptr;
    consumeLexeme(";", "Se esperaba ';'.");
    return std::make_unique<VarDecl>(typeToken.ubicacion, typeToken.lexema, nameToken.lexema, std::move(initializer));
}

StmtPtr Parser::parseStatement() {
    if ((peek().tipo == TipoToken::PALABRA_RESERVADA || peek().tipo == TipoToken::IDENTIFICADOR) &&
        knownTypes.count(peek().lexema) > 0) {
        return parseFunctionOrVarDecl();
    }
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
    consumeLexeme("}", "Se esperaba '}'.");
    return std::make_unique<BlockStmt>(blockToken.ubicacion, std::move(statements));
}

StmtPtr Parser::parseIfStatement() {
    Token ifToken = previous();
    consumeLexeme("(", "Se esperaba '('.");
    auto condition = parseExpression();
    consumeLexeme(")", "Se esperaba ')'.");

    // SOLUCIÓN: Permite bloques de una línea sin forzar el uso de '{}'
    StmtPtr thenBranch = parseStatement(); 
    StmtPtr elseBranch = matchLexeme("else") ? parseStatement() : nullptr;

    return std::make_unique<IfStmt>(ifToken.ubicacion, std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

StmtPtr Parser::parseWhileStatement() {
    Token whileToken = previous();
    consumeLexeme("(", "Se esperaba '('.");
    auto condition = parseExpression();
    consumeLexeme(")", "Se esperaba ')'.");
    
    StmtPtr body = parseStatement();

    return std::make_unique<WhileStmt>(whileToken.ubicacion, std::move(condition), std::move(body));
}

StmtPtr Parser::parseForStatement() {
    Token forToken = previous();
    consumeLexeme("(", "Se esperaba '('.");

    StmtPtr init = nullptr;
    if (!matchLexeme(";")) {
        if (check(TipoToken::PALABRA_RESERVADA)) {
            init = parseFunctionOrVarDecl();
        } else {
            init = parseExpressionStatement();
        }
    }

    ExprPtr condition = nullptr;
    if (!checkLexeme(";")) condition = parseExpression();
    consumeLexeme(";", "Se esperaba ';'.");

    ExprPtr increment = nullptr;
    if (!checkLexeme(")")) increment = parseExpression();
    consumeLexeme(")", "Se esperaba ')'.");

    StmtPtr body = parseStatement();
    return std::make_unique<ForStmt>(forToken.ubicacion, std::move(init), std::move(condition), std::move(increment), std::move(body));
}

StmtPtr Parser::parseReturnStatement() {
    Token retToken = previous();
    ExprPtr value = nullptr;
    if (!checkLexeme(";")) value = parseExpression();
    consumeLexeme(";", "Se esperaba ';'.");
    return std::make_unique<ReturnStmt>(retToken.ubicacion, std::move(value));
}

StmtPtr Parser::parseExpressionStatement() {
    Token t = peek();
    auto expr = parseExpression();
    consumeLexeme(";", "Se esperaba ';'.");
    return std::make_unique<ExprStmt>(t.ubicacion, std::move(expr));
}

ExprPtr Parser::parseExpression() {
    return parseAssignment();
}

// SOLUCIÓN L-VALUE Y RTTI OVERHEAD:
ExprPtr Parser::parseAssignment() {
    auto expr = parseLogicalOr(); 
    if (matchLexeme("=")) {
        Token equals = previous();
        auto value = parseAssignment();
        // Uso de Enums (NodeKind) en lugar de costoso dynamic_cast. Permite arrays y structs.
        NodeKind k = expr->kind();
        if (k == NodeKind::IdentifierExpr || k == NodeKind::MemberExpr) {
            return std::make_unique<BinaryExpr>(equals.ubicacion, std::move(expr), "=", std::move(value));
        }
        // D-04: antes solo se informaba el error y se devolvia el L-Value,
        // perdiendo "= valor". Ahora la declaracion completa se descarta.
        const std::string mensaje = "Destino de asignación inválido. L-Value requerido.";
        error(equals, mensaje);
        throw ParseError(mensaje);
    }
    return expr;
}

ExprPtr Parser::parseLogicalOr() {
    auto expr = parseLogicalAnd();
    while (matchLexeme("||")) {
        Token opToken = previous();
        auto right = parseLogicalAnd();
        expr = std::make_unique<BinaryExpr>(opToken.ubicacion, std::move(expr), opToken.lexema, std::move(right));
    }
    return expr;
}

ExprPtr Parser::parseLogicalAnd() {
    auto expr = parseEquality();
    while (matchLexeme("&&")) {
        Token opToken = previous();
        auto right = parseEquality();
        expr = std::make_unique<BinaryExpr>(opToken.ubicacion, std::move(expr), opToken.lexema, std::move(right));
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
    auto expr = parseShift();
    while (matchLexeme(">") || matchLexeme(">=") || matchLexeme("<") || matchLexeme("<=")) {
        Token opToken = previous();
        auto right = parseShift();
        expr = std::make_unique<BinaryExpr>(opToken.ubicacion, std::move(expr), opToken.lexema, std::move(right));
    }
    return expr;
}

ExprPtr Parser::parseShift() {
    auto expr = parseTerm();
    while (matchLexeme("<<") || matchLexeme(">>")) {
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
    auto expr = parseUnary(); 
    while (matchLexeme("*") || matchLexeme("/") || matchLexeme("%")) {
        Token opToken = previous();
        auto right = parseUnary();
        expr = std::make_unique<BinaryExpr>(opToken.ubicacion, std::move(expr), opToken.lexema, std::move(right));
    }
    return expr;
}

ExprPtr Parser::parseUnary() {
    if (matchLexeme("!") || matchLexeme("-") || matchLexeme("+") || matchLexeme("++") || matchLexeme("--")) {
        Token opToken = previous();
        auto operand = parseUnary(); 
        return std::make_unique<UnaryExpr>(opToken.ubicacion, opToken.lexema, std::move(operand), true);
    }

    auto expr = parsePrimary();

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
        
        while (true) {
            if (matchLexeme("(")) {
                std::vector<ExprPtr> args;
                if (!checkLexeme(")")) {
                    do {
                        args.push_back(parseExpression());
                    } while (matchLexeme(","));
                }
                consumeLexeme(")", "Se esperaba ')'.");
                expr = std::make_unique<CallExpr>(t.ubicacion, std::move(expr), std::move(args));
            } 
            else if (matchLexeme(".") || matchLexeme("->")) {
                std::string op = previous().lexema;
                Token memberToken = consume(TipoToken::IDENTIFICADOR, "Se esperaba identificador.");
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
        consumeLexeme(")", "Se esperaba ')'.");
        return expr;
    }

    error(peek(), "Expresión inválida.");
    throw ParseError("Expresión inválida.");
}

#endif  