#include "Optimizer.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Constantes numericas
// ---------------------------------------------------------------------------

struct Const {
    bool      isDouble = false;
    long long i        = 0;
    double    d        = 0.0;

    double asDouble() const { return isDouble ? d : static_cast<double>(i); }
    bool   truthy()   const { return isDouble ? d != 0.0 : i != 0; }
};

Const makeInt(long long v)  { Const c; c.isDouble = false; c.i = v; return c; }
Const makeDouble(double v)  { Const c; c.isDouble = true;  c.d = v; return c; }

bool fitsInt(long long v) { return v >= INT_MIN && v <= INT_MAX; }

// Convierte el lexema de un NUMERO. Los enteros fuera del rango de int y los
// valores no finitos NO se consideran constantes (no se tocan).
bool parseNumber(const std::string& lex, Const& out) {
    if (lex.empty()) return false;

    const bool integerLike = std::all_of(lex.begin(), lex.end(),
                                         [](char c) { return c >= '0' && c <= '9'; });
    if (integerLike) {
        long long v = 0;
        auto r = std::from_chars(lex.data(), lex.data() + lex.size(), v);
        if (r.ec != std::errc() || r.ptr != lex.data() + lex.size() || !fitsInt(v)) return false;
        out = makeInt(v);
        return true;
    }

    errno = 0;
    char* end = nullptr;
    const double d = std::strtod(lex.c_str(), &end);
    if (end != lex.c_str() + lex.size() || errno == ERANGE || !std::isfinite(d)) return false;
    out = makeDouble(d);
    return true;
}

// Representacion mas corta que vuelve a dar el mismo double (3.1416, no 3.1415999...).
std::string formatDouble(double d) {
    char buf[64];
    for (int prec = 15; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*g", prec, d);
        if (std::strtod(buf, nullptr) == d) break;
    }
    std::string s = buf;
    if (s.find_first_of(".eE") == std::string::npos) s += ".0";
    return s;
}

// Reconoce un literal numerico o "-literal" (asi es como el Parser representa
// los negativos: UnaryExpr('-', Literal)).
bool getConstant(const Expr* e, Const& out) {
    if (!e) return false;
    if (e->kind() == NodeKind::LiteralExpr) {
        const auto* lit = static_cast<const LiteralExpr*>(e);
        return lit->literalType == TipoToken::NUMERO && parseNumber(lit->value, out);
    }
    if (e->kind() == NodeKind::UnaryExpr) {
        const auto* u = static_cast<const UnaryExpr*>(e);
        if (u->isPrefix && u->op == "-" && u->operand &&
            u->operand->kind() == NodeKind::LiteralExpr) {
            Const inner;
            if (!getConstant(u->operand.get(), inner)) return false;
            out = inner.isDouble ? makeDouble(-inner.d) : makeInt(-inner.i);
            return true;
        }
    }
    return false;
}

// Los negativos se emiten como Unary('-', Literal) para que el resto del
// compilador (semantico, codigo intermedio) vea la misma forma que da el Parser.
ExprPtr makeConstant(const Const& c, Ubicacion loc) {
    const bool negative = c.isDouble ? std::signbit(c.d) : c.i < 0;
    std::string text;
    if (c.isDouble) text = formatDouble(std::fabs(c.d));
    else            text = std::to_string(c.i < 0 ? -c.i : c.i);

    ExprPtr lit = std::make_unique<LiteralExpr>(loc, text, TipoToken::NUMERO);
    if (negative) return std::make_unique<UnaryExpr>(loc, "-", std::move(lit), true);
    return lit;
}

bool foldUnary(const std::string& op, const Const& c, Const& out) {
    if (op == "-") {
        if (c.isDouble) { out = makeDouble(-c.d); return true; }
        if (!fitsInt(-c.i)) return false;
        out = makeInt(-c.i);
        return true;
    }
    if (op == "+") { out = c; return true; }
    if (op == "!") { out = makeInt(c.truthy() ? 0 : 1); return true; }
    return false;
}

