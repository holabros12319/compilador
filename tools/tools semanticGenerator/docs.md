# docs.md — Plano técnico para el Analizador Semántico

**Proyecto:** `Compiler` (compilador de un subconjunto de C++ escrito en C++17).
**Fase planificada:** Analizador Semántico.
**Fecha de verificación:** 2026-10-29.

## 0. Método, alcance y clasificación

Este documento **no** define requisitos (eso corresponde a `doc.md`). Define el **plano de arquitectura** para construir la siguiente fase: qué existe, qué se reutiliza, qué falta, cómo fluye la información y qué defectos o dudas pueden desviar el diseño.

Base de evidencia: inspección directa de los 13 archivos del proyecto, con sus tamaños verificados en esta sesión:

```
CMakeLists.txt 212 · Lexer/CMakeLists.txt 256 · Parser/CMakeLists.txt 208 · app/CMakeLists.txt 125
Lexer/Tokens.hpp 1604 · Lexer/Regex.hpp 3209 · Lexer/Lexer.hpp 460 · Lexer/Lexer.cpp 9103
Parser/AST.hpp 10054 · Parser/AST.cpp 4836 · Parser/Parser.hpp 1936 · Parser/Parser.cpp 14159
app/main.cpp 1039
```

`doc.md` se usó **de forma parcial**, solo para contraste. Toda afirmación de este documento se respalda con `archivo:línea` del código inspeccionado.

Cada hallazgo se clasifica según `AGENTS.md` §4: **Confirmado**, **Probable**, **Desconocido**, **Decisión del equipo**, **Fuera de alcance**.

Restricciones vigentes (`AGENTS.md` §3 y §7): no se modifican `*.cpp` ni `*.hpp` existentes; solo se editan archivos CMake y únicamente para conexiones entre fases; `doc.md` es de solo lectura.

---

## 1. Arquitectura

### 1.1 Tipo de arquitectura observada

Compilador **monolítico, en proceso, por fases acopladas por tipos de datos**, sin representación intermedia y sin emisión de código. Tres artefactos construibles: dos bibliotecas estáticas (`LexerLib`, `ParserLib`) y un ejecutable (`compiler_app`) que las orquesta (`CMakeLists.txt:9-11`, `app/CMakeLists.txt:1-8`).

| Fase | Implementación | Entrada | Salida | Estado |
|---|---|---|---|---|
| Léxico | `Lexer::tokenize()` (`Lexer.cpp:76-279`) | `std::string` fuente | `std::vector<Token>` con `FIN_ARCHIVO` al final (`Lexer.cpp:277`) | Presente |
| Sintáctico | `Parser::parse()` (`Parser.cpp:103-114`) | `std::vector<Token>` por valor | `std::unique_ptr<ProgramNode>` | Presente |
| Semántico | — | — | — | **Ausente** |
| Código/IR | — | — | — | **Ausente** |

Confirmado: no existe preprocesador. El carácter `#` no está en operadores (`Regex.hpp:22-32`) ni en delimitadores (`Regex.hpp:35-36`), por lo que produce `ERROR_LEXICO` (`Lexer.cpp:271-274`). El compilador **no puede procesar código C++ real con `#include`**, y por tanto no puede compilarse a sí mismo.

### 1.2 Capas y dependencias

```
compiler_app (app/main.cpp)
   ├── LexerLib : Lexer, Tokens.hpp, Regex.hpp
   └── ParserLib : Parser, AST.hpp, AST.cpp
            └── (PRIVATE) LexerLib
```

- `LexerLib` no depende de nadie y publica su carpeta de cabeceras (`Lexer/CMakeLists.txt:9-11`).
- `ParserLib` depende de `LexerLib` de forma **PRIVATE** (`Parser/CMakeLists.txt:11`); su única ruta pública es `Parser/` (`Parser/CMakeLists.txt:7-9`).
- `compiler_app` enlaza ambas explícitamente (`app/CMakeLists.txt:5-8`).

**Consecuencia directa para la fase nueva:** `AST.hpp` incluye `"Tokens.hpp"` (`AST.hpp:5`), pero `Tokens.hpp` está en `Lexer/` y `ParserLib` no propaga esa ruta por ser PRIVATE. Un futuro `SemanticLib` que solo enlace `ParserLib` **no compilará**. Solución permitida (edición CMake, no de código): enlazar también `LexerLib`, o promover el enlace de `ParserLib` a PUBLIC. Clasificación: **Confirmado**.

