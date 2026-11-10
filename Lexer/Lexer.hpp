#ifndef LEXER_HPP
#define LEXER_HPP

#include "Tokens.hpp"
#include <cstddef>
#include <string>
#include <vector>

class Lexer {

public:
    explicit Lexer (const std::string& sourceCode);

    std:: vector<Token> tokenize();

private:
    std:: string sourceCode;

    std::size_t position;

    int line;
    int column;

    char current () const;
    bool finished () const;
    void advance ();
    void advanceText (const std::string& text);

};

#endif