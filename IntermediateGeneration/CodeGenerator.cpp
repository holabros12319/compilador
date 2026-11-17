#include "CodeGenerator.h"

#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// ---------- Soporte para la fase 6 (codigo nativo) ----------
//
// El TAC clasico no conserva tipos ni ambitos. Para poder generar codigo
// maquina correcto este generador agrega, SIN cambiar los headers:
//   * literales con su forma original  ("texto", 'c', 42, 3.5)
//   * nombres unicos para variables con el mismo nombre en ambitos distintos
//     (x, x$1, x$2 ...) y para las que chocarian con temporales (t3 -> t3$0)
//   * anotaciones de tipo en campos que antes iban vacios (ver NativeBackend.cpp)
namespace {

struct EstadoAmbitos {
    std::vector<std::map<std::string, std::string>> ambitos{1};
    std::map<std::string, int> usos;         // veces que se declaro cada nombre (funcion actual)
    std::map<std::string, int> usosGlobales; // idem, solo globales
    void reiniciar() {
        ambitos.assign(1, {});
        usos.clear();
        usosGlobales.clear();
    }
};

EstadoAmbitos& estado() {
    static EstadoAmbitos e;
    return e;
}

bool pareceTemporal(const std::string& n) {
    if (n.size() < 2 || (n[0] != 't' && n[0] != 'L')) return false;
    for (std::size_t i = 1; i < n.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(n[i]))) return false;
    return true;
}

std::string declararNombre(const std::string& nombre) {
    EstadoAmbitos& e = estado();
    int k = e.usos[nombre]++;
    std::string unico = (k == 0 && !pareceTemporal(nombre)) ? nombre : nombre + "$" + std::to_string(k);
    e.ambitos.back()[nombre] = unico;
    if (e.ambitos.size() == 1) e.usosGlobales = e.usos;
    return unico;
}

std::string resolverNombre(const std::string& nombre) {
    const EstadoAmbitos& e = estado();
    for (auto it = e.ambitos.rbegin(); it != e.ambitos.rend(); ++it) {
        auto f = it->find(nombre);
        if (f != it->end()) return f->second;
    }
    return nombre;  // funcion, cout, cin...
}

std::string escaparLiteral(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        switch (c) {
            case '\\': r += "\\\\"; break;
            case '"': r += "\\\""; break;
            case '\'': r += "\\'"; break;
            case '\n': r += "\\n"; break;
            case '\t': r += "\\t"; break;
            case '\r': r += "\\r"; break;
            case '\0': r += "\\0"; break;
            default:
                if (c < 32 || c == 127) {
                    static const char* hex = "0123456789abcdef";
                    r += "\\x";
                    r += hex[c >> 4];
                    r += hex[c & 15];
                } else {
                    r += static_cast<char>(c);
                }
        }
    }
    return r;
}

}  // namespace

// ---------- Quadruple ----------

std::string Quadruple::print() const {
    // Representación legible: (op, arg1, arg2, result)
    std::string nombreOp;
    switch (op) {
        case OpCode::ADD: nombreOp = "ADD"; break;
        case OpCode::SUB: nombreOp = "SUB"; break;
        case OpCode::MUL: nombreOp = "MUL"; break;
        case OpCode::DIV: nombreOp = "DIV"; break;
        case OpCode::MOD: nombreOp = "MOD"; break;
        case OpCode::NEG: nombreOp = "NEG"; break;
        case OpCode::NOT: nombreOp = "NOT"; break;
        case OpCode::EQ:  nombreOp = "EQ";  break;
        case OpCode::NEQ: nombreOp = "NEQ"; break;
        case OpCode::LT:  nombreOp = "LT";  break;
        case OpCode::GT:  nombreOp = "GT";  break;
        case OpCode::LE:  nombreOp = "LE";  break;
        case OpCode::GE:  nombreOp = "GE";  break;
        case OpCode::AND: nombreOp = "AND"; break;
        case OpCode::OR:  nombreOp = "OR";  break;
        case OpCode::ASSIGN: nombreOp = "ASSIGN"; break;
        case OpCode::LABEL: nombreOp = "LABEL"; break;
        case OpCode::GOTO: nombreOp = "GOTO"; break;
        case OpCode::IF_FALSE_GOTO: nombreOp = "IF_FALSE_GOTO"; break;
        case OpCode::PARAM: nombreOp = "PARAM"; break;
        case OpCode::CALL: nombreOp = "CALL"; break;
        case OpCode::RETURN: nombreOp = "RETURN"; break;
        case OpCode::PRINT: nombreOp = "PRINT"; break;
        case OpCode::READ: nombreOp = "READ"; break;
        case OpCode::POP_PARAM: nombreOp = "POP_PARAM"; break;
        default: nombreOp = "UNKNOWN"; break;
    }
    return "(" + nombreOp + ", " + arg1 + ", " + arg2 + ", " + result + ")";
}

