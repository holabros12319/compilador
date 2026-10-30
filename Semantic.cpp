#include "Semantic.hpp"

#include <algorithm>
#include <charconv>
#include <iostream>
#include <limits>
#include <map>
#include <ostream>
#include <set>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace {

bool esNumerico(Tipo tipo) {
    return tipo.base == TipoBase::Int || tipo.base == TipoBase::Float ||
           tipo.base == TipoBase::Double || tipo.base == TipoBase::Char;
}

bool esEntero(Tipo tipo) {
    return tipo.base == TipoBase::Int || tipo.base == TipoBase::Char;
}

bool convertibleABool(Tipo tipo) {
    return esNumerico(tipo) || tipo.base == TipoBase::Bool;
}

const char* nombreTipo(Tipo tipo) {
    switch (tipo.base) {
    case TipoBase::Int: return "int";
    case TipoBase::Float: return "float";
    case TipoBase::Double: return "double";
    case TipoBase::Char: return "char";
    case TipoBase::Bool: return "bool";
    case TipoBase::Void: return "void";
    case TipoBase::Struct: return "struct";
    case TipoBase::Cadena: return "cadena";
    default: return "error";
    }
}

bool iguales(Tipo izquierda, Tipo derecha) {
    return izquierda.base == derecha.base && izquierda.estructura == derecha.estructura;
}

class Visitor final : public ASTVisitor {
public:
    Visitor(TablaSimbolos& tabla, std::vector<Diagnostico>& diagnosticos,
            std::size_t limite)
        : tabla_(tabla), diagnosticos_(diagnosticos), limite_(limite) {
        tabla_.ambitos_.push_back({"global", -1, {}});
        pila_.push_back(0);
    }

    void primeraPasada(const ProgramNode& programa) {
        for (std::size_t i = 0; i < programa.declarations.size() && !detenido_; ++i) {
            const StmtPtr& declaracion = programa.declarations[i];
            if (!declaracion) { emitir("E-06", "Nodo nulo en el programa.", programa.ubicacion); continue; }
            indiceGlobal_ = i;
            switch (declaracion->kind()) {
            case NodeKind::VarDecl: registrarVariable(static_cast<const VarDecl&>(*declaracion), true); break;
            case NodeKind::FunctionDecl: registrarFuncion(static_cast<const FunctionDecl&>(*declaracion)); break;
            case NodeKind::StructDecl: registrarStruct(static_cast<const StructDecl&>(*declaracion)); break;
            default: break;
            }
        }
        comprobarMain(programa);
        for (const auto& par : tabla_.ambitos_[0].simbolos) {
            const auto& simbolo = par.second;
            if (simbolo.categoria == TablaSimbolos::Ambito::Simbolo::Categoria::Funcion &&
                simbolo.prototipo != nullptr && simbolo.definicion == nullptr &&
                simbolo.nombre != "main") {
                advertir("W-05", "Prototipo sin definición: " + simbolo.nombre + ".",
                         simbolo.prototipo->ubicacion);
            }
        }
    }

    void visit(const ProgramNode& nodo) override {
        for (std::size_t i = 0; i < nodo.declarations.size() && !detenido_; ++i) {
            indiceGlobal_ = i;
            if (!nodo.declarations[i]) { emitir("E-06", "Nodo nulo en el programa.", nodo.ubicacion); continue; }
            switch (nodo.declarations[i]->kind()) {
            case NodeKind::VarDecl: visit(static_cast<const VarDecl&>(*nodo.declarations[i])); break;
            case NodeKind::FunctionDecl: visit(static_cast<const FunctionDecl&>(*nodo.declarations[i])); break;
            case NodeKind::StructDecl: visit(static_cast<const StructDecl&>(*nodo.declarations[i])); break;
            default: emitir("E-24", "Sentencia ejecutable en ámbito global.", nodo.declarations[i]->ubicacion); break;
            }
        }
    }

