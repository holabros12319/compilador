# REQUISITOS, INVARIANTES Y CONTRATOS (requirements.md)

**Proyecto:** `Compiler` — fase de Generación de Código Intermedio TAC (Three-Address Code).
**Lenguaje:** C++17.
**Ámbito:** módulo `IntermediateGeneration/`.
**Fuentes de verdad:** `Parser/AST.hpp`, `Lexer/Tokens.hpp` y este documento.
**Estado:** normativo para los requisitos, invariantes y contratos de la fase.

> **Precedencia:** `AGENTS.md` es normativo para el proceso, los permisos y la calidad del código. Este documento es normativo para el comportamiento funcional, los invariantes y los contratos de interfaz de la fase. Ante una contradicción, prevalece la restricción más estricta.

---

## 1. Requisitos Funcionales (RF)

### RF-01 — Implementación del Visitor

- **Enunciado:** El módulo debe heredar de `ASTVisitor` e implementar los **16 métodos `visit(...)`** obligatorios declarados en `AST.hpp`.
- **Alcance:** `ProgramNode`, `FunctionDecl`, `StructDecl`, `VarDecl`, `BlockStmt`, `IfStmt`, `WhileStmt`, `ForStmt`, `ReturnStmt`, `ExprStmt`, `LiteralExpr`, `IdentifierExpr`, `UnaryExpr`, `BinaryExpr`, `CallExpr` y `MemberExpr`.
- **Verificación:** la clase generadora compila como subclase concreta de `ASTVisitor`; no queda ningún método puro sin implementar; no se usa RTTI (`dynamic_cast`/`typeid`) para despachar nodos.

### RF-02 — Generación Lineal TAC

- **Enunciado:** Convertir el árbol de expresiones en una **lista plana de cuádruplas** `(OpCode, arg1, arg2, result)`.
- **Cobertura mínima:** operaciones aritméticas, relacionales y lógicas.
- **Verificación:** cada expresión visitada produce una o más cuádruplas en la lista; las operaciones aritméticas, relacionales y lógicas tienen una representación TAC verificable; el orden de emisión respeta la evaluación de operandos.

### RF-03 — Administración de Registros Virtuales

- **Enunciado:** Proveer un mecanismo seguro para generar **temporales únicos autoincrementales** (`t0`, `t1`, `t2`, ...).
- **Verificación:** dos temporales generados en la misma ejecución nunca comparten nombre; el contador es monótono y no retrocede; no se reutilizan temporales ya emitidos.

### RF-04 — Administración de Etiquetas de Salto

- **Enunciado:** Proveer un mecanismo para generar **etiquetas de bloque únicas** (`L0`, `L1`, `L2`, ...).
- **Verificación:** dos etiquetas generadas en la misma ejecución nunca comparten nombre; el contador es monótono; cada salto emitido referencia una etiqueta existente o generada por el mismo mecanismo.

### RF-05 — Manejo de Funciones

- **Enunciado:** Emitir un `LABEL` al inicio de un `FunctionDecl`. Emitir instrucciones `PARAM` seguidas de `CALL` para nodos `CallExpr`.
- **Verificación:**
  - La visita a un `FunctionDecl` emite exactamente un `LABEL` con el nombre de la función antes de recorrer su cuerpo.
  - La visita a un `CallExpr` emite un `PARAM` por cada argumento y, a continuación, una instrucción `CALL`.
  - El resultado de la llamada queda disponible en `lastTemp` como un temporal válido.

### RF-06 — Resolución de Miembros

- **Enunciado:** Simplificar `MemberExpr` **colapsando el acceso de campos** (`.` o `->`) en un **único identificador virtual tipo `std::string`**.
- **Verificación:** la visita a un `MemberExpr` no emite una cuádrupla de acceso a miembro; produce un único nombre virtual que representa la ruta completa del miembro y lo deja en `lastTemp`.

---

## 2. Requisitos No Funcionales (RNF)

### RNF-01 — Single-pass IR

- **Enunciado:** La generación de código intermedio debe lograrse en un **único recorrido en profundidad** del AST, con complejidad **O(N)** en el número de nodos.
- **Verificación:** cada nodo alcanzable se visita como máximo una vez; el número de cuádruplas emitidas es lineal respecto al tamaño del AST; no existen pasadas posteriores de reconstrucción o normalización del árbol.

### RNF-02 — Inmutabilidad

- **Enunciado:** La fase de generación de código **no debe alterar** la estructura, los datos ni los punteros del AST en memoria.
- **Verificación:** el AST se recibe por referencia constante; la representación `toString()` del AST es idéntica antes y después de la generación; ningún `unique_ptr` del AST se mueve, reasigna ni libera; no se añaden ni eliminan nodos.

---

## 3. Invariantes del Sistema

### INV-1 — Invariante de Paso de Estado

- **Enunciado:** Toda visita a una clase derivada de `Expr` **DEBE** depositar el nombre de su variable temporal final (o su valor literal equivalente) en la variable de clase `std::string lastTemp`.
- **Aplicación:** `LiteralExpr`, `IdentifierExpr`, `UnaryExpr`, `BinaryExpr`, `CallExpr` y `MemberExpr`.
- **Verificación:** tras cada `visit` de una `Expr`, `lastTemp` contiene un valor no vacío y coherente con la expresión evaluada. Ninguna `Expr` deja `lastTemp` sin actualizar.

### INV-2 — Invariante Null-Safe

- **Enunciado:** Ningún hijo del AST debe recibir la llamada `accept()` sin antes haber verificado `!= nullptr`.
- **Aplicación:** obligatorio para todos los `std::unique_ptr` y aliases `ExprPtr`, `StmtPtr`, `DeclPtr`, `std::unique_ptr<BlockStmt>` y `std::unique_ptr<VarDecl>`.
- **Verificación:** todo acceso a un hijo va precedido por una comprobación de nulidad; los nodos opcionales (`elseBranch`, `initializer`, `increment`, `value`, `body`, etc.) se tratan como ausentes sin desreferenciar.

### INV-3 — Invariante de Aislamiento

- **Enunciado:** Todo el estado mutable vivirá dentro de la **instancia local de `CodeGenerator`**, sin dependencias globales.
- **Verificación:** no existen variables globales mutables ni singletons; dos instancias de `CodeGenerator` sobre el mismo AST producen resultados independientes y deterministas; el estado (temporales, etiquetas, cuádruplas, `lastTemp`) pertenece a la instancia.

---

## 4. Contratos de Interfaz Clave

### Contrato: `visit(const FunctionDecl& node)`

- **Precondición:** `node.name` no es nulo (identificador válido y no vacío).
- **Poscondición:** se emite un `LABEL` con el nombre de la función. Los parámetros de `node.parameters` (tipo `Param`) se ignoran en la emisión de TAC por tratarse de declaraciones abstractas. Si `node.body` existe, se recorre.
- **Invariantes aplicables:** `INV-2` (validar `node.body` antes de `accept()`) e `INV-3` (todo el estado permanece en la instancia).
- **Verificación:** para un `FunctionDecl` con cuerpo, la primera instrucción emitida para la función es su `LABEL`, seguida de las instrucciones del cuerpo; para un prototipo (`body == nullptr`) solo se emite el `LABEL` y no se produce ningún acceso nulo.
