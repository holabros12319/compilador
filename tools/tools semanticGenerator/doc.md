# Fase de Análisis Semántico — Requisitos, Invariantes, Casos Límite y Criterios de Aceptación

**Proyecto:** `SimpleCompiler` — compilador dirigido de un subconjunto de C++, implementado en C++17 (MSVC / CMake).
**Fase documentada:** Análisis Semántico (siguiente a Léxico y Sintáctico).
**Base del documento:** código real de `Lexer/`, `Parser/` y `app/` (verificación en la sección 1).

---

## 0. Convenciones de este documento

| Prefijo | Significado |
|---|---|
| `RF-xx` | Requisito funcional (qué debe hacer la fase) |
| `RNF-xx` | Requisito no funcional (cómo debe hacerlo) |
| `INV-xx` | Invariante (condición que se cumple en todo momento) |
| `CL-xx` | Caso límite |
| `CA-xx` | Criterio de aceptación |
| `E-xx` | Diagnóstico de error semántico |
| `W-xx` | Diagnóstico de advertencia semántica |
| `V-xx` | Hallazgo de verificación del estado actual del proyecto |

Formato de diagnóstico obligatorio, heredado de `Parser::error` (`Parser/Parser.cpp:77`):

```
[Error Semántico] Línea <L>, Columna <C>: <mensaje> (<código>)
[Advertencia Semántica] Línea <L>, Columna <C>: <mensaje> (<código>)
```

Toda posición proviene de `ASTNode::ubicacion` (`Parser/AST.hpp:64`), es decir, de `Ubicacion{linea, columna}` (`Lexer/Tokens.hpp:37`). La fase **no** inventa posiciones ni usa la línea del token actual: usa la del nodo.

---

## 1. Verificación del estado actual del proyecto

### 1.1 Inventario verificado

| Archivo | Contenido relevante para la fase semántica |
|---|---|
| `Lexer/Tokens.hpp` | `TipoToken` (9 categorías), `Ubicacion`, `Token` |
| `Lexer/Regex.hpp` | 40 operadores, 84 palabras reservadas (C++ completo), delimitadores `()[]{};,.` |
| `Lexer/Lexer.cpp` | Emite `ERROR_LEXICO` y siempre cierra con `FIN_ARCHIVO` (`Lexer.cpp:277`) |
| `Parser/AST.hpp` | `ASTNode` (base), `Expr`, `Stmt`, `Decl`, 16 nodos, `ASTVisitor`, `NodeKind`, `ExprPtr/StmtPtr/DeclPtr` |
| `Parser/AST.cpp` | `nodeKindToString`, `ASTNode::pad`, todos los `print()` |
| `Parser/Parser.hpp/.cpp` | Descenso recursivo con precedencia, `parse()` → `unique_ptr<ProgramNode>`, recuperación con `synchronize()` |
| `app/main.cpp` | Lexer → Parser → `ast->toString()`; **no ejecuta fase semántica** |

### 1.2 Hallazgos que condicionan los requisitos

| ID | Hallazgo | Evidencia |
|---|---|---|
| **V-01** | `AST.cpp` **no está compilado por ninguna biblioteca ni ejecutable**: `ParserLib` solo lista `Parser.cpp`/`Parser.hpp`, `LexerLib` y `compiler_app` tampoco lo incluyen. Todo `print()` y `nodeKindToString` queda sin definición → error de enlace en `ast->toString()`. | `Parser/CMakeLists.txt:1-4`, `Lexer/CMakeLists.txt:1-6`, `app/CMakeLists.txt:1-3`, uso en `app/main.cpp:33` |
| **V-02** | El árbol puede ser **parcial**: `parse()` captura `ParseError` por declaración y sincroniza, devolviendo un `ProgramNode` válido pero incompleto. | `Parser/Parser.cpp:103-114` |
| **V-03** | La fase sintáctica **no rechaza** `ERROR_LEXICO`: no consulta el tipo del token, por lo que un error léxico se manifiesta como error sintáctico derivado. | `Parser.cpp:31-34` (`check` solo compara `tipo`) |
| **V-04** | Solo se aceptan **tipos de una palabra** en declaraciones: `int`, `float`, `double`, `void`. `char`, `bool`, `long`, `unsigned`, etc. son `PALABRA_RESERVADA` pero no inician declaración local. | `Parser.cpp:118-120` |
| **V-05** | En **campos de `struct`** y en **parámetros** sí se acepta *cualquier* `PALABRA_RESERVADA` como tipo (p. ej. `static`, `auto`, `const`, `char`, `unsigned`). | `Parser.cpp:132`, `Parser.cpp:153` |
| **V-06** | **No existe tipo struct expresable**: un campo/parámetro/variable de tipo struct requeriría un `IDENTIFICADOR` como tipo, y el parser exige `PALABRA_RESERVADA`. Por tanto `MemberExpr` (`.` / `->`) nunca puede tener una base de tipo struct válida. | `Parser.cpp:132`, `Parser.cpp:153`, `Parser.cpp:381-385` |
| **V-07** | **No hay punteros, arrays, `sizeof`, casts, bitwise, ternario, `break`, `continue`, `do`, `switch`** en el AST ni en el parser, aunque el lexer los tokenice. | `AST.hpp:96-291`, `Parser.cpp:180-187`, `Parser.cpp:313-401` |
| **V-08** | La asignación **no tiene nodo propio**: es `BinaryExpr` con `op == "="`; el parser valida el L-Value con `NodeKind` (sin RTTI). | `Parser.cpp:268-281` |
| **V-09** | Las declaraciones **son sentencias**: `Decl : public Stmt`, y `ProgramNode::declarations` es `vector<StmtPtr>`. El parser además admite sentencias sueltas en ámbito global. | `AST.hpp:86-90`, `AST.hpp:286`, `Parser.cpp:122` |
| **V-10** | `ASTVisitor` no tiene implementaciones por defecto: la fase semántica debe implementar **los 16 `visit()`**. | `AST.hpp:30-49` |
| **V-11** | `main.cpp` **siempre retorna 0** y no distingue fases fallidas; hoy no hay contrato de código de salida. | `app/main.cpp:38` |
| **V-12** | `CMakeCache.txt` existe pero sin compilador resuelto (`CMAKE_CXX_COMPILER-NOTFOUND`): la configuración del proyecto no está consolidada; la fase semántica debe poder configurarse desde cero. | `out/build/x64-Debug/CMakeCache.txt:27` |

