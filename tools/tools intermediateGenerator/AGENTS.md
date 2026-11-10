# AGENTS.md — Generación de Código Intermedio

> **Ámbito:** este documento es normativo para el proceso, la gobernanza, el stack y la calidad del código de la fase de **Generación de Código Intermedio** del compilador.
>
> **Precedencia:** las reglas generales del proyecto (`tools/AGENTS.md`) siguen vigentes en su totalidad. En caso de conflicto, `tools/AGENTS.md` prevalece en permisos sobre archivos existentes y organización del proyecto; este documento prevalece en reglas específicas de la fase de Generación de Código Intermedio.

---

## 0. Fuentes de verdad autorizadas

Todo el trabajo de esta fase se basa **única y exclusivamente** en:

1. El código C++ provisto: `Lexer/Tokens.hpp` y `Parser/AST.hpp`.
2. Las especificaciones locales de la fase, si existen en esta carpeta o en `tools/` (por ejemplo `specs.md`).
3. El código existente del proyecto, **leído directamente**, nunca asumido.

**Prohibido:**

- Inventar métodos, atributos, clases, firmas, tipos de datos o comportamientos no especificados explícitamente en el AST (`AST.hpp`) o en los archivos de especificación locales.
- Suponer qué hace el código existente sin verificarlo por inspección directa.
- Escribir código sin haberlo verificado antes.

Si una respuesta no puede sustentarse con evidencia del código o de la especificación, el trabajo se detiene y se reporta como **Desconocido** o **Probable** (según `tools/AGENTS.md` §4), en lugar de implementar sobre una suposición.

---

## 1. Stack Tecnológico Estricto

- **Lenguaje:** estándar **C++17** (`-std=c++17` / `/std:c++17`), sin extensiones de compilador.
- **Librerías autorizadas:** únicamente la biblioteca estándar (STL). **Prohibición total** de librerías externas o dependencias de terceros.
- **Cabeceras estándar autorizadas explícitamente:**
  - `<string>`, `<vector>`, `<memory>`, `<iostream>`, `<stdexcept>`, `<algorithm>`
- **Cabeceras transitivas autorizadas:** las ya utilizadas por las fuentes de verdad (`Tokens.hpp`, `AST.hpp`), por ejemplo `<ostream>`, `<utility>`. No se añaden cabeceras estándar adicionales sin necesidad real y verificable.
- **Sin APIs de plataforma:** no se usan APIs de Windows ni POSIX en el módulo. La E/S se inyecta mediante `std::ostream&`.
- **Sin RTTI:** prohibido `dynamic_cast` y `typeid`. El despacho se realiza mediante el patrón `ASTVisitor` y/o `NodeKind`.

---

## 2. Reglas Directivas de Código

### 2.1 Prohibición de invención (Cero Alucinaciones)

Jamás se crean métodos, atributos, clases ni firmas que no estén especificados explícitamente en el AST (`AST.hpp`) o en los archivos de especificación locales. Si la especificación no define una firma pública, no se inventa: se solicita/registra como incertidumbre (`U-xxx`) y el trabajo se detiene hasta tener autorización.

### 2.2 Aislamiento de la especificación

El código generado **jamás** debe contener referencias, menciones ni comentarios sobre los archivos `.md` de especificación. Prohibido escribir, por ejemplo, "Implementado según specs.md", "Ver AGENTS.md", "Según doc.md", o cualquier nombre de documento. El código se explica por sí mismo y por sus comentarios técnicos.

### 2.3 Comentarios útiles

Cada función debe incluir un comentario breve y descriptivo en **español** sobre su responsabilidad técnica. Los comentarios describen *qué* hace la función y su contrato, no repiten el código ni citan documentos.

### 2.4 Sintaxis limpia

- Evitar la sobreingeniería y el desorden de código.
- Estilo ordenado, declarativo y fácil de leer.
- Funciones complejas del lenguaje explicadas con comentarios claros y breves.
- Jamás implementar características complejas de C++ si no son explícitamente necesarias.
- Mantener siempre la facilidad de lectura, sin poner por debajo la calidad, la seguridad ni la robustez del código.

### 2.5 Convenciones de estilo (proyecto)

- `PascalCase` para clases y tipos.
- `camelCase` para métodos y funciones.
- Guardas de inclusión con `#ifndef` / `#define` / `#endif`.
- Ayudantes internos en `namespace {}` anónimo.
- Mensajes y diagnósticos en español.
- Sin advertencias con `-Wall -Wextra` en el entorno verificado.

### 2.6 No modificar código existente