### 1.3 Contratos de datos entre fases

| Contrato | Definición | Observación |
|---|---|---|
| Unidad de posición | `Ubicacion{int linea; int columna;}` (`Tokens.hpp:37-40`) | Inmutable dentro de `ASTNode` (`AST.hpp:64`) |
| Token | `TipoToken` de 9 valores, `lexema`, `ubicacion` (`Tokens.hpp:8-19`, `42-52`) | `lexema` **no siempre** es el lexema (ver D-07) |
| Nodo | `ASTNode`: `kind()`, `accept(ASTVisitor&)`, `print(int)` (`AST.hpp:62-82`) | Contrato que la fase nueva debe consumir, no ampliar |
| Raíz | `ProgramNode` con `std::vector<StmtPtr> declarations` (`AST.hpp:284-291`) | Contiene también sentencias sueltas (ver D-05) |
| Propiedad | `unique_ptr` + copia eliminada (`AST.hpp:69-70`, `92-94`) | La tabla de símbolos solo puede guardar observadores |
| Fin de flujo | Token `FIN_ARCHIVO` siempre presente (`Lexer.cpp:277`) | El parser lo usa como condición de parada (`Parser.cpp:27-29`) |

### 1.4 Decisiones heredadas: qué reutilizar y qué no

| Decisión existente | Evidencia | Veredicto para la fase nueva |
|---|---|---|
| `ASTVisitor` con un `visit` por tipo | `AST.hpp:30-49` | **Reutilizar**: es el único punto de extensión compatible con la prohibición de tocar `AST.hpp`. |
| Despacho por `NodeKind` sin RTTI | `AST.hpp:52-57`, uso en `Parser.cpp:273-277` | **Reutilizar**: coherente con RNF de no usar `dynamic_cast`. |
| Propiedad por `unique_ptr`, nodos no copiables | `AST.hpp:69-70`, `92-94` | **Reutilizar**: evita ciclos y fugas. |
| Errores por excepción + recuperación de pánico | `Parser.hpp:34-37`, `Parser.cpp:83-101`, `103-114` | **No reutilizar**: traga los errores y produce AST parcial sin señalizar; la semántica debe acumular diagnósticos, no lanzar. |
| Tipo como `std::string` (`VarDecl::type`, `Param::type`, `FunctionDecl::returnType`) | `AST.hpp:245`, `256`, `263` | **No reutilizar como modelo**: obliga a mapear cadenas a un tipo real y permite colar `static`, `auto`, `const` (D-06, A-05). |
| Presentación dentro del nodo (`print`) | `AST.hpp:77-78`, `AST.cpp:41-126` | **No extender**: acopla modelo y formato; la fase nueva debe imprimir diagnóstico y tabla en sus propias clases. |
| Ausencia de ámbitos y de tabla de símbolos | No existe en ningún archivo | **Hueco a llenar** por la fase nueva. |
| Ausencia de modelo de tipos | No existe en ningún archivo | **Hueco a llenar**. |
| Ausencia de diagnóstico estructurado | `Parser::error` imprime a `std::cerr` (`Parser.cpp:77-81`) | **Hueco a llenar**: severidad, código y posición. |
| Guardas `#ifndef NOMBRE_HPP` | `AST.hpp:2-3`, `Parser.hpp:1-2`, `Tokens.hpp:1-2` | **Seguir** en archivos nuevos; es estilo inocuo y consistente. |
| Sin `namespace` | Todos los archivos usan ámbito global | **Decisión del equipo** (Q-13), no obligación. |

### 1.5 Huecos arquitectónicos que la nueva fase debe cubrir

1. **Modelo de tipos** con categorías y conversiones (no existe).
2. **Tabla de símbolos con ámbitos anidados** (no existe).
3. **Diagnóstico estructurado** con severidad, código y posición exacta.
4. **Resultado formal de fase** (`ok`, lista de diagnósticos, tabla) que sirva de compuerta.
5. **Contexto de entrada** que declare si hubo errores léxicos o sintácticos previos, porque el AST no lo revela (D-13).
6. **Anotación de tipos sin mutar el AST**, imposible dentro de `AST.hpp` por la prohibición vigente (Q-03).

---

## 2. Componentes

### 2.1 Componentes existentes críticos

