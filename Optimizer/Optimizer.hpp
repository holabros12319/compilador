#ifndef OPTIMIZER_HPP
#define OPTIMIZER_HPP

#include "AST.hpp"

#include <cstddef>
#include <iosfwd>

// Contadores de lo que hizo el optimizador (útil para el reporte).
struct OptimizationStats {
    std::size_t constantsFolded             = 0; // 2 + 3 * 4  ->  14
    std::size_t algebraicSimplifications    = 0; // x * 1, x + 0, -(-x), +x
    std::size_t constantsPropagated         = 0; // int a = 5; ... a ...  ->  ... 5 ...
    std::size_t deadBranchesRemoved         = 0; // if (0), while (0), for con condicion falsa
    std::size_t unreachableStatementsRemoved = 0; // codigo despues de un return
    std::size_t deadVariablesRemoved        = 0; // variables locales que nunca se usan
    std::size_t pureStatementsRemoved       = 0; // "5;" o "x + 1;" (sin efectos)
    std::size_t emptyBlocksRemoved          = 0;
    std::size_t iterations                  = 0;

    std::size_t total() const {
        return constantsFolded + algebraicSimplifications + constantsPropagated +
               deadBranchesRemoved + unreachableStatementsRemoved +
               deadVariablesRemoved + pureStatementsRemoved + emptyBlocksRemoved;
    }

    void dump(std::ostream& out) const;
};

// Optimizador a nivel de AST. Se ejecuta DESPUES del analisis semantico
// (asume que el programa ya es valido) y modifica el arbol en sitio.
class Optimizer {
public:
    OptimizationStats optimize(ProgramNode& program, std::size_t maxIterations = 10);
};

#endif // OPTIMIZER_HPP
