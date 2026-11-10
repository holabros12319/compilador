# integration.md — Diseño de la fase de Análisis Semántico

**Proyecto:** `Compiler` — compilador de un subconjunto de C++ escrito en C++17.
**Fase diseñada:** Análisis Semántico, posterior a Léxico y Sintáctico.
**Documento normativo de referencia:** `specs.md` (requisitos aprobados).
**Estado:** propuesta de diseño, 2026-10-29.
**Alcance de este documento:** describir cómo se satisface `specs.md` e integrarse con el código existente. **No implementa código, no modifica código y no afirma que algo compile o funcione**: la fase no existe todavía.

---

## 0. Control del documento

### 0.1 Autoridad

| Documento | Papel frente a este diseño |
|---|---|
| `specs.md` | **Normativo.** Si el diseño y `specs.md` discrepan, manda `specs.md` y este documento se corrige. |
| `integration.md` (este) | **Propuesta de diseño.** No crea requisitos; propone cómo cumplirlos y qué decisiones faltan. |
| `docs.md` | **No normativo.** Antecedente de defectos y riesgos. |
| `doc.md` | **No normativo.** Antecedente de requisitos y catálogo de diagnósticos. Solo lectura. |
| `AGENTS.md` | **Normativo para el proceso** (permisos, clasificación de hallazgos). |
| Código fuente | **Evidencia primaria.** Toda afirmación se respalda con `archivo:línea` verificada. |

### 0.2 Identificadores

| Prefijo | Significado | Ámbito |
|---|---|---|
| `RF-xxx`, `RNF-xxx`, `CA-xxx`, `CL-xxx`, `N-xxx` | Requisitos y casos de `specs.md` | Se referencian, no se redefinen |
| `INV-A-xxx`, `INV-D-xxx`, `PRE-xxx`, `POST-xxx`, `SUP-xxx`, `HER-xxx`, `INT-xxx`, `DU-xxx`, `U-xxx`, `V-xxx` | Contratos y hechos de `specs.md` | Se referencian |
| `DIS-xx` | Decisión de diseño propuesta en este documento | Local; se ratifica como `DU-xxx` o queda como `U-xxx` |
| `HAL-xx` | Hallazgo de la investigación que no estaba en `specs.md` | Local |
| `RSG-xx` | Riesgo identificado | Local |
| `LIM-xx` | Limitación aceptada | Local |
| `TEST-xx` | Prueba planificada | Local |

### 0.3 Regla de evidencia

Toda afirmación sobre el proyecto cita `archivo:línea` verificada en esta sesión. Lo que no se pudo verificar se marca explícitamente como **no verificado** (`SUP-001`, `SUP-002`, `U-007`) y no se usa como base de ninguna decisión.

### 0.4 Estado de verificación

- Investigación del AST, del parser, del lexer y de CMake: **hecha por inspección directa**.
- Simulación manual del diseño sobre los programas de `CA-001` y `CA-002`: **hecha sobre el texto**, no ejecutada.
- Build de la fase: **no existe**; no se declara compilación.
- Pruebas de la fase: **planificadas** (§6), no ejecutadas.

---

## 1. Investigación del AST real

### 1.1 Clase base y tipos de nodo

`ASTNode` es la base de todo nodo (`Parser/AST.hpp:62-82`):

| Elemento | Detalle verificado |
|---|---|
| Ubicación | `const Ubicacion ubicacion` (`AST.hpp:64`); `Ubicacion` son dos `int`, `linea` y `columna` (`Lexer/Tokens.hpp:37-40`) |
| Identificación | `virtual NodeKind kind() const = 0` (`AST.hpp:75`) |
| Recorrido | `virtual void accept(ASTVisitor& visitor) const = 0` (`AST.hpp:76`) |
| Depuración | `virtual std::string print(int indent) const = 0` y `toString()` (`AST.hpp:77-78`) |
| Copia | Eliminada (`AST.hpp:69-70`) |
| Destrucción | `virtual ~ASTNode() = default` (`AST.hpp:67`) |

Jerarquía: `Expr : ASTNode` y `Stmt : ASTNode` (`AST.hpp:84-85`); `Decl : Stmt` (`AST.hpp:86-90`). **Consecuencia:** una declaración es también una sentencia, así que puede aparecer en cualquier lista de sentencias.

`NodeKind` tiene 16 valores (`AST.hpp:52-57`): `Program`, `FunctionDecl`, `StructDecl`, `VarDecl`, `BlockStmt`, `IfStmt`, `WhileStmt`, `ForStmt`, `ReturnStmt`, `ExprStmt`, `LiteralExpr`, `IdentifierExpr`, `UnaryExpr`, `BinaryExpr`, `CallExpr`, `MemberExpr`.

`ASTVisitor` declara 16 `visit` puros (`AST.hpp:30-49`), todos con parámetro `const T&` y retorno `void`.

### 1.2 Relaciones y estructura del árbol

| Nodo | Campos que lo componen | Evidencia |
|---|---|---|
| `ProgramNode` | `std::vector<StmtPtr> declarations` | `AST.hpp:284-291` |
| `FunctionDecl` | `returnType`, `vector<Param> parameters`, `unique_ptr<BlockStmt> body`, `isPrototype()` | `AST.hpp:261-272` |
| `StructDecl` | `vector<unique_ptr<VarDecl>> fields` | `AST.hpp:274-282` |
| `VarDecl` | `type`, `initializer` | `AST.hpp:243-252` |
| `BlockStmt` | `vector<StmtPtr> statements` | `AST.hpp:179-187` |
| `IfStmt` | `condition`, `thenBranch`, `elseBranch` | `AST.hpp:198-208` |
| `WhileStmt` | `condition`, `body` | `AST.hpp:210-219` |
| `ForStmt` | `init` (`StmtPtr`), `condition`, `increment`, `body` | `AST.hpp:221-232` |
| `ReturnStmt` | `value` (opcional) | `AST.hpp:234-241` |
| `ExprStmt` | `expression` | `AST.hpp:189-196` |
| `LiteralExpr` | `value` (lexema), `literalType` (`TipoToken`) | `AST.hpp:98-109` |
| `IdentifierExpr` | `name` | `AST.hpp:111-120` |
| `UnaryExpr` | `op`, `operand`, `isPrefix` | `AST.hpp:122-134` |
| `BinaryExpr` | `left`, `op`, `right` | `AST.hpp:136-148` |
| `CallExpr` | `callee`, `vector<ExprPtr> arguments` | `AST.hpp:150-161` |
| `MemberExpr` | `object`, `member`, `isPointer` | `AST.hpp:164-176` |

`Param` **no es un nodo** (`AST.hpp:254-259`): es una estructura con `type`, `name` y `ubicacion`. Ningún `visit` lo recibe; sus datos solo son accesibles desde `FunctionDecl`.

### 1.3 Creación, propiedad y destrucción

- El parser crea los nodos con `std::make_unique` y los entrega en `std::unique_ptr` (`Parser.cpp:110`, `148`, `168`, `179`, `184`, `203`, `216`, `227`, `252`, `260`, `267`, `283`, `299`, `309`, `319`, `329`, `339`, `349`, `358`, `365`, `374`, `379`, `390`, `395`).
- La propiedad es única y descendente: `ProgramNode` es dueño de todo el árbol; los `unique_ptr` intermedios transfieren propiedad en la construcción (`AST.hpp:92-94`).
- No hay subárboles compartidos ni ciclos: la copia está eliminada (`AST.hpp:69-70`), lo que impide duplicar un nodo con sus hijos.
- Destrucción: al destruirse el `ProgramNode` se destruyen recursivamente todos los nodos. No hay destructores manuales ni memoria sin dueño.

**Consecuencia para la fase:** la fase es observadora. No puede retener nodos más allá de la vida del AST (`PRE-001`, `INV-D-016`) ni asumir propiedad (`RNF-005`).

### 1.4 Mecanismos de recorrido existentes

| Mecanismo | Estado | Evidencia |
|---|---|---|
| `accept(ASTVisitor&) const` | Declarado e implementado en los 16 nodos, **sin ningún consumidor** | `AST.hpp:76`, `107`…`290`; búsqueda en todo el proyecto: solo aparece en `AST.hpp` |
| `print(int)` / `toString()` | Recorrido recursivo interno, orientado a texto | `AST.hpp:77-78`; `AST.cpp:45-127` |
| `nodeKindToString` | Traducción de `NodeKind` a texto, para depuración | `AST.cpp:19-39` |

**Hallazgo `HAL-01`:** el proyecto declara un Visitor pero nunca lo usa. Implementarlo no rompe ningún consumidor previo y no requiere tocar `Parser/` ni `AST.*` (`INT-001`).

### 1.5 Interfaces y dependencias

| Dependencia | Uso | Evidencia |
|---|---|---|
| `Tokens.hpp` | `Ubicacion`, `TipoToken` | `AST.hpp:5`; `Tokens.hpp:8-19`, `37-40` |
| `Parser::parse()` | Devuelve `unique_ptr<ProgramNode>` | `Parser.hpp:14`; `Parser.cpp:108-121` |
| `Parser::huboErroresSintacticos()` | Bandera de AST parcial | `Parser.hpp:18`; `Parser.cpp:10-12`, `82` |
| `ParserLib` → `LexerLib` | Propagación `PUBLIC` de rutas de inclusión | `Parser/CMakeLists.txt:11` |
| `AST.cpp` | Está en `ParserLib` | `Parser/CMakeLists.txt:1-5` |