| Componente | Ubicación | Por qué es crítico para el Analizador Semántico | Riesgo / límite |
|---|---|---|---|
| **ASTNode** | `AST.hpp:62-82` | Contrato base: ubicación, `kind`, `accept`, `print`. Todo nodo que se visite pasa por aquí. | `print` es obligatorio en cada nodo y la copia está eliminada; la fase no puede clonar nodos. |
| `ASTVisitor` | `AST.hpp:30-49` | Punto de entrada de la fase: 16 `visit` puros. | Sin implementaciones por defecto ni `visit(ASTNode&)`: la fase debe implementar los 16. |
| `NodeKind` + `nodeKindToString` | `AST.hpp:52-57`, `AST.cpp:19-39` | Base del despacho sin RTTI. | Tres puntos que mantener sincronizados si se añadieran nodos (no permitido hoy). |
| `Expr` / `Stmt` / `Decl` | `AST.hpp:84-90` | `Decl` deriva de `Stmt`; `name` vive en `Decl`. Determina cómo se separan declaraciones de sentencias. | `Decl` no redefine `kind`, así que un `VarDecl` en un `vector<StmtPtr>` debe distinguirse por `kind()`. |
| `ProgramNode` | `AST.hpp:284-291` | Raíz única del recorrido. | Guarda `StmtPtr`, no `DeclPtr`: hay que filtrar y rechazar sentencias globales. |
| `VarDecl` | `AST.hpp:243-252` | Símbolo local/global: nombre, tipo textual, inicializador. | No hay `const`, `static` ni arreglos; el tipo es texto libre. |
| `FunctionDecl` + `Param` | `AST.hpp:254-272` | Firma, cuerpo y prototipo (`isPrototype()`, `AST.hpp:268`). | `Param` no guarda posición del nombre (D-09); no hay firma normalizada ni sobrecarga. |
| `StructDecl` | `AST.hpp:274-282` | Definición de campos y sus inicializadores. | Campos son `vector<unique_ptr<VarDecl>>`; no puede anidar structs (D-06). |
| `LiteralExpr` | `AST.hpp:98-109` | Único nodo con valor y `TipoToken` léxico. | `value` ya viene desescapado y sin comillas (D-08); el tipo debe inferirse del texto (A-01). |
| `IdentifierExpr` | `AST.hpp:111-120` | Resolución de nombres. | Solo guarda el nombre; la resolución debe vivir fuera del nodo (Q-03). |
| `UnaryExpr` / `BinaryExpr` | `AST.hpp:122-148` | Operadores y **asignación** (`op == "="`, `Parser.cpp:276`). | La asignación no tiene nodo propio: hay que tratarla como caso especial. |
| `CallExpr` | `AST.hpp:150-161` | Llamadas y aridad. | `callee` puede ser otra `CallExpr` (`Parser.cpp:370-380`), no solo un identificador. |
| `MemberExpr` | `AST.hpp:164-176` | Acceso `.` y `->`. | Ningún tipo struct es expresable como variable (D-06), así que hoy toda base es de tipo primitivo. |
| `BlockStmt` / `IfStmt` / `WhileStmt` / `ForStmt` / `ReturnStmt` / `ExprStmt` | `AST.hpp:179-241` | Estructura de control y ámbitos. | Varios campos opcionales son `nullptr` por diseño (`else`, `init`, `condition`, `increment`, `value`). |
| `Ubicacion` / `Token` | `Tokens.hpp:37-52` | Precisión de los diagnósticos. | Para `ERROR_LEXICO`, `lexema` guarda el mensaje de error (D-07). |
| `Parser::parse` | `Parser.cpp:103-114` | Produce el AST que se va a analizar. | No informa si hubo errores ni qué se perdió (D-13). |
| `main` | `app/main.cpp:18-35` | Única orquestación existente. | No invoca fase semántica y siempre retorna 0 (D-14). |

### 2.2 Puntos de extensión obligatorios

1. **Los 16 `visit`** de `ASTVisitor`, sin excepción y sin añadir métodos a `AST.hpp`.
2. **`NodeKind`** como discriminante; nunca `dynamic_cast` ni `typeid`.
3. **`unique_ptr` del AST como dueño único**; la fase nueva no crea ni libera nodos.
4. **`const` en todo el recorrido**: `accept` recibe `ASTVisitor&` y entrega `const T&` (`AST.hpp:76`, `33-48`), por lo que cualquier anotación debe ser externa al nodo.

