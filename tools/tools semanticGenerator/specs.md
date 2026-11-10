# specs.md — Especificación de requisitos de la fase de Análisis Semántico

**Proyecto:** `Compiler` — compilador de un subconjunto de C++ escrito en C++17.
**Fase especificada:** Análisis Semántico (posterior a Léxico y Sintáctico).
**Estado:** borrador normativo, ratificado el 2026-10-29 (ver §10.1).
**Fuentes:** `AGENTS.md` (reglas), `docs.md` (análisis del código), `doc.md` (antecedente de requisitos, consultado de forma parcial) y verificación directa del código fuente.

---

## 0. Control del documento

### 0.1 Autoridad documental

| Documento | Papel frente a esta especificación |
|---|---|
| `specs.md` (este) | **Normativo.** Es la única fuente para decidir qué debe hacer la fase y cuándo se acepta. |
| `docs.md` | **No normativo.** Registro de análisis y defectos del proyecto. Se usa como contexto. |
| `doc.md` | **No normativo para esta fase.** Antecedente de requisitos. Sus afirmaciones sobre el código están sujetas a verificación; varias ya no coinciden (Anexo A). Se conserva íntegro porque es de solo lectura (`AGENTS.md` §6). |
| `AGENTS.md` | **Normativo para el proceso** (permisos, clasificación, reglas de trabajo). |

Regla de precedencia: si `specs.md` y `doc.md` discrepan, manda `specs.md`. Si `specs.md` y `docs.md` discrepan, manda la evidencia del código citada aquí.

### 0.2 Convención de identificadores

| Prefijo | Significado | Ejemplo |
|---|---|---|
| `RF-xxx` | Requisito funcional | `RF-001` |
| `RNF-xxx` | Requisito no funcional | `RNF-001` |
| `CA-xxx` | Criterio de aceptación | `CA-001` |
| `CL-xxx` | Caso límite | `CL-001` |
| `N-xxx` | Caso normal | `N-001` |
| `V-xxx` | Hecho verificado del proyecto actual | `V-001` |
| `INV-A-xxx` | Invariante del AST **verificada** | `INV-A-001` |
| `INV-D-xxx` | Propiedad **deseada** de la fase | `INV-D-001` |
| `PRE-xxx` / `POST-xxx` | Precondición / postcondición | `PRE-001` |
| `SUP-xxx` | Supuesto **no verificado** | `SUP-001` |
| `HER-xxx` | Comportamiento heredado que **no** debe conservarse | `HER-001` |
| `INT-xxx` | Restricción de integración | `INT-001` |
| `DU-xxx` | Decisión tomada (con fecha) | `DU-001` |
| `U-xxx` | Incertidumbre **abierta** | `U-007` |
| `E-xx` / `W-xx` | Código de diagnóstico (se conservan los de `doc.md`) | `E-04` |

Los identificadores son estables: no se reutilizan ni se renumeran. Un requisito retirado se marca `RETIRADO`, no se borra.

### 0.3 Regla de evidencia

Toda afirmación sobre el proyecto se respalda con `archivo:línea` **verificada en esta sesión**. No se heredan citas de `doc.md`: sus números de línea corresponden a una versión anterior del código (Anexo A, K-11).

### 0.4 Cómo se lee un requisito

Cada `RF`/`RNF` tiene: identificador, enunciado verificable y verificación (cómo se comprueba). Un requisito sin forma de comprobarlo no se acepta como requisito; se registra como `U-xxx`.

---

## 1. Objetivo y alcance

### 1.1 Problema que resuelve

El compilador hoy produce un AST sin validarlo. Un programa como `y = 3;` o `int x = "texto";` atraviesa léxico y sintáctico sin obstáculo y llega a la siguiente fase como si fuera correcto. La fase semántica existe para **decidir si el programa tiene sentido según las reglas del lenguaje** antes de generar código, y para **explicar cada rechazo** con posición y causa.

Evidencia del hueco: `app/main.cpp:82-85` imprime el AST y retorna éxito sin ninguna validación posterior.

### 1.2 Responsabilidades (dentro del alcance)

1. Resolver nombres: qué declara cada identificador y a qué declaración se refiere cada uso.
2. Comprobar tipos en declaraciones, expresiones, llamadas, condiciones y retornos.
3. Verificar reglas estructurales: `main` única, retorno garantizado, ámbitos, redeclaraciones.
4. Producir diagnósticos ordenados, deterministas y con posición.
5. Actuar de **compuerta**: si hay errores, la fase siguiente no puede ejecutarse.

### 1.3 Fuera de alcance

| Excluido | Motivo |
|---|---|
| Modificar el AST, el Lexer o el Parser | `AGENTS.md` §7; además el AST es contrato de solo lectura para la fase. |
| Anotar tipos dentro del AST | `ASTNode::ubicacion` es `const` (`AST.hpp:64`) y no hay hueco para anotaciones; la información de tipos vive en la fase. |
| Generación de código, IR, optimización | Fase posterior, no especificada aquí. |
| Preprocesador, `#include`, macros | No existe; `#` produce `ERROR_LEXICO` (`Lexer.cpp:271-274`). |
| Punteros, arreglos, `switch`, `do`, `break`, `continue`, `sizeof`, casts | El AST no los representa (`AST.hpp:96-291`). |
| Sobrecoarga de funciones | `Decl::name` es único (`AST.hpp:88`); decisión `DU-002`. |
| Análisis de flujo completo | Solo se verifica retorno garantizado estructural (`RF-025`). |
| Recuperación de errores semánticos | Se reportan y se continúa; no se repara el programa. |

### 1.4 Información que recibe y resultados que produce

**Entrada:** el `ProgramNode` producido por `Parser::parse()` y un `ContextoEntrada` con dos banderas (errores léxicos y sintácticos previos). Detalle en §5.1.

**Salida:** un `ResultadoSemantico` con: veredicto `ok()`, lista ordenada de diagnósticos, contadores y tabla de símbolos consultable. Detalle en §5.2.

### 1.5 Relación con las fases anteriores y posteriores

```
fuente ──► Lexer ──► tokens ──► Parser ──► ProgramNode
                                              │
                                              ├──► Análisis Semántico ──► ResultadoSemantico
                                              │                              │
                                              │                              ├─ ok()==false ──► FIN (no se genera código)
                                              │                              └─ ok()==true  ──► fase siguiente (no especificada)
                                              └──► toString() (solo depuración)
```

