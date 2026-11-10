#include "Optimizer.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Semantic.hpp"
#include "CodeGenerator.h"
namespace {


    const int SALIDA_OK = 0;
    const int SALIDA_ERROR_LEXICO = 1;
    const int SALIDA_ERROR_SINTACTICO = 2;
    const int SALIDA_ERROR_SEMANTICO = 3;

    //si hay un ERROR_LEXICO se guarda con su ubicacion exacta imprimiendo el error tal cual
    bool reportarErroresLexicos(const std::vector<Token>& tokens) {
        bool hayErrores = false;
        for (const auto& token : tokens) {
            if (token.tipo != TipoToken::ERROR_LEXICO) continue;
            std::cerr << "[Error Lexico] Linea " << token.ubicacion.linea
                      << ", Columna " << token.ubicacion.columna << ": "
                      << token.lexema << "\n";
            hayErrores = true;
        }
        return hayErrores;
    }

} // namespace

int main() {
    // Código de prueba en C++
    std::string codigoPrueba = R"(
        struct Punto {
           int x;
           int y;
        };

        int suma(int a, int b) {
            return a + b;
        }

        int main() {
           int x = 0;
           int y = 0;
           cin >> x;
           cin >> y;

           Punto p;
           p.x = x;
           p.y = y;

           if (x > y) {
              cout << "x es mayor" << '\n';
           } else {
                cout << "y es mayor o igual" << '\n';
           }

           int s = suma(x, y);
           cout << "suma = " << s << '\n';
           return 0;
        }
    )";

    // Instanciar el Lexer y obtener la lista de tokens
    Lexer lexer(codigoPrueba);
    std::vector<Token> tokens = lexer.tokenize();

    // Imprimir cada token capturado
    for (const auto& token : tokens) {
        std::cout << token << "\n";
    }

    //errores lexicos invalidan el flujo de trabajo. El parser recibira tokens de ERROR_LEXICO y producira un AST sin valor
    if (reportarErroresLexicos(tokens)) {
        std::cerr << "[Fase] Analisis lexico fallido; no se ejecuta el analisis sintactico.\n";
        return SALIDA_ERROR_LEXICO;
    }

    //salida en pamtalla del AST
    std::cout << "\nParser en ejecucion...\n";
    Parser parser(tokens);

    std::unique_ptr<ProgramNode> ast;
    try {
        ast = parser.parse();
    } catch (const std::exception& e) {
        std::cerr << "Fallo en el análisis sintáctico: " << e.what() << "\n";
        return SALIDA_ERROR_SINTACTICO;
    }

    //el parser in forma si recupero errores sintacticos, se muestra el AST parcial
    if (parser.huboErroresSintacticos()) {
        std::cout << "\n--- AST parcial: el analisis sintactico reporto errores ---\n";
        std::cout << ast->toString() << "\n";
        return SALIDA_ERROR_SINTACTICO;
    }

    SemanticAnalyzer analizador(std::cerr);
    ResultadoSemantico resultado = analizador.analyze(
        *ast, ContextoEntrada{false, parser.huboErroresSintacticos()});
    if (!resultado.ok()) return SALIDA_ERROR_SEMANTICO;

    // --- Optimización del AST (solo llega aquí si el semántico no marcó errores) ---
    std::cout << "\n--- AST antes de optimizar ---\n" << ast->toString() << "\n";

    Optimizer optimizador;
    OptimizationStats stats = optimizador.optimize(*ast);

    std::cout << "\n--- AST optimizado ---\n" << ast->toString() << "\n";
    stats.dump(std::cout);

    std::cout << "\n--- AST generado  ---\n";
    std::cout << ast->toString() << "\n";

    //generacion de codigo intermedio 
    std::cout << "\n--- Codigo intermedio  ---\n";
    CodeGenerator generador;
    ast->accept(generador);
    for (const auto& cuadrupla : generador.getBuffer().getInstructions()) {
        std::cout << cuadrupla.print() << "\n";
    }

    return SALIDA_OK;
}