// ---------- CodeBuffer ----------

std::string CodeBuffer::newTemp() {
    return "t" + std::to_string(tempCount++);
}

std::string CodeBuffer::newLabel() {
    return "L" + std::to_string(labelCount++);
}

void CodeBuffer::emit(OpCode op,
                      const std::string& arg1,
                      const std::string& arg2,
                      const std::string& result) {
    instructions.push_back({op, arg1, arg2, result});
}

const std::vector<Quadruple>& CodeBuffer::getInstructions() const {
    return instructions;
}

// ---------- CodeGenerator: helpers privados ----------

OpCode CodeGenerator::mapStringToOpCode(const std::string& op) {
    // Mapeo de operadores binarios textuales a opcodes TAC.
    if (op == "+")  return OpCode::ADD;
    if (op == "-")  return OpCode::SUB;
    if (op == "*")  return OpCode::MUL;
    if (op == "/")  return OpCode::DIV;
    if (op == "%")  return OpCode::MOD;
    if (op == "==") return OpCode::EQ;
    if (op == "!=") return OpCode::NEQ;
    if (op == "<")  return OpCode::LT;
    if (op == ">")  return OpCode::GT;
    if (op == "<=") return OpCode::LE;
    if (op == ">=") return OpCode::GE;
    if (op == "&&") return OpCode::AND;
    if (op == "||") return OpCode::OR;
    if (op == "=")  return OpCode::ASSIGN;
    // Operador sin opcode equivalente en el IR.
    throw std::invalid_argument("Operador binario no soportado por el IR: " + op);
}

OpCode CodeGenerator::mapStringToUnaryOpCode(const std::string& op) {
    // Mapeo de operadores unarios textuales a opcodes TAC.
    if (op == "-") return OpCode::NEG;
    if (op == "!") return OpCode::NOT;
    // El conjunto de opcodes definido por la especificación no incluye unario '+'.
    throw std::invalid_argument("Operador unario no soportado por el IR: " + op);
}

// ---------- CodeGenerator: getter de búfer ----------

CodeBuffer& CodeGenerator::getBuffer() {
    return buffer;
}

const CodeBuffer& CodeGenerator::getBuffer() const {
    return buffer;
}

// ---------- CodeGenerator: nodos hoja ----------

void CodeGenerator::visit(const LiteralExpr& node) {
    // El valor literal se convierte en el temporal actual sin emitir cuádrupla.
    // Cadenas y caracteres conservan su forma ("..." / '.') para que el backend sepa su tipo.
    if (node.literalType == TipoToken::CADENA) lastTemp = "\"" + escaparLiteral(node.value) + "\"";
    else if (node.literalType == TipoToken::CARACTER) lastTemp = "'" + escaparLiteral(node.value) + "'";
    else lastTemp = node.value;
}

void CodeGenerator::visit(const IdentifierExpr& node) {
    // El nombre del identificador se convierte en el temporal actual sin emitir cuádrupla.
    lastTemp = resolverNombre(node.name);
}

// ---------- CodeGenerator: expresiones compuestas ----------

void CodeGenerator::visit(const BinaryExpr& node) {
    if (node.op == "=") {
        if (node.right) {
            node.right->accept(*this);
        }
        std::string der = lastTemp;
        if (node.left) {
            node.left->accept(*this);
        }
        std::string izq = lastTemp;
        buffer.emit(OpCode::ASSIGN, der, "", izq);
        lastTemp = izq;
        return;
    }

    if (node.op == "<<" || node.op == ">>") {
        if (node.left) {
            node.left->accept(*this);
        }
        if (node.right) {
            node.right->accept(*this);
        }
        std::string valor = lastTemp;
        if (node.op == "<<") {
            buffer.emit(OpCode::PRINT, valor, "", "");
        } else {
            buffer.emit(OpCode::READ, "", "", valor);
        }
        return;
    }

    // && y || con evaluacion en cortocircuito (como en C): el operando derecho
    // solo se evalua si hace falta.
    if (node.op == "&&" || node.op == "||") {
        std::string res = buffer.newTemp();
        std::string lFin = buffer.newLabel();
        buffer.emit(OpCode::ASSIGN, "", "@bool", res);
        if (node.left) node.left->accept(*this);
        buffer.emit(OpCode::NEQ, lastTemp, "0", res);
        if (node.op == "&&") {
            buffer.emit(OpCode::IF_FALSE_GOTO, res, "", lFin);
        } else {
            std::string neg = buffer.newTemp();
            buffer.emit(OpCode::NOT, res, "", neg);
            buffer.emit(OpCode::IF_FALSE_GOTO, neg, "", lFin);
        }
        if (node.right) node.right->accept(*this);
        buffer.emit(OpCode::NEQ, lastTemp, "0", res);
        buffer.emit(OpCode::LABEL, "", "", lFin);
        lastTemp = res;
        return;
    }

    // Visitar operando izquierdo y salvar resultado local.
    if (node.left) {
        node.left->accept(*this);
    }
    std::string izq = lastTemp;

    // Visitar operando derecho y salvar resultado local.
    if (node.right) {
        node.right->accept(*this);
    }
    std::string der = lastTemp;

    // Generar temporal destino y emitir la cuádrupla correspondiente.
    std::string target = buffer.newTemp();
    buffer.emit(mapStringToOpCode(node.op), izq, der, target);
    lastTemp = target;
}