| Fase | Qué le entrega a la semántica | Qué espera de ella |
|---|---|---|
| Léxico | Tokens con `Ubicacion`; `FIN_ARCHIVO` siempre al final | Nada |
| Sintáctico | `ProgramNode` (posiblemente parcial) + `huboErroresSintacticos()` | Nada |
| Semántica | — | `ResultadoSemantico` |
| Posterior | — | `ok() == true` como condición necesaria |

### 1.6 Decisiones de alcance ya tomadas

`DU-001` a `DU-006` (§10.1). Las que siguen abiertas están en §10.2 y **no** se han resuelto por suposición.

---

## 2. Estado verificado del proyecto

Hechos comprobados en esta sesión. Sustituyen a `doc.md` §1.2 (`V-01`…`V-12`), varios de los cuales ya no son ciertos.

| ID | Hecho verificado | Evidencia | Cómo se verificó |
|---|---|---|---|
| **V-001** | `AST.cpp` está en `ParserLib`; `toString()` enlaza y se ejecuta | `Parser/CMakeLists.txt:5` | Enlace y ejecución reales |
| **V-002** | `ParserLib` propaga `LexerLib` como `PUBLIC` | `Parser/CMakeLists.txt:11` | Compilación de un consumidor con y sin la ruta de `Lexer` |
| **V-003** | `main` devuelve `0` éxito, `1` léxico, `2` sintáctico; `3` está reservado | `app/main.cpp:12-14,58,70,78,85` | Ejecución de 6 casos |
| **V-004** | El parser expone si recuperó errores | `Parser.cpp:10-12,82`; `Parser.hpp:18` | Ejecución de casos con y sin error |
| **V-005** | El AST puede ser parcial sin señal en el propio árbol | `Parser.cpp:113-120` | `int main( { }` produce 0 declaraciones y bandera activa |
| **V-006** | El ámbito global acepta sentencias ejecutables | `Parser.cpp:123-130` | Inspección (prueba dinámica pendiente) |
| **V-007** | Solo `int`, `float`, `double`, `void` inician declaración | `Parser.cpp:125-127` | Inspección |
| **V-008** | Campos de `struct` y parámetros aceptan cualquier palabra reservada como tipo | `Parser.cpp:139,160` | Inspección |
| **V-009** | Ningún tipo `struct` es expresable como variable, campo o parámetro | Consecuencia de V-008 | Inspección + `AST.hpp:245` |
| **V-010** | `ASTVisitor` tiene 16 métodos puros | `AST.hpp:30-49` | Conteo |
| **V-011** | La asignación no tiene nodo propio: es `BinaryExpr` con `op == "="` | `Parser.cpp:277-283` | Inspección |
| **V-012** | `Decl` deriva de `Stmt`; `ProgramNode::declarations` es `vector<StmtPtr>` | `AST.hpp:86-90,286` | Inspección |
| **V-013** | El nodo no es copiable y su ubicación es `const` | `AST.hpp:64,69-70` | Inspección |
| **V-014** | El lexer rechaza `''` y acepta `'ab'` como `CARACTER` | `Lexer.cpp:244-248` | Ejecución |
| **V-015** | El lexer acepta sufijos numéricos y `.5`; `0x10` se parte en `0` + `x10` | `Lexer.cpp:135-198` | Inspección + `doc.md` CL-54 |
| **V-016** | `main` no lee archivos ni argumentos; el fuente es un literal fijo | `app/main.cpp:34-43` | Inspección |
| **V-017** | Compila limpio con `-Wall -Wextra` (0 errores, 0 avisos) con clang 20.1.3 i686 | Build real de esta sesión | Ejecución |
| **V-018** | No hay `cmake` ni MSVC; el `CMakeCache.txt` tiene `CMAKE_CXX_COMPILER-NOTFOUND` | `out/build/x64-Debug/CMakeCache.txt` | Búsqueda en el entorno |
| **V-019** | Algunas ubicaciones no señalan el nombre: cuerpo de función usa el tipo de retorno; `Param` usa el tipo; `MemberExpr` usa `.`/`->` | `Parser.cpp:178,162,395` | Inspección |
| **V-020** | El lexer siempre cierra con `FIN_ARCHIVO` | `Lexer.cpp:277` | Volcado de tokens |

---

## 3. Requisitos funcionales

Formato: identificador, enunciado verificable, verificación. La columna **Origen** indica el requisito de `doc.md` del que procede (1:1: `RF-0nn` ↔ `doc.md RF-nn`); vacío significa requisito nuevo de esta especificación.

### 3.1 Preparación, contrato y resultado

| ID | Requisito | Verificación | Origen |
|---|---|---|---|
| **RF-001** | Debe existir `SemanticAnalyzer` con `ResultadoSemantico analyze(const ProgramNode&, const ContextoEntrada&)`, sin modificar el AST. | Compila con el AST en `const&`; el AST no cambia (CA-005). | RF-01 |
| **RF-002** | `ResultadoSemantico` expone `ok()`, `diagnosticos()`, `errores()`, `advertencias()` y `tabla()`. | Prueba unitaria de la API. | RF-02 |
| **RF-003** | Si `contexto.erroresLexicos` o `contexto.erroresSintacticos` es verdadero, se emite `E-00` y **no** se analiza. | Caso con bandera activa: exactamente un diagnóstico `E-00`, cero análisis. | RF-03 |
| **RF-004** | Debe implementar los 16 `visit` de `ASTVisitor` y despachar por `NodeKind`, sin RTTI. | Compila sin `dynamic_cast`/`typeid`; los 16 métodos existen. | RF-04 |
| **RF-005** | El análisis es de dos pasadas: (1) símbolos globales, (2) cuerpos y locales. | Programa con función usada antes de su definición: sin `E-04`. | RF-05 |
| **RF-006** | `TablaSimbolos` con ámbitos anidados, búsqueda de dentro hacia afuera y colisión solo en el ámbito actual. | Casos `CL-015` y `CL-016`. | RF-06 |
| **RF-007** | Modelo de tipos explícito con `TipoBase` y un tipo `Error` compatible con todo que no genera diagnósticos derivados. | Caso `CL-024`: un solo diagnóstico, sin cascada. | RF-07 |
| **RF-041** | El llamador obtiene `erroresSintacticos` de `Parser::huboErroresSintacticos()`, no de una nueva consulta al parser. | Inspección del punto de llamada; paso de la bandera en las pruebas. | — |

### 3.2 Programa y ámbitos

