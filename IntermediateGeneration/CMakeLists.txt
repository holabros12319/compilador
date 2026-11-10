# CMakeLists.txt — fase de Generación de Código Intermedio TAC
cmake_minimum_required(VERSION 3.10)
project(IntermediateGeneration LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(IntermediateCodeLib STATIC
    IntermediateCode.h
    CodeGenerator.h
    CodeGenerator.cpp
)

# Rutas de inclusión: esta carpeta + fases anteriores (Parser, Lexer).
target_include_directories(IntermediateCodeLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../Parser
    ${CMAKE_CURRENT_SOURCE_DIR}/../Lexer
)

# Enlazar con la fase sintáctica (ParserLib propaga LexerLib, V-002).
target_link_libraries(IntermediateCodeLib PUBLIC ParserLib)