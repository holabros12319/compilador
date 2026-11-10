cmake_minimum_required(VERSION 3.15)
project(SimpleCompiler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Indicar los directorios donde están los archivos .hpp / .h
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/Lexer
        ${CMAKE_CURRENT_SOURCE_DIR}/Parser
        "${CMAKE_CURRENT_SOURCE_DIR}/Analizador Semantico"
        "${CMAKE_CURRENT_SOURCE_DIR}/IntermediateGeneration"
)

add_subdirectory(Lexer)
add_subdirectory(Parser)
add_subdirectory("Analizador Semantico")
add_subdirectory("IntermediateGeneration")
add_subdirectory(app)