| ID | Requisito | Verificación | Origen |
|---|---|---|---|
| **RF-008** | Debe existir exactamente una función `int main()`; ausencia, duplicado, otro retorno o parámetros → `E-01`/`E-02`/`E-03`. | `CL-001`…`CL-004`. | RF-08 |
| **RF-009** | Todo elemento de `declarations` debe ser `VarDecl`, `FunctionDecl` o `StructDecl`; una sentencia global → `E-24`. | `CL-042`. | RF-09 |
| **RF-010** | Son ámbitos: global, bloque, cuerpo de función y `for`; la variable del `for` no escapa. | `CL-044`, `CL-045`. | RF-10 |
| **RF-011** | Redeclarar en el mismo ámbito → `E-03`; el sombreado anidado es legal. | `CL-015`, `CL-016`, `CL-017`. | RF-11 |
| **RF-012** | Uso antes de declarar → `E-05`; uso inexistente → `E-04`; los parámetros cuentan desde el inicio del cuerpo. | `CL-018`, `CL-019`, `CL-017`. | RF-12 |
| **RF-013** | Un nombre designa una sola cosa: sin sobrecarga ni función/variable homónimas en el mismo ámbito. | `CL-004`, `CL-040`. | RF-13 |

### 3.3 Tipos y expresiones

| ID | Requisito | Verificación | Origen |
|---|---|---|---|
| **RF-014** | Tipos válidos: `int`, `float`, `double`, `char`, `bool`, `void` (según contexto) y nombres de `struct` declarados. Otro valor → `E-06`. | `CL-008`. | RF-14 |
| **RF-015** | `void` solo como retorno; en variable → `E-07`; en campo/parámetro → `E-08`. | `CL-006`, `CL-007`. | RF-15 |
| **RF-016** | Clasificación de literales numéricos por su lexema: `.`/`e`/`E` → `Double`; sufijo `f`/`F` → `Float`; resto → `Int`; sufijos `u`/`U`/`l`/`L` se ignoran con `W-03`. | `CL-027`, `CL-028`, `CL-054`. | RF-16 |
| **RF-017** | Conversiones: `Int→Float→Double` sin diagnóstico; numérico→`Bool` en condición; `Char→Int`; `Double→Float` con `W-02`; `Double/Float→Int` e `Int→Char` → `E-13`. | `CL-024`…`CL-026`. | RF-17 |
| **RF-018** | `+ - * /` exigen numéricos; `%` exige enteros; comparaciones exigen la misma familia; `&& \|\| !` exigen convertible a `Bool`; unarios `+ -` exigen numérico. | Matriz de operadores con un caso por violación. | RF-18 |
| **RF-019** | `=`, `++` y `--` exigen destino asignable (`IdentifierExpr` de variable o `MemberExpr` de struct); el resultado tiene el tipo del destino. | `CL-023`, `CL-024`. | RF-19 |
| **RF-020** | `IdentifierExpr` que resuelve a función o struct en contexto de valor → `E-19`. | `CL-041`. | RF-20 |
| **RF-021** | `CallExpr`: el `callee` debe resolver a `FunctionDecl` (`E-16`), aridad exacta (`E-17`) y argumentos compatibles (`E-18`). | `CL-020`, `CL-021`, `CL-022`. | RF-21 |
| **RF-022** | `MemberExpr`: base de tipo struct (`E-20`), miembro existente (`E-21`) y `->` sobre no puntero (`E-22`). | `CL-037`, `CL-038`. | RF-22 |
| **RF-023** | Entero fuera de rango → `E-25`; carácter que no sea 1 byte → `E-26`; cadena en contexto numérico → `E-13`; cadena descartada → `W-07`. | `CL-027`, `CL-029`, `CL-030`, `CL-031`. | RF-23 |

### 3.4 Funciones

| ID | Requisito | Verificación | Origen |
|---|---|---|---|
| **RF-024** | `return` con valor en `void` → `E-09`; `return;` en no `void` → `E-10`. | `CL-009`, `CL-010`. | RF-24 |
| **RF-025** | Toda función no `void` debe garantizar retorno estructuralmente: última sentencia `ReturnStmt` con valor, o `IfStmt` con `else` cuyas dos ramas lo garantizan, o `BlockStmt` cuya última sentencia lo garantiza. | `CL-011`, `CL-012`, `CL-013`. | RF-25 |
| **RF-026** | Prototipo y definición con el mismo nombre deben coincidir en retorno y en la secuencia de tipos de parámetros; si no → `E-23`. Prototipo sin definición → `W-05`. | `CL-039`, `CL-040`. | RF-26 |
| **RF-027** | La recursión directa e indirecta es legal y no produce `E-04`. | `CL-043`. | RF-27 |

### 3.5 Sentencias

| ID | Requisito | Verificación | Origen |
|---|---|---|---|
| **RF-028** | La condición de `if`, `while` y `for` debe ser convertible a `Bool`; la omisión en `for (;;)` equivale a verdadero. | `CL-045` + caso de condición no convertible. | RF-28 |
| **RF-029** | Tras un `return` en el mismo bloque, las sentencias siguientes generan **una** `W-01` por tramo. | `CL-014`. | RF-29 |
| **RF-030** | Una sentencia de expresión sin efecto (`LiteralExpr`, `IdentifierExpr`) → `W-07`; con llamada, asignación o `++/--` no advierte. | `CL-031`. | RF-30 |

### 3.6 Structs

| ID | Requisito | Verificación | Origen |
|---|---|---|---|
| **RF-031** | Struct vacía → `W-06`; campo duplicado → `E-03`; campo `void` → `E-08`; struct duplicada → `E-03`. | `CL-032`, `CL-033`, `CL-007`. | RF-31 |
| **RF-032** | El inicializador de un campo debe ser expresión constante (literales y operaciones sobre literales, sin identificadores ni llamadas); si no → `E-27`. | `CL-034`. | RF-32 |
| **RF-033** | En evaluación constante, `/` o `%` con divisor cero → `E-28`. En ejecución, la división por cero **no** es error semántico. | `CL-035`, `CL-036`. | RF-33 |

### 3.7 Diagnóstico e integración