### 2.3 Componentes nuevos (propuesta, no implementada)

| Componente propuesto | Responsabilidad | Depende de | Requiere CMake |
|---|---|---|---|
| `SemanticAnalyzer` | Visitante que recorre `ProgramNode` en dos pasadas y emite diagnósticos. | `AST.hpp`, tabla, tipos | Sí |
| `TablaSimbolos` + `Ambito` | Pila de ámbitos, inserción con detección de colisión local, búsqueda hacia afuera. | `AST.hpp` (observadores) | Sí (misma biblioteca) |
| `ModeloTipos` (`TipoBase`, `Tipo`) | Normaliza cadenas de tipo, categorías, compatibilidad y conversión implícita. | `Tokens.hpp` | Sí |
| `Diagnostico` + `ResultadoSemantico` | Código, severidad, mensaje, posición, `ok()`, contadores. | `Ubicacion` | Sí |
| `EvaluadorConstante` | Valida inicializadores de campo (`AST.hpp:246`, `Parser.cpp:134`). | `AST.hpp`, tipos | Sí |
| `PrechequeoLexico` | Detecta `ERROR_LEXICO` en el vector de tokens antes de analizar. | `Tokens.hpp` | Sí |
| `VolcadoSemantico` | `dump` de tabla y tipos para pruebas. | Resultado | No |

Nombres y agrupación son **propuesta**. La carpeta debe ser designada explícitamente por el programador (`AGENTS.md` §3.6) y debe incluir su `CMakeLists.txt`; hoy esa designación no existe (Q-01).

---

## 3. Flujo y archivos relevantes

### 3.1 Flujo actual, paso a paso

| # | Acción | Archivo | Evidencia |
|---|---|---|---|
| 0 | CMake construye `LexerLib`, `ParserLib`, `compiler_app` | `CMakeLists.txt:9-11` | — |
| 1 | `main` entrega un literal de cadena al lexer | `app/main.cpp:7-19` | Cadena cruda `R"(...)"` |
| 2 | `Lexer::tokenize()` recorre la fuente y emite tokens; cierra con `FIN_ARCHIVO` | `Lexer.cpp:76-279` | `FIN_ARCHIVO` en `:277` |
| 3 | `main` imprime todos los tokens a `stdout` | `app/main.cpp:22-25` | — |
| 4 | `Parser parser(tokens)` **copia** el vector y lo mueve a su miembro | `app/main.cpp:28`, `Parser.cpp:7-8` | Paso por valor (D-15) |
| 5 | `parse()` recorre declaraciones; ante `ParseError` imprime y sincroniza | `Parser.cpp:103-114`, `83-101` | Errores a `std::cerr` |
| 6 | `main` imprime el AST con `toString()` | `app/main.cpp:31-33`, `AST.cpp:122-126` | Requiere `AST.cpp` enlazado (D-01) |
| 7 | `main` retorna 0 siempre | `app/main.cpp:38` | Sin contrato de salida |

Estado de la fase nueva: **no hay paso 6.5**. El AST se produce y se imprime; nadie lo valida.

### 3.2 Punto de inserción de la fase

```
tokenize() ──► Parser::parse() ──► [ProgramNode] ──► ¿? ──► toString()/siguiente fase
                                        │
                                        └── aquí entra SemanticAnalyzer
```

Entradas disponibles en el punto de inserción: el `ProgramNode` y, en `main`, la variable local `tokens` (que sí permite detectar `ERROR_LEXICO` sin tocar el parser). No hay bandera de errores sintácticos (D-13): esa es la limitación principal del punto de inserción.

Salida esperada de la fase: diagnósticos con posición exacta y un resultado booleano que actúe de compuerta. Hoy no existe consumidor de esa compuerta (Q-02).

### 3.3 Grafo CMake actual y conexiones necesarias

Grafo verificado:

```
Semantic (no existe)
LexerLib ──PRIVATE──► ParserLib ──► compiler_app
   ▲                                   ▲
   └───────────────────────────────────┘
```

Conexiones que la fase nueva exigirá (todas son ediciones de archivos CMake, por tanto dentro de la autorización de `AGENTS.md` §7):

