# Fase 6: Generacion de codigo nativo (x86-64, ensamblador GAS / sintaxis Intel).
# Igual que las demas fases: sin project(), hereda estandar y flags del CMakeLists raiz.
add_library(NativeCodeLib STATIC
    NativeBackend.hpp
    NativeBackend.cpp
    Toolchain.hpp
    Toolchain.cpp
)

target_include_directories(NativeCodeLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Consume las cuadruplas (Quadruple / OpCode) de la fase 5.
target_link_libraries(NativeCodeLib PUBLIC IntermediateCodeLib)

# ----------------------------------------------------------------------------
# Ensamblador/enlazador: se usa el driver gcc de MinGW-w64 (trae `as` y `ld`),
# no hace falta instalar NASM. Se busca primero junto al compilador de C++ con
# el que se esta compilando el proyecto (p. ej. el MinGW incluido en CLion).
# Con MSVC no hay gcc "hermano": se buscara en el PATH o con --cc en tiempo de ejecucion.
# ----------------------------------------------------------------------------
if(NOT MSVC)
    get_filename_component(_cxx_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(NATIVE_CC NAMES gcc x86_64-w64-mingw32-gcc clang
                 HINTS "${_cxx_dir}" NO_DEFAULT_PATH)
    if(NOT NATIVE_CC)
        find_program(NATIVE_CC NAMES gcc x86_64-w64-mingw32-gcc clang)
    endif()
endif()

if(NATIVE_CC)
    message(STATUS "Fase 6: driver de ensamblado/enlace = ${NATIVE_CC}")
    target_compile_definitions(NativeCodeLib PRIVATE "NATIVE_DEFAULT_CC=\"${NATIVE_CC}\"")
else()
    message(STATUS "Fase 6: no se encontro gcc/clang; se generara el .s pero no el ejecutable (usa --cc)")
endif()
