#include <iostream>
#include "Lexer.hpp"
#include "Parser.hpp"

int main() {
    // Código de prueba en C++
    std::string codigoPrueba = R"(
        int main() {
            float pi = 3.1416;
            // Comentario de prueba
            if (pi > 3.0) {
                return 1;
            }
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
    //salida en pamtalla del AST
    std::cout << "\nParser en ejecucion...\n";
    Parser parser(tokens);
    
    try {
        auto ast = parser.parse();
        std::cout << "\n--- AST generado con exito ---\n";
        std::cout << ast->toString() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Fallo en el análisis sintáctico: " << e.what() << "\n";
    }

    return 0;
}