### 1.6 Ubicaciones: precisión real

| Nodo | Token de origen de `ubicacion` | Evidencia |
|---|---|---|
| `ProgramNode` | Primer token del archivo o `{1,1}` | `Parser.cpp:109-110` |
| `StructDecl` | Palabra `struct` | `Parser.cpp:133`, `148` |
| `FunctionDecl` | Tipo de retorno | `Parser.cpp:168`, `179` |
| Cuerpo de función (`BlockStmt`) | Tipo de retorno | `Parser.cpp:178` |
| `Param` | Tipo del parámetro | `Parser.cpp:162` |
| `VarDecl` | Tipo de la variable | `Parser.cpp:143`, `184` |
| `LiteralExpr` | Token del literal | `Parser.cpp:374` |
| `IdentifierExpr` | Token del nombre | `Parser.cpp:379` |
| `UnaryExpr` | Token del operador | `Parser.cpp:358`, `365` |
| `BinaryExpr` | Token del operador | `Parser.cpp:283`, `299`, `309`, `319`, `329`, `339`, `349` |
| `CallExpr` | Token del identificador llamado | `Parser.cpp:390` |
| `MemberExpr` | Token del identificador base | `Parser.cpp:395` |
| `ReturnStmt` | Palabra `return` | `Parser.cpp:256`, `260` |
| `ExprStmt` | Primer token de la expresión | `Parser.cpp:264`, `267` |
| `BlockStmt` anidado | Llave `{` | `Parser.cpp:197`, `203` |

**Hallazgo `HAL-02`:** solo los usos de identificadores tienen la posición exacta del nombre. Las declaraciones apuntan al tipo. Esto ya está registrado como `HER-001`; el diseño no lo corrige y no promete precisión que el AST no da.

### 1.7 Literales: qué información llega

- `LiteralExpr::literalType` conserva el `TipoToken` original: `NUMERO`, `CADENA` o `CARACTER` (`AST.hpp:101`; `Parser.cpp:372-374`).
- `LiteralExpr::value` es el lexema **decodificado** por el lexer: en cadenas y caracteres, las secuencias de escape ya se convirtieron a su byte (`Lexer.cpp:204-234`), y en números se conservan sufijos y exponente (`Lexer.cpp:177-196`).
- El lexer acepta `.5`, `1e10`, `2.5E-3`, sufijos `f/F/l/L/u/U` (`Lexer.cpp:135-196`).
- `0x10` produce **un único** `ERROR_LEXICO` con el texto `Numero mal formado: 0x10` y nunca llega al parser (`Lexer.cpp:184-196`). Comprobado con un arnés temporal sobre `Lexer.cpp` (ver `HAL-11`).
- `''` produce error léxico y nunca llega al AST (`Lexer.cpp:244-246`); `'ab'` sí llega como `CARACTER` con `value == "ab"` (`Lexer.cpp:248`).

**Consecuencia:** el tipo del literal se decide por `literalType` y por el texto de `value`; no hace falta re-lexar.

### 1.8 Construcciones que el parser produce y la especificación no cubre

| Construcción | Evidencia | Estado en `specs.md` |
|---|---|---|
| `FunctionDecl` dentro de un bloque | `Parser.cpp:174`, `200` (llaman a `parseDeclaration`) | **No especificada** |
| `FunctionDecl` como `init` de un `for` | `Parser.cpp:234-240` | **No especificada** |
| `StructDecl` dentro de un bloque | `Parser.cpp:174`, `200` | **No especificada** |
| Sentencia ejecutable en ámbito global | `Parser.cpp:123-130` | Cubierta por `RF-009` (`E-24`) |
| `for` sin condición | `Parser.cpp:243-245` | Cubierta por `RF-028` |
| Cuerpo de función con la ubicación del tipo | `Parser.cpp:178` | Registrada en `HER-001` |

**Hallazgo `HAL-03`:** las declaraciones anidadas son alcanzables con el parser actual y no tienen comportamiento definido en `specs.md`. Es la laguna más importante del diseño y se trata en `DIS-04`.

### 1.9 Limitaciones que condicionan el diseño

| ID | Limitación | Evidencia | Efecto |
|---|---|---|---|
| `LIM-01` | Solo `int`, `float`, `double`, `void` inician una declaración | `Parser.cpp:125-127` | `char`, `bool` y tipos `struct` no son declarables como variables |
| `LIM-02` | Un tipo `struct` no es expresable como variable, parámetro ni retorno | `Parser.cpp:125-127`, `154-168` | `MemberExpr` solo se puede ejercitar con base primitiva (`E-20`) o con AST construido a mano |
| `LIM-03` | No hay punteros | `AST.hpp:96-291` | `->` siempre produce `E-22` tras `E-20` |
| `LIM-04` | `true`, `false`, `nullptr` son palabras reservadas, no literales | `Lexer/Regex.hpp:67-78`; `Parser.cpp:371-411` | No existen literales booleanos ni nulos |
| `LIM-05` | Las ubicaciones de declaraciones apuntan al tipo | `HAL-02` | Los diagnósticos de declaración no señalan el nombre |
| `LIM-06` | `char`/`bool` solo aparecen en campos y parámetros | `LIM-01` | `RF-014` y `RF-017` se prueban por esa vía o con AST manual (`U-008`) |
| `LIM-07` | El literal `-2147483648` se valida como el positivo `2147483648` | `AST.hpp:122-134`; `RF-023` | Se reporta `E-25`; se documenta como limitación, no se corrige |
| `LIM-08` | Una función declarada dentro de un bloque o en el `init` de un `for` no tiene semántica definida en `specs.md` | `Parser.cpp:174`, `200`, `234-240`; `HAL-03` | **Decisión pendiente:** no se implementa ni se le asigna diagnóstico hasta ratificar `DIS-04`. El `StructDecl` local sí se admite. |

### 1.10 Lo que no se pudo verificar

- Que un `switch` sin `default` produzca el aviso esperado con el toolchain real (depende de banderas, no del código).
- Comportamiento con MSVC (`SUP-001`), configuración CMake desde cero (`SUP-002`), fugas con ASan o valgrind (`U-007`).

---

## 2. Alternativas evaluadas

### 2.1 Criterios

1. Satisfacer `RF-004` sin cambiar la especificación.
2. No modificar el AST (`INT-001`, `RNF-004`).
3. Coste y memoria proporcionales al tamaño del árbol (`RNF-007`).
4. Mínima superficie nueva (`AGENTS.md` §3.4, §3.12).
5. Verificabilidad con el entorno disponible.

### 2.2 Alternativa A — `ASTVisitor` y mapa lateral de tipos

**Cómo funciona.** `SemanticAnalyzer::analyze` crea una sesión local con la tabla, el colector y un `AnalizadorVisitor`. La pasada 2 recorre el árbol con `accept`; cada `visit` tipa su nodo y guarda el resultado en `std::unordered_map<const Expr*, Tipo>`.

| Aspecto | Evaluación |
|---|---|
| Requisitos | Cumple `RF-004` por construcción: los 16 `visit` son métodos puros y el compilador obliga a implementarlos |
| Ventajas | Sin RTTI; sin tocar el AST; el mecanismo ya está declarado en `AST.hpp:30-49`; el mapa lateral sirve también para `RF-040` |
| Desventajas | Los `visit` devuelven `void`, así que el tipo se consulta en el mapa; un `visit` que olvide recorrer un hijo no produce error de compilación |
| Complejidad | Media: un mapa por expresión, banderas de contexto (cuerpo de función, función actual) y una guardia de profundidad |
| Riesgos | Olvidar una rama o un hijo; se mitiga contando visitas en las pruebas (`TEST-03`) |
| Supuestos | Nodos únicos y direcciones estables (`INV-A-001`, `SUP-004`) |

### 2.3 Alternativa B — `switch (kind())` con `static_cast`

**Cómo funciona.** Una clase con `Tipo analizarExpr(const Expr&)` y `void analizarStmt(const Stmt&)`; cada una abre un `switch` sobre `kind()` y hace `static_cast` al tipo concreto.

| Aspecto | Evaluación |
|---|---|
| Requisitos | **No cumple `RF-004`**, que exige implementar los 16 `visit` de `ASTVisitor` |
| Ventajas | Permite devolver `Tipo` sin mapa lateral; el `switch` sin `default` avisa si aparece un nodo nuevo |
| Desventajas | Ignora el punto de extensión que el AST ya declara; obliga a cambiar la especificación aprobada |
| Veredicto | **No seleccionable** sin autorización explícita de cambio de `RF-004` |

### 2.4 Alternativa C — Copia a una representación propia anotada

**Cómo funciona.** Se copia el AST a un árbol semántico propio con un campo `Tipo` por nodo.

| Aspecto | Evaluación |
|---|---|
| Requisitos | Los cumple, pero no añade cobertura |
| Ventajas | Tipos “dentro” del nodo; no hace falta mapa lateral |
| Desventajas | Duplica los 16 nodos, duplica la memoria, crea dos fuentes de verdad y un punto de divergencia con el AST |
| Veredicto | Descartada: sobreingeniería (`AGENTS.md` §3.4) |

### 2.5 Alternativa D — Anotar el AST existente

Imposible: `ubicacion` es `const` (`AST.hpp:64`), no hay campos de anotación y `RNF-004`/`CA-005` prohíben mutarlo. Descartada por evidencia.

