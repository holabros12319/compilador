add_executable(compiler_app
        main.cpp
)

target_include_directories(compiler_app PRIVATE
        ${CMAKE_SOURCE_DIR}/Lexer
        ${CMAKE_SOURCE_DIR}/Parser
        "${CMAKE_SOURCE_DIR}/Analizador Semantico"
        ${CMAKE_SOURCE_DIR}/IntermediateGeneration
)

target_link_libraries(compiler_app PRIVATE
        LexerLib
        ParserLib
        SemanticLib
        IntermediateCodeLib
)