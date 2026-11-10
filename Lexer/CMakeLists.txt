add_library(LexerLib 
    Lexer.cpp 
    Lexer.hpp 
    Tokens.hpp 
    Regex.hpp
)

# Permitir que otros módulos (como el Parser) encuentren estos headers limpiamente
target_include_directories(LexerLib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)