### 2.6 Alternativa E — Adaptador sin copia

**Cómo funciona.** Un `NodoSemantico { const ASTNode* origen; Tipo tipo; }` construido de forma perezosa.

Es la alternativa A con una indirección adicional: el mapa `const Expr* → Tipo` ya es exactamente ese adaptador. Añadir un objeto por nodo no aporta aislamiento que los requisitos pidan.

### 2.7 Patrones

| Patrón | ¿Se usa? | Justificación |
|---|---|---|
| Visitor | **Sí** | `RF-004` lo exige y `AST.hpp:30-49` ya lo define. No es una elección estética |
| Composite | Ya existe en el AST | No se añade nada |
| Factory | No | La fase solo crea valores (`Tipo`, `Simbolo`, `Diagnostico`); no hay jerarquías polimórficas que fabricar |
| Singleton | No | Prohibido por `RNF-015` (sin estado global mutable) |

### 2.8 Decisión

Se adopta la **alternativa A**. Es la única que cumple `RF-004` sin cambiar la especificación, sin copiar el árbol y sin mutarlo.

---

## 3. Arquitectura propuesta

### 3.1 Límites y responsabilidades

| Dentro | Fuera |
|---|---|
| Resolver nombres y declaraciones | Modificar el AST, el parser o el lexer |
| Tipar expresiones y comprobar conversiones | Generar código o IR |
| Verificar `main`, retorno, ámbitos y structs | Analizar flujo completo (`RF-025` es estructural) |
| Producir diagnósticos ordenados y deterministas | Recuperar o reparar el programa |
| Actuar como compuerta (`ok()`) | Leer archivos (eso es del ejecutable de pruebas) |

### 3.2 Componentes

```
Semantic/
├── CMakeLists.txt
├── Semantic.hpp / Semantic.cpp          fachada, precondición, dos pasadas, main, orden
├── Tipos.hpp / Tipos.cpp                modelo de tipos y conversiones
├── TablaSimbolos.hpp / TablaSimbolos.cpp ámbitos, símbolos, volcado
├── Diagnostico.hpp / Diagnostico.cpp    recolección, límite, orden, formato
├── AnalizadorVisitor.hpp / .cpp         los 16 visit y el tipado
└── tests/
    ├── pruebas_semantico.cpp            RF-042: archivo → 0/1/2/3
    └── pruebas_ast.cpp                  AST construido a mano (U-008)
```

| Componente | Responsabilidad | No hace |
|---|---|---|
| `SemanticAnalyzer` | Orquestar, aplicar la precondición, construir el resultado | No recorre nodos |
| `AnalizadorVisitor` | Recorrer, tipar, poblar la tabla, emitir diagnósticos | No decide la salida del proceso |
| `TablaSimbolos` | Guardar ámbitos y observadores, buscar, volcar | No emite diagnósticos |
| `ColectorDiagnosticos` | Acumular, limitar, ordenar, formatear | No conoce el AST |
| `Tipos` | Normalizar nombres y decidir compatibilidad | No conoce ámbitos |

Esta separación evita que la tabla dependa de los diagnósticos y que el colector dependa del AST.

### 3.3 Interfaces públicas

Solo `Semantic.hpp` es cabecera pública.

```cpp
struct ContextoEntrada {
    bool erroresLexicos = false;
    bool erroresSintacticos = false;
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

class ResultadoSemantico {
public:
    bool ok() const;                                  // errores() == 0
    const std::vector<Diagnostico>& diagnosticos() const;
    std::size_t errores() const;
    std::size_t advertencias() const;
    const TablaSimbolos& tabla() const;
    void dumpTabla(std::ostream& os) const;           // RF-040
};

enum class Severidad { Error, Advertencia };

struct Diagnostico {
    std::string codigo;        // "E-13", "W-04", ...
    Severidad   severidad;
    std::string mensaje;       // español, no vacío
    Ubicacion   ubicacion;
    std::size_t secuencia;     // orden de inserción, para desempate
};
```

**Decisión `DIS-09`:** `dumpTabla` vuelca los símbolos con su tipo resuelto (variables, funciones con firma completa, structs). No se vuelca el tipo de cada expresión: sería voluminoso y `CA-002` solo exige nombres, tipos y firmas.

### 3.4 Estructuras internas

```cpp
enum class TipoBase { Int, Float, Double, Char, Bool, Void, Struct, Cadena, Error };

struct Tipo {
    TipoBase base = TipoBase::Error;
    const StructDecl* estructura = nullptr;   // solo si base == Struct
};

enum class Categoria { Variable, Funcion, Struct };

struct Simbolo {
    std::string       nombre;
    Categoria         categoria;
    Tipo              tipo;                 // variable/struct: su tipo; función: retorno
    std::vector<Tipo> parametros;           // solo funciones
    const Decl*       declaracion;          // observador, nunca propietario
    const FunctionDecl* prototipo = nullptr;
    const FunctionDecl* definicion = nullptr;
    bool              usado = false;
    bool              esParametro = false;
    std::size_t       nivel = 0;            // 0 = ámbito global
    std::size_t       orden = 0;            // índice de la declaración global, para E-05
};

class TablaSimbolos {
public:
    void entrarAmbito(const std::string& etiqueta);
    void salirAmbito();                          // no destruye el ámbito
    bool declarar(const Simbolo& s);             // false si ya existe en el actual
    const Simbolo* buscar(const std::string& n) const;   // de dentro hacia afuera
    const Simbolo* buscarGlobal(const std::string& n) const;
    std::vector<Simbolo>& ambitoActual();
    std::size_t profundidad() const;             // 1 = solo ámbito global
    void dumpTabla(std::ostream& os) const;
};

struct Firma {
    Tipo              retorno;
    std::vector<Tipo> parametros;
};
```

**Representación de ámbitos:** `TablaSimbolos` guarda todos los ámbitos en un `std::vector<Ambito>` y una pila de índices. Cada `Ambito` tiene etiqueta, padre y símbolos. `salirAmbito` solo mueve la pila; **no borra**, para que el volcado final pueda mostrar todo el árbol de ámbitos. `buscar` recorre la cadena de padres desde el actual.

Esto cumple `INV-D-003` (la pila refleja la ruta raíz→nodo) y `INV-D-014` (un símbolo por declaración).

### 3.5 Flujo

```
analyze(programa, contexto)
│
├─ ¿contexto.erroresLexicos || contexto.erroresSintacticos?
│     └─ sí → E-00 → resultado con ok()==false → FIN
│
├─ PASADA 1: declaraciones globales
│     1. VarDecl global        → tipo (E-06/E-07), declarar (E-03)
│     2. FunctionDecl          → firma, fusionar (E-23), registrar
│     3. StructDecl            → registrar nombre (E-03)
│     4. Otro                  → no se registra (E-24 en pasada 2)
│
├─ CIERRE PASADA 1
│     5. main: E-01 / E-02 / (duplicado ya fue E-03)
│     6. prototipos sin definición → W-05 (excepto main)
│
├─ PASADA 2: programa.accept(visitor)
│     · ProgramNode  → fija visibilidad global, despacha declaraciones
│     · VarDecl      → inicializador (global) o tipo+declaración+inicializador (local)
│     · FunctionDecl → cuerpo, parámetros, retorno, W-04
│     · StructDecl   → campos, constantes, W-06
│     · sentencia global → E-24 y poda
│
├─ ORDEN: stable_sort por (línea, columna, secuencia)
├─ EMISIÓN al ostream
└─ RESULTADO
```

#### 3.5.1 Pasada 1: firma y fusión de funciones

Para cada `FunctionDecl`:

1. Normalizar el tipo de retorno y cada tipo de parámetro.
2. Si el nombre no existe en el ámbito global, declararlo.
3. Si existe:
   - Es variable o struct → `E-03`.
   - Es función:
     - Prototipo + prototipo: si las firmas coinciden, no hay diagnóstico; si no, `E-23`.
     - Prototipo + definición (en cualquier orden): si coinciden, se marca la definición; si no, `E-23`.
     - Definición + definición: `E-03` (redefinición), sin `E-23` adicional.
4. **Firma autoritativa:** la primera declaración encontrada. Si hay `E-23`, las llamadas se comprueban contra ella.
5. **Una definición cuenta como definición aunque su firma no coincida.** Si no fuera así, `CA-001` emitiría `W-05` sobre `f` además del `E-23`, y la lista de advertencias dejaría de ser `{W-03, W-04}`.

Las firmas ya normalizadas se guardan en un mapa lateral `const FunctionDecl* → Firma` mientras dura el análisis. La pasada 2 lo consulta para el cuerpo y los parámetros; así no se repite la normalización ni se duplica un `E-06` ya emitido.

**Decisión `DIS-06`:** resuelve la parte de `U-009` que afecta al orden de diagnósticos de `CA-001`. En `CA-001`, el prototipo `int f(int)` es la firma autoritativa y `f()` produce `E-17`.

#### 3.5.2 Pasada 2: programa y declaraciones

`visit(ProgramNode)` recorre `declarations` con un índice `i`:

