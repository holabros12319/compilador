#ifndef REGEX_HPP
#define REGEX_HPP

#include <vector>
#include <regex>
#include <string>
#include <unordered_set>

struct Regex {


    //expresiones regulares
    inline static const std :: regex identifier {R"([a-zA-Z_][a-zA-Z0-9_]*)"};
    inline static const std :: regex integer {R"([0-9]+)"};
    inline static const std :: regex decimales {R"([0-9]+\.[0-9]+)"};
    inline static const std :: regex  string {R"("([^"\\]|\\.)*")"};
    inline static const std :: regex  character {R"('([^'\\]|\\.)')"};
    inline static const std::regex comentarioLinea{ R"(//[^\n]*)"};
    inline static const std::regex comentarioBloque{ R"(/\*[\s\S]*?\*/)"};

    // operadores
    inline static const std::vector<std::string> operadores = {
        ">>=", "<<=", "->*", "...",
        "++", "--", "==", "!=", "<=", ">=",
        "&&", "||", "<<", ">>",
        "+=", "-=", "*=", "/=", "%=",
        "&=", "|=", "^=",
        "->", ".*", "::",
        "+", "-", "*", "/", "%", "=",
        "<", ">", "!", "&", "|", "^", "~",
        "?", ":"
    };

    //delimitadores
    inline static const std::string delimitadores =
    "()[]{};,.";

        inline static const std::unordered_set <std::string>
        palabrasReservadas = {
            "alignas",
            "alignof",
            "and",
            "and_eq",
            "asm",
            "auto",
            "bitand",
            "bitor",
            "bool",
            "break",
            "case",
            "catch",
            "char",
            "class",
            "compl",
            "const",
            "constexpr",
            "continue",
            "default",
            "delete",
            "do",
            "double",
            "else",
            "enum",
            "explicit",
            "export",
            "extern",
            "false",
            "float",
            "for",
            "friend",
            "goto",
            "if",
            "inline",
            "int",
            "long",
            "namespace",
            "new",
            "nullptr",
            "operator",
            "private",
            "protected",
            "public",
            "return",
            "short",
            "signed",
            "sizeof",
            "static",
            "struct",
            "switch",
            "template",
            "this",
            "throw",
            "true",
            "try",
            "typedef",
            "typename",
            "union",
            "unsigned",
            "using",
            "virtual",
            "void",
            "volatile",
            "while",
            // CORREGIDO: Palabras reservadas faltantes de C++ (C++11 hasta C++20)
            "dynamic_cast",
            "static_cast",
            "reinterpret_cast",
            "wchar_t",
            "char16_t",
            "char32_t",
            "char8_t",
            "decltype",
            "mutable",
            "noexcept",
            "thread_local",
            "static_assert",
            "typeid",
            "concept",
            "requires",
            "consteval",
            "constinit",
            "co_await",
            "co_return",
            "co_yield"
        };

};

#endif