### 1.3 Subconjunto real del lenguaje de entrada

Lo que la fase semántica **puede** recibir hoy, ni más ni menos:

```
programa    ::= { declaracion | sentencia }
declaracion ::= "struct" ID "{" { tipo_reservado ID [ "=" expresion ] ";" } "}" ";"
              | ("int"|"float"|"double"|"void") ID ( "(" [ params ] ")" ( ";" | bloque ) | [ "=" expresion ] ";" )
params      ::= tipo_reservado ID { "," tipo_reservado ID }
sentencia   ::= bloque | "if" "(" expresion ")" sentencia [ "else" sentencia ]
              | "while" "(" expresion ")" sentencia
              | "for" "(" [ declaracion_var | expresion ";" ] [ expresion ] ";" [ expresion ] ")" sentencia
              | "return" [ expresion ] ";"
              | expresion ";"
expresion   ::= asignacion
asignacion  ::= logico_o [ "=" asignacion ]        // L-Value: ID o miembro, según Parser.cpp:268-281
logico_o    ::= logico_y { "||" logico_y }
logico_y    ::= igualdad { "&&" igualdad }
igualdad    ::= comparacion { ("==" | "!=") comparacion }
comparacion ::= termino { ("<" | ">" | "<=" | ">=") termino }
termino     ::= factor { ("+" | "-") factor }
factor      ::= unario { ("*" | "/" | "%") unario }
unario      ::= ("!" | "-" | "+" | "++" | "--") unario | primario [ "++" | "--" ]
primario    ::= NUMERO | CADENA | CARACTER | ID { "(" args ")" | ("." | "->") ID } | "(" expresion ")"
args        ::= [ expresion { "," expresion } ]
bloque      ::= "{" { declaracion | sentencia } "}"
```

Consecuencia directa: **la semántica debe rechazar el mundo C++ amplio que el lexer reconoce** y aceptar exactamente este subconjunto, sin ambigüedad.

---

## 2. Requisitos funcionales

### 2.1 Preparación, contrato y resultado de la fase

| ID | Requisito |
|---|---|
| **RF-01** | Debe existir una clase `SemanticAnalyzer` en el módulo `Semantic/`, con API `ResultadoSemantico analyze(const ProgramNode& programa, const ContextoEntrada& contexto)`, sin modificar el AST (recorrido `const`). `ContextoEntrada` transporta `erroresLexicos` y `erroresSintacticos` (necesario para RF-03), porque el `ProgramNode` por sí solo no revela que hubo recuperación de errores (V-02). |
| **RF-02** | `ResultadoSemantico` debe exponer: `bool ok()`, `const std::vector<Diagnostico>& diagnosticos()`, contadores `errores()`/`advertencias()`, y `const TablaSimbolos& tabla()`. |
| **RF-03** | La fase debe validar su **precondición**: si el flujo de tokens contiene `TipoToken::ERROR_LEXICO` o si la fase sintáctica reportó errores, debe emitir `E-00` y **no** analizar, para evitar diagnósticos en cascada sobre un árbol parcial (V-02, V-03). |
| **RF-04** | La fase debe implementar `ASTVisitor` completo (16 métodos, V-10), despachando por `NodeKind` cuando necesite decisiones de tipo sin `dynamic_cast` (estilo `Parser.cpp:273-277`). |
| **RF-05** | El análisis debe ser de **dos pasadas**: (1) recolección de símbolos globales (`FunctionDecl`, `StructDecl`) y (2) verificación de cuerpos y declaraciones locales. Ningún uso puede resolverse antes de que su símbolo global esté registrado, salvo la recursión (RF-27). |
| **RF-06** | Debe existir una `TablaSimbolos` con ámbitos anidados (pila de mapas ordenados), consulta `buscar(nombre)` de adentro hacia afuera e inserción que detecte colisión **solo en el ámbito actual**. |
| **RF-07** | Debe existir un modelo de tipos explícito (`enum class TipoBase { Int, Float, Double, Char, Bool, Void, Cadena, Desconocido, Error }`) con nombre, categoría (numérico/void/cadena), compatibilidad y conversión implícita. El tipo `Error` es compatible con todo y **nunca** genera diagnósticos adicionales: es el mecanismo anti-cascada de INV-19. |

### 2.2 Programa y ámbitos

| ID | Requisito |
|---|---|
| **RF-08** | Debe existir exactamente una función `main` con firma `int main()`; su ausencia, duplicado, tipo distinto de `int` o parámetros → `E-01`/`E-02`/`E-03`. |
| **RF-09** | Todo elemento de `ProgramNode::declarations` debe ser una declaración (`VarDecl`, `FunctionDecl`, `StructDecl`); una sentencia ejecutable en ámbito global → `E-24` (V-09). |
| **RF-10** | Cada bloque `{ }`, cuerpo de función, `for` y el ámbito global constituyen un ámbito propio. La variable de inicialización de un `for` vive solo en ese `for` (condición, incremento y cuerpo). |
| **RF-11** | Redeclarar un nombre en el **mismo** ámbito (variable, función, struct, parámetro) → `E-03`. El sombreado en un ámbito anidado es legal y no genera diagnóstico. |
| **RF-12** | Toda variable debe declararse **antes** de su uso; uso previo → `E-05`; uso inexistente → `E-04`. Los parámetros cuentan como declarados al inicio del cuerpo y comparten ámbito con el bloque de nivel superior del cuerpo (redeclararlos → `E-03`). |
| **RF-13** | Un nombre no puede designar simultáneamente una función y una variable/struct en el mismo ámbito; el espacio de nombres es único (sin sobrecarga, sin `struct`/variable homónimos). |