- `VarDecl` → `accept` (el visitante comprueba que el símbolo global guardado apunta a **este** nodo; si apunta a otro, la declaración perdió un `E-03` y solo se omite, sin analizar el inicializador).
- `FunctionDecl` → `accept`.
- `StructDecl` → `accept`.
- Cualquier sentencia (`BlockStmt`, `IfStmt`, `WhileStmt`, `ForStmt`, `ReturnStmt`, `ExprStmt`) → `E-24` en el nodo y **poda**: no se recorre su interior, para no generar cascadas.

Antes de despachar cada declaración se fija `indiceGlobal = i`, que gobierna la visibilidad de las variables globales (§3.6.2).

#### 3.5.3 Pasada 2: cuerpo de función

1. Abrir ámbito con etiqueta `funcion <nombre>`.
2. Declarar los parámetros en orden (`E-03` si se repiten), marcados `esParametro = true`.
3. Fijar `funcionActual`.
4. Analizar el cuerpo con `esCuerpoDeFuncion = true`, para que el `BlockStmt` del cuerpo **no abra un segundo ámbito** (`CL-017`).
5. Si el retorno no es `void` y el cuerpo no garantiza retorno → `E-11`.
6. Recorrer los símbolos del ámbito y emitir `W-04` por cada variable local no usada (no parámetros, `DIS-08`).
7. Cerrar el ámbito.

### 3.6 Ámbitos y visibilidad

#### 3.6.1 Ámbitos que se crean

| Ámbito | Cuándo | Etiqueta del volcado | Evidencia |
|---|---|---|---|
| Global | Al iniciar el análisis | `global` | `RF-010` |
| Cuerpo de función | `visit(FunctionDecl)` | `funcion <nombre>` | `RF-010`; incluye parámetros |
| Bloque anidado | `visit(BlockStmt)` con `esCuerpoDeFuncion == false` | `bloque` | `RF-010` |
| `for` | `visit(ForStmt)` | `for` | `RF-010`, `CL-044` |

El cuerpo de la función y su bloque superior **comparten ámbito**, para que `int f(int a) { int a; }` produzca `E-03` (`CL-017`).

#### 3.6.2 Visibilidad y `E-05`

| Nombre | Visibilidad |
|---|---|
| Variable local | Desde su declaración hasta el final de su bloque |
| Variable de `for` | Desde el inicio del `for` hasta el final del cuerpo |
| Variable global | Desde su declaración en el archivo; las anteriores producen `E-05` |
| Función | Adelantada: toda función global es visible en todo el archivo (`RF-005`, `RF-027`) |
| Struct | Adelantada: todo struct global es visible en todo el archivo |

**Implementación de `E-05` local:** al entrar en un bloque se construye un `std::set<std::string>` con los nombres de las declaraciones **directas** del bloque. Si una búsqueda falla y el nombre está en ese conjunto, es `E-05`; si no, `E-04`. Coste O(k) por bloque, una sola vez.

**Implementación de `E-05` global:** `buscar` encuentra la variable global aunque esté más adelante. El analizador comprueba `simbolo->categoria == Variable && simbolo->nivel == 0 && simbolo->orden > indiceGlobal`; si se cumple, la trata como no visible y emite `E-05`.

**Decisión `DIS-05`:** esta regla hace que `int f() { return g; } int g;` produzca `E-05`, coherente con “uso antes de declarar” (`RF-012`), sin romper la visibilidad adelantada de funciones (`RF-005`).

#### 3.6.3 Redeclaración

- `declarar` falla si el nombre ya existe en el ámbito **actual**; el llamador emite `E-03` y **conserva el primer símbolo**.
- No se registra la declaración duplicada y **no se analiza su inicializador**, para evitar diagnósticos derivados.
- El sombreado en un ámbito anidado es legal (`CL-016`).
- La declaración se registra **antes** de analizar su inicializador (`DIS-07`), de modo que `int x = x;` se resuelve a sí misma.

En `CA-001`, esto produce `E-03` en la segunda `int x` y hace que los usos posteriores resuelvan a la primera (`INV-D-005`).

### 3.7 Tipos

#### 3.7.1 Normalización

| Texto | Tipo | Diagnóstico si no procede |
|---|---|---|
| `int` | `Int` | — |
| `float` | `Float` | — |
| `double` | `Double` | — |
| `char` | `Char` | — |
| `bool` | `Bool` | — |
| `void` | `Void` | `E-07` en variable, `E-08` en campo/parámetro |
| Nombre de struct declarado | `Struct` con puntero al `StructDecl` | `E-06` si no existe |
| Cualquier otro (`auto`, `static`, …) | `Error` | `E-06` |

`Error` es absorbente: cualquier operación con `Error` devuelve `Error` y no emite un diagnóstico nuevo (`RF-007`, `INV-D-015`).

#### 3.7.2 Tipo de un literal

Precedencia acordada (resuelve la contradicción `HAL-04` entre el orden de `RF-016` y `CL-054`):

1. `CARACTER` → `Char`. Si `value.size() != 1` → `E-26` y tipo `Error`.
2. `CADENA` → `Cadena`.
3. `NUMERO`:
   1. Si termina en `f` o `F` → `Float`.
   2. Si contiene `.`, `e` o `E` → `Double`.
   3. En otro caso → `Int`; si termina en `u/U/l/L` → `W-03`.
   4. Si es `Int`, convertir el texto a entero y comparar con `std::numeric_limits<int>`; fuera de rango → `E-25` y tipo `Error`.
4. Cualquier otro `literalType` → `E-06` y `Error`.

**Hallazgo `HAL-04`:** `RF-016` enumera “`.`/`e`/`E` → `Double`” antes de “sufijo `f`/`F` → `Float`”, pero `CL-054` exige que `3.14f` sea `Float`. La precedencia anterior resuelve el caso explícito sin cambiar el requisito.

**Decisión `DIS-03`:** `W-08` no se implementa: ningún `RF` lo exige y aplicarlo obligaría a inventar una regla de rango real. Se registra como `U-012`.

`LIM-07`: el literal `2147483648` se considera fuera de rango, de modo que `-2147483648` produce `E-25`. Es una limitación conocida, no un defecto del diseño.

### 3.8 Conversiones y operadores

#### 3.8.1 Compatibilidad

| Destino \ Origen | Int | Char | Float | Double | Bool | Cadena | Void | Struct |
|---|---|---|---|---|---|---|---|---|
| Int | ok | ok | `E-13` | `E-13` | `E-13` | `E-13` | `E-13` | `E-13` |
| Float | ok | `E-13` | ok | `W-02` | `E-13` | `E-13` | `E-13` | `E-13` |
| Double | ok | `E-13` | ok | ok | `E-13` | `E-13` | `E-13` | `E-13` |
| Char | `E-13` | ok | `E-13` | `E-13` | `E-13` | `E-13` | `E-13` | `E-13` |
| Bool | `E-13` | `E-13` | `E-13` | `E-13` | ok | `E-13` | `E-13` | `E-13` |
| Struct | `E-13` | `E-13` | `E-13` | `E-13` | `E-13` | `E-13` | `E-13` | solo la misma struct |

- Cualquier origen `Error` o destino `Error`: compatible, sin diagnóstico.
- `Bool` no admite conversión desde numéricos en asignación; sí en contexto booleano (`RF-017`).
- `void` como valor de una expresión → `E-13` en el uso (por ejemplo, `int x = f();` con `f` que devuelve `void`).

`convertibleABool(t)`: `Bool`, `Int`, `Char`, `Float`, `Double` sí; `Struct`, `Cadena`, `Void`, `Error` no.

#### 3.8.2 Operadores binarios

| Operador | Requisito | Resultado | Diagnóstico |
|---|---|---|---|
| `=` | Destino asignable y tipos compatibles | Tipo del destino | `E-12` / `E-13` |
| `+ - * /` | Ambos numéricos | `Double` si alguno lo es; si no, `Float` si alguno lo es; si no, `Int` | `E-14` |
| `%` | Ambos enteros (`Int`/`Char`) | `Int` | `E-14` |
| `== != < > <= >=` | Ambos numéricos, o ambos `Bool` | `Bool` | `E-14` |
| `&& \|\|` | Ambos convertibles a `Bool` | `Bool` | `E-14` |
| Otro | — | `Error` | `E-14` |

`Cadena` no admite comparación ni concatenación: `E-14` (`RF-018`).

#### 3.8.3 Operadores unarios

| Operador | Requisito | Resultado | Diagnóstico |
|---|---|---|---|
| `!` | Convertible a `Bool` | `Bool` | `E-15` |
| `- +` | Numérico | `Int` si es `Char`; si no, el mismo tipo | `E-15` |
| `++ --` | Destino asignable y numérico | Tipo del destino | `E-12` / `E-15` |
| Otro | — | `Error` | `E-15` |

### 3.9 Comprobaciones por nodo

