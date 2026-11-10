#ifndef INTERMEDIATECODE_H
#define INTERMEDIATECODE_H

#include <string>
#include <vector>

// Conjunto cerrado de opcodes del IR (TAC) definido por la especificación.
enum class OpCode {
    ADD, SUB, MUL, DIV, MOD,
    NEG, NOT,
    EQ, NEQ, LT, GT, LE, GE,
    AND, OR,
    ASSIGN,
    LABEL, GOTO, IF_FALSE_GOTO,
    PARAM, CALL, RETURN,
    PRINT, READ, POP_PARAM
};

// Cuádrupla de código intermedio de tres direcciones.
struct Quadruple {
    OpCode op;
    std::string arg1;
    std::string arg2;
    std::string result;

    // Devuelve una representación legible de la cuádrupla.
    std::string print() const;
};

// Búfer lineal de instrucciones TAC con administración de temporales y etiquetas.
class CodeBuffer {
private:
    std::vector<Quadruple> instructions;
    int tempCount = 0;
    int labelCount = 0;

public:
    // Genera un temporal único autoincremental (t0, t1, t2, ...).
    std::string newTemp();

    // Genera una etiqueta de bloque única (L0, L1, L2, ...).
    std::string newLabel();

    // Emite una cuádrupla (op, arg1, arg2, result) al final de la lista.
    void emit(OpCode op,
              const std::string& arg1,
              const std::string& arg2,
              const std::string& result);

    // Devuelve la lista plana de cuádruplas emitidas (solo lectura).
    const std::vector<Quadruple>& getInstructions() const;
};

#endif // INTERMEDIATECODE_H