#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "AST.hpp"

#include <cstddef>
#include <iostream>
#include <iosfwd>
#include <string>
#include <map>
#include <vector>

struct ContextoEntrada {
    bool erroresLexicos = false;
    bool erroresSintacticos = false;
};

enum class Severidad { Error, Advertencia };

struct Diagnostico {
    std::string codigo;
    Severidad severidad;
    std::string mensaje;
    Ubicacion ubicacion;
    std::size_t secuencia = 0;
};

enum class TipoBase { Int, Float, Double, Char, Bool, Void, Struct, Cadena, Error };

struct Tipo {
    TipoBase base = TipoBase::Error;
    const StructDecl* estructura = nullptr;
};

class TablaSimbolos {
public:
    void dumpTabla(std::ostream& salida) const;

    // Estado expuesto solo para la implementación de la fase. No contiene
    // propietarios del AST; los símbolos guardan observadores.
    struct Ambito {
        struct Simbolo {
            enum class Categoria { Variable, Funcion, Estructura };
            std::string nombre;
            Categoria categoria;
            Tipo tipo;
            std::vector<Tipo> parametros;
            const Decl* declaracion = nullptr;
            const FunctionDecl* prototipo = nullptr;
            const FunctionDecl* definicion = nullptr;
            bool usado = false;
            bool parametro = false;
            std::size_t ordenGlobal = 0;
        };
        std::string etiqueta;
        int padre = -1;
        std::map<std::string, Simbolo> simbolos;
        std::vector<std::string> ordenSimbolos;
    };
    std::vector<Ambito> ambitos_;
};

class ResultadoSemantico {
public:
    bool ok() const;
    const std::vector<Diagnostico>& diagnosticos() const;
    std::size_t errores() const;
    std::size_t advertencias() const;
    const TablaSimbolos& tabla() const;
    void dumpTabla(std::ostream& salida) const;

private:
    std::vector<Diagnostico> diagnosticos_;
    TablaSimbolos tabla_;

    friend class SemanticAnalyzer;
};

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(std::ostream& salida = std::cerr,
                              std::size_t limiteErrores = 100);
    ResultadoSemantico analyze(const ProgramNode& programa,
                               const ContextoEntrada& contexto);

private:
    std::ostream& salida_;
    std::size_t limiteErrores_;
};

#endif