| Nodo | Qué se comprueba | Diagnósticos | Tipo resultante |
|---|---|---|---|
| `ProgramNode` | Precondición, clases de declaración global, `main` | `E-00`, `E-24`, `E-01`, `E-02` | — |
| `FunctionDecl` | Firma, parámetros, cuerpo, retorno, prototipo | `E-03`, `E-06`, `E-08`, `E-23`, `E-09`, `E-10`, `E-11`, `W-05`, `W-04` | — |
| `StructDecl` | Vacío, campos duplicados, tipos, inicializadores | `W-06`, `E-03`, `E-06`, `E-08`, `E-13`, `E-27`, `E-28`, `W-02` | — |
| `VarDecl` | Tipo, declaración, inicializador | `E-06`, `E-07`, `E-03`, `E-13`, `W-02`, `W-04` | — |
| `BlockStmt` | Ámbito, código inalcanzable, retorno | `W-01` | — |
| `IfStmt` | Condición, ramas, retorno | `E-14` | — |
| `WhileStmt` | Condición | `E-14` | — |
| `ForStmt` | Ámbito, inicio, condición, incremento; función en el `init` | `E-14`; función pendiente de `DIS-04` (`LIM-08`) | — |
| `ReturnStmt` | Valor frente a retorno | `E-09`, `E-10`, `E-13`, `W-02` | — |
| `ExprStmt` | Expresión sin efecto | `W-07` | — |
| `LiteralExpr` | Rango, carácter, sufijo | `E-25`, `E-26`, `W-03` | `Int`/`Float`/`Double`/`Char`/`Cadena`/`Error` |
| `IdentifierExpr` | Resolución y contexto | `E-04`, `E-05`, `E-19` | Tipo del símbolo o `Error` |
| `UnaryExpr` | Operando | `E-12`, `E-15` | Según §3.8.3 |
| `BinaryExpr` | Operandos, asignación | `E-12`, `E-13`, `E-14` | Según §3.8.2 |
| `CallExpr` | Callee, aridad, argumentos | `E-16`, `E-04`, `E-05`, `E-17`, `E-18` | Retorno o `Error` |
| `MemberExpr` | Base, puntero, miembro | `E-20`, `E-22`, `E-21` | Tipo del campo o `Error` (`LIM-03`) |

#### 3.9.1 Retorno garantizado (`RF-025`, `INV-D-009`)

Se calcula **durante** la pasada 2, sin un tercer recorrido:

| Nodo | `garantizaRetorno` |
|---|---|
| `ReturnStmt` | `value != nullptr` |
| `BlockStmt` | OR acumulado de las sentencias, en orden |
| `IfStmt` | Tiene `else` y ambas ramas garantizan |
| Cualquier otro | `false` |

El OR acumulado hace que `return 1; int x;` siga garantizando retorno aunque la última sentencia no lo haga.

#### 3.9.2 Inicializadores constantes de campos (`RF-032`, `RF-033`)

`evaluarConstante(expr)` acepta:

| Forma | Resultado |
|---|---|
| `LiteralExpr` numérico | Valor |
| `UnaryExpr` `+`, `-`, `!` sobre constante | Valor |
| `BinaryExpr` aritmético, de comparación o lógico sobre constantes | Valor |
| `LiteralExpr` de cadena o carácter | Constante no numérica; el tipo se comprueba aparte |
| `IdentifierExpr`, `CallExpr`, `MemberExpr` | **No constante** → `E-27` |

- `/` o `%` con divisor cero durante la evaluación → `E-28` y se descarta ese inicializador.
- División por cero en ejecución (fuera de un campo) **no** es error (`RF-033`, `CL-036`).
- Tras la evaluación se comprueba la compatibilidad de tipo (`E-13`/`W-02`).

### 3.10 Errores, límite, profundidad y cascada

| Mecanismo | Regla |
|---|---|
| Acumulación | Los diagnósticos se guardan; no se lanza ninguna excepción (`RNF-003`) |
| Límite | Al registrar el error número 100 se añade `E-29` y se activa `detenido` |
| Parada | Cada `visit` empieza con `if (detenido) return;` |
| Profundidad | Guardia de 256 niveles; al superarla se emite `E-06` **una vez** y se poda la rama |
| Hijos nulos | Los campos opcionales se comprueban; un nulo no opcional produce `E-06` y se continúa |
| Cascada | `Error` absorbe; un nodo emite como máximo un diagnóstico propio |

**Decisión `DIS-10`:** se mantiene `E-06` para la poda por profundidad, como fijan `INV-D-009` y `CL-051`, aunque su significado catalogado sea “tipo no soportado”. `E-29` se emite como diagnóstico adicional (101.º) y cuenta como error; `ok()` ya es falso.

**Propiedad de cada diagnóstico** (para que `INV-D-015` sea comprobable):

| Diagnóstico | Nodo que lo posee |
|---|---|
| `E-04`, `E-05`, `E-19` | El `IdentifierExpr` |
| `E-12`, `E-13`, `E-14` | El `BinaryExpr` o `UnaryExpr` |
| `E-18` | El argumento incompatible |
| `E-17` | El `CallExpr` |
| `E-20`, `E-21`, `E-22` | El `MemberExpr` |
| `E-09`, `E-10` | El `ReturnStmt` |
| `E-11` | El `FunctionDecl` |
| `E-01`, `E-02` | El `ProgramNode` o el `FunctionDecl` de `main` |
| `W-04` | El `VarDecl` |

### 3.11 Diagnósticos: formato, orden y resultado

**Formato** (literal de `specs.md` §5.3, sin el sufijo `(Encontrado: ...)` del parser):

```
[Error Semántico] Línea <L>, Columna <C>: <mensaje> (<código>)
[Advertencia Semántica] Línea <L>, Columna <C>: <mensaje> (<código>)
```

La posición es la del nodo según §1.6. En declaraciones apunta al tipo, no al nombre (`LIM-05`, `HER-001`); por eso `CA-001` se verifica por línea, código y orden, no por la columna exacta del nombre.

**Decisión `DIS-12`:** se adopta el formato de `specs.md` §5.3 sin añadir `(Encontrado: ...)`; cierra `U-010`. El parser lo añade porque dispone del token exacto; la fase semántica no siempre lo tiene (`HAL-02`).

**Orden:** `stable_sort` por `(linea, columna, secuencia)`. La secuencia es el orden de inserción y desempata diagnósticos de la misma posición. Es determinista y no depende de direcciones de memoria (`RNF-009`, `CA-004`).

**Resultado:** `ok()` es verdadero si y solo si `errores() == 0` (`RF-037`, `INV-D-013`).

### 3.12 Ciclo de vida y propiedad

- El AST lo posee el llamador (`PRE-001`); la fase recibe `const ProgramNode&` y nunca lo modifica (`RNF-004`).
- `Simbolo::declaracion`, `Simbolo::prototipo`, `Simbolo::definicion` y `Tipo::estructura` son observadores; viven mientras viva el AST (`RNF-005`, `INV-D-012`, `INV-D-016`).
- El mapa `const Expr* → Tipo` es un observador indexado por dirección; no se itera nunca, así que su orden no afecta a la salida.
- `analyze` no conserva estado entre llamadas: cada invocación crea tabla, colector y visitante locales (`RNF-015`, `CL-052`).
- No hay `new`/`delete` en la fase; todo son valores y contenedores estándar.

### 3.13 Reutilización

| Elemento existente | Uso | No se modifica |
|---|---|---|
| `ASTVisitor` / `accept` | Recorrido de la pasada 2 | Sí |
| `NodeKind` | Despacho sin RTTI | Sí |
| `Ubicacion` | Posición de diagnósticos | Sí |
| `TipoToken` | Clasificación de literales | Sí |
| `Param` | Datos de parámetros | Sí |
| `StructDecl::fields` | Búsqueda de miembros | Sí |
| `Parser::huboErroresSintacticos()` | `ContextoEntrada` | Sí |
| `AST::toString()` | `CA-005` compara antes/después | Sí |
| Includes públicos de `ParserLib`/`LexerLib` | Compilar la fase sin rutas extra | Sí |

### 3.14 Cambios en lo existente

Solo los autorizados por `DU-005` y `AGENTS.md` §7.

| Archivo | Cambio exacto | Motivo |
|---|---|---|
| `CMakeLists.txt` | Añadir `add_subdirectory(Semantic)` entre `Parser` y `app` | `INT-005`, `RNF-011` |
| `app/CMakeLists.txt` | Añadir `SemanticLib` a `target_link_libraries(compiler_app PRIVATE ...)` | Enlazar la fase |
| `app/main.cpp` | `#include "Semantic.hpp"`; constante `SALIDA_ERROR_SEMANTICO = 3`; invocar `analyze` tras la comprobación sintáctica; devolver `3` si `!ok()`; imprimir el AST **solo** si `ok()` | `RF-038`, `RF-041`, `INT-007` |
| `Semantic/` | Todo el módulo nuevo | `DU-003` |

**Consecuencia en `main.cpp`:** las líneas 81-82 actuales (impresión del AST) se mueven detrás de la comprobación semántica. Es un cambio de orquestación exigido por `RF-038`; se registra en `DIS-01` para ratificación porque `DU-005` limita el alcance de `main.cpp`.

Esbozo del tramo final:

```cpp
std::cout << "\n--- AST generado con exito ---\n";   // solo si la semantica acepta
SemanticAnalyzer analizador;
ContextoEntrada contexto{ false, parser.huboErroresSintacticos() };
ResultadoSemantico resultado = analizador.analyze(*ast, contexto);
if (!resultado.ok()) return SALIDA_ERROR_SEMANTICO;
std::cout << ast->toString() << "\n";
return SALIDA_OK;
```

### 3.15 Volcado de tabla (`RF-040`)

Estilo `print()` del AST (`AST.cpp:41-58`), con etiquetas en inglés para ser coherente con `(Literal ...)`, `(Id ...)`, `(Binary ...)`:

```
(Tabla
  (Ambito global
    (Struct Punto)
    (Func fact : int (int) definicion)
    (Func suma : double (int double) definicion)
    (Func main : int () definicion)
  )
  (Ambito funcion main
    (Var i : int usada)
    (Var total : double usada)
  )
  (Ambito for
    (Var k : int usada)
  )
)
```