| ID | Requisito | Verificación | Origen |
|---|---|---|---|
| **RF-034** | Todo diagnóstico lleva código, mensaje no vacío en español, línea y columna, y se emite a un `std::ostream&` inyectable (por defecto `std::cerr`). | Inspección de la firma + captura en `std::ostringstream`. | RF-34 |
| **RF-035** | Los diagnósticos se emiten en orden de aparición (línea, luego columna) y son idénticos entre ejecuciones. | CA-004. | RF-35 |
| **RF-036** | Límite configurable de errores (por defecto 100): al alcanzarlo se emite `E-29`, se detiene el análisis y `ok()` es falso. | Entrada con 101 errores. | RF-36 |
| **RF-037** | `ok() == true` si y solo si no se emitió ningún `E-xx`; `ok() == true` es condición necesaria para la fase siguiente. | CA-001 y CA-002. | RF-37 |
| **RF-038** | `app/main.cpp` invoca la fase tras `parse()`, imprime sus diagnósticos y devuelve `3` si hay error semántico. El AST se imprime solo si `ok()` es verdadero. | CA-008. | RF-38 |
| **RF-039** | El módulo se integra en CMake con su propia biblioteca y `AST.cpp` permanece en `ParserLib`. | `cmake --build` sin errores (o build manual equivalente). | RF-39 |
| **RF-040** | Debe existir un volcado de tabla de símbolos y tipos inferidos (`dumpTabla(std::ostream&)`) para depuración. | CA-002 compara el volcado. | RF-40 |
| **RF-042** | Debe existir un ejecutable de pruebas propio de la fase que reciba un archivo fuente y devuelva `0`/`1`/`2`/`3`, sin modificar `main`. | Ejecución de los casos `CL-xxx` sobre ese binario. | — |

---

## 4. Requisitos no funcionales

Cada uno indica **cómo se evalúa**, no solo el objetivo.

| ID | Requisito | Evaluación | Origen |
|---|---|---|---|
| **RNF-001** | C++17, sin extensiones de compilador. | Compila con `-std=c++17` y con `/std:c++17`. | RNF-01 |
| **RNF-002** | Sin RTTI: prohibido `dynamic_cast` y `typeid`. | Búsqueda en el código nuevo; 0 apariciones. | RNF-02 |
| **RNF-003** | Los errores semánticos se acumulan, no se lanzan. | La fase no lanza en ningún caso `CL-xxx`; se comprueba con los casos de error. | RNF-03 |
| **RNF-004** | Recorrido `const`: no se muta el AST. | CA-005 compara el AST antes y después. | RNF-04 |
| **RNF-005** | La tabla guarda observadores (`const Decl*`), nunca propietarios. | Inspección de tipos de la tabla; compila sin `unique_ptr`/`shared_ptr` en ella. | RNF-05 |
| **RNF-006** | Sin fugas: RAII y contenedores estándar. | ASan o valgrind si el entorno lo permite; en su defecto, revisión de propiedad y contador de nodos vivos en el arnés. La elección definitiva es `U-007`. | RNF-06 |
| **RNF-007** | Coste lineal: como máximo dos visitas por nodo (una por pasada), O(n) en nodos y O(d) en profundidad. | Conteo de visitas por pasada en el arnés; medición en CA-006. | RNF-07 (corregido, K-06) |
| **RNF-008** | 100 000 nodos en menos de 100 ms en la máquina de verificación, compilado con `-O2`. | CA-006 registra el tiempo medido. Si no se cumple, se reporta el incumplimiento; el umbral no se ajusta sin aprobación. | RNF-08 |
| **RNF-009** | Determinismo: mismo AST → misma secuencia de diagnósticos, byte a byte. | CA-004 ejecuta dos veces y compara. | RNF-09 |
| **RNF-010** | Estilo del proyecto: `PascalCase` para clases, `camelCase` para métodos, guardas `#ifndef`, ayudantes en `namespace {}` anónimo, mensajes en español. | Revisión del código nuevo. | RNF-10 |
| **RNF-011** | El módulo es una biblioteca aparte con su `CMakeLists.txt`, para poder enlazarse desde pruebas. | CA-008. | RNF-11 |
| **RNF-012** | Sin advertencias con `-Wall -Wextra` (entorno verificado) y `/W4` (objetivo MSVC, no verificado hoy). | CA-007. | RNF-12 |
| **RNF-013** | Solo biblioteca estándar; sin APIs de Windows ni POSIX en el módulo; E/S inyectada. | Revisión de `#include` del módulo nuevo. | RNF-13 |
| **RNF-014** | Nunca aborta, cuelga ni lanza ante un AST parcial o malformado; emite `E-06` y continúa. | `CL-050`, `CL-051`. | RNF-14 |
| **RNF-015** | Testable sin el `main` de la aplicación, sin estado global mutable. | CA-002 usa dos instancias sobre el mismo AST. | RNF-15 |
| **RNF-016** | Configuración y build desde cero deben funcionar. | `cmake -S . -B <dir>` + `cmake --build <dir>`; **no verificable en el entorno actual** por ausencia de `cmake` (`V-018`, `SUP-002`). | RNF-16 |

---

## 5. Entradas, salidas y errores

### 5.1 Entrada

```cpp
struct ContextoEntrada {
    bool erroresLexicos;      // true si el vector de tokens contenia ERROR_LEXICO
    bool erroresSintacticos;  // true si Parser::huboErroresSintacticos() lo indico
};
```

| Aspecto | Regla |
|---|---|
| Fuente de `erroresSintacticos` | `Parser::huboErroresSintacticos()` (`RF-041`). No se inspecciona el AST para adivinarlo (`V-005`). |
| Fuente de `erroresLexicos` | Recorrido del vector de tokens ya existente en el llamador. |
| Propiedad del AST | El llamador conserva la propiedad; la fase solo recibe `const ProgramNode&` (`RNF-004`). |
| Vida de la tabla | La tabla contiene observadores; es válida mientras viva el AST (`INV-D-016`). |

### 5.2 Salida

```cpp
class ResultadoSemantico {
public:
    bool ok() const;
    const std::vector<Diagnostico>& diagnosticos() const;
    std::size_t errores() const;
    std::size_t advertencias() const;
    const TablaSimbolos& tabla() const;
};
```

`Diagnostico`: código, severidad (`Error`/`Advertencia`), mensaje, `Ubicacion`.

### 5.3 Diagnósticos

Formato obligatorio, alineado con el del parser (`Parser.cpp:83-85`):

```
[Error Semántico] Línea <L>, Columna <C>: <mensaje> (<código>)
[Advertencia Semántica] Línea <L>, Columna <C>: <mensaje> (<código>)
```

Se conservan los códigos de `doc.md` para no romper la trazabilidad: `E-00` a `E-29` y `W-01` a `W-08`. Catálogo completo en `doc.md` §2.8; esta especificación no lo duplica, solo fija su uso.

| Grupo | Códigos |
|---|---|
| Precondición | `E-00` |
| Programa y ámbitos | `E-01`, `E-02`, `E-03`, `E-04`, `E-05`, `E-24` |
| Tipos | `E-06`, `E-07`, `E-08`, `E-25`, `E-26` |
| Expresiones y llamadas | `E-12`…`E-22` |
| Funciones | `E-09`, `E-10`, `E-11`, `E-23` |
| Structs | `E-27`, `E-28` |
| Límite | `E-29` |
| Advertencias | `W-01`…`W-08` |

