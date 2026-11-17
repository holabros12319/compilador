#include "Optimizer.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Semantic.hpp"
#include "CodeGenerator.h"
#include "NativeBackend.hpp"
#include "Toolchain.hpp"
namespace {


    const int SALIDA_OK = 0;
    const int SALIDA_ERROR_LEXICO = 1;
    const int SALIDA_ERROR_SINTACTICO = 2;
    const int SALIDA_ERROR_SEMANTICO = 3;
    const int SALIDA_ERROR_ARCHIVO = 4;
    const int SALIDA_ERROR_GENERACION = 5;
    const int SALIDA_ERROR_ENSAMBLADO = 6;
    const int SALIDA_ERROR_USO = 7;

    // Opciones de linea de comandos de la fase 6.
    struct OpcionesCli {
        std::string fuente;                       // archivo .c (vacio = programa incrustado)
        std::string salida;                       // -o: ejecutable de salida
        std::string cc;                           // --cc: gcc/clang a usar para ensamblar y enlazar
        bool soloAsm = false;                     // --asm-only: no ensamblar ni enlazar
        bool ejecutar = false;                    // --run: ejecutar el programa generado
        TargetOS objetivo = defaultTargetOS();    // --target windows|linux
    };

    void mostrarUso() {
        std::cerr << "Uso: compiler_app [archivo_fuente] [-o salida] [--run] [--asm-only]\n"
                     "                  [--target windows|linux] [--cc ruta/al/gcc]\n";
    }

    bool leerOpciones(int argc, char* argv[], OpcionesCli& op) {
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            auto valor = [&](std::string& destino) {
                if (i + 1 >= argc) return false;
                destino = argv[++i];
                return true;
            };
            if (a == "-o") { if (!valor(op.salida)) return false; }
            else if (a == "--cc") { if (!valor(op.cc)) return false; }
            else if (a == "--run") op.ejecutar = true;
            else if (a == "--asm-only") op.soloAsm = true;
            else if (a == "--target") {
                std::string t;
                if (!valor(t)) return false;
                if (t == "windows" || t == "win64") op.objetivo = TargetOS::Windows;
                else if (t == "linux") op.objetivo = TargetOS::Linux;
                else return false;
            }
            else if (!a.empty() && a[0] == '-') return false;
            else if (op.fuente.empty()) op.fuente = a;
            else return false;
        }
        return true;
    }

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

    // Lee un archivo completo a un string. Devuelve false si no se pudo abrir.
    bool leerArchivo(const std::string& ruta, std::string& destino) {
        std::ifstream archivo(ruta, std::ios::in | std::ios::binary);
        if (!archivo) return false;
        std::ostringstream buffer;
        buffer << archivo.rdbuf();
        destino = buffer.str();
        return true;
    }

} // namespace

// Uso: compiler_app [archivo_fuente]
//  - Sin argumentos compila el programa de prueba incrustado.
//  - Con un argumento compila el archivo indicado.
int main(int argc, char* argv[]) {
    OpcionesCli opciones;
    if (!leerOpciones(argc, argv, opciones)) {
        mostrarUso();
        return SALIDA_ERROR_USO;
    }

    // Código de prueba por defecto
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

    if (!opciones.fuente.empty()) {
        if (!leerArchivo(opciones.fuente, codigoPrueba)) {
            std::cerr << "No se pudo abrir el archivo: " << opciones.fuente << "\n";
            return SALIDA_ERROR_ARCHIVO;
        }
    }

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

    //generacion de codigo intermedio 
    std::cout << "\n--- Codigo intermedio  ---\n";
    CodeGenerator generador;
    try {
        ast->accept(generador);
    } catch (const std::exception& e) {
        std::cerr << "Fallo en la generacion de codigo intermedio: " << e.what() << "\n";
        return SALIDA_ERROR_GENERACION;
    }
    for (const auto& cuadrupla : generador.getBuffer().getInstructions()) {
        std::cout << cuadrupla.print() << "\n";
    }

    // --- Fase 6: codigo nativo x86-64 ---
    const bool paraWindows = opciones.objetivo == TargetOS::Windows;
    std::cout << "\n--- Codigo nativo x86-64 (" << (paraWindows ? "Windows" : "Linux") << ") ---\n";

    namespace fs = std::filesystem;
    std::string rutaExe = opciones.salida;
    if (rutaExe.empty()) {
        rutaExe = opciones.fuente.empty() ? "programa" : fs::path(opciones.fuente).stem().string();
    }
    if (paraWindows && fs::path(rutaExe).extension() != ".exe") rutaExe += ".exe";
    const std::string rutaAsm = fs::path(rutaExe).replace_extension(".s").string();

    std::string ensamblador;
    try {
        NativeBackend backend(opciones.objetivo);
        ensamblador = backend.generate(generador.getBuffer().getInstructions());
    } catch (const std::exception& e) {
        std::cerr << "Fallo en la generacion de codigo nativo: " << e.what() << "\n";
        return SALIDA_ERROR_GENERACION;
    }

    {
        // En binario para conservar los fin de linea '\n' (GAS no necesita CRLF).
        std::ofstream archivoAsm(rutaAsm, std::ios::out | std::ios::binary);
        if (!archivoAsm || !(archivoAsm << ensamblador)) {
            std::cerr << "No se pudo escribir el ensamblador: " << rutaAsm << "\n";
            return SALIDA_ERROR_ARCHIVO;
        }
    }
    std::cout << "Ensamblador generado: " << rutaAsm << "\n";

    bool soloAsm = opciones.soloAsm;
    if (opciones.objetivo != defaultTargetOS() && !soloAsm) {
        std::cout << "El objetivo no coincide con este sistema: solo se genera el .s (no se enlaza).\n";
        soloAsm = true;
    }
    if (soloAsm) return SALIDA_OK;

    BuildResult build = buildExecutable(rutaAsm, rutaExe, opciones.cc);
    if (!build.ok) {
        std::cerr << build.message << "\n";
        return SALIDA_ERROR_ENSAMBLADO;
    }
    std::cout << "Ejecutable generado: " << rutaExe << "  (enlazado con " << build.compiler << ")\n";

    if (opciones.ejecutar) {
        std::cout << "\n--- Ejecucion ---\n" << std::flush;
        int codigo = runExecutable(rutaExe);
        std::cout << "\n[El programa termino con codigo " << codigo << "]\n";
        if (codigo == -1) return SALIDA_ERROR_ENSAMBLADO;
    }

    return SALIDA_OK;
}
