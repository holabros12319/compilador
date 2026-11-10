# ESPECIFICACIÓN TÉCNICA DE IMPLEMENTACIÓN (specs.md)

**Fase:** Generación de Código Intermedio TAC (Three-Address Code).
**Módulo:** `IntermediateGeneration/`.
**Archivos fuente a crear:** `IntermediateCode.h`, `CodeGenerator.h`, `CodeGenerator.cpp`.
**Fuentes de verdad:** `Parser/AST.hpp`, `Lexer/Tokens.hpp`, `AGENTS.md`, `requirements.md`.

> Esta especificación define la estructura y los patrones lógicos obligatorios. Las firmas exactas de los métodos auxiliares (por ejemplo `emit`) se concretan en la implementación siguiendo esta estructura, sin añadir métodos, atributos ni clases no contemplados aquí.

---

## 1. Archivo `IntermediateCode.h`

Estructuras base:

- `enum class OpCode`:
  `ADD, SUB, MUL, DIV, NEG, NOT, EQ, NEQ, LT, GT, LE, GE, AND, OR, ASSIGN, LABEL, GOTO, IF_FALSE_GOTO, PARAM, CALL, RETURN`.
- `struct Quadruple`:
  - Atributos: `OpCode op`, `std::string arg1`, `std::string arg2`, `std::string result`.
  - Incluir método `print()`.
- `class CodeBuffer`:
  - Privados: `std::vector<Quadruple> instructions`, `int tempCount = 0`, `int labelCount = 0`.
  - Públicos: `newTemp()`, `newLabel()`, `emit(...)`, `getInstructions()`.

### 1.1 Patrón lógico de `CodeBuffer`

- `newTemp()` devuelve un temporal único con el formato `t` + contador autoincremental (`t0`, `t1`, `t2`, ...). Incrementa `tempCount`.
- `newLabel()` devuelve una etiqueta única con el formato `L` + contador autoincremental (`L0`, `L1`, `L2`, ...). Incrementa `labelCount`.
- `emit(...)` añade una `Quadruple` a `instructions`.
- `getInstructions()` devuelve la lista plana de cuádruplas emitidas.

Guardas de inclusión con `#ifndef` / `#define` / `#endif`. Cabeceras estándar autorizadas: `<string>`, `<vector>`.

---

## 2. Archivo `CodeGenerator.h`

Declaración de la clase del backend:

- `class CodeGenerator : public ASTVisitor`
- Privados:
  - `CodeBuffer buffer`
  - `std::string lastTemp`
- Helpers privados:
  - `OpCode mapStringToOpCode(const std::string& op);` (Mapea operadores binarios).
  - `OpCode mapStringToUnaryOpCode(const std::string& op);` (Mapea operadores unarios).
- Públicos:
  - Getter de `CodeBuffer`.
  - Los 16 métodos `visit(const NodeType& node) override;`.

### 2.1 Patrón lógico del header

- Incluir `IntermediateCode.h` y `Parser/AST.hpp`.
- Cabeceras estándar autorizadas: `<string>`, `<memory>`, `<vector>` (transitivas de las fuentes de verdad).
- Los 16 métodos `visit` corresponden a: `ProgramNode`, `FunctionDecl`, `StructDecl`, `VarDecl`, `BlockStmt`, `IfStmt`, `WhileStmt`, `ForStmt`, `ReturnStmt`, `ExprStmt`, `LiteralExpr`, `IdentifierExpr`, `UnaryExpr`, `BinaryExpr`, `CallExpr`, `MemberExpr`.
- Guardas de inclusión con `#ifndef` / `#define` / `#endif`.

---

## 3. Archivo `CodeGenerator.cpp` (Patrones Lógicos por Nodo)

Aplicar estrictamente las siguientes lógicas en la implementación.

### 3.1 Nodos Hoja (`LiteralExpr`, `IdentifierExpr`)

- Asignar a `lastTemp = node.value` (o `node.name`).
- No emitir cuádruplas.

```cpp
void CodeGenerator::visit(const LiteralExpr& node) {
    lastTemp = node.value;
}

void CodeGenerator::visit(const IdentifierExpr& node) {
    lastTemp = node.name;
}
```

### 3.2 Expresiones Compuestas (`BinaryExpr`, `UnaryExpr`, `MemberExpr`)