bool foldBinary(const std::string& op, const Const& l, const Const& r, Const& out) {
    const bool anyDouble = l.isDouble || r.isDouble;

    // Comparaciones y logicos -> int 0/1
    if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
        bool res;
        if (anyDouble) {
            const double a = l.asDouble(), b = r.asDouble();
            if      (op == "==") res = a == b;
            else if (op == "!=") res = a != b;
            else if (op == "<")  res = a <  b;
            else if (op == "<=") res = a <= b;
            else if (op == ">")  res = a >  b;
            else                 res = a >= b;
        } else {
            const long long a = l.i, b = r.i;
            if      (op == "==") res = a == b;
            else if (op == "!=") res = a != b;
            else if (op == "<")  res = a <  b;
            else if (op == "<=") res = a <= b;
            else if (op == ">")  res = a >  b;
            else                 res = a >= b;
        }
        out = makeInt(res ? 1 : 0);
        return true;
    }
    if (op == "&&") { out = makeInt(l.truthy() && r.truthy() ? 1 : 0); return true; }
    if (op == "||") { out = makeInt(l.truthy() || r.truthy() ? 1 : 0); return true; }

    // Aritmetica
    if (!anyDouble) {
        const long long a = l.i, b = r.i; // ambos caben en int: a*b no desborda long long
        long long v;
        if      (op == "+") v = a + b;
        else if (op == "-") v = a - b;
        else if (op == "*") v = a * b;
        else if (op == "/") { if (b == 0) return false; v = a / b; }
        else if (op == "%") { if (b == 0) return false; v = a % b; }
        else return false;
        if (!fitsInt(v)) return false; // evita decidir aqui el desbordamiento de int
        out = makeInt(v);
        return true;
    }

    const double a = l.asDouble(), b = r.asDouble();
    double v;
    if      (op == "+") v = a + b;
    else if (op == "-") v = a - b;
    else if (op == "*") v = a * b;
    else if (op == "/") { if (b == 0.0) return false; v = a / b; }
    else return false;
    if (!std::isfinite(v)) return false;
    out = makeDouble(v);
    return true;
}

bool isIntConst(const Const& c, long long v) { return !c.isDouble && c.i == v; }

// ---------------------------------------------------------------------------
// Efectos secundarios
// ---------------------------------------------------------------------------

// Solo estos operadores son "puros". Cualquier otro (<<, >>, +=, ++, &, ...)
// se trata como si tuviera efectos (E/S, modifica variables). Lista blanca a
// proposito: si el Parser agrega operadores nuevos, el optimizador NO los
// borra ni los toma por constantes por accidente.
bool isPureBinaryOp(const std::string& op) {
    return op == "+" || op == "-" || op == "*" || op == "/" || op == "%" ||
           op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" ||
           op == ">=" || op == "&&" || op == "||";
}

bool isPureUnaryOp(const std::string& op) {
    return op == "-" || op == "+" || op == "!" || op == "~";
}

// true si evaluar la expresion no cambia nada observable (se puede eliminar).
bool isPure(const Expr* e) {
    if (!e) return true;
    switch (e->kind()) {
    case NodeKind::LiteralExpr:
    case NodeKind::IdentifierExpr:
        return true;
    case NodeKind::MemberExpr:
        return isPure(static_cast<const MemberExpr*>(e)->object.get());
    case NodeKind::UnaryExpr: {
        const auto* u = static_cast<const UnaryExpr*>(e);
        if (!isPureUnaryOp(u->op)) return false;
        return isPure(u->operand.get());
    }
    case NodeKind::BinaryExpr: {
        const auto* b = static_cast<const BinaryExpr*>(e);
        if (!isPureBinaryOp(b->op)) return false;
        if (b->op == "/" || b->op == "%") { // una division podria fallar en ejecucion
            Const c;
            if (!getConstant(b->right.get(), c) || (!c.isDouble && c.i == 0) ||
                (c.isDouble && c.d == 0.0)) return false;
        }
        return isPure(b->left.get()) && isPure(b->right.get());
    }
    default: // CallExpr y cualquier otra cosa: se asume con efectos
        return false;
    }
}

bool isEmptyBlock(const Stmt* s) {
    return s && s->kind() == NodeKind::BlockStmt &&
           static_cast<const BlockStmt*>(s)->statements.empty();
}

// ---------------------------------------------------------------------------
// Una pasada completa (analisis + reescritura)
// ---------------------------------------------------------------------------

class Pass {
public:
    explicit Pass(OptimizationStats& stats) : stats_(stats) {}