- Ámbitos en orden de creación (preorden).
- Símbolos en orden de declaración.
- Sin direcciones ni posiciones: el volcado es estable entre ejecuciones y entre versiones del parser.
- Una función con prototipo y definición se muestra una vez, con `definicion`; solo prototipo, con `prototipo`.

### 3.16 Ejecutable de pruebas (`RF-042`)

`pruebas_semantico <archivo>` hace el recorrido completo y devuelve `0/1/2/3`, sin tocar `main`:

| Paso | Salida |
|---|---|
| Leer el archivo con `std::ifstream` (biblioteca estándar, `RNF-013`) | Error de lectura → `2` con mensaje |
| Tokenizar y reportar errores léxicos | `1` |
| Parsear y consultar `huboErroresSintacticos()` | `2` |
| Analizar con `ContextoEntrada{false, false}` | `3` si `!ok()`, `0` si `ok()` |
| `--tabla` | Añade el volcado a `stdout` |

`pruebas_semantico_ast` construye nodos con los constructores públicos de `AST.hpp` para cubrir lo que el parser no produce (`LIM-02`, `LIM-06`): struct como variable, acceso a miembro válido, `E-21`, y nodos nulos (`CL-050`).

---

## 4. Coherencia con los requisitos

### 4.1 Requisitos funcionales

| Requisito | Dónde se cumple |
|---|---|
| `RF-001` | `SemanticAnalyzer::analyze(const ProgramNode&, ...)` (§3.3); AST en `const&` |
| `RF-002` | `ResultadoSemantico` (§3.3) |
| `RF-003` | Precondición `E-00` (§3.5) |
| `RF-004` | 16 `visit` (§3.2, §3.9) |
| `RF-005` | Dos pasadas (§3.5) |
| `RF-006` | Ámbitos anidados y búsqueda interior→exterior (§3.6) |
| `RF-007` | `Tipo::Error` absorbente (§3.7.1) |
| `RF-008` | Comprobación de `main` tras la pasada 1 (§3.5, §3.5.1) |
| `RF-009` | `visit(ProgramNode)` con `E-24` y poda (§3.5.2) |
| `RF-010` | Cuatro tipos de ámbito, incluido `for` (§3.6.1) |
| `RF-011` | `declarar` en el ámbito actual; sombreado legal (§3.6.3) |
| `RF-012` | `E-04`/`E-05` con visibilidad local y global (§3.6.2) |
| `RF-013` | Un solo espacio de nombres; `E-03` (§3.6.3) |
| `RF-014` | Normalización de tipos (§3.7.1) |
| `RF-015` | `E-07`/`E-08` (§3.7.1, §3.9) |
| `RF-016` | Clasificación de literales con precedencia resuelta (§3.7.2) |
| `RF-017` | Tabla de compatibilidad (§3.8.1) |
| `RF-018` | Matriz de operadores (§3.8.2, §3.8.3) |
| `RF-019` | `=`/`++`/`--` con destino asignable (§3.8.2, §3.9) |
| `RF-020` | `E-19` en `IdentifierExpr` de función/struct (§3.9) |
| `RF-021` | `CallExpr`: callee, aridad y argumentos (§3.9) |
| `RF-022` | `MemberExpr`: `E-20`, `E-22`, `E-21` (§3.9) |
| `RF-023` | `E-25`, `E-26`, `E-13`, `W-07` (§3.7.2, §3.9) |
| `RF-024` | `E-09`/`E-10` en `ReturnStmt` (§3.9) |
| `RF-025` | Retorno estructural durante la pasada 2 (§3.9.1) |
| `RF-026` | Fusión prototipo/definición y `W-05` (§3.5.1) |
| `RF-027` | Visibilidad adelantada de funciones (§3.6.2) |
| `RF-028` | Condiciones convertibles a `Bool` (§3.9) |
| `RF-029` | `W-01` por tramo en `BlockStmt` (§3.9) |
| `RF-030` | `W-07` en `ExprStmt` (§3.9) |
| `RF-031` | `StructDecl`: `W-06`, `E-03`, `E-08` (§3.9) |
| `RF-032` | Evaluador constante (§3.9.2) |
| `RF-033` | `E-28` solo en evaluación constante (§3.9.2) |
| `RF-034` | `Diagnostico` + `ostream` inyectable (§3.3, §3.11) |
| `RF-035` | Orden por línea/columna/secuencia (§3.11) |
| `RF-036` | Límite 100 + `E-29` (§3.10) |
| `RF-037` | `ok()` si y solo si no hay `E-xx` (§3.11) |
| `RF-038` | Integración en `main` y AST condicionado (§3.14) |
| `RF-039` | `SemanticLib` y `add_subdirectory` (§7) |
| `RF-040` | `dumpTabla` (§3.15) |
| `RF-041` | `Parser::huboErroresSintacticos()` en el llamador (§3.14) |
| `RF-042` | `pruebas_semantico` (§3.16) |

### 4.2 Requisitos no funcionales

| Requisito | Dónde se cumple |
|---|---|
| `RNF-001` | C++17 estándar; sin extensiones (§7) |
| `RNF-002` | Sin `dynamic_cast`, `typeid` ni conversiones descendentes: el recorrido seleccionado usa los 16 `visit` de `ASTVisitor` (§2.2, §3.9). |
| `RNF-003` | Sin excepciones para errores semánticos (§3.10) |
| `RNF-004` | Recorrido `const`; sin mutación (§3.12) |
| `RNF-005` | La tabla guarda `const Decl*` (§3.4) |
| `RNF-006` | RAII y contenedores; verificación en `U-007` (§6) |
| `RNF-007` | Dos pasadas; una visita por nodo y pasada; búsqueda O(d) con d ≤ 256 (§3.5) |
| `RNF-008` | Medición en `CA-006` (§6) |
| `RNF-009` | Orden determinista; sin iterar mapas de punteros (§3.11) |
| `RNF-010` | Estilo del proyecto (§7) |
| `RNF-011` | Biblioteca y CMake propios (§7) |
| `RNF-012` | `-Wall -Wextra`; MSVC como objetivo (§6, `SUP-001`) |
| `RNF-013` | Solo biblioteca estándar; E/S inyectada (§3.16, §7) |
| `RNF-014` | Nulos, profundidad y `kind()` desconocido (§3.10) |
| `RNF-015` | Estado local por análisis (§3.12) |
| `RNF-016` | Build desde cero: `SUP-002` (§7) |

### 4.3 Invariantes

| Invariante | Cómo se respeta |
|---|---|
| `INV-D-001` | Una visita por nodo y pasada; el cuerpo de función no se re-despacha como bloque |
| `INV-D-002` | Pasada 1 antes de la pasada 2; funciones visibles desde el inicio |
| `INV-D-003` | Pila de ámbitos con entrada/salida equilibradas por guardia RAII |
| `INV-D-004` | `declarar` rechaza duplicados en el ámbito actual |
| `INV-D-005` | Todo uso resuelve o diagnostica; si resuelve, no diagnostica |
| `INV-D-006` | Todo `Expr` despachado guarda su tipo, incluido `Error` |
| `INV-D-007` | `errores()` solo crece; no se borra un diagnóstico |
| `INV-D-008` | Código, mensaje y posición obligatorios; sin duplicados idénticos |
| `INV-D-009` | Guardia de 256 y poda con `E-06` |
| `INV-D-010` | Cada `analyze` crea su propia sesión |
| `INV-D-011` | Orden y volcado deterministas |
| `INV-D-012` | Todos los punteros de la tabla apuntan al AST recibido |
| `INV-D-013` | `ok()` derivado de `errores()` |
| `INV-D-014` | Un símbolo por declaración, con nombre, tipo y declaración |
| `INV-D-015` | `Error` absorbente y un diagnóstico por nodo propietario |
| `INV-D-016` | La tabla no se usa después de morir el AST |

### 4.4 Recorrido manual de los criterios de aceptación

No se ejecutó nada; es una traza sobre el texto de `doc.md` para comprobar que las reglas producen el resultado esperado.

#### CA-001

| Línea | Esperado | Regla del diseño | ¿Coincide? |
|---|---|---|---|
| 1 | `E-03` campo `a` duplicado | `visit(StructDecl)` detecta el segundo campo | Sí |
| 4 | `E-23` prototipo/definición | Fusión en pasada 1, diagnóstico en la segunda declaración | Sí |
| 6 | `E-09` `return 1` en `void` | `visit(ReturnStmt)` con retorno `Void` y valor | Sí |
| 8 | `E-11` sin retorno | `garantizaRetorno` falso al cerrar el cuerpo | Sí |
| 12 | `E-03` `int x` duplicado | `declarar` falla; se conserva el primer `x` | Sí |
| 13 | `E-13` `x = "texto"` | `Cadena` no compatible con `Int`; `Error` absorbente evita `E-04` | Sí |
| 14 | `E-04` `y` | No declarada; no está en el conjunto del bloque | Sí |
| 15 | `E-25` entero fuera de rango | Clasificación de literal | Sí |
| 16 | `E-26` `'ab'` | `value.size() == 2` | Sí |
| 17 | `E-17` aridad | `f()` con firma autoritativa `int(int)` | Sí |
| 18 | `E-19` `f` como valor | `IdentifierExpr` de función en contexto de valor | Sí |