### 5.4 Códigos de salida

| Código | Significado | Quién lo produce |
|---|---|---|
| `0` | Éxito | `main` (ya implementado, `V-003`) |
| `1` | Error léxico | `main` (ya implementado) |
| `2` | Error sintáctico | `main` (ya implementado) |
| `3` | Error semántico | `main` tras la fase (`RF-038`) |

---

## 6. Casos normales y casos límite

### 6.1 Casos normales

| ID | Entrada | Resultado esperado |
|---|---|---|
| **N-001** | Programa embebido en `app/main.cpp:34-43` | `ok() == true`; como máximo `W-02` por `float pi = 3.1416;`; exit `0` |
| **N-002** | Declaración y uso simple: `int main() { int x; x = 2; return x; }` | `ok() == true` |
| **N-003** | Programa de `doc.md` CA-02 (struct, prototipos, recursión, `for`, sombreado) | `ok() == true`; tabla con `Punto`, `fact`, `suma`, `main` |
| **N-004** | Función usada antes de su definición | `ok() == true` (pasada 1) |

### 6.2 Casos límite

| ID | Situación | Esperado |
|---|---|---|
| **CL-001** | Programa vacío | `E-01` (no `E-00`) |
| **CL-002** | Sin `main` | `E-01` |
| **CL-003** | `void main()` o `int main(int)` | `E-02` |
| **CL-004** | Dos `main` | `E-03` en el segundo |
| **CL-005** | Solo comentarios | `E-01` |
| **CL-006** | `void x;` local | `E-07` |
| **CL-007** | `struct S { void v; };` | `E-08` |
| **CL-008** | `struct S { static x; };` / `int f(auto a)` | `E-06` |
| **CL-009** | `void f() { return 1; }` | `E-09` |
| **CL-010** | `int f() { return; }` | `E-10` |
| **CL-011** | `int f() { int x = 1; }` | `E-11` + `W-04` |
| **CL-012** | `int f() { if (1) return 1; }` | `E-11` |
| **CL-013** | `if/else` con retorno en ambas ramas | Válido |
| **CL-014** | Sentencias tras `return` | `W-01` (una) |
| **CL-015** | `int x; int x;` | `E-03` |
| **CL-016** | `int x; { int x; }` | Válido |
| **CL-017** | `int f(int a) { int a; }` | `E-03` |
| **CL-018** | `x = 1; int x;` | `E-05` |
| **CL-019** | Uso fuera del bloque que lo declara | `E-04` |
| **CL-020** | `f()` con aridad distinta | `E-17` |
| **CL-021** | Llamar una variable | `E-16` |
| **CL-022** | `f()()` | `E-16` |
| **CL-023** | `f = 1;` con `f` función | `E-12` |
| **CL-024** | `x = "hola";` | `E-13` (sin cascada) |
| **CL-025** | `float f = 1;` | Válido |
| **CL-026** | `float pi = 3.1416;` | Válido + `W-02` |
| **CL-027** | `int x = 9999999999999;` | `E-25` |
| **CL-028** | `int x = 10u;` | Válido + `W-03` |
| **CL-029** | `int x = 'ab';` | `E-26` solo |
| **CL-030** | `int x = "a";` | `E-13` |
| **CL-031** | `"a";` | `W-07` |
| **CL-032** | `struct S { };` | `W-06` |
| **CL-033** | Campo duplicado | `E-03` |
| **CL-034** | `struct S { int a = f(); };` | `E-27` |
| **CL-035** | `struct S { int a = 1/0; };` | `E-28` |
| **CL-036** | `return 1/0;` | Sin error semántico |
| **CL-037** | `pi.x` sobre primitivo | `E-20` |
| **CL-038** | `x->y` sobre primitivo | `E-20` antes que `E-22` |
| **CL-039** | Prototipo sin definición | `W-05` |
| **CL-040** | Prototipo y definición discrepantes | `E-23` |
| **CL-041** | Función usada como valor | `E-19` |
| **CL-042** | `x = 1;` en ámbito global | `E-24` |
| **CL-043** | Recursión | Válido |
| **CL-044** | `for` y uso de su variable fuera | `E-04` |
| **CL-045** | `for (;;)` | Válido |
| **CL-046** | `else` colgante | Válido (ya resuelto por el parser) |
| **CL-047** | Variable no usada | `W-04` |
| **CL-048** | Error léxico previo | `E-00`, sin análisis |
| **CL-049** | Error sintáctico previo | `E-00`, sin análisis |
| **CL-050** | Nodo `nullptr` en `statements` | No aborta; `E-06` y continúa |
| **CL-051** | 10 000 bloques anidados | Termina o poda con `E-06` al superar 256 |
| **CL-052** | Analizar dos veces el mismo AST | Resultados idénticos |
| **CL-053** | `int _; int x1;` | Válido |
| **CL-054** | `.5`, `1e10`, `2.5E-3`, `3.14f`, `0x10` | `Double`, `Double`, `Double`, `Float`; `0x10` es error sintáctico previo |

---

## 7. Contratos e invariantes

### 7.1 Invariantes del AST existente — **verificadas**

Solo se afirman las que resisten inspección y prueba. No se declara una propiedad por "parecer razonable".

| ID | Invariante | Evidencia | Cómo se verificó |
|---|---|---|---|
| **INV-A-001** | Propiedad única: `unique_ptr`, copia eliminada, sin ciclos. | `AST.hpp:69-70,92-94` | Inspección + compilación (la copia no compila) |
| **INV-A-002** | Hay exactamente 16 tipos de nodo y 16 `visit`. | `AST.hpp:30-49` | Conteo |
| **INV-A-003** | Todo nodo despacha con `accept(ASTVisitor&) const` a un `visit(const T&)`. | `AST.hpp:76` | Inspección |
| **INV-A-004** | `Decl : Stmt` y `ProgramNode::declarations` es `vector<StmtPtr>`. | `AST.hpp:86-90,286` | Inspección |
| **INV-A-005** | La ubicación del nodo es `const` desde su construcción. | `AST.hpp:64` | Inspección |
| **INV-A-006** | Toda ubicación tiene `linea >= 1` y `columna >= 1` y proviene de un token real. | `Lexer.cpp:277`; `Parser.cpp:109` | Volcado de tokens y de AST |
| **INV-A-007** | El flujo de tokens termina en `FIN_ARCHIVO`. | `Lexer.cpp:277` | Volcado de tokens |
| **INV-A-008** | El AST puede contener sentencias donde se esperaban declaraciones. | `Parser.cpp:123-130` | Inspección (V-006) |