    void run(ProgramNode& program) {
        scanProgram(program);
        buildConstants();

        std::vector<StmtPtr> out;
        for (auto& d : program.declarations) {
            if (!d) continue;
            switch (d->kind()) {
            case NodeKind::FunctionDecl: {
                auto& f = static_cast<FunctionDecl&>(*d);
                if (f.body) rewriteBlock(*f.body);
                break;
            }
            case NodeKind::VarDecl:
                rewriteExpr(static_cast<VarDecl&>(*d).initializer);
                break;
            case NodeKind::StructDecl:
                for (auto& field : static_cast<StructDecl&>(*d).fields) rewriteExpr(field->initializer);
                break;
            default:
                rewriteStmt(d);
                break;
            }
            if (d) out.push_back(std::move(d));
        }
        program.declarations = std::move(out);
    }

private:
    struct VarInfo {
        int          decls    = 0;
        int          mentions = 0;
        bool         modified = false;
        const VarDecl* decl   = nullptr;
    };

    OptimizationStats&                    stats_;
    std::unordered_map<std::string, VarInfo> vars_;
    std::unordered_map<std::string, Const>   constVars_;

    // ----- Analisis (solo lectura) -----

    void markModified(const Expr* target) {
        while (target && target->kind() == NodeKind::MemberExpr)
            target = static_cast<const MemberExpr*>(target)->object.get();
        if (target && target->kind() == NodeKind::IdentifierExpr)
            vars_[static_cast<const IdentifierExpr*>(target)->name].modified = true;
    }

    void scanExpr(const Expr* e) {
        if (!e) return;
        switch (e->kind()) {
        case NodeKind::IdentifierExpr:
            ++vars_[static_cast<const IdentifierExpr*>(e)->name].mentions;
            break;
        case NodeKind::UnaryExpr: {
            const auto* u = static_cast<const UnaryExpr*>(e);
            if (!isPureUnaryOp(u->op)) markModified(u->operand.get()); // ++, --, &, ...
            scanExpr(u->operand.get());
            break;
        }
        case NodeKind::BinaryExpr: {
            const auto* b = static_cast<const BinaryExpr*>(e);
            if (b->op == "=" || b->op == "<<") {
                markModified(b->left.get());          // x = ...   /   cout << ...
            } else if (!isPureBinaryOp(b->op)) {
                markModified(b->left.get());          // cin >> x, x += 1, ...
                markModified(b->right.get());
            }
            scanExpr(b->left.get());
            scanExpr(b->right.get());
            break;
        }
        case NodeKind::CallExpr: {
            const auto* c = static_cast<const CallExpr*>(e);
            scanExpr(c->callee.get());
            for (const auto& a : c->arguments) scanExpr(a.get());
            break;
        }
        case NodeKind::MemberExpr:
            scanExpr(static_cast<const MemberExpr*>(e)->object.get());
            break;
        default:
            break;
        }
    }

    void scanStmt(const Stmt* s) {
        if (!s) return;
        switch (s->kind()) {
        case NodeKind::BlockStmt:
            for (const auto& st : static_cast<const BlockStmt*>(s)->statements) scanStmt(st.get());
            break;
        case NodeKind::ExprStmt:
            scanExpr(static_cast<const ExprStmt*>(s)->expression.get());
            break;
        case NodeKind::VarDecl: {
            const auto* v = static_cast<const VarDecl*>(s);
            auto& info = vars_[v->name];
            ++info.decls;
            info.decl = v;
            scanExpr(v->initializer.get());
            break;
        }
        case NodeKind::IfStmt: {
            const auto* i = static_cast<const IfStmt*>(s);
            scanExpr(i->condition.get());
            scanStmt(i->thenBranch.get());
            scanStmt(i->elseBranch.get());
            break;
        }
        case NodeKind::WhileStmt: {
            const auto* w = static_cast<const WhileStmt*>(s);
            scanExpr(w->condition.get());
            scanStmt(w->body.get());
            break;
        }
        case NodeKind::ForStmt: {
            const auto* f = static_cast<const ForStmt*>(s);
            scanStmt(f->init.get());
            scanExpr(f->condition.get());
            scanExpr(f->increment.get());
            scanStmt(f->body.get());
            break;
        }
        case NodeKind::ReturnStmt:
            scanExpr(static_cast<const ReturnStmt*>(s)->value.get());
            break;
        case NodeKind::FunctionDecl: {
            const auto* f = static_cast<const FunctionDecl*>(s);
            ++vars_[f->name].decls;           // un nombre de funcion tampoco es una "variable constante"
            for (const auto& p : f->parameters) ++vars_[p.name].decls;
            scanStmt(f->body.get());
            break;
        }
        case NodeKind::StructDecl: {
            const auto* sd = static_cast<const StructDecl*>(s);
            ++vars_[sd->name].decls;
            for (const auto& field : sd->fields) scanExpr(field->initializer.get());
            break;
        }
        default:
            break;
        }
    }

