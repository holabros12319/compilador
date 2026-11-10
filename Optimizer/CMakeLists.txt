add_library(OptimizerLib
        Optimizer.cpp
        Optimizer.hpp
)

target_include_directories(OptimizerLib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(OptimizerLib PUBLIC ParserLib)