**Advertencia sobre `INV-A-006`:** se cumple en sentido literal (la posición existe y es un token real), pero **no garantiza** que sea el token del nombre. Ver `V-019` y `HER-001`. Ningún requisito puede apoyarse en "ubicación exacta" sin aceptar esa desviación.

### 7.2 Propiedades deseadas de la fase

| ID | Propiedad | Origen |
|---|---|---|
| **INV-D-001** | Una sola visita por nodo y por pasada; ninguna rama revisita un hijo. | INV-05 |
| **INV-D-002** | Declaraciones antes que usos, salvo recursión. | INV-06 |
| **INV-D-003** | La pila de ámbitos refleja la ruta raíz→nodo en todo instante. | INV-07 |
| **INV-D-004** | No hay dos símbolos homónimos en el mismo ámbito. | INV-08 |
| **INV-D-005** | Todo uso resuelto o diagnosticado; nunca ambos, nunca ninguno. | INV-09 |
| **INV-D-006** | Todo `Expr` visitado tiene tipo, o produjo diagnóstico y su tipo es `Error`. | INV-10 |
| **INV-D-007** | `errores()` solo crece; `ok()` no vuelve a verdadero. | INV-11 |
| **INV-D-008** | Todo diagnóstico tiene código válido, mensaje no vacío y posición válida; sin duplicados idénticos. | INV-12 |
| **INV-D-009** | Terminación acotada y profundidad máxima 256 con poda y `E-06`. | INV-13 |
| **INV-D-010** | Dos instancias no comparten estado mutable. | INV-14 |
| **INV-D-011** | Determinismo byte a byte. | INV-15 |
| **INV-D-012** | Todo `Decl*` de la tabla apunta a un nodo vivo. | INV-16 |
| **INV-D-013** | `ok() == true` si y solo si no hubo `E-xx`. | INV-17 |
| **INV-D-014** | La tabla es fiel: un símbolo por declaración, con su nombre, tipo y posición. | INV-18 |
| **INV-D-015** | Sin cascada: un nodo produce como máximo un diagnóstico propio. | INV-19 |
| **INV-D-016** | La tabla no sobrevive al AST: su vida útil está acotada a la del árbol. | RNF-05 |

### 7.3 Precondiciones y postcondiciones

| ID | Tipo | Enunciado | Comprobación |
|---|---|---|---|
| **PRE-001** | Pre | El `ProgramNode` recibido está vivo y es propiedad del llamador. | Contrato de firma |
| **PRE-002** | Pre | `contexto.erroresSintacticos` refleja `Parser::huboErroresSintacticos()`. | `RF-041` |
| **PRE-003** | Pre | Los nodos alcanzables no contienen punteros nulos en campos no opcionales. | `RNF-014` cubre la violación |
| **POST-001** | Post | El AST no cambió. | CA-005 |
| **POST-002** | Post | Cada `IdentifierExpr`/`CallExpr`/`MemberExpr` está resuelto o diagnosticado. | `INV-D-005` |
| **POST-003** | Post | `ok() == false` si y solo si hay al menos un `E-xx`. | `INV-D-013` |
| **POST-004** | Post | Los diagnósticos están en orden de aparición. | `RF-035` |
| **POST-005** | Post | La tabla solo contiene observadores a nodos del AST recibido. | `INV-D-012` |

### 7.4 Supuestos **no verificados**

| ID | Supuesto | Estado | Impacto |
|---|---|---|---|
| **SUP-001** | MSVC con `/W4` y `/std:c++17` compila el código nuevo sin avisos. | **No verificado** (no hay MSVC, `V-018`) | Afecta a `RNF-012`. Se declara objetivo, no hecho. |
| **SUP-002** | `cmake` configurará y construirá el proyecto desde cero. | **No verificado** (`CMakeCache.txt` incompleto) | Afecta a `RNF-016` y `CA-008`. |
| **SUP-003** | El AST nunca contiene un `kind()` fuera del conjunto de 16. | **No verificado**: es una clase abstracta con `kind()` puro, pero un subtipo nuevo lo rompería | La fase debe emitir `E-06` y continuar, no asumir. |
| **SUP-004** | Ningún nodo comparte subárboles. | Se deduce de `unique_ptr`, no se probó exhaustivamente | Si se violara, la fase lo trataría como dos nodos; no hay corrupción. |
| **SUP-005** | El programa embebido de `main` sigue siendo el mismo entre ejecuciones. | Verificado hoy por lectura, no garantizado a futuro | Solo afecta a `N-001`. |

### 7.5 Comportamientos heredados que **no** deben conservarse

| ID | Comportamiento heredado | Por qué no conservarlo |
|---|---|---|
| **HER-001** | La ubicación del nodo no siempre es la del nombre (cuerpo de función, `Param`, `MemberExpr`). | La fase debe usar la mejor posición disponible y documentarlo; no puede prometer precisión que el AST no da. |
| **HER-002** | Errores reportados escribiendo directamente a `std::cerr` sin inyección. | La fase nueva usa `std::ostream&` inyectable (`RF-034`). |
| **HER-003** | AST parcial devuelto como si fuera completo. | La fase no analiza un árbol parcial: `E-00` y fin (`RF-003`). |
| **HER-004** | Errores por excepción con recuperación de pánico. | La semántica acumula diagnósticos (`RNF-003`). |
| **HER-005** | Tipo como `std::string` libre, sin validar. | La fase normaliza contra una tabla cerrada (`RF-014`). |
| **HER-006** | `print()` dentro del nodo como única salida de depuración. | La fase tiene su propio volcado (`RF-040`); no se extiende el AST. |

---

## 8. Restricciones de integración y compatibilidad

| ID | Restricción | Verificación |
|---|---|---|
| **INT-001** | La fase no modifica `Lexer/`, `Parser/` ni `AST.*`. | Revisión de archivos tocados. |
| **INT-002** | El AST sigue siendo dueño único; la tabla solo observa. | `RNF-005` |
| **INT-003** | Un solo espacio de nombres para funciones, variables y structs. | `RF-013` |
| **INT-004** | La biblioteca nueva enlaza `ParserLib` y `LexerLib` (la ruta de `Lexer` ya se propaga, `V-002`). | Compila un consumidor que solo enlaza la biblioteca nueva. |
| **INT-005** | El módulo incluye su propio `CMakeLists.txt` y se registra en la raíz. | `RNF-011`, `RNF-016` |
| **INT-006** | Solo biblioteca estándar; sin dependencias nuevas. | `RNF-013` |
| **INT-007** | `main.cpp` solo se toca para invocar la fase y devolver `3`. | `DU-005`, registrado en `AGENTS.md` §7. |
| **INT-008** | El resto de `main` (volcado de tokens, mensajes, programa embebido) se conserva. | `RF-038` |