1. `CMakeLists.txt` raíz: `add_subdirectory(<carpeta designada>)`.
2. `<carpeta>/CMakeLists.txt`: `add_library(...)` + `target_include_directories(... PUBLIC ...)` + `target_link_libraries(... PUBLIC ParserLib LexerLib)`. Enlazar `LexerLib` es obligatorio por D-08.
3. `app/CMakeLists.txt`: enlazar la biblioteca nueva **solo si** el ejecutable va a usarla. Enlazarla no la ejecuta: la invocación vive en `app/main.cpp`, que está prohibido tocar (Q-02).

Ninguna de estas conexiones cambia lógica ni opciones de compilación.

### 3.4 Archivos relevantes: hoy, fase semántica y futuro

| Archivo | Papel hoy | Papel en la fase semántica | Papel futuro |
|---|---|---|---|
| `Lexer/Tokens.hpp` | Contrato de token y posición | Único proveedor de `Ubicacion` y `TipoToken` | Compartido por todas las fases |
| `Lexer/Regex.hpp` | Operadores, delimitadores, reservadas | Define qué palabras pueden llegar como tipo | Base del preprocesador futuro (hoy inexistente) |
| `Lexer/Lexer.cpp` | Escáner manual | No se toca; se consume su salida | Fuente del prechequeo léxico |
| `Parser/AST.hpp` | Modelo del árbol | **Contrato central**, de solo lectura | Base de IR/codegen |
| `Parser/AST.cpp` | Impresión y `nodeKindToString` | Apoyo de depuración | Reutilizable para volcados |
| `Parser/Parser.hpp` / `.cpp` | Construcción del AST | No se tocan; definen qué es representable | Se ampliarán en otra fase, con autorización |
| `app/main.cpp` | Orquestación | Punto donde debería invocarse (prohibido por ahora) | Orquestación final de todas las fases |
| `CMakeLists.txt`, `*/CMakeLists.txt` | Build | Único lugar editable para conectar | Igual |

---

## 4. Errores, ambigüedades y dudas que afectan al diseño

### 4.1 Defectos confirmados

> **Actualización 2026-10-29:** D-01, D-04, D-08, D-10, D-13, D-14 y C-01 fueron corregidos y verificados dinámicamente (build con clang 20.1.3 i686, 6 casos sobre `main.cpp`, 9 casos de biblioteca y prueba de terminación de C-01). Las filas siguientes se conservan como registro histórico del estado previo; el estado vigente y sus consecuencias están en `specs.md` §2.