    void visit(const FunctionDecl& nodo) override {
        // El parser permite esta forma, pero la especificación no le da
        // semántica. Se usa E-06, código existente para construcción no
        // soportada, sin inventar el E-30 propuesto y no ratificado.
        if (pila_.size() != 1) {
            emitir("E-06", "Función local no soportada.", nodo.ubicacion);
            return;
        }
        if (nodo.isPrototype() || !esDefinicionRegistrada(nodo)) return;
        entrar("funcion " + nodo.name);
        funcionActual_ = nodo.returnType == "void"
            ? Tipo{TipoBase::Void, nullptr}
            : tipoDeTexto(nodo.returnType, nodo.ubicacion, false);
        for (const Param& parametro : nodo.parameters) {
            Tipo tipo = tipoDeTexto(parametro.type, parametro.ubicacion, true);
            declararVariable(parametro.name, tipo, nullptr, parametro.ubicacion, true, false);
        }
        if (!nodo.body) emitir("E-06", "Cuerpo de función nulo.", nodo.ubicacion);
        else visitarSentencia(*nodo.body, true);
        if (funcionActual_.base != TipoBase::Void && nodo.body && !garantizaRetorno(*nodo.body)) {
            emitir("E-11", "La función no garantiza un retorno.", nodo.ubicacion);
        }
        salir();
        funcionActual_ = {TipoBase::Error, nullptr};
    }

    void visit(const StructDecl& nodo) override {
        if (nodo.fields.empty()) advertir("W-06", "La estructura está vacía.", nodo.ubicacion);
        std::set<std::string> campos;
        for (const auto& campo : nodo.fields) {
            if (!campo) { emitir("E-06", "Campo nulo.", nodo.ubicacion); continue; }
            if (!campos.insert(campo->name).second) emitir("E-03", "Campo duplicado: " + campo->name + ".", campo->ubicacion);
            Tipo tipo = tipoDeTexto(campo->type, campo->ubicacion, true);
            if (campo->initializer) {
                visitarExpresion(*campo->initializer);
                bool divisionCero = false;
                if (!esConstante(*campo->initializer, divisionCero)) emitir("E-27", "El inicializador del campo no es constante.", campo->initializer->ubicacion);
                else if (divisionCero) emitir("E-28", "División o módulo por cero en constante.", campo->initializer->ubicacion);
                comprobarConversion(tipo, tipoExpr(*campo->initializer), campo->initializer->ubicacion, "E-13");
            }
        }
    }

    void visit(const VarDecl& nodo) override {
        if (pila_.size() == 1) {
            const auto* simbolo = buscarActual(nodo.name);
            if (!simbolo || simbolo->declaracion != &nodo) return;
        } else {
            Tipo tipo = tipoDeTexto(nodo.type, nodo.ubicacion, false);
            if (!declararVariable(nodo.name, tipo, &nodo, nodo.ubicacion, false, false)) return;
        }
        if (nodo.initializer) {
            visitarExpresion(*nodo.initializer);
            const auto* simbolo = buscar(nodo.name);
            if (simbolo) comprobarConversion(simbolo->tipo, tipoExpr(*nodo.initializer), nodo.initializer->ubicacion, "E-13");
        }
    }