### 2.3 Tipos y expresiones

| ID | Requisito |
|---|---|
| **RF-14** | Solo son válidos los tipos `int`, `float`, `double`, `char`, `bool`, `void` (según contexto) y los nombres de `struct` declarados. Cualquier otra `PALABRA_RESERVADA` usada como tipo (p. ej. `static`, `auto`, `const`, `unsigned`, `long`) en campo o parámetro → `E-06` (V-05). |
| **RF-15** | `void` solo es válido como tipo de retorno; como variable local, parámetro o campo → `E-07`/`E-08`. |
| **RF-16** | Clasificación de literales numéricos a partir del lexema crudo (`LiteralExpr::value`): con `.` o `e`/`E` → `Double`; con sufijo `f`/`F` → `Float`; en otro caso `Int`. Los sufijos `u/U/l/L` no modelan un tipo distinto: se ignora el sufijo y se emite `W-03`. |
| **RF-17** | Conversiones implícitas: `Int → Float → Double` es ensanchamiento y no genera diagnóstico; `Int/Float/Double → Bool` se permite en contexto de condición (0 = falso); `Char → Int` es promoción numérica permitida. `Double → Float` es estrechamiento permitido con `W-02` (así `float pi = 3.1416;` de `app/main.cpp:9` sigue siendo válido). `Double/Float → Int` y `Int → Char` → `E-13`. |
| **RF-18** | Operadores aritméticos `+ - * /` exigen operandos numéricos; `%` exige enteros; `== != < > <= >=` exigen operandos de la misma familia numérica; `&& \|\|` exigen operandos convertibles a `Bool`; `!` exige convertible a `Bool`; `+ -` unarios exigen numérico. Incumplimiento → `E-14`/`E-15`. |
| **RF-19** | `++`/`--` (prefijo y postfijo) y el lado izquierdo de `=` exigen un destino asignable: `IdentifierExpr` que resuelva a **variable** o `MemberExpr` sobre struct. Literal, llamada o nombre de función → `E-12`. El tipo del resultado de `=` es el tipo del destino, y el valor asignado debe ser compatible según RF-17 (`E-13`). |
| **RF-20** | `IdentifierExpr`: si resuelve a función o struct en un contexto de valor → `E-19`. Si resuelve a variable, su tipo es el declarado. |
| **RF-21** | `CallExpr`: el `callee` debe resolver a `FunctionDecl`; si es variable, literal o el resultado de otra llamada (`f()()`, permitido por `Parser.cpp:370-380`) → `E-16`. Debe verificarse aridad exacta (`E-17`) y compatibilidad de cada argumento (`E-18`). |
| **RF-22** | `MemberExpr`: el tipo del objeto debe ser un struct declarado (`E-20`), el miembro debe existir en él (`E-21`) y `isPointer == true` (`->`) exige base de tipo puntero, inexistente en el lenguaje actual → `E-22`. Se documenta como limitación conocida que hoy ningún objeto puede tener tipo struct (V-06); la regla se implementa para cuando el parser se amplíe. |
| **RF-23** | Literales: entero fuera del rango de `int` (`-2147483648..2147483647`) → `E-25`; literal de carácter cuya cadena procesada no tenga exactamente 1 byte → `E-26`; literal de cadena `Cadena` solo es válido como expresión descartada (`W-07`) y en cualquier contexto que exija tipo numérico/void → `E-13`. |

### 2.4 Funciones

| ID | Requisito |
|---|---|
| **RF-24** | `return` con valor en función `void` → `E-09`; `return;` en función no `void` → `E-10`. |
| **RF-25** | Toda función no `void` debe garantizar retorno por **regla estructural verificable**: la última sentencia del cuerpo es `ReturnStmt` con valor, o un `IfStmt` con `else` donde ambas ramas la satisfacen, o un `BlockStmt` cuya última sentencia la satisface. En caso contrario → `E-11`. |
| **RF-26** | Prototipo (`isPrototype() == true`) y definición con el mismo nombre deben coincidir en tipo de retorno y en la secuencia `(tipo, cantidad)` de parámetros; discrepancia → `E-23`. Un prototipo nunca definido es legal y emite `W-05`. |
| **RF-27** | La recursión directa e indirecta es legal: la función se registra en la pasada 1 antes de verificar su cuerpo (RF-05). Llamada a función declarada más adelante en el archivo → válida. |

### 2.5 Sentencias

| ID | Requisito |
|---|---|
| **RF-28** | La condición de `if`, `while` y `for` debe ser convertible a `Bool` (RF-17); si no → `E-14`. La omisión de la condición en `for (;;)` es legal y equivale a verdadero. |
| **RF-29** | Tras un `return` en el mismo bloque, las sentencias siguientes son inalcanzables → `W-01` (una sola advertencia por tramo, no una por sentencia). |
| **RF-30** | Toda sentencia de expresión debe tener efecto: `LiteralExpr` o `IdentifierExpr` descartados → `W-07`. Las expresiones con llamada, asignación o `++/--` tienen efecto y no advierten. |

### 2.6 Structs