void CodeGenerator::visit(const UnaryExpr& node) {
    // Visitar operando y salvar resultado local.
    if (node.operand) {
        node.operand->accept(*this);
    }
    std::string operando = lastTemp;

    // '+' unario: no cambia el valor, no requiere cuádrupla.
    if (node.op == "+") {
        lastTemp = operando;
        return;
    }

    // '++' / '--' (prefijo y postfijo): el IR no tiene opcode de incremento,
    // se baja a  t = operando +/- 1;  operando = t  con los opcodes existentes.
    if (node.op == "++" || node.op == "--") {
        std::string valorPrevio;
        if (!node.isPrefix) {
            // Postfijo: el valor de la expresión es el anterior a la modificación.
            valorPrevio = buffer.newTemp();
            buffer.emit(OpCode::ASSIGN, operando, "", valorPrevio);
        }
        std::string nuevo = buffer.newTemp();
        buffer.emit(node.op == "++" ? OpCode::ADD : OpCode::SUB, operando, "1", nuevo);
        buffer.emit(OpCode::ASSIGN, nuevo, "", operando);
        lastTemp = node.isPrefix ? operando : valorPrevio;
        return;
    }

    // Generar temporal destino y emitir la cuádrupla correspondiente.
    std::string target = buffer.newTemp();
    buffer.emit(mapStringToUnaryOpCode(node.op), operando, "", target);
    lastTemp = target;
}

void CodeGenerator::visit(const MemberExpr& node) {
    // Visitar el objeto base.
    if (node.object) {
        node.object->accept(*this);
    }
    // Colapsar el acceso de campo en un único identificador virtual (RF-06).
    lastTemp = lastTemp + (node.isPointer ? "->" : ".") + node.member;
}

// ---------- CodeGenerator: sentencias base ----------

void CodeGenerator::visit(const VarDecl& node) {
    // El inicializador se evalua antes de declarar el nombre (ve el ambito exterior).
    std::string valorInicial;
    if (node.initializer) {
        node.initializer->accept(*this);
        valorInicial = lastTemp;
    }
    // Declaracion tipada (la usa el backend para reservar espacio) + asignacion inicial.
    std::string unico = declararNombre(node.name);
    buffer.emit(OpCode::ASSIGN, "", "@" + node.type, unico);
    if (node.initializer) {
        buffer.emit(OpCode::ASSIGN, valorInicial, "", unico);
    }
}

void CodeGenerator::visit(const ExprStmt& node) {
    // Evaluar la expresión si existe.
    if (node.expression) {
        node.expression->accept(*this);
    }
}

void CodeGenerator::visit(const ReturnStmt& node) {
    // Evaluar el valor de retorno si existe.
    if (node.value) {
        node.value->accept(*this);
        buffer.emit(OpCode::RETURN, lastTemp, "", "");
    } else {
        buffer.emit(OpCode::RETURN, "", "", "");
    }
}

// ---------- CodeGenerator: control de flujo ----------

void CodeGenerator::visit(const IfStmt& node) {
    // Evaluar la condición.
    if (node.condition) {
        node.condition->accept(*this);
    }
    std::string cond = lastTemp;

    // Crear etiquetas según la existencia de la rama else.
    std::string lElse = (node.elseBranch != nullptr) ? buffer.newLabel() : "";
    std::string lEnd = buffer.newLabel();

    // Salto condicional.
    buffer.emit(OpCode::IF_FALSE_GOTO, cond, "",
                (node.elseBranch != nullptr) ? lElse : lEnd);

    // Rama then.
    if (node.thenBranch) {
        node.thenBranch->accept(*this);
    }

    // Rama else (si existe).
    if (node.elseBranch) {
        buffer.emit(OpCode::GOTO, "", "", lEnd);
        buffer.emit(OpCode::LABEL, "", "", lElse);
        node.elseBranch->accept(*this);
    }

    // Etiqueta de salida.
    buffer.emit(OpCode::LABEL, "", "", lEnd);
}

