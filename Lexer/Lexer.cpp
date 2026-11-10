#include "Lexer.hpp"
#include "Regex.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {

    bool isLetter(char ch) {
        return std::isalpha(static_cast<unsigned char>(ch)) != 0;
    }

    bool isDigit(char ch) {
        return std::isdigit(static_cast<unsigned char>(ch)) != 0;
    }

    bool isAlphaNumeric(char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) != 0;
    }

    bool isWhitespace(char ch) {
        return std::isspace(static_cast<unsigned char>(ch)) != 0;
    }

    bool isIdentifierStart(char ch) {
        return isLetter(ch) || ch == '_';
    }

    bool isIdentifierPart(char ch) {
        return isAlphaNumeric(ch) || ch == '_';
    }

} // namespace

Lexer::Lexer(const std::string& sourceCode)
: sourceCode(sourceCode),
position(0),
line(1),
column(1) {
}

char Lexer::current() const {
    return finished() ? '\0' : sourceCode[position];
}

bool Lexer::finished() const {
    return position >= sourceCode.size();
}

void Lexer::advance() {
    if (finished()) {
        return;
    }

    if (sourceCode[position++] == '\n') {
        ++line;
        column = 1;
    } else {
        ++column;
    }
}
//Funcion de avance de texto corregida
void Lexer::advanceText(const std::string& text) {

    if (sourceCode.compare(position, text.size(), text) != 0){
        advance ();
        return;         
    }
    
    for (std::size_t i = 0; i < text.size(); ++i) {
        advance();
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!finished()) {
        const char ch = current();
        if (isWhitespace(ch)) {
            advance();
            continue;
        }

        const int startLine = line;
        const int startColumn = column;

        if (ch == '/' && position + 1 < sourceCode.size()) {
            if (sourceCode[position + 1] == '/') {
                advanceText("//");
                while (!finished() && current() != '\n') {
                    advance();
                }
                continue;
            }
            if (sourceCode[position + 1] == '*') {
                const std::size_t start = position;
                advanceText("/*");
                bool closed = false;
                while (!finished()) {
                    if (position + 1 < sourceCode.size() && current() == '*' && sourceCode[position + 1] == '/') {
                        advanceText("*/");
                        closed = true;
                        break;
                    }
                    advance();
                }
            
                if (!closed) {
                    tokens.emplace_back(TipoToken::ERROR_LEXICO,
                                        "Comentario de bloque sin cerrar: " + sourceCode.substr(start),
                                        startLine, startColumn);
                }
                continue;
            }
        }

        if (isIdentifierStart(ch)) {
            const std::size_t start = position;
            advance();
            while (!finished() && isIdentifierPart(current())) {
                advance();
            }

            const std::string lexema = sourceCode.substr(start, position - start);
            const TipoToken tipo = Regex::palabrasReservadas.count(lexema) > 0
            ? TipoToken::PALABRA_RESERVADA
            : TipoToken::IDENTIFICADOR;
            tokens.emplace_back(tipo, lexema, startLine, startColumn);
            continue;
        }

        // CORREGIDO: Soporte para números que empiezan con punto decimal (ej. .5) y notación científica (ej. 1e10, 2.5E-3)
        if (isDigit(ch) || (ch == '.' && position + 1 < sourceCode.size() && isDigit(sourceCode[position + 1]))) {
            const std::size_t start = position;
            bool hasDecimalPoint = false;
            bool malformed = false;

            // Revisar punto decimal inicial
            if (ch == '.') {
                hasDecimalPoint = true;
                advance();
            }

            //Consumir dígitos
            while (!finished() && isDigit(current())) {
                advance();
            }

            // Revisar punto decimal intermedio
            if (!hasDecimalPoint && !finished() && current() == '.') {
                hasDecimalPoint = true;
                advance();
                while (!finished() && isDigit(current())) {
                    advance();
                }
            }

            // Exponentes 
            if (!finished() && (current() == 'e' || current() == 'E')) {
                advance();
                if (!finished() && (current() == '+' || current() == '-')) {
                    advance();
                }
                
                //Esto solo se ejecuta si hubo una 'e' o 'E'
                if (finished() || !isDigit(current())) {
                    malformed = true; 
                } else {
                    while (!finished() && isDigit(current())) {
                        advance();
                    }
                }
            }

            //Sufijos de tipo en C/C++ 
            while (!finished() && (current() == 'f' || current() == 'F' || 
                                   current() == 'l' || current() == 'L' || 
                                   current() == 'u' || current() == 'U')) {
                advance();
            }

            //Revisar si pegaron letras no válidas al número
            if (!finished() && isIdentifierStart(current())) {
                malformed = true;
                while (!finished() && isIdentifierPart(current())) {
                    advance();
                }
            }

            //Generar el token
            const std::string lexema = sourceCode.substr(start, position - start);
            tokens.emplace_back(malformed ? TipoToken::ERROR_LEXICO : TipoToken::NUMERO,
                                malformed ? "Numero mal formado: " + lexema : lexema,
                                startLine, startColumn);
            continue;
        }

        if (ch == '"' || ch == '\'') {
            const char delimiter = ch;
            // Consumir la comilla de apertura antes de leer el contenido
            advance();
            bool closed = false;
            std::string lexemaReal = "";

            while (!finished() && current() != delimiter) {
                if (current() == '\n') {
                    lexemaReal += current();
                    advance();
                    continue;
                }

                if (current() == '\\') {
                    advance();
                    if (finished()) break;

                    char escaped = current();
                    switch (escaped) {
                        case 'n': lexemaReal += '\n'; break;
                        case 't': lexemaReal += '\t'; break;
                        case 'r': lexemaReal += '\r'; break;
                        case '\\': lexemaReal += '\\'; break;
                        case '"': lexemaReal += '"'; break;
                        case '\'': lexemaReal += '\''; break;
                        default: lexemaReal += escaped; break;
                    }

                    advance();
                    continue;
                }

                if (current() == '\n') {
                    lexemaReal += current();
                    advance();
                    continue;
                }

                lexemaReal += current();
                advance();
            }

            if (!finished() && current() == delimiter) {
                advance();
                closed = true;
            }

            if (!closed) {
                tokens.emplace_back(TipoToken::ERROR_LEXICO, "Cadena de texto sin cerrar", startLine, startColumn);
            } else if (delimiter == '\'' && lexemaReal.empty()) {
                // D-10: '' no es un literal de caracter valido.
                tokens.emplace_back(TipoToken::ERROR_LEXICO, "Literal de caracter vacio: ''", startLine, startColumn);
            } else {
                tokens.emplace_back(delimiter == '"' ? TipoToken::CADENA : TipoToken::CARACTER, lexemaReal, startLine, startColumn);
            }
            continue;
        }
       // Funcion de operadores corregida, para siempre ir en busca de los operadores mas largos (maxima coincidencia)
       const std::string* matchedOperator = nullptr;
       for (const std::string& candidate : Regex::operadores) {
        if (candidate.size() > (matchedOperator ? matchedOperator->size() : 0) &&
            sourceCode.compare(position, candidate.size(), candidate) == 0) {
            matchedOperator = &candidate;
          }
       }

       if (matchedOperator != nullptr) {
        const std::string lexema = *matchedOperator;
        advanceText(lexema);
        tokens.emplace_back(TipoToken::OPERADOR, lexema, startLine, startColumn);
        continue;

       }  
        if (Regex::delimitadores.find(ch) != std::string::npos) {
            advance();
            tokens.emplace_back(TipoToken::DELIMITADOR, std::string(1, ch), startLine, startColumn);
            continue;
        }

        advance();
        tokens.emplace_back(TipoToken::ERROR_LEXICO,
                            "Caracter no reconocido: " + std::string(1, ch),
                            startLine, startColumn);
    }

    tokens.emplace_back(TipoToken::FIN_ARCHIVO, "", line, column);
    return tokens;
}