    void scanProgram(const ProgramNode& p) {
        for (const auto& d : p.declarations) scanStmt(d.get());
    }

    // Una variable es "constante" si se declara una sola vez en TODO el
    // programa, nunca se modifica y su inicializador es un numero.
    void buildConstants() {
        for (const auto& [name, info] : vars_) {
            if (info.decls != 1 || info.modified || !info.decl || !info.decl->initializer) continue;
            Const c;
            if (!getConstant(info.decl->initializer.get(), c)) continue;

            const std::string& t = info.decl->type;
            if (t == "int") {
                if (c.isDouble) continue;                 // int x = 3.7; (truncaria)
            } else if (t == "float" || t == "double") {
                if (!c.isDouble) c = makeDouble(static_cast<double>(c.i)); // float f = 3; -> 3.0
            } else {
                continue;                                  // char, bool, struct...
            }
            constVars_[name] = c;
        }
    }

    // ----- Reescritura de expresiones -----

    void rewriteExpr(ExprPtr& e) {
        if (!e) return;

        switch (e->kind()) {
        case NodeKind::IdentifierExpr: {
            const auto& id = static_cast<const IdentifierExpr&>(*e);
            auto it = constVars_.find(id.name);
            if (it != constVars_.end()) {
                const Ubicacion loc = id.ubicacion;
                e = makeConstant(it->second, loc);
                ++stats_.constantsPropagated;
            }
            return;
        }

        case NodeKind::UnaryExpr: {
            auto& u = static_cast<UnaryExpr&>(*e);
            if (!isPureUnaryOp(u.op)) return; // ++, --, &...: el operando es un lvalue
            rewriteExpr(u.operand);
            if (!u.isPrefix || !u.operand) return;

            // "-literal" ya es la forma canonica de un negativo: no se vuelve a plegar
            const bool canonicalNegative = u.op == "-" && u.operand->kind() == NodeKind::LiteralExpr;
            Const c, r;
            if (!canonicalNegative && getConstant(u.operand.get(), c) && foldUnary(u.op, c, r)) {
                const Ubicacion loc = u.ubicacion;
                e = makeConstant(r, loc);
                ++stats_.constantsFolded;
                return;
            }
            if (u.op == "+") {                      // +x -> x
                ExprPtr inner = std::move(u.operand);
                e = std::move(inner);
                ++stats_.algebraicSimplifications;
                return;
            }
            if (u.op == "-" && u.operand->kind() == NodeKind::UnaryExpr) { // -(-x) -> x
                auto& in = static_cast<UnaryExpr&>(*u.operand);
                if (in.isPrefix && in.op == "-") {
                    ExprPtr inner = std::move(in.operand);
                    e = std::move(inner);
                    ++stats_.algebraicSimplifications;
                }
            }
            return;
        }

        case NodeKind::BinaryExpr: {
            auto& b = static_cast<BinaryExpr&>(*e);
            if (b.op == "=") {                      // el lado izquierdo es el destino
                rewriteExpr(b.right);
                return;
            }
            rewriteExpr(b.left);
            rewriteExpr(b.right);

            Const l, r, res;
            const bool lc = getConstant(b.left.get(), l);
            const bool rc = getConstant(b.right.get(), r);
            const Ubicacion loc = b.ubicacion;

            if (lc && rc && foldBinary(b.op, l, r, res)) {
                e = makeConstant(res, loc);
                ++stats_.constantsFolded;
                return;
            }

            // Simplificaciones algebraicas (solo con identidades enteras, para
            // no cambiar el tipo del resultado).
            if (b.op == "+") {
                if (rc && isIntConst(r, 0)) return replaceWith(e, b.left);
                if (lc && isIntConst(l, 0)) return replaceWith(e, b.right);
            } else if (b.op == "-") {
                if (rc && isIntConst(r, 0)) return replaceWith(e, b.left);
            } else if (b.op == "*") {
                if (rc && isIntConst(r, 1)) return replaceWith(e, b.left);
                if (lc && isIntConst(l, 1)) return replaceWith(e, b.right);
                if (rc && isIntConst(r, 0) && isPure(b.left.get())) {
                    e = makeConstant(makeInt(0), loc);
                    ++stats_.algebraicSimplifications;
                    return;
                }
                if (lc && isIntConst(l, 0) && isPure(b.right.get())) {
                    e = makeConstant(makeInt(0), loc);
                    ++stats_.algebraicSimplifications;
                    return;
                }
            } else if (b.op == "/") {
                if (rc && isIntConst(r, 1)) return replaceWith(e, b.left);
            }
            return;
        }

        case NodeKind::CallExpr: {
            auto& c = static_cast<CallExpr&>(*e);
            if (c.callee && c.callee->kind() != NodeKind::IdentifierExpr) rewriteExpr(c.callee);
            for (auto& a : c.arguments) rewriteExpr(a);
            return;
        }

        case NodeKind::MemberExpr:
            rewriteExpr(static_cast<MemberExpr&>(*e).object);
            return;

        default:
            return;
        }
    }