void CodeGenerator::visit(const WhileStmt& node) {
    std::string lStart = buffer.newLabel();
    std::string lEnd = buffer.newLabel();

    // Etiqueta de inicio del bucle.
    buffer.emit(OpCode::LABEL, "", "", lStart);

    // Evaluar condición y saltar si es falsa.
    if (node.condition) {
        node.condition->accept(*this);
    }
    buffer.emit(OpCode::IF_FALSE_GOTO, lastTemp, "", lEnd);

    // Cuerpo del bucle.
    if (node.body) {
        node.body->accept(*this);
    }

    // Volver al inicio y marcar salida.
    buffer.emit(OpCode::GOTO, "", "", lStart);
    buffer.emit(OpCode::LABEL, "", "", lEnd);
}

void CodeGenerator::visit(const ForStmt& node) {
    // La variable del for vive solo dentro del bucle.
    estado().ambitos.emplace_back();
    // Inicialización.
    if (node.init) {
        node.init->accept(*this);
    }

    std::string lStart = buffer.newLabel();
    std::string lEnd = buffer.newLabel();

    // Etiqueta de inicio del bucle.
    buffer.emit(OpCode::LABEL, "", "", lStart);

    // Condición (si existe).
    if (node.condition) {
        node.condition->accept(*this);
        buffer.emit(OpCode::IF_FALSE_GOTO, lastTemp, "", lEnd);
    }

    // Cuerpo del bucle.
    if (node.body) {
        node.body->accept(*this);
    }

    // Incremento (si existe).
    if (node.increment) {
        node.increment->accept(*this);
    }

    // Volver al inicio y marcar salida.
    buffer.emit(OpCode::GOTO, "", "", lStart);
    buffer.emit(OpCode::LABEL, "", "", lEnd);
    estado().ambitos.pop_back();
}

// ---------- CodeGenerator: bloques y colecciones ----------

void CodeGenerator::visit(const ProgramNode& node) {
    estado().reiniciar();
    for (const auto& stmt : node.declarations) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
}

void CodeGenerator::visit(const BlockStmt& node) {
    estado().ambitos.emplace_back();
    for (const auto& stmt : node.statements) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
    estado().ambitos.pop_back();
}

// ---------- CodeGenerator: funciones y estructuras ----------

void CodeGenerator::visit(const StructDecl& node) {
    // Sin codigo ejecutable, pero el backend necesita el layout: "campo:tipo;campo:tipo".
    std::string campos;
    for (const auto& campo : node.fields) {
        if (!campo) continue;
        if (!campos.empty()) campos += ";";
        campos += campo->name + ":" + campo->type;
    }
    buffer.emit(OpCode::ASSIGN, campos, "@struct", node.name);
}

void CodeGenerator::visit(const FunctionDecl& node) {
    // Un prototipo no genera codigo (antes dejaba una etiqueta sin cuerpo).
    if (!node.body) return;

    EstadoAmbitos& e = estado();
    e.usos = e.usosGlobales;       // cada funcion numera sus propios nombres
    e.ambitos.emplace_back();      // ambito de la funcion (parametros)

    // Etiqueta con el nombre de la función; arg1 "@tipo" marca el inicio y el tipo de retorno.
    buffer.emit(OpCode::LABEL, "@" + node.returnType, "", node.name);

    // Vincular parámetros (con su tipo).
    for (const auto& param : node.parameters) {
        std::string unico = declararNombre(param.name);
        buffer.emit(OpCode::POP_PARAM, "", "@" + param.type, unico);
    }

    // Recorrer el cuerpo.
    node.body->accept(*this);

    // Marca de fin de funcion.
    buffer.emit(OpCode::LABEL, "@end", "", node.name);
    e.ambitos.pop_back();
}

void CodeGenerator::visit(const CallExpr& node) {
    // Evaluar el callee y guardar su nombre.
    if (node.callee) {
        node.callee->accept(*this);
    }
    std::string calleeName = lastTemp;

    // Evaluar argumentos y emitir PARAM por cada uno.
    int argCount = 0;
    for (const auto& arg : node.arguments) {
        if (arg) {
            arg->accept(*this);
            buffer.emit(OpCode::PARAM, lastTemp, "", "");
            ++argCount;
        }
    }

    // Generar temporal destino y emitir CALL.
    std::string target = buffer.newTemp();
    buffer.emit(OpCode::CALL, calleeName, std::to_string(argCount), target);
    lastTemp = target;
}