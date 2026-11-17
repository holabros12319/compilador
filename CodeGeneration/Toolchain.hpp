#pragma once
// Ensamblado + enlace del .s generado por NativeBackend usando gcc/clang
// (MinGW-w64 en Windows). No hace falta NASM: el driver de gcc ya trae `as`.
#include <string>

struct BuildResult {
    bool ok = false;
    std::string message;   // diagnostico legible si ok == false
    std::string compiler;  // driver usado
};

// ccHint: ruta explicita al driver (vacio = autodeteccion:
// --cc, variable CC, el gcc hermano del compilador de CMake, gcc, mingw, clang).
BuildResult buildExecutable(const std::string& asmPath,
                            const std::string& exePath,
                            const std::string& ccHint);

// Ejecuta el programa generado y devuelve su codigo de salida (-1 si no pudo lanzarse).
int runExecutable(const std::string& exePath);