- `BinaryExpr`:
  1. Visitar `left` y salvar el resultado local `izq = lastTemp`.
  2. Visitar `right` y salvar el resultado local `der = lastTemp`.
  3. Generar `target = buffer.newTemp()`.
  4. Emitir cuádrupla con el código de `mapStringToOpCode(node.op)`, argumentos `izq` y `der`, resultado `target`.
  5. Actualizar `lastTemp = target`.

- `UnaryExpr`:
  1. Visitar `operand` y salvar el resultado local.
  2. Generar `target = buffer.newTemp()`.
  3. Emitir cuádrupla con `mapStringToUnaryOpCode(node.op)`.
  4. Actualizar `lastTemp = target`.

- `MemberExpr`:
  1. Visitar `object`.
  2. Concatenar el resultado con la propiedad: `lastTemp = lastTemp + (node.isPointer ? "->" : ".") + node.member`.
  3. No emitir cuádrupla.

```cpp
void CodeGenerator::visit(const BinaryExpr& node) {
    if (node.left) node.left->accept(*this);
    std::string izq = lastTemp;
    if (node.right) node.right->accept(*this);
    std::string der = lastTemp;
    std::string target = buffer.newTemp();
    buffer.emit(mapStringToOpCode(node.op), izq, der, target);
    lastTemp = target;
}

void CodeGenerator::visit(const UnaryExpr& node) {
    if (node.operand) node.operand->accept(*this);
    std::string operando = lastTemp;
    std::string target = buffer.newTemp();
    buffer.emit(mapStringToUnaryOpCode(node.op), operando, "", target);
    lastTemp = target;
}

void CodeGenerator::visit(const MemberExpr& node) {
    if (node.object) node.object->accept(*this);
    lastTemp = lastTemp + (node.isPointer ? "->" : ".") + node.member;
}
```

### 3.3 Sentencias Base (`VarDecl`, `ExprStmt`, `ReturnStmt`)

- `VarDecl`:
  1. Si `node.initializer != nullptr`, visitarlo.
  2. Emitir `ASSIGN` con `lastTemp` hacia `node.name`.

- `ExprStmt`:
  - Si `node.expression != nullptr`, invocar `accept`.

- `ReturnStmt`:
  1. Si `node.value != nullptr`, invocar `accept`.
  2. Emitir `RETURN` con `lastTemp`.

```cpp
void CodeGenerator::visit(const VarDecl& node) {
    if (node.initializer) node.initializer->accept(*this);
    buffer.emit(OpCode::ASSIGN, lastTemp, "", node.name);
}

void CodeGenerator::visit(const ExprStmt& node) {
    if (node.expression) node.expression->accept(*this);
}

void CodeGenerator::visit(const ReturnStmt& node) {
    if (node.value) node.value->accept(*this);
    buffer.emit(OpCode::RETURN, lastTemp, "", "");
}
```

### 3.4 Control de Flujo (`IfStmt`, `WhileStmt`, `ForStmt`)

- `IfStmt`:
  1. Visitar `condition`.
  2. Crear `L_else` (si `elseBranch != nullptr`) y `L_end`.
  3. Emitir `IF_FALSE_GOTO` evaluando `lastTemp`.
  4. Visitar `thenBranch`.
  5. Si hay `elseBranch`: emitir `GOTO L_end`, `LABEL L_else`, visitar `elseBranch`.
  6. Finalmente emitir `LABEL L_end`.

- `WhileStmt`:
  1. Crear `L_start` y `L_end`.
  2. Emitir `LABEL L_start`.
  3. Visitar `condition`.
  4. Emitir `IF_FALSE_GOTO L_end`.
  5. Visitar `body`.
  6. Emitir `GOTO L_start`.
  7. Emitir `LABEL L_end`.

- `ForStmt`:
  1. Si `init` existe, visitarlo.
  2. Emitir `LABEL L_start`.
  3. Si `condition` existe, visitarlo y emitir `IF_FALSE_GOTO L_end`.
  4. Visitar `body`.
  5. Si `increment` existe, visitarlo.
  6. Emitir `GOTO L_start`.
  7. Finalmente emitir `LABEL L_end`.

