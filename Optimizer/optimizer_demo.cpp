#include <iostream>
#include <string>

#include "Lexer.hpp"
#include "Parser.hpp"
#include "Optimizer.hpp"

int main() {
    const std::string codigo = R"(
        int g = 4;

        int cuadrado(int n) {
            return n * n;
        }

        int main() {
            int a = 2 + 3 * 4;
            int b = a * 1 + 0;
            float pi = 3;
            int muerto = 99;
            int x = 10;
            int c = 7 / 2;
            int d = 1 / 0;
            int neg = 2 - 5;

            if (1) { x = x + 1; } else { x = 0; }
            if (0) { x = 5; }
            while (0) { x = x + 1; }
            for (int i = 0; 0; i++) { x = x + 2; }

            int y = x * 0;
            5;
            x + 1;
            int r = cuadrado(g + 1);
            int s = cuadrado(2);
            int t = -(-x);
            return a + b + y + c + d + neg + r + s + t;
            x = 100;
            x = 200;
        }
    )";

    Lexer lexer(codigo);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();

    std::cout << "=== ANTES ===\n" << ast->toString() << "\n\n";

    Optimizer optimizer;
    OptimizationStats stats = optimizer.optimize(*ast);

    std::cout << "=== DESPUES ===\n" << ast->toString() << "\n\n";
    stats.dump(std::cout);
    return 0;
}
