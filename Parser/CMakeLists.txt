add_library(ParserLib 
    Parser.cpp 
    Parser.hpp
    AST.cpp
)

target_include_directories(ParserLib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ParserLib PUBLIC LexerLib)