    void visit(const BlockStmt& nodo) override { visitarBloque(nodo, false); }
    void visit(const IfStmt& nodo) override {
        visitarExpresionSegura(nodo.condition, nodo.ubicacion);
        if (nodo.condition && !convertibleABool(tipoExpr(*nodo.condition)) && tipoExpr(*nodo.condition).base != TipoBase::Error)
            emitir("E-14", "La condición no es convertible a bool.", nodo.condition->ubicacion);
        visitarSentenciaSegura(nodo.thenBranch, nodo.ubicacion);
        if (nodo.elseBranch) visitarSentencia(*nodo.elseBranch, false);
    }
    void visit(const WhileStmt& nodo) override {
        visitarExpresionSegura(nodo.condition, nodo.ubicacion);
        if (nodo.condition && !convertibleABool(tipoExpr(*nodo.condition)) && tipoExpr(*nodo.condition).base != TipoBase::Error)
            emitir("E-14", "La condición no es convertible a bool.", nodo.condition->ubicacion);
        visitarSentenciaSegura(nodo.body, nodo.ubicacion);
    }
    void visit(const ForStmt& nodo) override {
        entrar("for");
        if (nodo.init) visitarSentencia(*nodo.init, false);
        if (nodo.condition) {
            visitarExpresion(*nodo.condition);
            if (!convertibleABool(tipoExpr(*nodo.condition)) && tipoExpr(*nodo.condition).base != TipoBase::Error)
                emitir("E-14", "La condición no es convertible a bool.", nodo.condition->ubicacion);
        }
        if (nodo.increment) visitarExpresion(*nodo.increment);
        visitarSentenciaSegura(nodo.body, nodo.ubicacion);
        salir();
    }
    void visit(const ReturnStmt& nodo) override {
        if (nodo.value) {
            visitarExpresion(*nodo.value);
            if (funcionActual_.base == TipoBase::Void) emitir("E-09", "return con valor en función void.", nodo.ubicacion);
            else comprobarConversion(funcionActual_, tipoExpr(*nodo.value), nodo.value->ubicacion, "E-13");
        } else if (funcionActual_.base != TipoBase::Void && funcionActual_.base != TipoBase::Error) {
            emitir("E-10", "return sin valor en función no void.", nodo.ubicacion);
        }
    }
    void visit(const ExprStmt& nodo) override {
        visitarExpresionSegura(nodo.expression, nodo.ubicacion);
        if (nodo.expression && !tieneEfecto(*nodo.expression)) advertir("W-07", "Expresión sin efecto.", nodo.expression->ubicacion);
    }
    void visit(const LiteralExpr& nodo) override {
        Tipo tipo{TipoBase::Error, nullptr};
        if (nodo.literalType == TipoToken::CADENA) tipo = {TipoBase::Cadena, nullptr};
        else if (nodo.literalType == TipoToken::CARACTER) {
            if (nodo.value.size() != 1) emitir("E-26", "Literal de carácter inválido.", nodo.ubicacion);
            else tipo = {TipoBase::Char, nullptr};
        } else if (nodo.literalType == TipoToken::NUMERO) {
            if (!nodo.value.empty() && (nodo.value.back() == 'f' || nodo.value.back() == 'F')) tipo = {TipoBase::Float, nullptr};
            else if (nodo.value.find_first_of(".eE") != std::string::npos) tipo = {TipoBase::Double, nullptr};
            else {
                tipo = {TipoBase::Int, nullptr};
                if (!nodo.value.empty() && std::string("uUlL").find(nodo.value.back()) != std::string::npos) {
                    advertir("W-03", "Sufijo numérico ignorado.", nodo.ubicacion);
                }
                std::string texto = nodo.value;
                while (!texto.empty() && std::string("uUlL").find(texto.back()) != std::string::npos) texto.pop_back();
                long long valor = 0;
                const auto conversion = std::from_chars(texto.data(), texto.data() + texto.size(), valor);
                if (conversion.ec != std::errc{} || valor > std::numeric_limits<int>::max()) {
                    emitir("E-25", "Literal entero fuera de rango.", nodo.ubicacion);
                    tipo = {TipoBase::Error, nullptr};
                }
            }
        } else emitir("E-06", "Tipo de literal no soportado.", nodo.ubicacion);
        tiposExpr_[&nodo] = tipo;
    }
    void visit(const IdentifierExpr& nodo) override {
        auto* simbolo = buscarMutable(nodo.name);
        if (!simbolo) { emitir(esUsoAntes(nodo.name) ? "E-05" : "E-04", esUsoAntes(nodo.name) ? "Identificador usado antes de declararse: " + nodo.name + "." : "Identificador no declarado: " + nodo.name + ".", nodo.ubicacion); tiposExpr_[&nodo] = {TipoBase::Error, nullptr}; return; }
        if (simbolo->categoria != TablaSimbolos::Ambito::Simbolo::Categoria::Variable) {
            emitir("E-19", "Función o estructura usada como valor: " + nodo.name + ".", nodo.ubicacion);
            tiposExpr_[&nodo] = {TipoBase::Error, nullptr}; return;
        }
        simbolo->usado = true;
        tiposExpr_[&nodo] = simbolo->tipo;
    }
    void visit(const UnaryExpr& nodo) override {
        visitarExpresionSegura(nodo.operand, nodo.ubicacion);
        Tipo operando = nodo.operand ? tipoExpr(*nodo.operand) : Tipo{};
        if (operando.base == TipoBase::Error) { tiposExpr_[&nodo] = operando; return; }
        if (nodo.op == "++" || nodo.op == "--") {
            if (!esAsignable(nodo.operand.get())) emitir("E-12", "Destino no asignable.", nodo.ubicacion);
            else if (!esNumerico(operando)) emitir("E-15", "Operando no numérico.", nodo.ubicacion);
            tiposExpr_[&nodo] = operando; return;
        }
        if (nodo.op == "!") {
            if (!convertibleABool(operando)) emitir("E-15", "Operando no convertible a bool.", nodo.ubicacion);
            tiposExpr_[&nodo] = {TipoBase::Bool, nullptr}; return;
        }
        if (!esNumerico(operando)) emitir("E-15", "Operando no numérico.", nodo.ubicacion);
        tiposExpr_[&nodo] = operando.base == TipoBase::Char ? Tipo{TipoBase::Int, nullptr} : operando;
    }
    void visit(const BinaryExpr& nodo) override {
        visitarExpresionSegura(nodo.left, nodo.ubicacion); visitarExpresionSegura(nodo.right, nodo.ubicacion);
        Tipo izq = nodo.left ? tipoExpr(*nodo.left) : Tipo{};
        Tipo der = nodo.right ? tipoExpr(*nodo.right) : Tipo{};
        if (nodo.op == "=") {
            if (!esAsignable(nodo.left.get())) emitir("E-12", "Destino no asignable.", nodo.ubicacion);
            else comprobarConversion(izq, der, nodo.ubicacion, "E-13");
            tiposExpr_[&nodo] = izq; return;
        }
        if (izq.base == TipoBase::Error || der.base == TipoBase::Error) { tiposExpr_[&nodo] = {TipoBase::Error, nullptr}; return; }
        if (nodo.op == "&&" || nodo.op == "||") {
            if (!convertibleABool(izq) || !convertibleABool(der)) emitir("E-14", "Operandos no convertibles a bool.", nodo.ubicacion);
            tiposExpr_[&nodo] = {TipoBase::Bool, nullptr}; return;
        }
        if (nodo.op == "%") {
            if (!esEntero(izq) || !esEntero(der)) emitir("E-14", "El operador % requiere enteros.", nodo.ubicacion);
            tiposExpr_[&nodo] = {TipoBase::Int, nullptr}; return;
        }
        if (nodo.op == "==" || nodo.op == "!=" || nodo.op == "<" || nodo.op == ">" || nodo.op == "<=" || nodo.op == ">=") {
            if (!((esNumerico(izq) && esNumerico(der)) || (izq.base == TipoBase::Bool && der.base == TipoBase::Bool))) emitir("E-14", "Operandos incompatibles en comparación.", nodo.ubicacion);
            tiposExpr_[&nodo] = {TipoBase::Bool, nullptr}; return;
        }
        if (!esNumerico(izq) || !esNumerico(der)) { emitir("E-14", "Operandos no numéricos.", nodo.ubicacion); tiposExpr_[&nodo] = {TipoBase::Error, nullptr}; return; }
        tiposExpr_[&nodo] = promover(izq, der);
    }
    void visit(const CallExpr& nodo) override {
        for (const auto& argumento : nodo.arguments) visitarExpresionSegura(argumento, nodo.ubicacion);
        const IdentifierExpr* identificador = nullptr;
        if (nodo.callee && nodo.callee->kind() == NodeKind::IdentifierExpr) identificador = static_cast<const IdentifierExpr*>(nodo.callee.get());
        const auto* simbolo = identificador ? buscar(identificador->name) : nullptr;
        if (!simbolo || simbolo->categoria != TablaSimbolos::Ambito::Simbolo::Categoria::Funcion) {
            emitir("E-16", "La llamada no tiene como destino una función.", nodo.ubicacion); tiposExpr_[&nodo] = {TipoBase::Error, nullptr}; return;
        }
        if (simbolo->parametros.size() != nodo.arguments.size()) emitir("E-17", "Cantidad incorrecta de argumentos.", nodo.ubicacion);
        const std::size_t total = std::min(simbolo->parametros.size(), nodo.arguments.size());
        for (std::size_t i = 0; i < total; ++i) {
            if (!nodo.arguments[i]) continue;
            comprobarConversion(simbolo->parametros[i], tipoExpr(*nodo.arguments[i]), nodo.arguments[i]->ubicacion, "E-18");
        }
        tiposExpr_[&nodo] = simbolo->tipo;
    }
    void visit(const MemberExpr& nodo) override {
        visitarExpresionSegura(nodo.object, nodo.ubicacion);
        Tipo objeto = nodo.object ? tipoExpr(*nodo.object) : Tipo{};
        if (objeto.base != TipoBase::Struct) { emitir("E-20", "Acceso a miembro sobre tipo no struct.", nodo.ubicacion); tiposExpr_[&nodo] = {TipoBase::Error, nullptr}; return; }
        if (nodo.isPointer) emitir("E-22", "-> requiere una base puntero.", nodo.ubicacion);
        const auto* estructura = objeto.estructura;
        if (!estructura) { tiposExpr_[&nodo] = {TipoBase::Error, nullptr}; return; }
        for (const auto& campo : estructura->fields) if (campo && campo->name == nodo.member) { tiposExpr_[&nodo] = tipoDeTexto(campo->type, campo->ubicacion, true); return; }
        emitir("E-21", "Miembro inexistente: " + nodo.member + ".", nodo.ubicacion);
        tiposExpr_[&nodo] = {TipoBase::Error, nullptr};
    }

private:
    TablaSimbolos& tabla_; std::vector<Diagnostico>& diagnosticos_; std::size_t limite_; std::vector<int> pila_;
    std::unordered_map<const Expr*, Tipo> tiposExpr_; Tipo funcionActual_{TipoBase::Error, nullptr};
    std::vector<std::set<std::string>> pendientes_;
    std::size_t profundidad_ = 0;
    std::size_t indiceGlobal_ = 0; std::size_t secuencia_ = 0; bool detenido_ = false;