| ID | Defecto | Evidencia | Efecto sobre el Analizador Semántico | Clasificación |
|---|---|---|---|---|
| **D-01** | `AST.cpp` no estaba en ninguna biblioteca (símbolos `print` y `nodeKindToString` sin definición) | `Parser/CMakeLists.txt` antes de la conexión; uso en `app/main.cpp:33` | Sin él, `toString()` no enlaza. Ya se añadió `AST.cpp` a `ParserLib` (cambio CMake autorizado), pero el enlace **no pudo verificarse** por falta de toolchain: la corrección sigue sin confirmar | **Confirmado** |
| **D-02** | Un `.cpp` usa guardas de cabecera (`#ifndef AST_CPP`) | `AST.cpp:2-3`, cierre en `:128` | Ninguno funcional; indica confusión entre unidad de traducción y cabecera. No se corrige | **Confirmado** |
| **D-03** | `#include "cstddef"` con comillas para una cabecera estándar | `AST.cpp:7` | Depende del directorio actual de inclusión; riesgo de portabilidad | **Confirmado** |
| **D-04** | El parser **descarta en silencio** una asignación con destino inválido: imprime el error y devuelve solo el lado izquierdo, perdiendo `=` y el derecho | `Parser.cpp:268-281` (el `error` no lanza y luego `return expr`) | Código inválido (`1 = 2;`) llega a la semántica como `ExprStmt(Literal)`, sin rastro del error: la compuerta no puede confiar en el AST | **Confirmado** |
| **D-05** | El ámbito global acepta sentencias ejecutables (`if`, `for`, `return`, expresiones) porque la declaración cae a `parseStatement()` | `Parser.cpp:116-123` | La semántica debe rechazar lo que el parser acepta; no puede asumir que `declarations` solo contiene declaraciones | **Confirmado** |
| **D-06** | Ningún tipo definido por el usuario es expresable: campos y parámetros exigen `PALABRA_RESERVADA` como tipo, no `IDENTIFICADOR` | `Parser.cpp:132`, `Parser.cpp:153` | `MemberExpr` nunca tiene base de tipo struct; `.` y `->` son siempre error semántico | **Confirmado** |
| **D-07** | En tokens `ERROR_LEXICO`, `lexema` contiene el **mensaje** de error, no el texto fuente | `Lexer.cpp:111-113`, `194-196`, `242-243`, `272-274` | Un prechequeo léxico puede detectar el tipo, pero no reconstruir el texto; los mensajes deben citar el mensaje ya formateado | **Confirmado** |
| **D-08** | Los literales de cadena y carácter llegan sin delimitadores y con escapes ya resueltos | `Lexer.cpp:204-246` | La semántica no puede distinguir `"a"` de `'a'` por el texto; debe usar `LiteralExpr::literalType` | **Confirmado** |
| **D-09** | Posiciones imprecisas en varios nodos: el cuerpo de función usa la ubicación del tipo (`Parser.cpp:171`), `MemberExpr` usa la del objeto (`:384`), `Param` la del tipo (`:155`) | Ver referencias | Diagnósticos pueden señalar una columna distinta de la que el usuario espera; hay que documentarlo | **Confirmado** |
| **D-10** | El lexer acepta `''` y produce un `CARACTER` con valor vacío | Bucle en `Lexer.cpp:207-235` no exige contenido | La validez del literal debe decidirse en semántica, no en léxico | **Confirmado** |
| **D-11** | Dos fuentes de verdad léxica: `Regex.hpp` define `identifier`, `integer`, `decimales`, `string`, `character`, comentarios (`:13-19`), pero `Lexer.cpp` **no las usa** y escanea a mano | `Regex.hpp:13-19` vs `Lexer.cpp:119-248` | El diseño de tipos no puede basarse en `Regex.hpp`: describe una intención, no el comportamiento | **Confirmado** |
| **D-12** | El lexer no reconoce hexadecimal: `0x10` se parte en `0` y `x10` | `Lexer.cpp:135-198`, `Regex.hpp:22-32` | Cualquier programa con `0x` falla en sintaxis; la semántica nunca lo ve | **Confirmado** |
| **D-13** | `parse()` captura `ParseError` y **nunca** informa que hubo errores; devuelve un AST parcial como si fuera completo | `Parser.cpp:103-114` | La fase nueva no puede distinguir "programa válido" de "programa mutilado" sin información externa (Q-04) | **Confirmado** |
| **D-14** | `main` retorna 0 siempre y no distingue fases fallidas | `app/main.cpp:38` | No hay contrato de código de salida que la fase pueda usar (Q-08) | **Confirmado** |
| **D-15** | El parser recibe el vector de tokens **por valor**; `main` pasa un lvalue, por lo que se copia | `Parser.hpp:13`, `Parser.cpp:7-8`, `app/main.cpp:28` | Costo de memoria/tiempo; irrelevante para la corrección, pero la fase nueva debe decidir si recibe el vector por referencia | **Confirmado** |

### 4.2 Ambigüedades