| ID | Requisito |
|---|---|
| **RF-31** | Un struct sin campos es legal pero emite `W-06`; campo duplicado → `E-03`; campo `void` → `E-08`; nombre de struct duplicado → `E-03`. |
| **RF-32** | El inicializador de un campo (`struct S { int x = 1; };`, permitido por `Parser.cpp:134`) debe ser una **expresión constante**: literal o composición unaria/binaria de literales, sin identificadores ni llamadas; en caso contrario → `E-27`. |
| **RF-33** | En evaluación constante (RF-32), `/` o `%` con divisor literal cero → `E-28`. En expresiones no constantes, la división por cero es un asunto de ejecución y **no** es error semántico. |

### 2.7 Diagnóstico e integración

| ID | Requisito |
|---|---|
| **RF-34** | Todo diagnóstico debe llevar código (`E-xx`/`W-xx`), mensaje en español, línea y columna del nodo implicado, y emitirse a un `std::ostream&` inyectable (por defecto `std::cerr`, como `Parser::error`). |
| **RF-35** | Los diagnósticos deben emitirse en orden de aparición en el código fuente (línea ascendente, y columna ascendente a igual línea) y ser **deterministas** entre ejecuciones (INV-15). |
| **RF-36** | Debe existir un límite configurable de errores (por defecto 100) para evitar cascadas; al alcanzarlo se detiene el análisis, se emite `E-29` y el resultado queda `ok() == false`. |
| **RF-37** | Si `ok() == true` no debe emitirse ningún `E-xx`; si `ok() == false` el programa **no** puede pasar a generación de código (compuerta obligatoria). |
| **RF-38** | `app/main.cpp` debe ejecutar la fase tras `parse()`, imprimir sus diagnósticos y devolver código de salida: `0` éxito, `1` error léxico, `2` error sintáctico, `3` error semántico (V-11). El volcado del AST solo se imprime si `ok()` es verdadero. |
| **RF-39** | Debe añadirse a CMake el módulo `Semantic/` (`add_subdirectory(Semantic)` en la raíz) con `SemanticLib` enlazado a `ParserLib`, y **`AST.cpp` debe incorporarse a `ParserLib`** para resolver V-01. |
| **RF-40** | La fase debe exponer una función de volcado de la tabla de símbolos y de los tipos inferidos (`dumpTabla(std::ostream&)`), reutilizando el estilo de `ASTNode::print`/`toString`, para verificación y depuración. |

### 2.8 Catálogo de diagnósticos

| Código | Severidad | Condición |
|---|---|---|
| `E-00` | Error | Precondición incumplida: errores léxicos o sintácticos previos (RF-03) |
| `E-01` | Error | Programa vacío o sin `main` |
| `E-02` | Error | `main` con firma distinta de `int main()` |
| `E-03` | Error | Redeclaración/duplicado en el mismo ámbito (variable, función, struct, parámetro, campo) |
| `E-04` | Error | Identificador no declarado |
| `E-05` | Error | Uso antes de la declaración |
| `E-06` | Error | Tipo no soportado por el compilador |
| `E-07` | Error | Variable de tipo `void` |
| `E-08` | Error | Campo/parámetro de tipo `void` |
| `E-09` | Error | `return` con valor en función `void` |
| `E-10` | Error | `return;` en función no `void` |
| `E-11` | Error | Falta de retorno garantizado (RF-25) |
| `E-12` | Error | Destino no asignable en `=`, `++` o `--` |
| `E-13` | Error | Tipos incompatibles en asignación/inicialización/argumento |
| `E-14` | Error | Operandos incompatibles en operador binario o condición |
| `E-15` | Error | Operando incompatible en operador unario |
| `E-16` | Error | Llamada sobre algo que no es función |
| `E-17` | Error | Aridad incorrecta |
| `E-18` | Error | Argumento con tipo incompatible |
| `E-19` | Error | Función o struct usado como valor |
| `E-20` | Error | Acceso a miembro sobre tipo no struct |
| `E-21` | Error | Miembro inexistente |
| `E-22` | Error | `->` sobre base no puntero |
| `E-23` | Error | Conflicto prototipo/definición |
| `E-24` | Error | Sentencia ejecutable en ámbito global |
| `E-25` | Error | Literal entero fuera de rango |
| `E-26` | Error | Literal de carácter inválido |
| `E-27` | Error | Inicializador de campo no constante |
| `E-28` | Error | División/módulo por cero en expresión constante |
| `E-29` | Error | Límite de errores alcanzado |
| `W-01` | Advertencia | Código inalcanzable tras `return` |
| `W-02` | Advertencia | Estrechamiento `double → float` |
| `W-03` | Advertencia | Sufijo numérico `u/U/l/L` ignorado |
| `W-04` | Advertencia | Variable declarada y no usada |
| `W-05` | Advertencia | Prototipo sin definición |
| `W-06` | Advertencia | `struct` vacía |
| `W-07` | Advertencia | Expresión sin efecto |
| `W-08` | Advertencia | Literal real fuera del rango representable |

---

## 3. Requisitos no funcionales