    // e = <hijo>; (el nodo actual se destruye, el hijo sobrevive)
    void replaceWith(ExprPtr& e, ExprPtr& child) {
        ExprPtr keep = std::move(child);
        e = std::move(keep);
        ++stats_.algebraicSimplifications;
    }

    // ----- Reescritura de sentencias -----

    // Cuerpos de if/while/for: nunca quedan en null (otras fases asumen que existen).
    void rewriteChild(StmtPtr& s) {
        if (!s) return;
        if (s->kind() == NodeKind::BlockStmt) {
            rewriteBlock(static_cast<BlockStmt&>(*s));
            return;
        }
        const Ubicacion loc = s->ubicacion;
        rewriteStmt(s);
        if (!s) s = std::make_unique<BlockStmt>(loc);
    }

    void rewriteBlock(BlockStmt& b) {
        std::vector<StmtPtr> out;
        bool terminated = false;

        for (auto& st : b.statements) {
            if (!st) continue;
            if (terminated) {                       // codigo despues de un return
                ++stats_.unreachableStatementsRemoved;
                continue;
            }

            if (st->kind() == NodeKind::VarDecl) {
                auto& vd = static_cast<VarDecl&>(*st);
                rewriteExpr(vd.initializer);
                auto it = vars_.find(vd.name);
                if (it != vars_.end() && it->second.mentions == 0) { // variable que nadie lee
                    ++stats_.deadVariablesRemoved;
                    if (vd.initializer && !isPure(vd.initializer.get())) {
                        // se conserva el efecto: int x = f();  ->  f();
                        const Ubicacion loc = vd.ubicacion;
                        out.push_back(std::make_unique<ExprStmt>(loc, std::move(vd.initializer)));
                    }
                    continue;
                }
                out.push_back(std::move(st));
                continue;
            }

            rewriteStmt(st);
            if (!st) continue;
            const bool isReturn = st->kind() == NodeKind::ReturnStmt;
            out.push_back(std::move(st));
            if (isReturn) terminated = true;
        }
        b.statements = std::move(out);
    }