| ID | Ambigüedad | Evidencia | Riesgo de diseño | Clasificación |
|---|---|---|---|---|
| **A-01** | El tipo de un literal numérico no está definido en ningún sitio: `3.1416` no dice si es `float` ni `10u` si es `unsigned` | `LiteralExpr` solo guarda texto y `TipoToken` (`AST.hpp:100-104`) | Dos implementaciones pueden discrepar; afecta `float pi = 3.1416;` (`app/main.cpp:9`) | **Decisión del equipo** |
| **A-02** | `char` y `bool` son válidos como tipo de campo y parámetro (D-06), pero no existen variables locales de ese tipo ni literales `true`/`false` | `Parser.cpp:118-120` vs `:132`, `:153`; `true` es reservada (`Regex.hpp:67,93`) | El modelo de tipos debe soportarlos sin poder ejercitarlos plenamente | **Decisión del equipo** |
| **A-03** | `void` es aceptado como tipo de campo y de parámetro por el parser | Mismo mecanismo que D-06 | La semántica debe rechazarlo, aunque el parser lo produzca | **Confirmado** (comportamiento) |
| **A-04** | `.` y `->` comparten nodo (`MemberExpr::isPointer`) y no hay operador de indirección en el AST | `AST.hpp:164-176`, `Parser.cpp:381-385` | `->` es semánticamente inalcanzable hoy; hay que decidir si se rechaza o se difiere | **Decisión del equipo** |
| **A-05** | El tipo es texto arbitrario; `static`, `auto`, `const`, `unsigned` llegan como "tipo" sin validación | `Parser.cpp:132`, `:153` | Sin un normalizador, la semántica aceptaría tipos inexistentes | **Confirmado** (riesgo) |
| **A-06** | Las declaraciones son sentencias (`Decl : Stmt`), así que un `VarDecl` puede aparecer en cualquier lista de sentencias | `AST.hpp:86-90`, `286` | El análisis debe separar declaración de ejecución en un mismo recorrido | **Confirmado** |
| **A-07** | El parser no distingue `return` con valor de `return;` como error: ambos producen `ReturnStmt.value` nulo o no | `Parser.cpp:248-254`, `AST.hpp:234-241` | La regla de retorno depende enteramente de la semántica | **Confirmado** |
| **A-08** | `main` no es palabra reservada ni requisito del parser: puede no existir, repetirse o tener otra firma | `Regex.hpp:38-125` no contiene `main`; `Parser.cpp:103-114` no lo busca | Que `main` sea obligatorio es una decisión, no una consecuencia | **Decisión del equipo** |
| **A-09** | No hay nodos de ámbito: el alcance solo puede reconstruirse del anidamiento de bloques | `AST.hpp:179-187` | La tabla de símbolos debe crearse durante el recorrido, no leerse del árbol | **Confirmado** |
| **A-10** | El parser escribe a `std::cerr` directamente, sin inyección de flujo | `Parser.cpp:77-81` | Los diagnósticos semánticos y sintácticos no pueden ordenarse ni capturarse juntos (Q-09) | **Confirmado** |

### 4.3 Riesgos de diseño para la fase nueva

| ID | Riesgo | Motivo | Mitigación permitida | Clasificación |
|---|---|---|---|---|
| **R-01** | Normalizar `std::string` a tipo real aceptando de más | A-05 | Tabla cerrada de tipos válidos; cualquier otro valor produce diagnóstico | **Decisión del equipo** |
| **R-02** | Anotar tipos sin poder tocar `AST.hpp` | §2.2.4, Q-03 | Estructura paralela con clave `const Expr*`, propiedad del AST y vida acotada al análisis | **Decisión del equipo** |
| **R-03** | Cascadas de errores por un solo fallo | D-04, D-13 | Tipo `Error` compatible con todo y supresión de diagnósticos derivados | **Probable** (requiere prueba) |
| **R-04** | Mensajes en columnas engañosas | D-09 | Documentar y, cuando el nodo no tenga mejor posición, usar la del token más cercano disponible | **Confirmado** |
| **R-05** | Colisión de nombres entre función, variable y struct | `Decl::name` único (`AST.hpp:88`) | Un solo espacio de nombres; sin sobrecarga (Q-07) | **Decisión del equipo** |
| **R-06** | Falso "programa limpio" tras un parse fallido | D-13 | Prechequeo léxico sobre `tokens`; no hay forma de detectar el fallo sintáctico sin autorización | **Confirmado** |
| **R-07** | Salida no determinista al volcar tablas | uso de contenedores no ordenados | Contenedores ordenados o volcado ordenado antes de imprimir | **Decisión del equipo** |

### 4.4 Dudas latentes