---

## 9. Criterios de aceptación

Cada criterio indica precondición, entrada, pasos, resultado esperado, criterio de fallo y requisitos que cubre.

### CA-001 — Errores semánticos: detección, código y orden

- **Cubre:** `RF-003`…`RF-037`, `INV-D-005`, `INV-D-015`.
- **Precondición:** el programa pasa léxico y sintáctico; el ejecutable de pruebas (`RF-042`) está construido.
- **Entrada:** el programa de `doc.md` CA-01 (`caso_negativo`), que incluye campo duplicado, prototipo discrepante, `return` inválido, falta de retorno, redeclaración, asignación incompatible, uso de no declarado, entero fuera de rango, carácter multibyte, aridad incorrecta y función como valor.
- **Pasos:** ejecutar el binario de pruebas sobre el archivo; capturar `stdout`, `stderr` y código; repetir.
- **Esperado:** exactamente los 11 errores de `doc.md` CA-01 en el orden indicado; advertencias solo de `{W-03, W-04}`; código `3`; `ok() == false`; salida idéntica en ambas ejecuciones.
- **Fallo:** falta o sobra un `E-xx`, cambia el orden, aparece una advertencia fuera del conjunto, o el código no es `3`.

### CA-002 — Programa correcto, tabla y compuerta

- **Cubre:** `RF-001`, `RF-002`, `RF-040`, `RNF-015`, `INV-D-013`, `INV-D-014`.
- **Entrada:** el programa de `doc.md` CA-02 (`caso_positivo`).
- **Pasos:** analizar; volcar la tabla; analizar de nuevo con otra instancia.
- **Esperado:** cero `E-xx`; `ok() == true`; tabla con `Punto`, `fact`, `suma` y `main` con las firmas indicadas; ambas ejecuciones idénticas.
- **Fallo:** cualquier `E-xx`, tabla distinta o resultado no determinista.

### CA-003 — Precondición (árbol parcial)

- **Cubre:** `RF-003`, `RF-041`, `HER-003`.
- **Precondición:** invocación directa de la biblioteca (en la aplicación la compuerta de `main` hace inalcanzable este caso).
- **Entrada:** `ContextoEntrada{false, true}` con cualquier AST; y `ContextoEntrada{true, false}`.
- **Esperado:** exactamente un `E-00`; ningún otro diagnóstico; `ok() == false`.
- **Fallo:** se analiza el árbol, se emiten diagnósticos derivados o `ok()` es verdadero.

### CA-004 — Determinismo y repetición

- **Cubre:** `RF-035`, `RNF-009`, `INV-D-010`, `INV-D-011`.
- **Pasos:** ejecutar CA-001 y CA-002 dos veces cada uno, en procesos distintos, comparando byte a byte.
- **Esperado:** salidas idénticas.
- **Fallo:** cualquier diferencia.

### CA-005 — No mutación del AST

- **Cubre:** `RF-001`, `RNF-004`, `POST-001`.
- **Pasos:** obtener `ast->toString()` antes y después del análisis; comparar.
- **Esperado:** cadenas idénticas y el mismo número de nodos.
- **Fallo:** cualquier diferencia.

### CA-006 — Rendimiento y profundidad

- **Cubre:** `RNF-007`, `RNF-008`, `INV-D-009`.
- **Entrada:** programa generado con 100 000 nodos; y 10 000 bloques anidados.
- **Pasos:** medir con `-O2`; registrar el tiempo.
- **Esperado:** menos de 100 ms para 100 000 nodos; el anidamiento extremo termina o poda con `E-06` sin desbordar la pila.
- **Fallo:** supera el umbral, no termina, o desborda la pila.

### CA-007 — Compilación limpia

- **Cubre:** `RNF-001`, `RNF-012`.
- **Pasos:** compilar el módulo nuevo con `-std=c++17 -Wall -Wextra` en el entorno verificado (`V-017`).
- **Esperado:** 0 errores y 0 advertencias.
- **Fallo:** cualquier advertencia en archivos nuevos.

### CA-008 — Integración, compuerta y código de salida

- **Cubre:** `RF-038`, `RF-039`, `RNF-011`, `RNF-016`, `INT-005`, `INT-007`.
- **Pasos:** construir; ejecutar un programa válido, uno con error léxico, uno sintáctico y uno semántico.
- **Esperado:** `0`, `1`, `2`, `3` respectivamente; el AST no se imprime si `ok() == false`.
- **Fallo:** códigos distintos, el AST se imprime pese al error, o el build falla.
- **Nota:** la parte de `cmake` no se puede verificar en el entorno actual (`SUP-002`); se sustituye por build manual documentado, y la verificación CMake queda pendiente.

---

## 10. Incertidumbres

### 10.1 Decisiones tomadas (2026-10-29)

| ID | Decisión | Consecuencia |
|---|---|---|
| **DU-001** | `specs.md` es el documento normativo de la fase, con identificadores de tres dígitos. `doc.md` queda como antecedente no normativo. | Una sola lista de requisitos; la trazabilidad con `doc.md` se mantiene por número (§11). |
| **DU-002** | Se ratifica el propósito de `doc.md`: validar el subconjunto descrito y usar su catálogo de diagnósticos. | El alcance funcional queda cerrado; no se inventan reglas nuevas. |
| **DU-003** | La fase vive en la carpeta `Semantic/`, con su `CMakeLists.txt`. | Habilita `INT-005` y `RNF-011`. La creación de la carpeta es trabajo de la fase de diseño. |
| **DU-004** | La verificación se hace con un ejecutable de pruebas propio de la fase que recibe un archivo fuente; `main` no gana lectura de archivos. | Resuelve `RF-042` y hace ejecutables los `CA`. |
| **DU-005** | `app/main.cpp` solo se modifica para invocar la fase y devolver `3`. | Registrado en `AGENTS.md` §7; el resto de `main` se conserva (`INT-008`). |
| **DU-006** | El entorno de verificación es clang 20.1.3 i686 con `-Wall -Wextra`; MSVC/x64 queda como objetivo no verificado. | `RNF-012` y `RNF-016` se reportan como parcialmente verificados; `SUP-001` y `SUP-002`. |

### 10.2 Incertidumbres abiertas

