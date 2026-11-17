#pragma once
// Fase 6: generacion de codigo nativo x86-64 (sintaxis Intel, ensamblador GAS)
// a partir de las cuadruplas TAC.
//
// Entrada : std::vector<Quadruple> producido por CodeGenerator (fase 5).
// Salida  : texto .s listo para `gcc archivo.s -o programa.exe` (MinGW-w64).
//
// Objetivos soportados:
//   TargetOS::Windows -> ABI Microsoft x64 (rcx, rdx, r8, r9 + shadow space)
//   TargetOS::Linux   -> ABI System V AMD64 (rdi, rsi, rdx, rcx, r8, r9)
#include <string>
#include <vector>

#include "CodeGenerator.h"  // Quadruple, OpCode

enum class TargetOS { Windows, Linux };

inline TargetOS defaultTargetOS() {
#ifdef _WIN32
    return TargetOS::Windows;
#else
    return TargetOS::Linux;
#endif
}

class NativeBackend {
public:
    explicit NativeBackend(TargetOS target = defaultTargetOS()) : target_(target) {}

    // Devuelve el ensamblador completo. Lanza std::runtime_error si el TAC
    // contiene algo que el backend no puede traducir.
    std::string generate(const std::vector<Quadruple>& tac) const;

private:
    TargetOS target_;
};
