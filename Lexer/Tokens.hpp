#ifndef TOKENS_HPP
#define TOKENS_HPP

#include <string>
#include <ostream>
#include <utility>

enum class TipoToken{
    IDENTIFICADOR,
    NUMERO,
    CADENA,
    PALABRA_RESERVADA,
    OPERADOR,
    DELIMITADOR,
    FIN_ARCHIVO,
    DESCONOCIDO,
    CARACTER,
    ERROR_LEXICO
};


inline std::string tipoToString(TipoToken t) {
    switch (t) {
    case TipoToken::IDENTIFICADOR:     return "IDENTIFICADOR";
    case TipoToken::NUMERO:            return "NUMERO";
    case TipoToken::CADENA:            return "CADENA";
    case TipoToken::PALABRA_RESERVADA: return "PALABRA_RESERVADA";
    case TipoToken::OPERADOR:          return "OPERADOR";
    case TipoToken::DELIMITADOR:       return "DELIMITADOR";
    case TipoToken::FIN_ARCHIVO:       return "FIN_ARCHIVO";
    case TipoToken::CARACTER:          return "CARACTER";
    case TipoToken::ERROR_LEXICO:      return "ERROR_LEXICO";
    default:                           return "DESCONOCIDO";
    }
}

struct Ubicacion{
    int linea;
    int columna;
};

struct Token{
    TipoToken tipo;
    std::string lexema;
    Ubicacion ubicacion;

    Token(TipoToken t = TipoToken :: DESCONOCIDO,
            std::string lex = "",
            int linea = 1,
            int columna = 1)
            :tipo(t), lexema(std::move(lex)), ubicacion{linea, columna}{}
};

inline std::ostream& operator<<(std::ostream& os, const Token& t){
    os << "<" << tipoToString(t.tipo) << ", \"" << t.lexema << "\", "
        << t.ubicacion.linea << ":" << t.ubicacion.columna << ">";
    return os;
}

#endif