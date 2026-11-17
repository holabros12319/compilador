cmake_minimum_required(VERSION 3.15)
project(SimpleCompiler LANGUAGES CXX)

# ----------------------------------------------------------------------------
# Estandar de C++
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# compile_commands.json: lo usan CLion, clangd y otras herramientas.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ----------------------------------------------------------------------------
# Tipo de compilacion
# Si nadie lo indica (cmake por consola) se usa Debug, que es lo que necesita
# gdb. CLion ya manda su propio perfil (Debug / Release), eso se respeta.
# ----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Tipo de compilacion" FORCE)
endif()

# ----------------------------------------------------------------------------
# Opciones de compilador (se aplican a TODAS las fases)
# ----------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W3 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wno-missing-field-initializers -Wno-misleading-indentation)
    # -g3 : maxima informacion de depuracion para gdb (incluye macros)
    # -O0 : sin optimizar, para que gdb no "salte" lineas ni oculte variables
    set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -fno-omit-frame-pointer")
endif()

# Sanitizers opcionales para cazar fallos de memoria (no disponibles en MinGW/MSVC).
option(COMPILER_SANITIZERS "Activar AddressSanitizer + UBSan" OFF)
if(COMPILER_SANITIZERS AND NOT MSVC AND NOT MINGW)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ----------------------------------------------------------------------------
# Fases del compilador (cada carpeta define su propia libreria estatica y
# exporta sus headers con target_include_directories(... PUBLIC ...)).
#
#   LexerLib  <-  ParserLib  <-  SemanticLib
#                            <-  OptimizerLib
#                            <-  IntermediateCodeLib  <-  NativeCodeLib (x86-64)
# ----------------------------------------------------------------------------
add_subdirectory(Lexer)
add_subdirectory(Parser)
add_subdirectory("Analizador Semantico")
add_subdirectory(Optimizer)
add_subdirectory(IntermediateGeneration)
add_subdirectory(CodeGeneration)

# ----------------------------------------------------------------------------
# Ejecutable final + pruebas (ctest)
# ----------------------------------------------------------------------------
enable_testing()
add_subdirectory(app)