    // Puede dejar s en null (sentencia eliminada) o reemplazarla por otra.
    void rewriteStmt(StmtPtr& s) {
        if (!s) return;

        switch (s->kind()) {
        case NodeKind::ExprStmt: {
            auto& es = static_cast<ExprStmt&>(*s);
            rewriteExpr(es.expression);
            if (isPure(es.expression.get())) {
                s.reset();
                ++stats_.pureStatementsRemoved;
            }
            return;
        }

        case NodeKind::VarDecl:
            rewriteExpr(static_cast<VarDecl&>(*s).initializer);
            return;

        case NodeKind::BlockStmt: {
            auto& b = static_cast<BlockStmt&>(*s);
            rewriteBlock(b);
            if (b.statements.empty()) {
                s.reset();
                ++stats_.emptyBlocksRemoved;
            }
            return;
        }

        case NodeKind::ReturnStmt:
            rewriteExpr(static_cast<ReturnStmt&>(*s).value);
            return;

        case NodeKind::IfStmt: {
            auto& i = static_cast<IfStmt&>(*s);
            rewriteExpr(i.condition);
            rewriteChild(i.thenBranch);
            rewriteChild(i.elseBranch);
            if (i.elseBranch && isEmptyBlock(i.elseBranch.get())) {
                i.elseBranch.reset();
                ++stats_.emptyBlocksRemoved;
            }

            Const c;
            if (getConstant(i.condition.get(), c)) {
                // if (constante): se queda solo la rama que se ejecuta (como bloque,
                // para conservar el alcance de sus variables)
                StmtPtr chosen = c.truthy() ? std::move(i.thenBranch) : std::move(i.elseBranch);
                s = std::move(chosen);              // puede ser null -> sentencia eliminada
                ++stats_.deadBranchesRemoved;
                return;
            }
            if (isEmptyBlock(i.thenBranch.get()) && !i.elseBranch) {
                const Ubicacion loc = i.ubicacion;
                if (isPure(i.condition.get())) {
                    s.reset();
                } else {
                    s = std::make_unique<ExprStmt>(loc, std::move(i.condition));
                }
                ++stats_.deadBranchesRemoved;
            }
            return;
        }

        case NodeKind::WhileStmt: {
            auto& w = static_cast<WhileStmt&>(*s);
            rewriteExpr(w.condition);
            rewriteChild(w.body);
            Const c;
            if (getConstant(w.condition.get(), c) && !c.truthy()) { // while (0)
                s.reset();
                ++stats_.deadBranchesRemoved;
            }
            return;
        }

        case NodeKind::ForStmt: {
            auto& f = static_cast<ForStmt&>(*s);
            if (f.init) {
                if (f.init->kind() == NodeKind::VarDecl)
                    rewriteExpr(static_cast<VarDecl&>(*f.init).initializer);
                else if (f.init->kind() == NodeKind::ExprStmt)
                    rewriteExpr(static_cast<ExprStmt&>(*f.init).expression);
            }
            rewriteExpr(f.condition);
            rewriteExpr(f.increment);
            rewriteChild(f.body);

            Const c;
            if (f.condition && getConstant(f.condition.get(), c) && !c.truthy()) {
                // for (init; 0; ...) -> solo se ejecuta init
                const Ubicacion loc = f.ubicacion;
                ++stats_.deadBranchesRemoved;
                if (f.init) {
                    std::vector<StmtPtr> only;
                    only.push_back(std::move(f.init));
                    s = std::make_unique<BlockStmt>(loc, std::move(only));
                } else {
                    s.reset();
                }
            }
            return;
        }

        default:
            return;
        }
    }
};

} // namespace

// ---------------------------------------------------------------------------

void OptimizationStats::dump(std::ostream& out) const {
    out << "--- Reporte del optimizador ---\n"
        << "Iteraciones:                     " << iterations << "\n"
        << "Constantes plegadas:             " << constantsFolded << "\n"
        << "Simplificaciones algebraicas:    " << algebraicSimplifications << "\n"
        << "Constantes propagadas:           " << constantsPropagated << "\n"
        << "Ramas muertas eliminadas:        " << deadBranchesRemoved << "\n"
        << "Codigo inalcanzable eliminado:   " << unreachableStatementsRemoved << "\n"
        << "Variables sin uso eliminadas:    " << deadVariablesRemoved << "\n"
        << "Expresiones sin efecto borradas: " << pureStatementsRemoved << "\n"
        << "Bloques vacios eliminados:       " << emptyBlocksRemoved << "\n";
}

OptimizationStats Optimizer::optimize(ProgramNode& program, std::size_t maxIterations) {
    OptimizationStats stats;
    for (std::size_t i = 0; i < maxIterations; ++i) {
        const std::size_t before = stats.total();
        Pass pass(stats);
        pass.run(program);
        ++stats.iterations;
        if (stats.total() == before) break; // punto fijo: ya no hay nada que mejorar
    }
    return stats;
}