    TablaSimbolos::Ambito& actual() { return tabla_.ambitos_[pila_.back()]; }
    void entrar(const std::string& etiqueta) { tabla_.ambitos_.push_back({etiqueta, pila_.back(), {}}); pila_.push_back(static_cast<int>(tabla_.ambitos_.size() - 1)); }
    void salir() { if (pila_.size() > 1) pila_.pop_back(); }
    void emitir(const std::string& codigo, const std::string& mensaje, Ubicacion ubicacion) {
        if (detenido_) return;
        if (limite_ == 0 || static_cast<std::size_t>(std::count_if(diagnosticos_.begin(), diagnosticos_.end(), [](const Diagnostico& d) { return d.severidad == Severidad::Error; })) >= limite_) {
            diagnosticos_.push_back({"E-29", Severidad::Error, "Límite de errores alcanzado.", ubicacion, secuencia_++}); detenido_ = true; return;
        }
        diagnosticos_.push_back({codigo, Severidad::Error, mensaje, ubicacion, secuencia_++});
    }
    void advertir(const std::string& codigo, const std::string& mensaje, Ubicacion ubicacion) { if (!detenido_) diagnosticos_.push_back({codigo, Severidad::Advertencia, mensaje, ubicacion, secuencia_++}); }
    void emitirNoUsadas(const TablaSimbolos::Ambito& ambito) { for (const auto& par : ambito.simbolos) { const auto& simbolo = par.second; if (simbolo.categoria == TablaSimbolos::Ambito::Simbolo::Categoria::Variable && !simbolo.parametro && !simbolo.usado && simbolo.declaracion) advertir("W-04", "Variable declarada y no usada: " + simbolo.nombre + ".", simbolo.declaracion->ubicacion); } }
    const TablaSimbolos::Ambito::Simbolo* buscarActual(const std::string& nombre) const { const auto it = tabla_.ambitos_[pila_.back()].simbolos.find(nombre); return it == tabla_.ambitos_[pila_.back()].simbolos.end() ? nullptr : &it->second; }
    const TablaSimbolos::Ambito::Simbolo* buscar(const std::string& nombre) const { for (int i = static_cast<int>(pila_.size()) - 1; i >= 0; --i) { const auto& a = tabla_.ambitos_[pila_[i]]; const auto it = a.simbolos.find(nombre); if (it != a.simbolos.end()) { if (pila_.size() == 1 && it->second.categoria == TablaSimbolos::Ambito::Simbolo::Categoria::Variable && it->second.ordenGlobal > indiceGlobal_) return nullptr; return &it->second; } } return nullptr; }
    TablaSimbolos::Ambito::Simbolo* buscarMutable(const std::string& nombre) { for (int i = static_cast<int>(pila_.size()) - 1; i >= 0; --i) { auto& a = tabla_.ambitos_[pila_[i]]; auto it = a.simbolos.find(nombre); if (it != a.simbolos.end()) { if (pila_.size() == 1 && it->second.categoria == TablaSimbolos::Ambito::Simbolo::Categoria::Variable && it->second.ordenGlobal > indiceGlobal_) return nullptr; return &it->second; } } return nullptr; }
    bool esUsoAntes(const std::string& nombre) const { if (!pendientes_.empty() && pendientes_.back().count(nombre)) return true; const auto it = tabla_.ambitos_[0].simbolos.find(nombre); return it != tabla_.ambitos_[0].simbolos.end() && it->second.categoria == TablaSimbolos::Ambito::Simbolo::Categoria::Variable && it->second.ordenGlobal > indiceGlobal_; }
    Tipo tipoDeTexto(const std::string& texto, Ubicacion ubicacion, bool campoOParametro) {
        if (texto == "int") return {TipoBase::Int, nullptr}; if (texto == "float") return {TipoBase::Float, nullptr}; if (texto == "double") return {TipoBase::Double, nullptr}; if (texto == "char") return {TipoBase::Char, nullptr}; if (texto == "bool") return {TipoBase::Bool, nullptr};
        if (texto == "void") { emitir(campoOParametro ? "E-08" : "E-07", campoOParametro ? "Campo o parámetro de tipo void." : "Variable de tipo void.", ubicacion); return {TipoBase::Error, nullptr}; }
        const auto it = tabla_.ambitos_[0].simbolos.find(texto); if (it != tabla_.ambitos_[0].simbolos.end() && it->second.categoria == TablaSimbolos::Ambito::Simbolo::Categoria::Estructura) return {TipoBase::Struct, static_cast<const StructDecl*>(it->second.declaracion)};
        emitir("E-06", "Tipo no soportado: " + texto + ".", ubicacion); return {TipoBase::Error, nullptr};
    }
    bool declararVariable(const std::string& nombre, Tipo tipo, const Decl* declaracion, Ubicacion ubicacion, bool parametro, bool global) {
        if (actual().simbolos.count(nombre)) { emitir("E-03", "Nombre duplicado: " + nombre + ".", ubicacion); return false; }
        actual().simbolos.emplace(nombre, TablaSimbolos::Ambito::Simbolo{nombre, TablaSimbolos::Ambito::Simbolo::Categoria::Variable, tipo, {}, declaracion, nullptr, nullptr, false, parametro, global ? indiceGlobal_ : 0}); actual().ordenSimbolos.push_back(nombre); return true;
    }
    void registrarVariable(const VarDecl& nodo, bool global) { Tipo tipo = tipoDeTexto(nodo.type, nodo.ubicacion, false); declararVariable(nodo.name, tipo, &nodo, nodo.ubicacion, false, global); }
    void registrarStruct(const StructDecl& nodo) { if (actual().simbolos.count(nodo.name)) { emitir("E-03", "Nombre duplicado: " + nodo.name + ".", nodo.ubicacion); return; } actual().simbolos.emplace(nodo.name, TablaSimbolos::Ambito::Simbolo{nodo.name, TablaSimbolos::Ambito::Simbolo::Categoria::Estructura, {TipoBase::Struct, &nodo}, {}, &nodo}); actual().ordenSimbolos.push_back(nodo.name); }
    void registrarFuncion(const FunctionDecl& nodo) {
        Tipo retorno = nodo.returnType == "void" ? Tipo{TipoBase::Void, nullptr} : tipoDeTexto(nodo.returnType, nodo.ubicacion, false); std::vector<Tipo> parametros; for (const Param& p : nodo.parameters) parametros.push_back(tipoDeTexto(p.type, p.ubicacion, true));
        auto it = actual().simbolos.find(nodo.name); if (it == actual().simbolos.end()) { TablaSimbolos::Ambito::Simbolo s{nodo.name, TablaSimbolos::Ambito::Simbolo::Categoria::Funcion, retorno, parametros, &nodo}; if (nodo.isPrototype()) s.prototipo = &nodo; else s.definicion = &nodo; actual().simbolos.emplace(nodo.name, std::move(s)); actual().ordenSimbolos.push_back(nodo.name); return; }
        auto& s = it->second; if (s.categoria != TablaSimbolos::Ambito::Simbolo::Categoria::Funcion) { emitir("E-03", "Nombre duplicado: " + nodo.name + ".", nodo.ubicacion); return; }
        if (!iguales(s.tipo, retorno) || s.parametros.size() != parametros.size() || !std::equal(s.parametros.begin(), s.parametros.end(), parametros.begin(), iguales)) emitir("E-23", "Conflicto entre prototipo y definición: " + nodo.name + ".", nodo.ubicacion);
        if (!nodo.isPrototype() && s.definicion) emitir("E-03", "Definición duplicada: " + nodo.name + ".", nodo.ubicacion); else if (!nodo.isPrototype()) s.definicion = &nodo; else if (!s.prototipo) s.prototipo = &nodo;
    }
    bool esDefinicionRegistrada(const FunctionDecl& nodo) const { const auto* s = buscar(nodo.name); return s && s->definicion == &nodo; }
    void comprobarMain(const ProgramNode& programa) {
        const auto it = tabla_.ambitos_[0].simbolos.find("main");
        if (it == tabla_.ambitos_[0].simbolos.end() ||
            it->second.categoria != TablaSimbolos::Ambito::Simbolo::Categoria::Funcion ||
            it->second.definicion == nullptr) {
            emitir("E-01", "No existe una definición de la función main.", programa.ubicacion);
            return;
        }
        const auto& simbolo = it->second;
        if (simbolo.tipo.base != TipoBase::Int || !simbolo.parametros.empty())
            emitir("E-02", "main debe tener la firma int main().", simbolo.definicion->ubicacion);
    }
    void visitarSentencia(const Stmt& nodo, bool cuerpo) { if (detenido_) return; if (++profundidad_ > 256) { --profundidad_; emitir("E-06", "Profundidad máxima excedida.", nodo.ubicacion); return; } if (nodo.kind() == NodeKind::BlockStmt && cuerpo) visitarBloque(static_cast<const BlockStmt&>(nodo), true); else nodo.accept(*this); --profundidad_; }
    void visitarSentenciaSegura(const StmtPtr& nodo, Ubicacion ubicacion) { if (!nodo) emitir("E-06", "Sentencia nula.", ubicacion); else visitarSentencia(*nodo, false); }
    void visitarBloque(const BlockStmt& nodo, bool compartir) { if (!compartir) entrar("bloque"); std::set<std::string> nombres; for (const auto& sentencia : nodo.statements) if (sentencia && sentencia->kind() == NodeKind::VarDecl) nombres.insert(static_cast<const VarDecl&>(*sentencia).name); pendientes_.push_back(std::move(nombres)); bool retorno = false; for (const auto& sentencia : nodo.statements) { if (!sentencia) { emitir("E-06", "Sentencia nula.", nodo.ubicacion); continue; } if (retorno) { advertir("W-01", "Código inalcanzable tras return.", sentencia->ubicacion); retorno = false; } if (sentencia->kind() == NodeKind::VarDecl) pendientes_.back().erase(static_cast<const VarDecl&>(*sentencia).name); visitarSentencia(*sentencia, false); if (garantizaRetorno(*sentencia)) retorno = true; } pendientes_.pop_back(); emitirNoUsadas(actual()); if (!compartir) salir(); }
    void visitarExpresion(const Expr& nodo) { if (detenido_) return; if (++profundidad_ > 256) { --profundidad_; emitir("E-06", "Profundidad máxima excedida.", nodo.ubicacion); return; } nodo.accept(*this); --profundidad_; }
    void visitarExpresionSegura(const ExprPtr& nodo, Ubicacion ubicacion) { if (!nodo) emitir("E-06", "Expresión nula.", ubicacion); else visitarExpresion(*nodo); }
    Tipo tipoExpr(const Expr& nodo) const { const auto it = tiposExpr_.find(&nodo); return it == tiposExpr_.end() ? Tipo{} : it->second; }
    Tipo promover(Tipo a, Tipo b) const { if (a.base == TipoBase::Double || b.base == TipoBase::Double) return {TipoBase::Double, nullptr}; if (a.base == TipoBase::Float || b.base == TipoBase::Float) return {TipoBase::Float, nullptr}; return {TipoBase::Int, nullptr}; }
    bool esAsignable(const Expr* expr) const { if (!expr) return false; if (expr->kind() == NodeKind::MemberExpr) return true; if (expr->kind() != NodeKind::IdentifierExpr) return false; const auto* s = buscar(static_cast<const IdentifierExpr*>(expr)->name); return s && s->categoria == TablaSimbolos::Ambito::Simbolo::Categoria::Variable; }
    void comprobarConversion(Tipo destino, Tipo origen, Ubicacion ubicacion, const std::string& codigo) { if (destino.base == TipoBase::Error || origen.base == TipoBase::Error || iguales(destino, origen)) return; if ((destino.base == TipoBase::Float && origen.base == TipoBase::Int) || (destino.base == TipoBase::Double && (origen.base == TipoBase::Int || origen.base == TipoBase::Float)) || (destino.base == TipoBase::Int && origen.base == TipoBase::Char)) return; if (destino.base == TipoBase::Float && origen.base == TipoBase::Double) { advertir("W-02", "Conversión de double a float.", ubicacion); return; } emitir(codigo, "Tipos incompatibles: " + std::string(nombreTipo(origen)) + " a " + nombreTipo(destino) + ".", ubicacion); }
    bool garantizaRetorno(const Stmt& nodo) const { if (nodo.kind() == NodeKind::ReturnStmt) return static_cast<const ReturnStmt&>(nodo).value != nullptr; if (nodo.kind() == NodeKind::IfStmt) { const auto& i = static_cast<const IfStmt&>(nodo); return i.thenBranch && i.elseBranch && garantizaRetorno(*i.thenBranch) && garantizaRetorno(*i.elseBranch); } if (nodo.kind() == NodeKind::BlockStmt) { const auto& b = static_cast<const BlockStmt&>(nodo); return !b.statements.empty() && b.statements.back() && garantizaRetorno(*b.statements.back()); } return false; }
    bool tieneEfecto(const Expr& expr) const { if (expr.kind() == NodeKind::CallExpr) return true; if (expr.kind() == NodeKind::UnaryExpr) { const auto& u = static_cast<const UnaryExpr&>(expr); return u.op == "++" || u.op == "--"; } return expr.kind() == NodeKind::BinaryExpr && static_cast<const BinaryExpr&>(expr).op == "="; }
    bool esConstante(const Expr& expr, bool& divisionCero) const { if (expr.kind() == NodeKind::LiteralExpr) return true; if (expr.kind() == NodeKind::UnaryExpr) { const auto& u = static_cast<const UnaryExpr&>(expr); return (u.op == "+" || u.op == "-" || u.op == "!") && u.operand && esConstante(*u.operand, divisionCero); } if (expr.kind() != NodeKind::BinaryExpr) return false; const auto& b = static_cast<const BinaryExpr&>(expr); if (!b.left || !b.right || !esConstante(*b.left, divisionCero) || !esConstante(*b.right, divisionCero)) return false; if ((b.op == "/" || b.op == "%") && b.right->kind() == NodeKind::LiteralExpr && static_cast<const LiteralExpr&>(*b.right).value == "0") divisionCero = true; return true; }
};

} // namespace

