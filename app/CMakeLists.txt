add_executable(compiler_app main.cpp)

# Los include dirs llegan solos por target_link_libraries (son PUBLIC en cada fase).
target_link_libraries(compiler_app PRIVATE
    LexerLib
    ParserLib
    SemanticLib
    OptimizerLib
    IntermediateCodeLib
    NativeCodeLib
)

# ----------------------------------------------------------------------------
# Atajos para ejecutar / depurar desde consola o desde CLion (Run > Build Target)
#   cmake --build <dir> --target run_compiler
#   cmake --build <dir> --target debug_gdb
# Archivo a compilar: -DCOMPILER_INPUT=ruta/al/archivo.c (vacio = programa de prueba incrustado)
# ----------------------------------------------------------------------------
set(COMPILER_INPUT "" CACHE FILEPATH "Archivo fuente para run_compiler / debug_gdb")

add_custom_target(run_compiler
    COMMAND compiler_app ${COMPILER_INPUT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Ejecutando compiler_app"
)

find_program(GDB_EXECUTABLE gdb)
if(GDB_EXECUTABLE)
    add_custom_target(debug_gdb
        COMMAND ${GDB_EXECUTABLE} -q --args $<TARGET_FILE:compiler_app> ${COMPILER_INPUT}
        DEPENDS compiler_app
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        USES_TERMINAL
        COMMENT "Abriendo compiler_app en gdb"
    )
endif()

# ----------------------------------------------------------------------------
# Pruebas (ctest). Los codigos de salida de compiler_app son:
#   0 ok | 1 lexico | 2 sintactico | 3 semantico | 4 archivo | 5 generacion
# ----------------------------------------------------------------------------
set(EXAMPLES_DIR ${CMAKE_SOURCE_DIR}/examples)

add_test(NAME programa_incrustado COMMAND compiler_app)
add_test(NAME ok_bucles           COMMAND compiler_app ${EXAMPLES_DIR}/ok_bucles.c)
add_test(NAME ok_estructuras      COMMAND compiler_app ${EXAMPLES_DIR}/ok_estructuras.c)

foreach(caso IN ITEMS err_lexico err_sintactico err_semantico)
    add_test(NAME ${caso} COMMAND compiler_app ${EXAMPLES_DIR}/${caso}.c)
    set_tests_properties(${caso} PROPERTIES WILL_FAIL TRUE)
endforeach()

# ----------------------------------------------------------------------------
# Pruebas de la fase 6 (compilan a ejecutable nativo y lo ejecutan).
# Solo se registran si hay gcc/clang disponible (NATIVE_CC, ver CodeGeneration/CMakeLists.txt).
# ----------------------------------------------------------------------------
set(NATIVE_OUT ${CMAKE_BINARY_DIR}/native_tests)
file(MAKE_DIRECTORY ${NATIVE_OUT})

add_test(NAME nativo_solo_asm
         COMMAND compiler_app ${EXAMPLES_DIR}/ok_bucles.c --asm-only -o ${NATIVE_OUT}/solo_asm)

if(NATIVE_CC)
    add_test(NAME nativo_bucles
             COMMAND compiler_app ${EXAMPLES_DIR}/ok_bucles.c --run -o ${NATIVE_OUT}/ok_bucles)
    set_tests_properties(nativo_bucles PROPERTIES
        PASS_REGULAR_EXPRESSION "--- Ejecucion ---[\r\n]+143120")

    add_test(NAME nativo_estructuras
             COMMAND compiler_app ${EXAMPLES_DIR}/ok_estructuras.c --run -o ${NATIVE_OUT}/ok_estructuras)
    set_tests_properties(nativo_estructuras PROPERTIES
        PASS_REGULAR_EXPRESSION "--- Ejecucion ---[\r\n]+suma = 7")

    add_test(NAME nativo_tipos_y_control
             COMMAND compiler_app ${EXAMPLES_DIR}/ok_nativo.c --run -o ${NATIVE_OUT}/ok_nativo)
    set_tests_properties(nativo_tipos_y_control PROPERTIES
        PASS_REGULAR_EXPRESSION "--- Ejecucion ---[\r\n]+7 2.5 1.5 A 1")
endif()