Las seis dudas iniciales de la propuesta previa quedaron resueltas y se registran como `DU-001` a `DU-006`; por eso la numeración abierta continúa en U-007.

| ID | Qué se desconoce | Por qué importa | Evidencia | Qué decisión depende | ¿Bloquea? |
|---|---|---|---|---|---|
| **U-007** | Cómo se verifica "sin fugas" sin ASan ni valgrind. | `RNF-006` no es evaluable como está. | `V-018`; no hay valgrind en el entorno. | Criterio de aceptación de memoria. | No (hasta CA) |
| **U-008** | Si `struct`, `char` y `bool` se implementan aunque el parser no pueda producirlos. | Afecta a `RF-014`/`RF-022` y a si `CA` debe cubrirlos con AST construido a mano. | `V-008`, `V-009` | Alcance de las pruebas positivas. | No |
| **U-009** | Qué firma es la autoritativa tras un choque prototipo/definición (`E-23`). | Cambia si `f()` produce además `E-17` en CA-001. | `doc.md` RF-026/CA-01 no lo fijan | Orden y número de diagnósticos. | No |
| **U-010** | Si el formato de diagnóstico debe replicar literalmente `(Encontrado: '...')` del parser. | Afecta a la comparación byte a byte de CA-001. | `Parser.cpp:83-85` vs `doc.md` §0 | Formato exacto. | No |
| **U-011** | Si `docs.md` §4.1 debe reescribirse o basta la nota de actualización ya añadida. | Las filas describen defectos ya corregidos; se conservaron como registro histórico. | `docs.md` §4.1 (nota del 2026-10-29) | Nivel de detalle del expediente. | No |

### 10.3 Correspondencia con las dudas de `docs.md`

| Duda | Estado |
|---|---|
| Q-01 carpeta | Resuelta por `DU-003` |
| Q-02 invocación | Resuelta por `DU-004` y `DU-005` |
| Q-03 anotar tipos en el AST | Resuelta: no se anota; el AST es de solo lectura (`RF-001`) |
| Q-04 errores previos | Resuelta por `RF-041` |
| Q-05 `main` obligatorio | Ratificada: sí (`RF-008`) |
| Q-06 sobrecarga | Ratificada: no (`RF-013`) |
| Q-07 conversiones | Fijadas en `RF-017` |
| Q-08 códigos de salida | Fijados en §5.4 |
| Q-09 formato de diagnósticos | Fijado en §5.3; el detalle literal es `U-010` |
| Q-10 `doc.md` vs `AGENTS.md` | Resuelta por `DU-001` |
| Q-11 y Q-12 pruebas | Resueltas por `DU-004` |
| Q-13 `namespace` | Fijado en `RNF-010` |

---

## 11. Trazabilidad

| `doc.md` | `specs.md` |
|---|---|
| `RF-01`…`RF-40` | `RF-001`…`RF-040` (mismo orden) |
| `RNF-01`…`RNF-16` | `RNF-001`…`RNF-016` (mismo orden) |
| `INV-01`…`INV-04` | `INV-A-001`…`INV-A-004` |
| `INV-05`…`INV-19` | `INV-D-001`…`INV-D-015` |
| `CL-01`…`CL-54` | `CL-001`…`CL-054` (mismo orden) |
| `CA-01`, `CA-02` | `CA-001`, `CA-002`; se añaden `CA-003`…`CA-008` |
| Catálogo `E-00`…`E-29`, `W-01`…`W-08` | Se conserva sin cambios (§5.3) |

Requisitos nuevos sin origen en `doc.md`: `RF-041`, `RF-042`, `INT-001`…`INT-008`, `PRE-001`…`PRE-003`, `POST-001`…`POST-005`, `HER-001`…`HER-006`, `SUP-001`…`SUP-005`.

---

## 12. Glosario

| Término | Significado |
|---|---|
| **AST** | Árbol de sintaxis abstracta: la representación del programa que produce el parser. |
| **Nodo** | Cada elemento del AST (`VarDecl`, `BinaryExpr`, etc.). |
| **Ámbito** | Región donde un nombre es visible (global, bloque, cuerpo de función, `for`). |
| **Símbolo** | Entrada de la tabla: una declaración con nombre y tipo. |
| **L-Value** | Expresión que designa un almacenamiento y puede recibir una asignación. |
| **Ensanchamiento / estrechamiento** | Conversión que no pierde precisión / que puede perderla. |
| **Compuerta** | Condición que impide avanzar a la fase siguiente si hay errores. |
| **Diagnóstico** | Mensaje con severidad, código y posición. |
| **Cascada** | Serie de errores derivados de uno solo; la fase debe evitarla. |
| **Precondición / postcondición** | Lo que debe cumplirse antes / después de ejecutar la fase. |

---

## Anexo A — Correcciones a `doc.md`

`doc.md` no se modifica (solo lectura). Estas son las afirmaciones que ya no coinciden con el código, con la evidencia actual.

| ID | Afirmación de `doc.md` | Estado observado |
|---|---|---|
| **K-01** | `AGENTS.md` §3.3 permite `main.cpp`; §7 lo prohíbe. | Contradicción resuelta por `DU-005` y registrada en `AGENTS.md` §7. |
| **K-02** | V-01: `AST.cpp` no está en ninguna biblioteca. | Falso: está en `ParserLib` (`Parser/CMakeLists.txt:5`) y enlaza. |
| **K-03** | V-11: `main` siempre retorna 0. | Falso: devuelve `0/1/2` (`main.cpp:12-14,58,70,78,85`). |
| **K-04** | RF-01/03: el parser no revela errores. | El `Parser` sí los revela (`Parser.hpp:18`); el `ProgramNode` no. |
| **K-05** | INV-02 garantiza "ubicación exacta". | Se cumple en sentido literal, no garantiza la posición del nombre (`V-019`). |
| **K-06** | RNF-07: "una sola visita por nodo" frente a dos pasadas. | Corregido en `RNF-007`. |
| **K-07** | RNF-12/16 exigen MSVC, `/W4`, `x64-Debug`. | No verificable en el entorno actual (`V-018`, `SUP-001`, `SUP-002`). |
| **K-08** | RNF-06 exige ASan/valgrind. | `U-007`. |
| **K-09** | CA-01/CA-02 alimentan `compiler_app` con archivos. | `main` no lee archivos ni `argv` (`V-016`); resuelto por `DU-004`. |
| **K-10** | `docs.md` lista defectos ya corregidos. | `docs.md` se actualiza en la misma fecha (ver nota allí). |
| **K-11** | Citas de línea de `doc.md`. | Obsoletas por las correcciones aplicadas; aquí se citan líneas verificadas. |