- Los archivos `*.cpp` y `*.hpp` de `Lexer/`, `Parser/` (incluidos `Tokens.hpp` y `AST.hpp`) **no se tocan jamás**, ni para corregir, ni para "conectar", ni para adaptar.
- Únicamente se autorizan cambios en archivos `CMakeLists.txt` (solo para establecer conexiones entre fases) y en `app/main.cpp` (solo con autorización explícita registrada, y únicamente para invocar la fase y devolver el código de salida de la compuerta).
- Toda autorización no listada se considera **no concedida**.

### 2.7 Organización de archivos

- Toda implementación nueva de esta fase se organiza en la carpeta asignada por el programador: `IntermediateGeneration/`.
- Dicha carpeta debe incluir su propio `CMakeLists.txt` para facilitar la comunicación con las fases anteriores.
- No se crean archivos fuera de `IntermediateGeneration/` sin autorización explícita.
- El header existente `IntermediateGeneration/CodigoIntermedio.h` es el punto de partida autorizado de la fase (actualmente vacío, a completar).

### 2.8 Prohibido ampliar el alcance

No se modifica código ya existente como medio para implementar código nuevo. La compatibilidad entre archivos se preserva, pero la compatibilidad no se confunde con la corrección.

---

## 3. Seguridad y Prevención de Vulnerabilidades (Efecto Bola de Nieve)

Los errores pequeños en generación de código intermedio se propagan y amplifican en fases posteriores. Las siguientes reglas son **obligatorias**.

### 3.1 Control de Punteros (Null-Safety)

Es **obligatorio** validar cualquier puntero inteligente (`std::unique_ptr`, y por extensión `ExprPtr`, `StmtPtr`, `DeclPtr`, `std::unique_ptr<BlockStmt>`, `std::unique_ptr<VarDecl>`) contra `nullptr` **antes** de invocar `->accept(*this)` o cualquier acceso a miembro.

Nodos que suelen ser nulos de forma **legítima**:

- `IfStmt::elseBranch`
- `VarDecl::initializer`
- `ForStmt::increment` (y también `init`, `condition`, `body`)
- `ReturnStmt::value`
- `ExprStmt::expression`
- `UnaryExpr::operand`
- `BinaryExpr::left` / `BinaryExpr::right`
- `CallExpr::callee`
- `MemberExpr::object`
- `WhileStmt::condition` / `WhileStmt::body`
- `FunctionDecl::body` (prototipo, verificable con `isPrototype()`)
- Elementos de `ProgramNode::declarations`, `BlockStmt::statements`, `StructDecl::fields`, `CallExpr::arguments` (pueden ser `nullptr`, ver `CL-050`).

**Regla general:** ante cualquier `unique_ptr`/`*Ptr`, comprobar `if (ptr)` (o `if (ptr != nullptr)`) antes de desreferenciar. Nunca asumir que un campo opcional está presente.

### 3.2 Limpieza de Estado (`lastTemp`)

La variable interna `lastTemp` debe actualizarse **predeciblemente**:

- Toda evaluación de una **Expresión** (`Expr`: `LiteralExpr`, `IdentifierExpr`, `UnaryExpr`, `BinaryExpr`, `CallExpr`, `MemberExpr`) **debe sobreescribir `lastTemp` invariablemente**.
- Al visitar un nodo de tipo **Declaración** (`Decl`) o **Sentencia** (`Stmt`) que no produce valores (por ejemplo `BlockStmt`, `IfStmt`, `WhileStmt`, `ForStmt`, `ReturnStmt`, `ExprStmt`, `VarDecl`, `FunctionDecl`, `StructDecl`, `ProgramNode`), `lastTemp` **no es relevante** y no debe asumirse con un valor previo significativo. No se depende de él tras una sentencia.
- No se lee `lastTemp` esperando que contenga el resultado de una expresión anterior si en medio hubo una sentencia.

### 3.3 Prevención de Segfaults

- Acceso a los nodos **estrictamente por referencia constante** (`const NodeType&`). No se copian nodos (la copia está eliminada en `ASTNode`).
- No se muta el AST. El recorrido es `const`.
- No se desreferencian punteros sin validación (ver §3.1).
- No se asume que un vector de nodos no contiene `nullptr`.

### 3.4 Sin RTTI ni casts inseguros

No se usa `dynamic_cast` ni `typeid`. El despacho de nodos se hace mediante `accept(ASTVisitor&)` y los 16 métodos `visit`. Si se necesita distinguir por tipo de nodo, se usa `NodeKind` o el visitor.

### 3.5 Acumulación de diagnósticos

Los errores se **acumulan**, no se lanzan. La fase no lanza excepciones para flujo normal de diagnóstico. Se reporta y se continúa (salvo límite de errores, definido por la especificación local).