Advertencias previstas: `W-04` para `x` de `sinRetorno`, `v`, `c`, `z`; `W-03` para `10u`. Ninguna otra.

Puntos delicados comprobados en la traza:

- El segundo `int x` **no** se registra, así que no produce `W-04` y los usos posteriores resuelven al primero.
- `int z = f();` produce `E-17` y el tipo de la llamada es `Error`, por lo que **no** se añade `E-13`.
- `x = f;` produce `E-19` y el tipo es `Error`, por lo que **no** se añade `E-13`.
- `f` tiene definición (aunque discrepe), así que **no** se emite `W-05`.

#### CA-002

- `Punto`, `fact`, `suma` y `main` quedan en la tabla global con las firmas esperadas.
- `fact`: recursión resuelta en la pasada 1; `if/else` con retorno en ambas ramas no produce `E-11`.
- `suma`: `a + b` con `Int + Double → Double`, compatible con el retorno.
- `main`: `total + k` con `Int → Double`; `suma(i, total)` compatible; `if/else` final garantiza retorno.
- `k` vive en el ámbito del `for` y no se usa fuera; no hay `E-04`.
- No hay `W-02` (`double = 0` es `Int → Double`, sin estrechamiento).
- No hay `W-04`: `i`, `total` y `k` se usan; los parámetros se excluyen.

### 4.5 Contradicciones y hallazgos

| ID | Hallazgo | Categoría | Tratamiento |
|---|---|---|---|
| `HAL-01` | `ASTVisitor` existe y no se usa | Confirmado | Se convierte en el mecanismo de la fase |
| `HAL-02` | Las ubicaciones no señalan el nombre en declaraciones | Confirmado (`HER-001`) | Se acepta; no se promete precisión |
| `HAL-03` | El parser produce declaraciones anidadas y `FunctionDecl` en `for` | Confirmado (`Parser.cpp:174`, `200`, `237`) | `DIS-04` |
| `HAL-04` | `RF-016` ordena `Double` antes que `Float`, pero `CL-054` exige `3.14f → Float` | Confirmado | Precedencia de §3.7.2 |
| `HAL-05` | `W-08` no tiene requisito que lo exija | Confirmado | `DIS-03` |
| `HAL-06` | `parseStatement` no admite declaraciones: las ramas de `if`/`while` sin llaves nunca contienen `VarDecl` | Confirmado (`Parser.cpp:187-194`) | Simplifica `visit(IfStmt)` y `visit(WhileStmt)` |
| `HAL-07` | No existen literales `true`/`false`/`nullptr` | Confirmado (`Regex.hpp:67-78`) | `Bool` solo por tipo; documentado en `LIM-04` |
| `HAL-08` | `MemberExpr` con base struct es inalcanzable desde el parser | Confirmado (`LIM-02`) | Pruebas con AST manual (`DIS-11`) |
| `HAL-09` | `E-14`/`E-15` no están asignados en `RF-018`, aunque `doc.md` los define | Confirmado (hueco documental) | `DIS-02` |
| `HAL-10` | `RF-038` exige mover la impresión del AST en `main.cpp`, más allá de `DU-005` | Confirmado | `DIS-01` |
| `HAL-11` | `0x10` produce `ERROR_LEXICO`, no dos tokens ni error sintáctico | Confirmado (ejecución de un arnés temporal sobre `Lexer.cpp`) | Requiere corregir `V-015` y el texto de `CL-054` en `specs.md`; no afecta al diseño ni se modifica ese documento aquí |

### 4.6 Supuestos no verificados

| ID | Supuesto | Impacto |
|---|---|---|
| `SUP-001` | MSVC `/W4` compila sin avisos | `RNF-012` |
| `SUP-002` | CMake configura y construye desde cero | `RNF-016`, `CA-008` |
| `SUP-003` | No aparecerá un `kind()` fuera de los 16 | `E-06` defensivo |
| `SUP-004` | No hay subárboles compartidos | El mapa por puntero sería ambiguo |
| `SUP-005` | El programa embebido de `main` no cambia | Solo `N-001` |

### 4.7 Complejidad descartada

| Tentación | Por qué no |
|---|---|
| Copiar el AST a un IR propio | Duplica nodos y crea dos fuentes de verdad |
| Un segundo recorrido para el retorno | `INV-D-001`/`RNF-007`; se calcula durante la pasada 2 |
| Buscar `E-05` recorriendo todo el bloque en cada fallo | Sería O(n²); se precalcula el conjunto de nombres por bloque |
| Un diagnóstico por cada argumento incompatible… sí, pero uno por argumento | Se atribuye al argumento, no al `CallExpr`, para no violar `INV-D-015` |
| Fábricas o jerarquías nuevas | No hay objetos polimórficos que crear |
| `std::variant` o plantillas propias | Innecesario; `Tipo` es una struct pequeña |

---

## 5. Decisiones de diseño propuestas

Estas decisiones se ratifican como `DU-xxx` en `specs.md` o quedan como `U-xxx`. Ninguna se ha aplicado al código.

| ID | Decisión | Estado | Impacto |
|---|---|---|---|
| `DIS-01` | Mover la impresión del AST detrás de `ok()` en `main.cpp` | **Requiere ratificación** | `RF-038` frente a `DU-005` |
| `DIS-02` | Asignar `E-14` a binarios/condición y `E-15` a unarios; precedencia `f/F` antes que `.`/`e` | Propuesta | Cierra `U-009` en parte y `HAL-04` |
| `DIS-03` | No implementar `W-08` | Propuesta | `HAL-05` → `U-012` |
| `DIS-04` | `FunctionDecl` local o en `for`-init → `E-30` (código nuevo); `StructDecl` local se admite | **Requiere ratificación** | Cierra `HAL-03`; exige ampliar el catálogo |
| `DIS-05` | Visibilidad: locales desde su declaración; globales desde su declaración; funciones/structs adelantadas | Propuesta | `RF-005`, `RF-012` |
| `DIS-06` | Fusión de firmas: prototipo idéntico repetido legal; doble definición `E-03`; `main` solo prototipo `E-01`; firma autoritativa la primera | Propuesta | Cierra `U-009` |
| `DIS-07` | Declarar antes de analizar el inicializador | Propuesta | `int x = x;` se resuelve a sí misma |
| `DIS-08` | `W-04` solo para variables locales, sin parámetros ni globales; no para duplicados | Propuesta | Coherente con `CA-001` |
| `DIS-09` | `dumpTabla` vuelca símbolos con tipo resuelto, sin posiciones | Propuesta | `RF-040` |
| `DIS-10` | `E-06` para poda por profundidad; `E-29` como diagnóstico 101 | Propuesta | `INV-D-009`, `RF-036` |
| `DIS-11` | Pruebas con AST construido a mano para lo inalcanzable | Propuesta | `U-008` |
| `DIS-12` | Formato de diagnóstico literal de `specs.md` §5.3, sin `(Encontrado: ...)` | Propuesta | Cierra `U-010` |
| `DIS-13` | Un `E-18` por argumento incompatible, atribuido al argumento | Propuesta | `INV-D-015` |
| `DIS-14` | Un `E-13` cuando una expresión de tipo `void` se usa como valor | Propuesta | `RF-017`, `RF-024` |

**Nota sobre `DIS-04`:** si no se aprueba `E-30`, el diseño no puede definir un comportamiento responsable para una construcción que el parser produce. El respaldo provisional sería `E-06` con mensaje “función anidada no soportada”, documentado como desviación temporal. No se implementa nada hasta que se decida.

---

## 6. Plan de verificación

### 6.1 Niveles

| Nivel | Herramienta | Qué demuestra |
|---|---|---|
| `TEST-01` | `pruebas_semantico` sobre archivos | `CL-001`…`CL-054` alcanzables por el parser |
| `TEST-02` | `pruebas_semantico_ast` con AST manual | Constructos inalcanzables (`LIM-02`, `LIM-06`), `CL-050`, `CL-051` |
| `TEST-03` | Arnés de conteo de visitas | `INV-D-001`, `RNF-007` |
| `TEST-04` | Dos ejecuciones y `diff` | `CA-004`, `RNF-009` |
| `TEST-05` | `toString()` antes/después | `CA-005`, `RNF-004` |
| `TEST-06` | Medición con `-O2` | `RNF-008` |

### 6.2 Cobertura de casos

| Grupo de `CL` | Mecanismo | Observación |
|---|---|---|
| `CL-001`…`CL-005` | `TEST-01` | Programa vacío, sin `main`, `main` inválido, duplicado |
| `CL-006`…`CL-017` | `TEST-01` | `void`, tipos, retorno, ámbitos |
| `CL-018`, `CL-019` | `TEST-01` | `E-05` y `E-04` |
| `CL-020`…`CL-031` | `TEST-01` | Llamadas, tipos, literales, `W-07` |
| `CL-032`…`CL-036` | `TEST-01` | Structs y constantes |
| `CL-037`, `CL-038` | `TEST-01` | `E-20` y orden `E-20`→`E-22` |
| `CL-039`…`CL-046` | `TEST-01` | Prototipos, recursión, global, `for` |
| `CL-047` | `TEST-01` | `W-04` |
| `CL-048`, `CL-049` | Llamada directa a la biblioteca | `E-00` con banderas |
| `CL-050`, `CL-051` | `TEST-02` | Nulos y profundidad |
| `CL-052` | `TEST-04` | Dos instancias |
| `CL-053`, `CL-054` | `TEST-01` | Identificadores y literales; la parte `0x10` de `CL-054` es un error léxico y se verifica en el lexer (`HAL-11`) |
| `E-21`, `E-22` válido, struct como variable | `TEST-02` | Inalcanzables por el parser |

