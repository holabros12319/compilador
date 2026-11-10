#include "CodeGenerator.h"

#include <stdexcept>
#include <string>

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
    // El conjunto de opcodes definido por la especificación no incluye MOD.
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
    lastTemp = node.value;
}

void CodeGenerator::visit(const IdentifierExpr& node) {
    // El nombre del identificador se convierte en el temporal actual sin emitir cuádrupla.
    lastTemp = node.name;
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
    // Si hay inicializador, evaluarlo y emitir asignación.
    if (node.initializer) {
        node.initializer->accept(*this);
        buffer.emit(OpCode::ASSIGN, lastTemp, "", node.name);
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
}

// ---------- CodeGenerator: bloques y colecciones ----------

void CodeGenerator::visit(const ProgramNode& node) {
    for (const auto& stmt : node.declarations) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
}

void CodeGenerator::visit(const BlockStmt& node) {
    for (const auto& stmt : node.statements) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
}

// ---------- CodeGenerator: funciones y estructuras ----------

void CodeGenerator::visit(const StructDecl& node) {
    // Método intencionalmente vacío (declaración abstracta, sin TAC).
    (void)node;
}

void CodeGenerator::visit(const FunctionDecl& node) {
    // Etiqueta con el nombre de la función.
    buffer.emit(OpCode::LABEL, "", "", node.name);

    // Vincular parámetros.
    for (const auto& param : node.parameters) {
        buffer.emit(OpCode::POP_PARAM, "", "", param.name);
    }

    // Recorrer el cuerpo si existe.
    if (node.body) {
        node.body->accept(*this);
    }
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