### 3.6 Pruebas (obligatorio)

Ningún bloque de código crítico debe quedarse sin probar. Deben hacerse:

- **Pruebas unitarias** de cada componente (generación por tipo de nodo, manejo de temporales, null-safety).
- **Pruebas de integración** en conjunto con otros módulos (Lexer, Parser, y la fase semántica si está disponible) para validar el funcionamiento completo.
- **Casos límite** verificados explícitamente (nodos nulos, árboles parciales, anidamiento extremo, entradas mínimas/máximas).

Toda nueva implementación debe tener una prueba que demuestre que funciona.

---

## 4. Referencia del AST (inventario verificado)

> Esta sección es una **referencia** del contrato de solo lectura `AST.hpp`. No crea ni modifica nada; solo documenta lo disponible para evitar invención.

### 4.1 Clases base y aliases

- `ASTNode` (abstracta): `ubicacion` (`const Ubicacion`), `line()`, `column()`, `kind()` (puro), `accept(ASTVisitor&)` (puro), `print(int)` (puro), `toString()`.
- `Expr : ASTNode`
- `Stmt : ASTNode`
- `Decl : Stmt` (añade `name : std::string`)
- Aliases: `ExprPtr = std::unique_ptr<Expr>`, `StmtPtr = std::unique_ptr<Stmt>`, `DeclPtr = std::unique_ptr<Decl>`.

### 4.2 Patrón Visitor (16 métodos puros)

`ASTVisitor` declara (y toda implementación debe proporcionar):

`visit(const ProgramNode&)`, `visit(const FunctionDecl&)`, `visit(const StructDecl&)`, `visit(const VarDecl&)`, `visit(const BlockStmt&)`, `visit(const IfStmt&)`, `visit(const WhileStmt&)`, `visit(const ForStmt&)`, `visit(const ReturnStmt&)`, `visit(const ExprStmt&)`, `visit(const LiteralExpr&)`, `visit(const IdentifierExpr&)`, `visit(const UnaryExpr&)`, `visit(const BinaryExpr&)`, `visit(const CallExpr&)`, `visit(const MemberExpr&)`.

### 4.3 Enumeración `NodeKind` (16 valores)

`Program`, `FunctionDecl`, `StructDecl`, `VarDecl`, `BlockStmt`, `IfStmt`, `WhileStmt`, `ForStmt`, `ReturnStmt`, `ExprStmt`, `LiteralExpr`, `IdentifierExpr`, `UnaryExpr`, `BinaryExpr`, `CallExpr`, `MemberExpr`. Función auxiliar: `nodeKindToString(NodeKind)`.

### 4.4 Inventario de nodos y campos

**Expresiones:**

| Nodo | Campos |
|---|---|
| `LiteralExpr` | `value : std::string`, `literalType : TipoToken` |
| `IdentifierExpr` | `name : std::string` |
| `UnaryExpr` | `op : std::string`, `operand : ExprPtr`, `isPrefix : bool` (default `true`) |
| `BinaryExpr` | `left : ExprPtr`, `op : std::string`, `right : ExprPtr` |
| `CallExpr` | `callee : ExprPtr`, `arguments : std::vector<ExprPtr>` |
| `MemberExpr` | `object : ExprPtr`, `member : std::string`, `isPointer : bool` (default `false`) |

**Sentencias y declaraciones:**

| Nodo | Campos |
|---|---|
| `BlockStmt` | `statements : std::vector<StmtPtr>` |
| `ExprStmt` | `expression : ExprPtr` |
| `IfStmt` | `condition : ExprPtr`, `thenBranch : StmtPtr`, `elseBranch : StmtPtr` (default `nullptr`) |
| `WhileStmt` | `condition : ExprPtr`, `body : StmtPtr` |
| `ForStmt` | `init : StmtPtr`, `condition : ExprPtr`, `increment : ExprPtr`, `body : StmtPtr` |
| `ReturnStmt` | `value : ExprPtr` (default `nullptr`) |
| `VarDecl` (Decl) | `type : std::string`, `initializer : ExprPtr` (default `nullptr`) (más `name` heredado) |
| `FunctionDecl` (Decl) | `returnType : std::string`, `parameters : std::vector<Param>`, `body : std::unique_ptr<BlockStmt>` (default `nullptr`); `isPrototype() const` |
| `StructDecl` (Decl) | `fields : std::vector<std::unique_ptr<VarDecl>>` (más `name` heredado) |
| `ProgramNode` (ASTNode) | `declarations : std::vector<StmtPtr>` |

**Auxiliar:**