| ID | Requisito |
|---|---|
| **RNF-01** | **Lenguaje y estándar:** C++17, coherente con `CMakeLists.txt:5-6`; sin extensiones de compilador. |
| **RNF-02** | **Sin RTTI:** prohibido `dynamic_cast`/`typeid`; las decisiones de tipo usan `NodeKind` y `kind()` (estilo declarado en `Parser.cpp:267-277`). |
| **RNF-03** | **Sin excepciones para errores esperados:** los errores semánticos se acumulan en `ResultadoSemantico`, no se lanzan. `std::bad_alloc` y errores de programación quedan fuera del contrato. |
| **RNF-04** | **Const-correctness:** `analyze(const ProgramNode&, const ContextoEntrada&)`; el visitante recibe `const T&` (firma de `AST.hpp:33-48`); la fase no muta el AST ni reposiciona `unique_ptr`. |
| **RNF-05** | **Propiedad de memoria:** la tabla de símbolos guarda **observadores** (`const Decl*`, `const StructDecl*`), nunca `unique_ptr` ni `shared_ptr` del AST; el AST sigue siendo dueño único (INV-01). |
| **RNF-06** | **Sin fugas:** todas las estructuras internas usan RAII/contenedores estándar; compilación limpia bajo ASan/`_CrtSetDbgFlag` y valgrind. |
| **RNF-07** | **Complejidad:** una sola visita por nodo; O(n) en nodos y O(d) en profundidad de ámbitos, con `d` acotado por anidamiento real. Búsqueda de símbolos O(log s) o O(1) esperado; nunca O(n²) sobre todo el programa. |
| **RNF-08** | **Rendimiento:** un archivo de 10 000 líneas / 100 000 nodos debe analizarse en < 100 ms en `x64-Debug` sobre el equipo de referencia. |
| **RNF-09** | **Determinismo:** mismo AST → mismos diagnósticos, mismo orden y mismo texto; prohibido depender del orden de iteración de `unordered_map` en la salida (usar `std::map`/ordenamiento estable previo a emitir). |
| **RNF-10** | **Estilo del proyecto:** clases en `PascalCase`, métodos en `camelCase`, miembros `const` cuando aplica, guardas `#ifndef NOMBRE_HPP` (como `AST_HPP`, `PARSER_HPP`), `namespace {}` anónimo para ayudantes en `.cpp`, mensajes en español. |
| **RNF-11** | **Estructura de módulo:** carpeta `Semantic/` hermana de `Lexer/` y `Parser/`, con `SemanticAnalyzer.hpp/.cpp`, `SymbolTable.hpp`, `Types.hpp`, `Diagnostic.hpp` y `Semantic/CMakeLists.txt`; `SemanticLib` es biblioteca aparte para poder enlazarla desde pruebas. |
| **RNF-12** | **Compilación sin advertencias** con `/W4` (MSVC) y `-Wall -Wextra` (GCC/Clang) en los archivos nuevos; sin variables sin usar, sin conversiones con pérdida implícitas. |
| **RNF-13** | **Portabilidad:** solo biblioteca estándar; sin APIs de Windows ni POSIX en el módulo `Semantic/` (la E/S se inyecta por `std::ostream`). |
| **RNF-14** | **Tolerancia a fallos de entrada:** el módulo nunca debe abortar, entrar en bucle infinito ni lanzar por un AST malformado o parcial; ante un nodo con `kind()` inesperado en una posición, emite `E-06` y continúa. |
| **RNF-15** | **Testabilidad:** la fase debe poder ejecutarse sin `main` de la aplicación (biblioteca), con diagnósticos capturables en un `std::ostringstream` y sin estado global mutable (instancias independientes y reutilizables). |
| **RNF-16** | **Requisito de arranque:** la configuración CMake completa (`Configure` + `Build`) debe funcionar desde cero en `out/build/x64-Debug`, resolviendo V-01 y V-12 antes de considerar la fase terminada. |

---

## 4. Invariantes

Se clasifican por el momento en que deben cumplirse: **I** = sobre la entrada (AST), **D** = durante el análisis, **S** = sobre la salida (resultado y símbolos).

| ID | Tipo | Invariante |
|---|---|---|
| **INV-01** | I | **Propiedad única:** cada nodo del AST tiene exactamente un propietario (`unique_ptr`); no hay ciclos, ni nodos compartidos, ni propietarios dobles. La fase semántica no crea ni destruye nodos. |
| **INV-02** | I | **Ubicación válida:** para todo nodo alcanzable, `ubicacion.linea >= 1` y `ubicacion.columna >= 1`, y la posición corresponde a un token real del archivo fuente. |
| **INV-03** | I | **Alcanzabilidad:** todo nodo analizado es alcanzable desde la raíz `ProgramNode`; no se analizan nodos huérfanos ni se ignoran subárboles alcanzables. |
| **INV-04** | I | **Cobertura de la jerarquía:** `Decl` ⊆ `Stmt`; `ProgramNode` no es `Stmt`; `ProgramNode::declarations` contiene solo `StmtPtr` y, **tras** la fase, todos sus elementos son `VarDecl`, `FunctionDecl` o `StructDecl` (RF-09). |
| **INV-05** | D | **Un solo despacho por nodo:** en cada pasada, cada nodo recibe exactamente una visita; ninguna rama del visitante vuelve a visitar un hijo ya procesado. |
| **INV-06** | D | **Padres antes que hijos** en la recolección de símbolos (pasada 1) y **declaraciones antes que usos** en la verificación (pasada 2), salvo la recursión de funciones, explícitamente permitida (RF-27). |
| **INV-07** | D | **Ámbito consistente:** en todo instante, la pila de ámbitos refleja exactamente la ruta raíz→nodo actual; toda inserción y toda consulta ocurren en la cima de la pila correspondiente al nodo en proceso. |
| **INV-08** | D | **Clave única por ámbito:** no existen dos símbolos con el mismo nombre en el mismo ámbito; un nombre puede repetirse solo en ámbitos distintos (sombreado). |
| **INV-09** | D | **Resolución total:** para cada `IdentifierExpr`/`MemberExpr`/`CallExpr` procesado, o bien queda registrada exactamente una resolución (símbolo destino), o bien se emitió al menos un diagnóstico. Nunca ambas, nunca ninguna. |
| **INV-10** | D | **Tipo total:** `tipoDe(nodo)` está definido para todo `Expr` visitado, o el nodo produjo diagnóstico y su tipo es `Error`; no existen expresiones sin tipo ni tipos "adivinados" sin regla (RF-16 a RF-23). |
| **INV-11** | D | **Monotonía de errores:** `errores()` solo crece; un error nunca se degrada a advertencia ni se elimina; `ok()` es falso desde el primer `E-xx` y no vuelve a verdadero. |
| **INV-12** | D | **Diagnóstico con contrato:** cada diagnóstico tiene código válido del catálogo, mensaje no vacío, `linea >= 1`, `columna >= 1`; no existen dos diagnósticos idénticos (mismo código y misma ubicación) para el mismo nodo. |
| **INV-13** | D | **Terminación y ausencia de recursión desbordada:** el análisis de n nodos termina en un número de pasos acotado por c·n; la profundidad de recursión no excede la profundidad del AST ni un límite configurable (por defecto 256), y al alcanzarlo se emite `E-06` y se poda la rama. |
| **INV-14** | D | **Aislamiento de estado:** dos instancias de `SemanticAnalyzer` no comparten estado mutable; analizar dos programas seguidos con la misma instancia produce el mismo resultado que con instancias nuevas (sin contaminación de ámbitos ni de contadores). |
| **INV-15** | S | **Determinismo:** dos ejecuciones sobre el mismo AST producen la misma secuencia de diagnósticos, byte a byte, y la misma tabla de símbolos (RNF-09). |
| **INV-16** | S | **Integridad referencial de la tabla:** todo `Decl*`/`StructDecl*` registrado apunta a un nodo vivo del AST analizado; ninguna referencia queda colgando mientras el resultado y el AST coexistan (RNF-05). |
| **INV-17** | S | **Compuerta:** `ok() == true` ⟺ no se emitió ningún `E-xx`; y `ok() == true` es condición necesaria para generar código (RF-37). |
| **INV-18** | S | **Fidelidad de la tabla:** todo símbolo de la tabla corresponde a exactamente una declaración del AST, con el mismo nombre, tipo y ubicación; no hay símbolos sintéticos ni entradas eliminadas por error (salvo el cierre ordenado de ámbitos al salir). |
| **INV-19** | D | **No cascada:** si un nodo ya produjo un diagnóstico, sus ancestros no generan diagnósticos derivados del mismo motivo; el nodo afectado se tipa como `Error` (RF-07) y el análisis continúa. Un error en una expresión produce como máximo un diagnóstico propio, nunca una cadena de ellos. |

