add_library(SemanticLib
    Semantic.cpp
    Semantic.hpp
)

target_include_directories(SemanticLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SemanticLib PUBLIC ParserLib LexerLib)

