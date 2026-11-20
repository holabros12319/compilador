# Compilador

Fases: Lexer -> Parser -> Analizador Semantico -> Optimizer -> Codigo intermedio (TAC) -> Codigo nativo x86-64.

## Compilar y ejecutar
    cmake -S . -B build
    cmake --build build
    ./build/app/compiler_app                      # programa de prueba incrustado
    ./build/app/compiler_app examples/ok_bucles.c # un archivo

## Depurar con gdb
    cmake --build build --target debug_gdb        # -DCOMPILER_INPUT=ruta.c para elegir archivo

## Pruebas
    ctest --test-dir build --output-on-failure

Codigos de salida: 0 ok, 1 lexico, 2 sintactico, 3 semantico, 4 archivo, 5 generacion,
6 ensamblado/enlace, 7 uso incorrecto de la linea de comandos.

## Generar y ejecutar un programa nativo (Windows)
    compiler_app                                     # compila y ejecuta el programa de prueba de main.cpp
    compiler_app examples/ok_bucles.c               # genera ok_bucles.s y ok_bucles.exe y lo ejecuta
    compiler_app examples/ok_bucles.c --run         # (se ejecuta por defecto; --no-run lo desactiva)
    compiler_app examples/ok_entrada.c -o suma.exe --run
    compiler_app examples/ok_bucles.c --asm-only    # solo el ensamblador (.s)
    compiler_app archivo.c --cc C:/mingw64/bin/gcc.exe   # elegir el ensamblador/enlazador

La fase 6 (carpeta `CodeGeneration/`) traduce el TAC a ensamblador x86-64 (GAS, sintaxis
Intel, ABI Microsoft x64) y el driver gcc de MinGW-w64 lo ensambla y enlaza. No requiere NASM.
`--target linux` genera System V AMD64 (para probar en Linux/WSL).

Soportado: int, float, double, char, bool, string, struct (campos escalares y anidados),
variables globales y locales con ambitos, funciones (recursion, cualquier numero de
argumentos), if/else, while, for, `&&` y `||` en cortocircuito, `cout <<` y `cin >>`.
No soportado todavia: structs pasados o devueltos por valor, concatenacion de strings.