---

## 5. Casos límite

| ID | Situación | Entrada representativa | Comportamiento esperado |
|---|---|---|---|
| `CL-01` | Programa vacío | `""` | `E-01` (sin `main`), no `E-00` |
| `CL-02` | Sin `main`, con otras funciones | `int f() { return 1; }` | `E-01` |
| `CL-03` | `main` mal tipada | `void main() { }` / `int main(int argc)` | `E-02` |
| `CL-04` | Dos `main` | dos definiciones | `E-03` en la segunda |
| `CL-05` | Solo comentarios | `/* ... */` | `E-01` |
| `CL-06` | Variable `void` | `void x;` dentro de `main` | `E-07` |
| `CL-07` | Campo `void` | `struct S { void v; };` | `E-08` |
| `CL-08` | Tipo reservado no soportado en campo/parámetro | `struct S { static x; };` / `int f(auto a)` | `E-06` (el parser lo acepta: V-05) |
| `CL-09` | `void f() { return 1; }` | — | `E-09` |
| `CL-10` | `int f() { return; }` | — | `E-10` |
| `CL-11` | `int f() { int x = 1; }` | — | `E-11` + `W-04` (`x` sin usar) |
| `CL-12` | Retorno en una sola rama | `int f() { if (1) return 1; }` | `E-11` (regla estructural RF-25) |
| `CL-13` | `int f() { if (1) return 1; else return 2; }` | — | Válido |
| `CL-14` | Código tras `return` | `return 0; int x = 1;` | `W-01`, y `x` declarada pero no usada → `W-04` |
| `CL-15` | Redeclaración local | `int x; int x;` | `E-03` |
| `CL-16` | Sombreado anidado | `int x; { int x; }` | Legal, sin diagnóstico |
| `CL-17` | Parámetro redeclarado en el cuerpo | `int f(int a) { int a; }` | `E-03` |
| `CL-18` | Uso antes de declaración | `x = 1; int x;` | `E-05` |
| `CL-19` | Uso fuera de bloque | `{ int y; } return y;` | `E-04` |
| `CL-20` | Aridad incorrecta | `int f(int a); int main() { return f(); }` | `E-17` |
| `CL-21` | Llamada a variable | `int x; int main() { return x(); }` | `E-16` |
| `CL-22` | Llamada sobre resultado | `int f(); int main() { return f()(); }` | `E-16` (el parser lo produce: RF-21) |
| `CL-23` | Asignación a función | `int f(); int main() { f = 1; return 0; }` | `E-12` (el destino resuelve a función, no es asignable) |
| `CL-24` | Asignación incompatible | `int x; x = "hola";` | `E-13` |
| `CL-25` | Ensanchamiento | `float f = 1;` / `double d = 1;` | Válido |
| `CL-26` | Estrechamiento | `float pi = 3.1416;` | Válido + `W-02` (no debe romper `app/main.cpp`) |
| `CL-27` | Literal entero enorme | `int x = 9999999999999;` | `E-25` |
| `CL-28` | Sufijo numérico | `int x = 10u;` | Válido + `W-03` |
| `CL-29` | Carácter multibyte | `int x = 'ab';` | `E-26` únicamente: el literal queda con tipo `Error` y no se añade `E-13` |
| `CL-30` | Cadena en contexto numérico | `int x = "a";` | `E-13` |
| `CL-31` | Cadena descartada | `"a";` dentro de `main` | `W-07`, sin error |
| `CL-32` | `struct` vacía | `struct S { };` | `W-06` |
| `CL-33` | Campo duplicado | `struct S { int a; int a; };` | `E-03` |
| `CL-34` | Inicializador de campo no constante | `struct S { int a = f(); };` | `E-27` |
| `CL-35` | División constante por cero | `struct S { int a = 1/0; };` | `E-28` |
| `CL-36` | División por cero en tiempo de ejecución | `int main() { return 1/0; }` | Sin error semántico (RF-33) |
| `CL-37` | Acceso a miembro sobre primitivo | `float pi = 3.0; int main() { return pi.x; }` | `E-20` |
| `CL-38` | `->` sobre no puntero | `int x; int main() { return x->y; }` | `E-20` (precedencia documentada: primero base no-struct, luego `E-22`) |
| `CL-39` | Prototipo sin definición | `int g(int a); int main() { return 0; }` | `W-05` |
| `CL-40` | Prototipo/definición discrepantes | `int g(int a); int g(float a) { return 0; }` | `E-23` |
| `CL-41` | Función como valor | `int f(); int main() { int x = f; return 0; }` | `E-19` |
| `CL-42` | Sentencia global | `x = 1;` fuera de función | `E-24` |
| `CL-43` | Recursión directa | `int fact(int n) { if (n) return n * fact(n - 1); else return 1; }` | Válido |
| `CL-44` | Alcance de `for` | `for (int i = 0; i < 3; i++) { } return i;` | `E-04` en `return i` |
| `CL-45` | `for` sin condición | `int main() { for (;;) { return 0; } }` | Válido (la condición omitida equivale a verdadero) |
| `CL-46` | `if` sin `else`, `else` colgante | `if (a) if (b) x = 1; else x = 2;` | Válido, `else` asociado al `if` interno (ya resuelto por el parser) |
| `CL-47` | Variable no usada | `int x = 1; return 0;` | `W-04` |
| `CL-48` | Error léxico previo | `int main() { return 1; /* sin cerrar` | `E-00` y **no** análisis semántico (RF-03) |
| `CL-49` | Error sintáctico previo | `int main( { return 0; }` | `E-00`; nada de `E-04` en cascada |
| `CL-50` | Árbol malformado/parcial | `BlockStmt` con `nullptr` en `statements` (construcción manual en prueba) | No aborta: `E-06` y continúa (RNF-14) |
| `CL-51` | Anidamiento extremo | 10 000 bloques anidados | Termina o poda con `E-06` al superar el límite de profundidad (INV-13), sin desbordar la pila |
| `CL-52` | Repetición de análisis | analizar el mismo `ProgramNode` dos veces | Resultados idénticos (INV-14, INV-15) |
| `CL-53` | Nombre de un carácter / guion bajo | `int _; int x1;` | Válido |
| `CL-54` | Literales especiales | `.5`, `1e10`, `2.5E-3`, `3.14f`, `0x10` | `.5`/`1e10`/`2.5E-3` → `Double`; `3.14f` → `Float`; `0x10` no es hexadecimal para el lexer: entrega `0` (`NUMERO`) y `x10` (`IDENTIFICADOR`) → error sintáctico, no semántico |