void TablaSimbolos::dumpTabla(std::ostream& salida) const {
    salida << "(Tabla\n";
    for (const Ambito& ambito : ambitos_) {
        salida << "  (Ambito " << ambito.etiqueta << "\n";
        for (const std::string& nombre : ambito.ordenSimbolos) {
            const Ambito::Simbolo& s = ambito.simbolos.at(nombre);
            salida << "    (" << (s.categoria == Ambito::Simbolo::Categoria::Variable ? "Var" : s.categoria == Ambito::Simbolo::Categoria::Funcion ? "Func" : "Struct") << " " << s.nombre << " : " << nombreTipo(s.tipo) << ")\n";
        }
        salida << "  )\n";
    }
    salida << ")\n";
}

bool ResultadoSemantico::ok() const { return errores() == 0; }
const std::vector<Diagnostico>& ResultadoSemantico::diagnosticos() const { return diagnosticos_; }
std::size_t ResultadoSemantico::errores() const { return static_cast<std::size_t>(std::count_if(diagnosticos_.begin(), diagnosticos_.end(), [](const Diagnostico& d) { return d.severidad == Severidad::Error; })); }
std::size_t ResultadoSemantico::advertencias() const { return diagnosticos_.size() - errores(); }
const TablaSimbolos& ResultadoSemantico::tabla() const { return tabla_; }
void ResultadoSemantico::dumpTabla(std::ostream& salida) const { tabla_.dumpTabla(salida); }