| ID | Duda | Por qué bloquea o desvía | Clasificación |
|---|---|---|---|
| **Q-01** | ¿En qué carpeta debe vivir la fase? Ninguna fue designada aún | `AGENTS.md` §3.6 exige designación explícita; la carpeta debe traer su CMake | **Decisión del equipo** |
| **Q-02** | ¿Cómo se ejecuta la fase si `app/main.cpp` no puede modificarse? | Sin invocación no hay integración; alternativas: ejecutable de pruebas propio en la carpeta nueva, o autorización para tocar `main.cpp` | **Decisión del equipo** |
| **Q-03** | ¿El AST se anota o solo se diagnostica? | Determina si hace falta una estructura paralela (R-02) y qué consume la futura generación de código | **Decisión del equipo** |
| **Q-04** | ¿Cómo se comunica que hubo errores léxicos/sintácticos previos? | `Parser` no expone bandera (D-13); `doc.md` proponía un `ContextoEntrada` | **Decisión del equipo** |
| **Q-05** | ¿`main` es obligatorio? | Cambia el conjunto de programas válidos y las pruebas de aceptación | **Decisión del equipo** |
| **Q-06** | ¿Se admite sobrecarga de funciones? | Hoy el nombre es único; admitirla exige firma normalizada y cambia la tabla | **Decisión del equipo** |
| **Q-07** | ¿Qué conversiones son válidas (`char` a `int`, `double` a `float`, `double` a `int`)? | Define casi todo el análisis de expresiones (A-01, A-02) | **Decisión del equipo** |
| **Q-08** | ¿Qué códigos de salida debe usar el ejecutable? | D-14: hoy siempre es 0; sin contrato no hay compuerta observable | **Decisión del equipo** |
| **Q-09** | ¿Formato e idioma de los diagnósticos? ¿Se inyecta `std::ostream`? | Debe convivir con `[Error Sintáctico]` del parser (A-10) | **Decisión del equipo** |
| **Q-10** | ¿`doc.md` sigue vigente donde contradice a `AGENTS.md`? | `doc.md` §RF-38/39 exige modificar `main.cpp` y CMake; `AGENTS.md` solo autoriza CMake | **Decisión del equipo** |
| **Q-11** | ¿Con qué se prueba la fase? | El proyecto no tiene framework, ni carpeta de pruebas, ni `ctest` | **Decisión del equipo** |
| **Q-12** | ¿Se permite un ejecutable nuevo para pruebas? | Es creación, no modificación; aun así necesita carpeta designada y CMake | **Decisión del equipo** |
| **Q-13** | ¿Se usa `namespace` en el código nuevo? | El proyecto no usa ninguno; introducirlo mejora el aislamiento pero rompe la uniformidad | **Decisión del equipo** |

### 4.5 Fuera de alcance

No deben abordarse sin autorización explícita: preprocesador y `#include`; punteros, arreglos y variables de tipo struct; IR, optimización y generación de código; modificar o reemplazar `Lexer`, `Parser` o `AST`; corregir D-01…D-15; instalar o configurar un toolchain.

### 4.6 Matriz de impacto sobre el Analizador Semántico

| Hallazgo | Efecto directo | Mitigación sin tocar código existente |
|---|---|---|
| D-04, D-13 | El AST no es prueba de corrección sintáctica | Prechequeo léxico + resultado conservador; documentar el límite |
| D-05, D-06, A-03, A-05 | El árbol contiene construcciones inválidas para la semántica | Validación defensiva en la primera pasada |
| D-07, D-08, A-01 | Los literales no traen tipo semántico | Inferencia explícita desde `LiteralExpr::value` y `literalType` |
| D-09 | Posiciones imprecisas | Aceptar la posición del nodo y documentar la desviación |
| D-11, D-12 | No se puede confiar en `Regex.hpp` para tipar | Tabla de tipos cerrada y explícita |
| D-14, Q-02 | No hay compuerta observable | Resultado `ok()` en la biblioteca; integración pendiente de decisión |
| D-01, D-08 (CMake) | El enlace de la fase fallaría | Enlazar `LexerLib` en la biblioteca nueva y verificar con build real |
| D-02, D-03 | Ruido de estilo en un archivo que se reutiliza | No se corrige; se convive con ello |

---

## 5. Checklist de arranque para la implementación

Antes de escribir la primera línea de la fase semántica, deben estar resueltas, por escrito, Q-01 a Q-05 como mínimo: carpeta designada con CMake, forma de invocación, decisión sobre anotación de tipos, comunicación de errores previos y obligatoriedad de `main`. Sin esas respuestas, cualquier implementación se apoyaría en suposiciones, lo que `AGENTS.md` §3.7 prohíbe.

Además, la primera verificación obligatoria es un build real (`cmake -S . -B <dir>` + `cmake --build <dir>`), hoy imposible por ausencia de toolchain: sin él, D-01 queda en **Probable** y ninguna fase nueva puede declararse funcional.

> **Actualización 2026-10-29:** el build real ya se hizo con `clang++ 20.1.3` (i686-w64-mingw32), compilando y enlazando `app/main.cpp`, `Lexer.cpp`, `Parser.cpp` y `AST.cpp` con `-std=c++17 -Wall -Wextra`: 0 errores y 0 advertencias. `cmake` sigue ausente, así que la verificación por CMake queda pendiente. Las dudas Q-01 a Q-05 están resueltas en `specs.md` §10.1 (`DU-001` a `DU-006`).