> Casos de frontera, los más fáciles de implementar mal (frontera entre `E` y `W`, o entre error y silencio): `CL-11`, `CL-12`, `CL-17`, `CL-26`, `CL-31`, `CL-34`, `CL-38`, `CL-51`.

---

## 6. Criterios de aceptación

### CA-01 — Detección, clasificación y reporte determinista de errores semánticos

**Objetivo:** demostrar que la fase detecta el conjunto completo de faltas previstas, con el código correcto, la ubicación exacta, severidad adecuada y sin cascadas ni falsos positivos.

**Precondiciones:** `E-00` no aplica (programa léxica y sintácticamente limpio); build configurado desde cero (`Configure` + `Build` en `out/build/x64-Debug`); `compiler_app` ejecutable.

**Entrada (`caso_negativo.cpp`, debe pasar el lexer y el parser):**

```cpp
struct S { int a; int a; };

int f(int a);
float f(float b) { return b; }

void malo() { return 1; }

int sinRetorno() { int x = 1; }

int main() {
    int x = 1;
    int x = 2;
    x = "texto";
    y = 3;
    int v = 9999999999999;
    int c = 'ab';
    int z = f();
    x = f;
    x = 10u;
    return 0;
}
```

**Pasos:**
1. Ejecutar `compiler_app` sobre la entrada (o el binario de pruebas que use `SemanticAnalyzer`).
2. Capturar `stdout`, `stderr` y el código de salida.
3. Repetir la ejecución una segunda vez.
4. Comparar ambas salidas byte a byte.

**Resultado esperado:**
- Se emiten **exactamente 11 errores**, en este orden (línea: código): `1:E-03` (campo `a` duplicado), `4:E-23` (prototipo `int f(int)` vs definición `float f(float)`), `6:E-09` (`return 1;` en `void`), `8:E-11` (`sinRetorno` sin retorno), `12:E-03` (`int x` duplicado), `13:E-13` (`x = "texto"`), `14:E-04` (`y` no declarada), `15:E-25` (entero fuera de rango), `16:E-26` (`'ab'`), `17:E-17` (`f()` con aridad 0 frente a 1), `18:E-19` (`f` usada como valor).
- Las advertencias emitidas pertenecen **solo** a `{W-03, W-04}`: `19:W-03` (sufijo `u` ignorado) y `W-04` para `x` de `sinRetorno`, `v`, `c` y `z`.
- Ningún `E-04` derivado de `x = "texto"` ni de `x = f`, y ninguna cascada por el `int x` duplicado; los usos posteriores de `x` resuelven a la primera declaración (INV-09, INV-12).
- Código de salida `3`.
- `ok() == false` y el AST **no** se imprime (RF-38).
- La segunda ejecución produce una salida idéntica (INV-15).
- Cada línea cumple el formato `[Error Semántico] Línea L, Columna C: mensaje (E-xx)` o `[Advertencia Semántica] ... (W-xx)`.