- `Param`: `type : std::string`, `name : std::string`, `ubicacion : Ubicacion`.

### 4.5 Ubicaciones y tokens (`Tokens.hpp`)

- `Ubicacion`: `linea : int`, `columna : int`. Todo nodo tiene `ubicacion` válida con `linea >= 1` y `columna >= 1`.
- `TipoToken`: `IDENTIFICADOR`, `NUMERO`, `CADENA`, `PALABRA_RESERVADA`, `OPERADOR`, `DELIMITADOR`, `FIN_ARCHIVO`, `DESCONOCIDO`, `CARACTER`, `ERROR_LEXICO`.
- `Token`: `tipo : TipoToken`, `lexema : std::string`, `ubicacion : Ubicacion`.
- Función auxiliar: `tipoToString(TipoToken)`.

### 4.6 Propiedades verificadas del AST (de solo lectura)

- Propiedad única mediante `std::unique_ptr`; copia eliminada; sin ciclos.
- Exactamente 16 tipos de nodo y 16 métodos `visit`.
- Todo nodo despacha con `accept(ASTVisitor&) const` a un `visit(const T&)`.
- `Decl : Stmt`; `ProgramNode::declarations` es `std::vector<StmtPtr>`.
- La ubicación del nodo es `const` desde su construcción.
- El AST puede contener sentencias donde se esperaban declaraciones.
- El AST puede ser parcial (sin señal en el propio árbol); la fase previa (semántica) actúa como compuerta.

---

## 5. Estructura del Módulo

```
IntermediateGeneration/
├── AGENTS.md
├── CMakeLists.txt          (a crear; autorizado)
├── CodigoIntermedio.h      (existente, vacío, a completar)
├── CodigoIntermedio.cpp    (a crear)
└── tests/                  (pruebas unitarias y de integración)
```

- La fase recibe el AST producido por el parser (y validado por la fase semántica, si está disponible). La generación solo procede cuando la compuerta semántica es `ok() == true`.
- El módulo debe poder enlazarse como biblioteca aparte (patrón de las fases anteriores).
- `AST.cpp` permanece en `ParserLib`; esta fase no lo duplica ni lo modifica.
- Las firmas públicas de la fase se definen **únicamente** por la especificación local cuando exista. Mientras no exista, no se inventan firmas; se registran como incertidumbre y se solicita autorización.

---

## 6. Proceso y Verificación (resumen)

Se aplican en su totalidad las reglas de `tools/AGENTS.md` (clasificación de hallazgos, verificación antes de implementar, no modificar código existente, etc.). Resumen de obligaciones para esta fase:

1. **No adoptar patrones existentes de forma automática** ni tomar el código previo como correcto/óptimo/seguro.
2. **Identificar defectos** del código existente y usarlos solo para evitar nuevos errores; no corregirlos (salvo autorización).
3. **Verificar antes de implementar:** qué hace el código, qué dice la documentación, qué defectos afectan, qué carpeta asignó el programador, qué prueba demuestra el funcionamiento.
4. **Clasificar todo hallazgo** como Confirmado / Probable / Desconocido / Decisión del equipo / Fuera de alcance.
5. **Toda nueva implementación debe ser testeada** (unitaria + integración + casos límite).
6. **Ante funciones confusas o contradictorias**, evaluar todos los caminos y elegir una única solución, considerando los estragos posibles.
7. **Preservar compatibilidad correcta entre archivos**, sin confundir compatibilidad con corrección.
8. **Evitar código difícil de leer.**

---

## 7. Lista de Verificación antes de Entregar

- [ ] Compila con `-std=c++17 -Wall -Wextra` (entorno verificado) con **0 errores y 0 advertencias** en archivos nuevos.
- [ ] No se modificó ningún `*.cpp`/`*.hpp` existente de `Lexer/` o `Parser/`.
- [ ] No hay referencias, menciones ni comentarios sobre archivos `.md` en el código generado.
- [ ] No se crearon métodos/atributos/clases/firmas no especificados en el AST o la especificación.
- [ ] Todo puntero inteligente se valida contra `nullptr` antes de desreferenciar.
- [ ] Toda evaluación de `Expr` sobreescribe `lastTemp`.
- [ ] El acceso a nodos es por referencia constante (`const NodeType&`).
- [ ] No se usa RTTI (`dynamic_cast`/`typeid`).
- [ ] Existen pruebas unitarias y de integración, incluyendo casos límite.
- [ ] La carpeta incluye su `CMakeLists.txt`.
- [ ] Todo bloque crítico está probado.

---

*Este documento es de gobernanza para la fase de Generación de Código Intermedio. Complementa, no reemplaza, a `tools/AGENTS.md`.*