### 6.3 Criterios

- `CA-001`: códigos, líneas y orden exactos; advertencias en `{W-03, W-04}`; código `3`; AST no impreso.
- `CA-002`: cero errores; tabla con `Punto`, `fact`, `suma`, `main`; dos ejecuciones idénticas.
- `CA-003`: exactamente un `E-00` por cada bandera.
- `CA-004`: `diff` vacío entre ejecuciones.
- `CA-005`: `toString()` idéntico antes y después.
- `CA-006`: 100 000 nodos en menos de 100 ms; 10 000 bloques terminan o podan.
- `CA-007`: 0 avisos con `-std=c++17 -Wall -Wextra`.
- `CA-008`: `0/1/2/3` y AST condicionado; parte CMake pendiente (`SUP-002`).

### 6.4 Verificación de memoria (`U-007`)

Sin ASan ni valgrind, se propone:

1. Revisión de propiedad: la fase no asigna memoria con `new`; la tabla y el mapa son observadores.
2. Contador de nodos vivos en el arnés: se construye y destruye un AST y se comprueba que el contador vuelve a cero.
3. Ejecución repetida de `CA-004`: un crecimiento de memoria entre ejecuciones sería visible en el consumo del proceso.

Se declara **verificación parcial**, no equivalencia con ASan.

---

## 7. Inventario de archivos y cambios

### 7.1 Archivos nuevos

| Archivo | Contenido |
|---|---|
| `Semantic/CMakeLists.txt` | `SemanticLib`, `pruebas_semantico`, `pruebas_semantico_ast` |
| `Semantic/Semantic.hpp` | API pública (`ContextoEntrada`, `ResultadoSemantico`, `SemanticAnalyzer`, `Diagnostico`) |
| `Semantic/Semantic.cpp` | Precondición, pasadas, `main`, orden, emisión |
| `Semantic/Tipos.hpp` / `Tipos.cpp` | `Tipo`, `TipoBase`, normalización y conversiones |
| `Semantic/TablaSimbolos.hpp` / `TablaSimbolos.cpp` | Ámbitos, símbolos, búsqueda y volcado |
| `Semantic/Diagnostico.hpp` / `Diagnostico.cpp` | Colector, límite, orden y formato |
| `Semantic/AnalizadorVisitor.hpp` / `AnalizadorVisitor.cpp` | 16 `visit`, tipado y ámbitos |
| `Semantic/tests/pruebas_semantico.cpp` | Ejecutable `RF-042` |
| `Semantic/tests/pruebas_ast.cpp` | AST manual para lo inalcanzable |

### 7.2 Archivos existentes que se modifican

| Archivo | Líneas actuales | Cambio |
|---|---|---|
| `CMakeLists.txt` | 9-11 | Insertar `add_subdirectory(Semantic)` |
| `app/CMakeLists.txt` | 5-8 | Añadir `SemanticLib` |
| `app/main.cpp` | 1-6, 12-14, 80-85 | Incluir cabecera, constante `3`, invocar y condicionar la impresión |

### 7.3 Lo que no se toca

`Lexer/` (`Lexer.cpp`, `Lexer.hpp`, `Regex.hpp`, `Tokens.hpp`, `CMakeLists.txt`), `Parser/` (`Parser.cpp`, `Parser.hpp`, `AST.cpp`, `AST.hpp`, `CMakeLists.txt`), `doc.md`, `AGENTS.md`, `specs.md`.

### 7.4 CMake propuesto

```cmake
add_library(SemanticLib
    Semantic.cpp
    Semantic.hpp
    Tipos.cpp
    Tipos.hpp
    TablaSimbolos.cpp
    TablaSimbolos.hpp
    Diagnostico.cpp
    Diagnostico.hpp
    AnalizadorVisitor.cpp
    AnalizadorVisitor.hpp
)
target_include_directories(SemanticLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SemanticLib PUBLIC ParserLib)

add_executable(pruebas_semantico tests/pruebas_semantico.cpp)
target_link_libraries(pruebas_semantico PRIVATE SemanticLib)

add_executable(pruebas_semantico_ast tests/pruebas_ast.cpp)
target_link_libraries(pruebas_semantico_ast PRIVATE SemanticLib)
```

`ParserLib` propaga `LexerLib` como `PUBLIC` (`V-002`), así que el ejecutable de pruebas obtiene `Lexer.hpp` sin rutas adicionales.

---

## 8. Riesgos

| ID | Riesgo | Probabilidad | Mitigación |
|---|---|---|---|
| `RSG-01` | Un `visit` olvida recorrer un hijo y el árbol queda parcialmente analizado | Media | `TEST-03` cuenta visitas por pasada; revisión de los 16 métodos |
| `RSG-02` | Los diagnósticos apuntan al tipo y no al nombre | Alta (estructural) | Aceptado en `HER-001`; los criterios comparan línea, no columna exacta del nombre |
| `RSG-03` | `E-23` cambia el número de diagnósticos según la firma autoritativa | Media | `DIS-06` fija la primera firma; traza de `CA-001` |
| `RSG-04` | `E-18` por argumento produce más diagnósticos de los previstos | Baja | `DIS-13`; `CA-001` no tiene argumentos incompatibles |
| `RSG-05` | El límite de 100 errores produce un diagnóstico extra (`E-29`) | Baja | Documentado en `DIS-10`; los casos de aceptación tienen menos de 100 |
| `RSG-06` | `unordered_map` con punteros ralentiza o consume memoria | Baja | `reserve` inicial; nunca se itera |
| `RSG-07` | CMake no se puede verificar en este entorno | Alta | `SUP-002`; build manual documentado; se reporta como parcial |
| `RSG-08` | Declaraciones anidadas sin comportamiento definido | Alta | `DIS-04` bloquea la implementación de ese caso |
| `RSG-09` | `int` de 32 bits asumido para `E-25` | Baja | Usar `std::numeric_limits<int>`, no una constante |
| `RSG-10` | El AST contiene un `kind()` desconocido | Muy baja | `E-06` defensivo tras el `switch` |
| `RSG-11` | La fase se usa después de destruir el AST | Baja | Contrato `PRE-001`/`INV-D-016`; el resultado no se puede construir sin AST vivo |

---

## 9. Dudas abiertas

| ID | Duda | ¿Bloquea la implementación? | Qué la resuelve |
|---|---|---|---|
| `U-007` | Verificación de fugas sin ASan/valgrind | No | §6.4; decisión del equipo |
| `U-008` | Alcance de las pruebas con AST manual | No | `DIS-11` |
| `U-009` | Firma autoritativa tras `E-23` | **Sí, parcialmente** | `DIS-06` |
| `U-010` | Formato literal del diagnóstico | No | `DIS-12` |
| `U-011` | Nivel de detalle de `docs.md` | No | — |
| `U-012` (propuesta) | `W-08` sin requisito | No | `DIS-03` |
| `U-013` (propuesta) | Código para función anidada | **Sí** | `DIS-04` |
| `U-014` (propuesta) | `E-13` para `void` usado como valor | No | `DIS-14` |
| `U-015` (propuesta) | `DIS-01` frente a `DU-005` | **Sí** | Ratificación |
| `U-016` (propuesta) | `V-015` y `CL-054` describen mal `0x10` (es error léxico, no dos tokens ni error sintáctico) | No | Corregir `specs.md` (`HAL-11`) |

Además, `SUP-001` y `SUP-002` seguirán abiertos mientras no haya MSVC ni CMake en el entorno.

---

## 10. Trazabilidad y criterios de salida

### 10.1 Qué cubre este diseño

- Los 42 `RF` y los 16 `RNF` tienen un componente y una sección asignados (§4.1, §4.2).
- Los 8 `CA` tienen procedimiento (§6.3).
- Los 54 `CL` tienen mecanismo de prueba (§6.2), salvo los que dependen de `U-008`.
- Las 16 `INV-D` tienen una regla que las sostiene (§4.3).

### 10.2 Qué no cubre

| Tema | Motivo |
|---|---|
| `W-08` | Sin requisito (`DIS-03`) |
| Función anidada | Sin decisión (`DIS-04`); bloqueado |
| Acceso a miembro con base struct por el parser | Imposible (`LIM-02`); solo AST manual |
| MSVC y CMake desde cero | Entorno (`SUP-001`, `SUP-002`) |
| Fugas medidas con ASan | Entorno (`U-007`) |

### 10.3 Criterios para pasar a implementación

1. `DIS-01`, `DIS-04`, `DIS-06`, `DIS-09` ratificadas (o corregidas).
2. `U-013` resuelta: qué código se usa para una función anidada.
3. Confirmación de que el formato de `DIS-12` es el definitivo.
4. Aceptación de que `CA-001` se verifica por línea, código y orden, no por columna exacta del nombre.

Si estos puntos se cumplen, la implementación puede empezar por `Tipos` y `TablaSimbolos`, seguir por `Diagnostico`, después `AnalizadorVisitor` y terminar en `Semantic` y la integración, con las pruebas de §6 en cada paso.