SemanticAnalyzer::SemanticAnalyzer(std::ostream& salida, std::size_t limiteErrores) : salida_(salida), limiteErrores_(limiteErrores) {}
ResultadoSemantico SemanticAnalyzer::analyze(const ProgramNode& programa, const ContextoEntrada& contexto) {
    ResultadoSemantico resultado;
    if (contexto.erroresLexicos || contexto.erroresSintacticos) {
        resultado.diagnosticos_.push_back({"E-00", Severidad::Error, "Hay errores léxicos o sintácticos previos.", programa.ubicacion, 0});
    } else {
        Visitor visitante(resultado.tabla_, resultado.diagnosticos_, limiteErrores_);
        visitante.primeraPasada(programa);
        programa.accept(visitante);
    }
    std::stable_sort(resultado.diagnosticos_.begin(), resultado.diagnosticos_.end(), [](const Diagnostico& a, const Diagnostico& b) { if (a.ubicacion.linea != b.ubicacion.linea) return a.ubicacion.linea < b.ubicacion.linea; if (a.ubicacion.columna != b.ubicacion.columna) return a.ubicacion.columna < b.ubicacion.columna; return a.secuencia < b.secuencia; });
    for (const Diagnostico& d : resultado.diagnosticos_) salida_ << (d.severidad == Severidad::Error ? "[Error Semántico]" : "[Advertencia Semántica]") << " Línea " << d.ubicacion.linea << ", Columna " << d.ubicacion.columna << ": " << d.mensaje << " (" << d.codigo << ")\n";
    return resultado;
}