```cpp
void CodeGenerator::visit(const IfStmt& node) {
    if (node.condition) node.condition->accept(*this);
    std::string cond = lastTemp;
    std::string lElse = (node.elseBranch != nullptr) ? buffer.newLabel() : "";
    std::string lEnd = buffer.newLabel();
    buffer.emit(OpCode::IF_FALSE_GOTO, cond, "", (node.elseBranch != nullptr) ? lElse : lEnd);
    if (node.thenBranch) node.thenBranch->accept(*this);
    if (node.elseBranch) {
        buffer.emit(OpCode::GOTO, "", "", lEnd);
        buffer.emit(OpCode::LABEL, "", "", lElse);
        node.elseBranch->accept(*this);
    }
    buffer.emit(OpCode::LABEL, "", "", lEnd);
}

void CodeGenerator::visit(const WhileStmt& node) {
    std::string lStart = buffer.newLabel();
    std::string lEnd = buffer.newLabel();
    buffer.emit(OpCode::LABEL, "", "", lStart);
    if (node.condition) node.condition->accept(*this);
    buffer.emit(OpCode::IF_FALSE_GOTO, lastTemp, "", lEnd);
    if (node.body) node.body->accept(*this);
    buffer.emit(OpCode::GOTO, "", "", lStart);
    buffer.emit(OpCode::LABEL, "", "", lEnd);
}

void CodeGenerator::visit(const ForStmt& node) {
    if (node.init) node.init->accept(*this);
    std::string lStart = buffer.newLabel();
    std::string lEnd = buffer.newLabel();
    buffer.emit(OpCode::LABEL, "", "", lStart);
    if (node.condition) {
        node.condition->accept(*this);
        buffer.emit(OpCode::IF_FALSE_GOTO, lastTemp, "", lEnd);
    }
    if (node.body) node.body->accept(*this);
    if (node.increment) node.increment->accept(*this);
    buffer.emit(OpCode::GOTO, "", "", lStart);
    buffer.emit(OpCode::LABEL, "", "", lEnd);
}
```

### 3.5 Bloques y Colecciones (`ProgramNode`, `BlockStmt`)

- Recorrer los vectores `declarations` o `statements`.
- Invocar `accept` validando contra `nullptr`.

```cpp
void CodeGenerator::visit(const ProgramNode& node) {
    for (const auto& stmt : node.declarations) {
        if (stmt) stmt->accept(*this);
    }
}

void CodeGenerator::visit(const BlockStmt& node) {
    for (const auto& stmt : node.statements) {
        if (stmt) stmt->accept(*this);
    }
}
```

### 3.6 Funciones y Estructuras (`FunctionDecl`, `StructDecl`, `CallExpr`)

- `StructDecl`: Dejar el método vacío.
- `FunctionDecl`:
  1. Emitir `LABEL` con `node.name`.
  2. Ignorar `node.parameters`.
  3. Si `body` existe, visitarlo.
- `CallExpr`:
  1. Visitar `callee` y guardar `calleeName = lastTemp`.
  2. Recorrer `arguments`, visitarlos y emitir `PARAM lastTemp` por cada uno.
  3. Generar `target = buffer.newTemp()`.
  4. Emitir `CALL calleeName, arg_count, target`.
  5. Actualizar `lastTemp = target`.

```cpp
void CodeGenerator::visit(const StructDecl& node) {
    // Método intencionalmente vacío.
}

void CodeGenerator::visit(const FunctionDecl& node) {
    buffer.emit(OpCode::LABEL, "", "", node.name);
    if (node.body) node.body->accept(*this);
}

void CodeGenerator::visit(const CallExpr& node) {
    if (node.callee) node.callee->accept(*this);
    std::string calleeName = lastTemp;
    int argCount = 0;
    for (const auto& arg : node.arguments) {
        if (arg) {
            arg->accept(*this);
            buffer.emit(OpCode::PARAM, lastTemp, "", "");
            ++argCount;
        }
    }
    std::string target = buffer.newTemp();
    buffer.emit(OpCode::CALL, calleeName, std::to_string(argCount), target);
    lastTemp = target;
}
```

### 3.7 Nodos restantes (`ExprStmt`, `IfStmt`, `WhileStmt`, `ForStmt`, `ReturnStmt`, `VarDecl`) y `BlockStmt`

Cubiertos en las secciones anteriores. Los métodos `visit` de `ExprStmt`, `IfStmt`, `WhileStmt`, `ForStmt`, `ReturnStmt` y `VarDecl` se implementan según los patrones de las secciones 3.3 y 3.4.

---

## Notas de Cumplimiento

- Toda visita a una `Expr` deposita su resultado en `lastTemp` (INV-1).
- Ningún hijo recibe `accept()` sin validación previa contra `nullptr` (INV-2).
- Todo el estado mutable (`buffer`, `lastTemp`, contadores) vive en la instancia local de `CodeGenerator` (INV-3).
- La generación se realiza en un único recorrido en profundidad del AST (RNF-01).
- El AST se recibe por referencia constante y no se modifica (RNF-02).