**Criterio de fallo:** falta o sobra cualquier `E-xx` de la lista, aparece un `E-xx` distinto o una advertencia fuera de `{W-03, W-04}`, el orden no es el del fuente, el texto difiere entre ejecuciones o el código de salida es `0`.

---

### CA-02 — Aceptación de programa correcto, tabla de símbolos y compuerta

**Objetivo:** demostrar que un programa válido del subconjunto real (§1.3) no produce errores, que la tabla de símbolos y los tipos inferidos son correctos, y que la compuerta hacia la siguiente fase funciona.

**Precondiciones:** el programa pasa lexer y parser; `AST.cpp` ya está enlazado (V-01 resuelto).

**Entrada (`caso_positivo.cpp`):**

```cpp
struct Punto { int x = 0; int y = 0; };

int fact(int n);
double suma(int a, double b);

int fact(int n) {
    if (n <= 1) return 1;
    else return n * fact(n - 1);
}

double suma(int a, double b) {
    return a + b;
}

int main() {
    int i = 0;
    double total = 0;
    for (int k = 0; k < 3; k++) {
        total = total + k;
    }
    {
        int i = 5;
        total = total + i;
    }
    total = suma(i, total);
    if (total > 0) {
        return 1;
    } else {
        return 0;
    }
}
```

**Pasos:**
1. Ejecutar la fase semántica y capturar diagnósticos y código de salida.
2. Volcar la tabla de símbolos (`dumpTabla`, RF-40).
3. Repetir el análisis con una segunda instancia de `SemanticAnalyzer` sobre el mismo AST.

**Resultado esperado:**
- **Cero** diagnósticos de error; como máximo advertencias justificadas y ninguna de ellas `E-xx` (INV-17).
- Código de salida `0` y `ok() == true`.
- Tabla de símbolos (global): `Punto` (struct con 2 campos `int`), `fact` `int(int)`, `suma` `double(int,double)`, `main` `int()`.
- Tipos inferidos correctos: `a + b` → `double` y compatible con el retorno `double`; `total + k` → `double` (ensanchamiento `Int → Double`); `suma(i, total)` → `double`, asignable a `total`; `k` existe solo dentro del `for` (su salida del ámbito no genera `E-04` porque no se usa fuera); el `int i` interno sombrea al externo sin `E-03`.
- Sin `E-11` en `fact` ni en `main` (ambos satisfacen RF-25) ni en `suma`; sin `E-13` en `total = suma(...)` porque `double = double`; sin `W-02` en el programa.
- La segunda ejecución devuelve exactamente la misma tabla y los mismos diagnósticos (INV-14, INV-15).
- Bajo ASan/valgrind no se reportan fugas ni accesos inválidos (RNF-06).

**Criterio de fallo:** aparece cualquier `E-xx`, el código de salida no es `0`, la tabla difiere en nombres/tipos/firmas de lo indicado, se reporta una fuga, o el resultado cambia entre ejecuciones.

---

## 7. Trazabilidad resumida

| Área | Requisitos | Invariantes | Casos límite | Criterio |
|---|---|---|---|---|
| Precondición y árbol parcial | RF-03, RNF-14 | INV-01–INV-04, INV-13 | CL-48–CL-51 | CA-01 (precondición), CA-02 |
| Ámbitos y símbolos | RF-06, RF-10–RF-13 | INV-07–INV-09, INV-16, INV-18 | CL-15–CL-19, CL-44 | CA-01, CA-02 |
| Tipos y expresiones | RF-07, RF-14–RF-23 | INV-10, INV-19 | CL-24–CL-31, CL-37, CL-38, CL-54 | CA-01 |
| Funciones y retorno | RF-24–RF-27 | INV-06 | CL-09–CL-13, CL-39–CL-41, CL-43 | CA-01, CA-02 |
| Structs | RF-31–RF-33 | INV-10 | CL-07, CL-08, CL-32–CL-36 | CA-01 |
| Diagnóstico y determinismo | RF-34–RF-37, RNF-09 | INV-11, INV-12, INV-15 | CL-01–CL-05 | CA-01 |
| Integración y build | RF-38, RF-39, RNF-16 | INV-17 | — | CA-02 |

---

## 8. Limitaciones conocidas que la fase semántica debe declarar (no ocultar)

1. **Ningún tipo struct es expresable como variable, parámetro o campo** (V-06): `MemberExpr` se implementa y se valida, pero todo uso actual producirá `E-20`. Es una restricción del parser, no de la fase semántica.
2. **Sin punteros ni arrays** (V-07): `->`, `*` unario de indirección, `[]`, `sizeof` y todo lo que dependa de ellos queda fuera de alcance; si el parser se amplía, la tabla de tipos ya contempla `Puntero` como extensión prevista, pero no se implementa ahora.
3. **Sin `char`/`bool` locales** (V-04): se aceptan en parámetros y campos (V-05); el modelo de tipos los contempla para no rechazar programas que el parser sí produce.
4. **Sin sobrecarga de funciones** (RF-13): mismo nombre ⇒ mismo símbolo; dos firmas distintas ⇒ `E-03`/`E-23`.
5. **Sin análisis de flujo completo** (RF-25): el retorno garantizado se verifica estructuralmente; `while (1) { }` sin `return` posterior no se reconoce como terminante y produce `E-11`, decisión deliberada por simplicidad y previsibilidad.
6. **`0x10` no es un literal hexadecimal** para el lexer actual (V-07/CL-54): la fase semántica no puede corregirlo; se documenta como error sintáctico.
