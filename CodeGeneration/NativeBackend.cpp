#include "NativeBackend.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

// ============================================================================
// Contrato TAC -> backend
// ----------------------------------------------------------------------------
// El TAC clasico no lleva tipos, asi que CodeGenerator anota la informacion
// que el backend necesita usando campos que antes iban vacios:
//   (ASSIGN, "",  "@<tipo>", nombre)   declaracion de variable (tipo = int, float,
//                                      double, char, bool, string o nombre de struct)
//   (ASSIGN, "x:int;y:int", "@struct", Nombre)   definicion de struct
//   (LABEL,  "@<tipoRet>", "", f)      inicio de la funcion f
//   (POP_PARAM, "", "@<tipo>", p)      parametro formal de la funcion actual
//   (LABEL,  "@end", "", f)            fin de la funcion f
// Los literales viajan con su forma: "texto" (cadena), 'c' (caracter) o numero.
// Las cuadruplas fuera de toda funcion son la inicializacion de globales.
// ============================================================================

namespace {

enum class Ty { Int, Char, Bool, Float, Double, Str, Void, Struct };

bool isIntLike(Ty t) { return t == Ty::Int || t == Ty::Char || t == Ty::Bool; }
bool isFp(Ty t) { return t == Ty::Float || t == Ty::Double; }

Ty promote(Ty a, Ty b) {
    if (a == Ty::Double || b == Ty::Double) return Ty::Double;
    if (a == Ty::Float || b == Ty::Float) return Ty::Float;
    return Ty::Int;
}

[[noreturn]] void fail(const std::string& msg) {
    throw std::runtime_error("[Backend x86-64] " + msg);
}

// Nombre seguro para simbolos del ensamblador.
std::string sym(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '_') {
            r += static_cast<char>(c);
        } else {
            char b[8];
            std::snprintf(b, sizeof b, "_%02X", c);
            r += b;
        }
    }
    return r;
}

std::string decodeEscapes(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c != '\\' || i + 1 >= in.size()) {
            out += c;
            continue;
        }
        char n = in[++i];
        switch (n) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case '0': out += '\0'; break;
            case 'a': out += '\a'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'v': out += '\v'; break;
            case 'x': {
                int v = 0, k = 0;
                while (k < 2 && i + 1 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1]))) {
                    char h = in[++i];
                    v = v * 16 + (std::isdigit(static_cast<unsigned char>(h)) ? h - '0' : (std::tolower(h) - 'a' + 10));
                    ++k;
                }
                out += static_cast<char>(v);
                break;
            }
            default: out += n; break;
        }
    }
    return out;
}

struct Opd {
    enum Kind { None, Str, Chr, Num, Name } kind = None;
    std::string text;  // contenido decodificado (Str/Chr), numero o nombre
};

Opd parseOpd(const std::string& t) {
    Opd o;
    if (t.empty()) return o;
    if (t.front() == '"') {
        o.kind = Opd::Str;
        o.text = decodeEscapes(t.size() >= 2 ? t.substr(1, t.size() - 2) : "");
        return o;
    }
    if (t.front() == '\'') {
        o.kind = Opd::Chr;
        o.text = decodeEscapes(t.size() >= 2 ? t.substr(1, t.size() - 2) : "");
        return o;
    }
    unsigned char c0 = static_cast<unsigned char>(t[0]);
    bool num = std::isdigit(c0) ||
               ((t[0] == '-' || t[0] == '+' || t[0] == '.') && t.size() > 1 &&
                (std::isdigit(static_cast<unsigned char>(t[1])) || t[1] == '.'));
    o.kind = num ? Opd::Num : Opd::Name;
    o.text = t;
    return o;
}

Ty numType(const std::string& t) {
    size_t s = (t[0] == '-' || t[0] == '+') ? 1 : 0;
    if (t.size() > s + 1 && t[s] == '0' && (t[s + 1] == 'x' || t[s + 1] == 'X')) return Ty::Int;
    char last = t.back();
    if (last == 'f' || last == 'F') return Ty::Float;
    if (t.find_first_of(".eE") != std::string::npos) return Ty::Double;
    return Ty::Int;
}

long long numInt(std::string t) {
    while (!t.empty() && std::strchr("uUlL", t.back())) t.pop_back();
    return std::strtoll(t.c_str(), nullptr, 0);
}

double numFp(std::string t) {
    while (!t.empty() && (t.back() == 'f' || t.back() == 'F' || t.back() == 'l' || t.back() == 'L')) t.pop_back();
    return std::strtod(t.c_str(), nullptr);
}

struct Field { std::string name, type; };
struct StructDef { std::vector<Field> fields; };

struct Var {
    Ty ty = Ty::Int;
    bool global = false;
    long off = 0;            // [rbp - off] para locales
    std::string symbol;      // simbolo para globales
    std::string structName;  // si ty == Struct
};

struct Func {
    std::string name;
    std::string retText;
    Ty ret = Ty::Void;
    std::vector<std::string> pnames, ptext;
    std::vector<Ty> ptypes;
    std::vector<Quadruple> body;
    bool isInit = false;

    std::unordered_map<std::string, Var> vars;
    long nslots = 0;
    int maxArgs = 0;
    std::vector<std::pair<long, Ty>> zero;  // variables de usuario a inicializar
    std::string label() const { return isInit ? "cg_init_globals" : "u_" + sym(name); }
};

struct Loc {
    bool reg = false;
    bool fp = false;
    int idx = 0;  // indice de registro o ranura de pila
};

class Gen {
public:
    explicit Gen(bool win) : win_(win) {}

private:
    bool win_;
    std::map<std::string, StructDef> structs_;
    std::deque<Func> funcs_;
    Func init_;
    std::map<std::string, Func*> funcIndex_;
    std::unordered_map<std::string, Var> globals_;
    std::vector<std::string> globalOrder_;

    Func* cur_ = nullptr;
    std::vector<std::string> pending_;

    std::ostringstream t_;  // .text
    std::vector<std::string> strPool_;
    std::map<std::string, int> strIdx_;
    std::vector<std::string> fpPool_;  // lineas ".long"/".quad"
    std::map<std::string, int> fpIdx_;
    int sbufCount_ = 0;
    int lblCount_ = 0;

    // ---- utilidades ----
    void I(const std::string& s) { t_ << "\t" << s << "\n"; }
    void L(const std::string& s) { t_ << s << ":\n"; }
    std::string newLabel() { return ".Lcg" + std::to_string(lblCount_++); }
    std::string tacLabel(const std::string& n) { return ".Ltac_" + sym(n); }
    void callC(const std::string& f) { I(win_ ? "call " + f : "call " + f + "@PLT"); }

    Ty tyFromText(const std::string& t) const {
        if (t == "int") return Ty::Int;
        if (t == "char") return Ty::Char;
        if (t == "bool") return Ty::Bool;
        if (t == "float") return Ty::Float;
        if (t == "double") return Ty::Double;
        if (t == "string") return Ty::Str;
        if (t == "void") return Ty::Void;
        if (structs_.count(t)) return Ty::Struct;
        fail("tipo desconocido: " + t);
    }

    // ---- pools ----
    std::string strLabel(const std::string& s) {
        auto it = strIdx_.find(s);
        if (it == strIdx_.end()) {
            it = strIdx_.emplace(s, static_cast<int>(strPool_.size())).first;
            strPool_.push_back(s);
        }
        return ".LC" + std::to_string(it->second);
    }
    std::string fpLabel(Ty t, double v) {
        char b[64];
        std::string key;
        if (t == Ty::Float) {
            float f = static_cast<float>(v);
            uint32_t u;
            std::memcpy(&u, &f, 4);
            std::snprintf(b, sizeof b, "\t.long 0x%08x", u);
            key = std::string("F") + b;
        } else {
            uint64_t u;
            std::memcpy(&u, &v, 8);
            std::snprintf(b, sizeof b, "\t.quad 0x%016llx", static_cast<unsigned long long>(u));
            key = std::string("D") + b;
        }
        auto it = fpIdx_.find(key);
        if (it == fpIdx_.end()) {
            it = fpIdx_.emplace(key, static_cast<int>(fpPool_.size())).first;
            fpPool_.push_back(key);
        }
        return ".LFP" + std::to_string(it->second);
    }

    // ---- variables ----
    Var* findVar(const std::string& n) {
        auto it = cur_->vars.find(n);
        if (it != cur_->vars.end()) return &it->second;
        auto g = globals_.find(n);
        return g != globals_.end() ? &g->second : nullptr;
    }
    Var& needVar(const std::string& n) {
        Var* v = findVar(n);
        if (!v) fail("variable no declarada en el TAC: " + n);
        return *v;
    }
    std::string mem(const Var& v) const {
        if (v.global) return "[rip + " + v.symbol + "]";
        return "[rbp - " + std::to_string(v.off) + "]";
    }
    static const char* ptr(Ty t) {
        switch (t) {
            case Ty::Char: return "BYTE PTR ";
            case Ty::Int: case Ty::Bool: case Ty::Float: return "DWORD PTR ";
            default: return "QWORD PTR ";
        }
    }

    void declareVar(const std::string& name, const std::string& typeText, bool global, bool isParam, int depth = 0) {
        if (depth > 32) fail("struct anidado demasiado profundo: " + typeText);
        Ty t = tyFromText(typeText);
        if (t == Ty::Void) fail("variable void: " + name);
        auto& table = global ? globals_ : cur_->vars;
        auto prev = table.find(name);
        if (prev != table.end()) {
            if (prev->second.ty != t) fail("redeclaracion con otro tipo: " + name);
            return;
        }
        Var v;
        v.ty = t;
        if (t == Ty::Struct) {
            v.structName = typeText;
            table[name] = v;
            for (const auto& f : structs_.at(typeText).fields)
                declareVar(name + "." + f.name, f.type, global, isParam, depth + 1);
            return;
        }
        v.global = global;
        if (global) {
            v.symbol = "g_" + sym(name);
            globalOrder_.push_back(name);
        } else {
            v.off = 8 * (++cur_->nslots);
            if (!isParam) cur_->zero.push_back({v.off, t});
        }
        table[name] = v;
    }

    void defineTemp(const std::string& name, Ty t) {
        if (findVar(name)) return;
        Var v;
        v.ty = t;
        v.off = 8 * (++cur_->nslots);
        cur_->vars[name] = v;
    }

    Ty typeOf(const Opd& o) {
        switch (o.kind) {
            case Opd::None: fail("operando vacio");
            case Opd::Str: return Ty::Str;
            case Opd::Chr: return Ty::Char;
            case Opd::Num: return numType(o.text);
            case Opd::Name: return needVar(o.text).ty;
        }
        return Ty::Int;
    }
    Ty typeOf(const std::string& tok) { return typeOf(parseOpd(tok)); }

    Ty arithType(const Quadruple& q) {
        Ty a = typeOf(q.arg1), b = typeOf(q.arg2);
        if (a == Ty::Str || b == Ty::Str || a == Ty::Struct || b == Ty::Struct)
            fail("operacion aritmetica/comparacion sobre cadenas o structs");
        return promote(a, b);
    }

    // ---- fase 1: partir el TAC en funciones ----
    void split(const std::vector<Quadruple>& tac) {
        init_.name = "cg_init_globals";
        init_.isInit = true;
        Func* cur = nullptr;
        for (const auto& q : tac) {
            if (q.op == OpCode::LABEL && !q.arg1.empty() && q.arg1[0] == '@') {
                if (q.arg1 == "@end") { cur = nullptr; continue; }
                if (cur) fail("funcion anidada: " + q.result);
                if (funcIndex_.count(q.result)) fail("funcion duplicada: " + q.result);
                funcs_.emplace_back();
                cur = &funcs_.back();
                cur->name = q.result;
                cur->retText = q.arg1.substr(1);
                funcIndex_[q.result] = cur;
                continue;
            }
            if (cur && q.op == OpCode::POP_PARAM) {
                cur->pnames.push_back(q.result);
                cur->ptext.push_back(q.arg2.size() > 1 ? q.arg2.substr(1) : "int");
                continue;
            }
            if (!cur && q.op == OpCode::ASSIGN && q.arg2 == "@struct") {
                StructDef d;
                std::stringstream ss(q.arg1);
                std::string item;
                while (std::getline(ss, item, ';')) {
                    auto c = item.find(':');
                    if (c == std::string::npos) continue;
                    d.fields.push_back({item.substr(0, c), item.substr(c + 1)});
                }
                structs_[q.result] = d;
                continue;
            }
            (cur ? cur->body : init_.body).push_back(q);
        }
        if (cur) fail("funcion sin cierre: " + cur->name);
    }

    Func* findFunc(const std::string& n) {
        auto it = funcIndex_.find(n);
        return it == funcIndex_.end() ? nullptr : it->second;
    }

    // ---- fase 2: tipos y marco de pila ----
    void layout(Func& f) {
        cur_ = &f;
        for (size_t i = 0; i < f.pnames.size(); ++i) declareVar(f.pnames[i], f.ptext[i], false, true);
        for (const auto& q : f.body) {
            switch (q.op) {
                case OpCode::ASSIGN:
                    if (!q.arg2.empty() && q.arg2[0] == '@') {
                        declareVar(q.result, q.arg2.substr(1), f.isInit, false);
                    } else if (!findVar(q.result)) {
                        defineTemp(q.result, typeOf(q.arg1));
                    }
                    break;
                case OpCode::ADD: case OpCode::SUB: case OpCode::MUL:
                case OpCode::DIV: case OpCode::MOD:
                    defineTemp(q.result, arithType(q));
                    break;
                case OpCode::EQ: case OpCode::NEQ: case OpCode::LT: case OpCode::GT:
                case OpCode::LE: case OpCode::GE: case OpCode::AND: case OpCode::OR:
                    defineTemp(q.result, Ty::Bool);
                    break;
                case OpCode::NOT:
                    defineTemp(q.result, Ty::Bool);
                    break;
                case OpCode::NEG: {
                    Ty a = typeOf(q.arg1);
                    if (a == Ty::Str || a == Ty::Struct) fail("negacion de cadena/struct");
                    defineTemp(q.result, promote(a, a));
                    break;
                }
                case OpCode::CALL: {
                    Func* c = findFunc(q.arg1);
                    if (!c) fail("llamada a funcion no definida: " + q.arg1);
                    if (c->ret != Ty::Void) defineTemp(q.result, c->ret);
                    f.maxArgs = std::max(f.maxArgs, std::atoi(q.arg2.c_str()));
                    break;
                }
                default: break;
            }
        }
    }

    long frameSize(const Func& f) const {
        long out = 8L * std::max(4, f.maxArgs);
        long total = 8L * f.nslots + out;
        return (total + 15) / 16 * 16;
    }

    // ---- convenciones de llamada ----
    std::vector<Loc> assignLocs(const std::vector<Ty>& pts) const {
        std::vector<Loc> r(pts.size());
        int ic = 0, fc = 0, ss = 0;
        for (size_t i = 0; i < pts.size(); ++i) {
            bool fp = isFp(pts[i]);
            r[i].fp = fp;
            if (win_) {
                if (i < 4) { r[i].reg = true; r[i].idx = static_cast<int>(i); }
                else { r[i].reg = false; r[i].idx = static_cast<int>(i); }
            } else {
                if (fp) {
                    if (fc < 8) { r[i].reg = true; r[i].idx = fc++; } else { r[i].idx = ss++; }
                } else {
                    if (ic < 6) { r[i].reg = true; r[i].idx = ic++; } else { r[i].idx = ss++; }
                }
            }
        }
        return r;
    }
    const char* intReg32(int i) const {
        static const char* w[] = {"ecx", "edx", "r8d", "r9d"};
        static const char* s[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
        return win_ ? w[i] : s[i];
    }
    const char* intReg64(int i) const {
        static const char* w[] = {"rcx", "rdx", "r8", "r9"};
        static const char* s[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        return win_ ? w[i] : s[i];
    }

    // ---- carga / conversion / almacenamiento ----
    void convert(Ty st, Ty want, int bank) {
        if (st == want) return;
        if (st == Ty::Str || want == Ty::Str || st == Ty::Struct || want == Ty::Struct || want == Ty::Void)
            fail("conversion de tipos no valida");
        const std::string r32 = bank ? "ecx" : "eax", r8 = bank ? "cl" : "al", x = bank ? "xmm1" : "xmm0";
        if (isIntLike(st)) {
            switch (want) {
                case Ty::Int: break;
                case Ty::Char: I("movsx " + r32 + ", " + r8); break;
                case Ty::Bool:
                    I("test " + r32 + ", " + r32);
                    I("setne " + r8);
                    I("movzx " + r32 + ", " + r8);
                    break;
                case Ty::Float: I("cvtsi2ss " + x + ", " + r32); break;
                case Ty::Double: I("cvtsi2sd " + x + ", " + r32); break;
                default: break;
            }
            return;
        }
        const bool fl = (st == Ty::Float);
        const std::string cmp = fl ? "ucomiss" : "ucomisd";
        switch (want) {
            case Ty::Float: if (!fl) I("cvtsd2ss " + x + ", " + x); break;
            case Ty::Double: if (fl) I("cvtss2sd " + x + ", " + x); break;
            case Ty::Int: I((fl ? "cvttss2si " : "cvttsd2si ") + r32 + ", " + x); break;
            case Ty::Char:
                I((fl ? "cvttss2si " : "cvttsd2si ") + r32 + ", " + x);
                I("movsx " + r32 + ", " + r8);
                break;
            case Ty::Bool:
                I("xorps xmm5, xmm5");
                I(cmp + " " + x + ", xmm5");
                I("setne " + r8);
                I("setp r11b");
                I("or " + r8 + ", r11b");
                I("movzx " + r32 + ", " + r8);
                break;
            default: break;
        }
    }

    // Carga `token` (literal, variable o temporal) en el banco (0: eax/xmm0/rax, 1: ecx/xmm1/rcx) como `want`.
    void load(const std::string& token, Ty want, int bank) {
        Opd o = parseOpd(token);
        const std::string r32 = bank ? "ecx" : "eax", r64 = bank ? "rcx" : "rax", x = bank ? "xmm1" : "xmm0";
        Ty st = Ty::Int;
        switch (o.kind) {
            case Opd::None: fail("operando vacio");
            case Opd::Str:
                st = Ty::Str;
                I("lea " + r64 + ", [rip + " + strLabel(o.text) + "]");
                break;
            case Opd::Chr:
                st = Ty::Char;
                I("mov " + r32 + ", " + std::to_string(static_cast<int>(static_cast<signed char>(o.text.empty() ? 0 : o.text[0]))));
                break;
            case Opd::Num:
                st = numType(o.text);
                if (st == Ty::Int) {
                    I("mov " + r32 + ", " + std::to_string(static_cast<int32_t>(numInt(o.text))));
                } else if (st == Ty::Float) {
                    I("movss " + x + ", DWORD PTR [rip + " + fpLabel(Ty::Float, numFp(o.text)) + "]");
                } else {
                    I("movsd " + x + ", QWORD PTR [rip + " + fpLabel(Ty::Double, numFp(o.text)) + "]");
                }
                break;
            case Opd::Name: {
                Var& v = needVar(o.text);
                st = v.ty;
                const std::string m = mem(v);
                switch (st) {
                    case Ty::Int: case Ty::Bool: I("mov " + r32 + ", DWORD PTR " + m); break;
                    case Ty::Char: I("movsx " + r32 + ", BYTE PTR " + m); break;
                    case Ty::Float: I("movss " + x + ", DWORD PTR " + m); break;
                    case Ty::Double: I("movsd " + x + ", QWORD PTR " + m); break;
                    case Ty::Str: I("mov " + r64 + ", QWORD PTR " + m); break;
                    default: fail("no se puede usar el struct '" + o.text + "' como valor escalar");
                }
                break;
            }
        }
        convert(st, want, bank);
    }

    void store(const std::string& name) {
        Var& v = needVar(name);
        const std::string m = mem(v);
        switch (v.ty) {
            case Ty::Int: case Ty::Bool: I("mov DWORD PTR " + m + ", eax"); break;
            case Ty::Char: I("mov BYTE PTR " + m + ", al"); break;
            case Ty::Float: I("movss DWORD PTR " + m + ", xmm0"); break;
            case Ty::Double: I("movsd QWORD PTR " + m + ", xmm0"); break;
            case Ty::Str: I("mov QWORD PTR " + m + ", rax"); break;
            default: fail("no se puede almacenar en el struct '" + name + "'");
        }
    }

    void copyStruct(const std::string& dst, const std::string& src) {
        Var& d = needVar(dst);
        Var& s = needVar(src);
        if (d.structName != s.structName) fail("asignacion entre structs distintos: " + dst + " = " + src);
        for (const auto& f : structs_.at(d.structName).fields) {
            const std::string dn = dst + "." + f.name, sn = src + "." + f.name;
            Var& dv = needVar(dn);
            if (dv.ty == Ty::Struct) {
                copyStruct(dn, sn);
            } else {
                load(sn, dv.ty, 0);
                store(dn);
            }
        }
    }

    // ---- E/S ----
    void printfValue(const std::string& fmt, Ty vt) {
        // valor ya cargado en eax / xmm0 / rax
        if (win_) {
            if (vt == Ty::Double) { I("movapd xmm1, xmm0"); I("movq rdx, xmm0"); }
            else if (vt == Ty::Str) I("mov rdx, rax");
            else I("mov edx, eax");
            I("lea rcx, [rip + " + strLabel(fmt) + "]");
        } else {
            if (vt == Ty::Str) I("mov rsi, rax");
            else if (vt == Ty::Int) I("mov esi, eax");
            I("lea rdi, [rip + " + strLabel(fmt) + "]");
            I(vt == Ty::Double ? "mov eax, 1" : "xor eax, eax");
        }
        callC("printf");
    }

    void flushAll() {
        I(win_ ? "xor ecx, ecx" : "xor edi, edi");
        callC("fflush");
    }

    void emitPrint(const Quadruple& q) {
        Ty t = typeOf(q.arg1);
        switch (t) {
            case Ty::Str: load(q.arg1, Ty::Str, 0); printfValue("%s", Ty::Str); break;
            case Ty::Char: load(q.arg1, Ty::Char, 0); printfValue("%c", Ty::Int); break;
            case Ty::Int: case Ty::Bool: load(q.arg1, Ty::Int, 0); printfValue("%d", Ty::Int); break;
            case Ty::Float: case Ty::Double: load(q.arg1, Ty::Double, 0); printfValue("%g", Ty::Double); break;
            default: fail("no se puede imprimir un struct completo: " + q.arg1);
        }
    }

    void emitRead(const Quadruple& q) {
        Var& v = needVar(q.result);
        std::string fmt;
        switch (v.ty) {
            case Ty::Int: case Ty::Bool: fmt = "%d"; break;
            case Ty::Char: fmt = " %c"; break;
            case Ty::Float: fmt = "%f"; break;
            case Ty::Double: fmt = "%lf"; break;
            case Ty::Str: fmt = "%255s"; break;
            default: fail("no se puede leer un struct completo: " + q.result);
        }
        flushAll();
        std::string bufLabel;
        if (v.ty == Ty::Str) bufLabel = ".Lsbuf" + std::to_string(sbufCount_++);
        const std::string target = v.ty == Ty::Str ? "[rip + " + bufLabel + "]" : mem(v);
        if (win_) {
            I("lea rcx, [rip + " + strLabel(fmt) + "]");
            I("lea rdx, " + target);
        } else {
            I("lea rdi, [rip + " + strLabel(fmt) + "]");
            I("lea rsi, " + target);
            I("xor eax, eax");
        }
        callC("scanf");
        if (v.ty == Ty::Bool) {
            I("cmp DWORD PTR " + mem(v) + ", 0");
            I("setne al");
            I("movzx eax, al");
            I("mov DWORD PTR " + mem(v) + ", eax");
        } else if (v.ty == Ty::Str) {
            I("lea rax, " + target);
            I("mov QWORD PTR " + mem(v) + ", rax");
        }
    }

    // ---- llamadas ----
    void emitCall(const Quadruple& q) {
        Func* c = findFunc(q.arg1);
        if (!c) fail("llamada a funcion no definida: " + q.arg1);
        size_t n = static_cast<size_t>(std::atoi(q.arg2.c_str()));
        if (pending_.size() < n) fail("PARAM insuficientes para " + q.arg1);
        if (n != c->ptypes.size()) fail("numero de argumentos incorrecto al llamar a " + q.arg1);
        std::vector<std::string> args(pending_.end() - static_cast<long>(n), pending_.end());
        pending_.resize(pending_.size() - n);
        auto locs = assignLocs(c->ptypes);
        // 1) argumentos en pila
        for (size_t i = 0; i < n; ++i) {
            if (locs[i].reg) continue;
            Ty pt = c->ptypes[i];
            load(args[i], pt, 0);
            std::string m = "[rsp + " + std::to_string(8 * locs[i].idx) + "]";
            switch (pt) {
                case Ty::Float: I("movss DWORD PTR " + m + ", xmm0"); break;
                case Ty::Double: I("movsd QWORD PTR " + m + ", xmm0"); break;
                case Ty::Str: I("mov QWORD PTR " + m + ", rax"); break;
                default: I("mov DWORD PTR " + m + ", eax"); break;
            }
        }
        // 2) argumentos en registros (de atras hacia adelante: xmm0/eax son el area de trabajo)
        for (size_t k = n; k-- > 0;) {
            if (!locs[k].reg) continue;
            Ty pt = c->ptypes[k];
            load(args[k], pt, 0);
            if (locs[k].fp) {
                if (locs[k].idx != 0) I("movaps xmm" + std::to_string(locs[k].idx) + ", xmm0");
            } else if (pt == Ty::Str) {
                I(std::string("mov ") + intReg64(locs[k].idx) + ", rax");
            } else {
                I(std::string("mov ") + intReg32(locs[k].idx) + ", eax");
            }
        }
        I("call " + c->label());
        if (c->ret != Ty::Void) store(q.result);
    }

    // ---- una funcion ----
    void emitFunction(Func& f) {
        cur_ = &f;
        pending_.clear();
        const std::string endLbl = ".Lret_" + sym(f.label());
        t_ << "\n";
        L(f.label());
        I("push rbp");
        I("mov rbp, rsp");
        I("sub rsp, " + std::to_string(frameSize(f)));
        for (auto& z : f.zero) {
            if (z.second == Ty::Str) {
                I("lea rax, [rip + " + strLabel("") + "]");
                I("mov QWORD PTR [rbp - " + std::to_string(z.first) + "], rax");
            } else {
                I("mov QWORD PTR [rbp - " + std::to_string(z.first) + "], 0");
            }
        }
        if (f.isInit) {
            for (const auto& g : globalOrder_) {
                const Var& v = globals_.at(g);
                if (v.ty == Ty::Str) {
                    I("lea rax, [rip + " + strLabel("") + "]");
                    I("mov QWORD PTR " + mem(v) + ", rax");
                }
            }
        }
        // parametros -> ranuras locales
        auto locs = assignLocs(f.ptypes);
        for (size_t i = 0; i < f.pnames.size(); ++i) {
            const Var& v = cur_->vars.at(f.pnames[i]);
            const std::string m = mem(v);
            if (locs[i].reg) {
                if (locs[i].fp) {
                    I(std::string(v.ty == Ty::Float ? "movss DWORD PTR " : "movsd QWORD PTR ") + m + ", xmm" + std::to_string(locs[i].idx));
                } else if (v.ty == Ty::Str) {
                    I("mov QWORD PTR " + m + ", " + intReg64(locs[i].idx));
                } else {
                    I("mov DWORD PTR " + m + ", " + intReg32(locs[i].idx));
                }
            } else {
                std::string src = "[rbp + " + std::to_string(16 + 8 * locs[i].idx) + "]";
                if (v.ty == Ty::Str || v.ty == Ty::Double) {
                    I("mov rax, QWORD PTR " + src);
                    I("mov QWORD PTR " + m + ", rax");
                } else {
                    I("mov eax, DWORD PTR " + src);
                    I("mov DWORD PTR " + m + ", eax");
                }
            }
        }
        for (const auto& q : f.body) emitQuad(f, q, endLbl);
        // caida al final sin return: devuelve 0
        if (!f.isInit && f.ret != Ty::Void) {
            if (isFp(f.ret)) I("xorps xmm0, xmm0");
            else if (f.ret == Ty::Str) I("lea rax, [rip + " + strLabel("") + "]");
            else I("xor eax, eax");
        }
        L(endLbl);
        I("leave");
        I("ret");
    }

    void emitQuad(Func& f, const Quadruple& q, const std::string& endLbl) {
        switch (q.op) {
            case OpCode::LABEL: L(tacLabel(q.result)); break;
            case OpCode::GOTO: I("jmp " + tacLabel(q.result)); break;
            case OpCode::IF_FALSE_GOTO:
                load(q.arg1, Ty::Bool, 0);
                I("test eax, eax");
                I("jz " + tacLabel(q.result));
                break;
            case OpCode::ASSIGN: {
                if (!q.arg2.empty() && q.arg2[0] == '@') break;  // declaracion: ya reservada
                Var& d = needVar(q.result);
                if (d.ty == Ty::Struct) { copyStruct(q.result, q.arg1); break; }
                load(q.arg1, d.ty, 0);
                store(q.result);
                break;
            }
            case OpCode::ADD: case OpCode::SUB: case OpCode::MUL: case OpCode::DIV: case OpCode::MOD: {
                Ty rt = arithType(q);
                if (q.op == OpCode::MOD && rt != Ty::Int) fail("el operador % requiere operandos enteros");
                load(q.arg1, rt, 0);
                load(q.arg2, rt, 1);
                if (rt == Ty::Int) {
                    switch (q.op) {
                        case OpCode::ADD: I("add eax, ecx"); break;
                        case OpCode::SUB: I("sub eax, ecx"); break;
                        case OpCode::MUL: I("imul eax, ecx"); break;
                        case OpCode::DIV: I("cdq"); I("idiv ecx"); break;
                        default: I("cdq"); I("idiv ecx"); I("mov eax, edx"); break;
                    }
                } else {
                    const char* sfx = rt == Ty::Float ? "ss" : "sd";
                    const char* nm = q.op == OpCode::ADD ? "add" : q.op == OpCode::SUB ? "sub" : q.op == OpCode::MUL ? "mul" : "div";
                    I(std::string(nm) + sfx + " xmm0, xmm1");
                }
                store(q.result);
                break;
            }
            case OpCode::EQ: case OpCode::NEQ: case OpCode::LT: case OpCode::GT: case OpCode::LE: case OpCode::GE: {
                Ty rt = arithType(q);
                load(q.arg1, rt, 0);
                load(q.arg2, rt, 1);
                if (rt == Ty::Int) {
                    I("cmp eax, ecx");
                    const char* cc = q.op == OpCode::EQ ? "sete" : q.op == OpCode::NEQ ? "setne" : q.op == OpCode::LT ? "setl"
                                   : q.op == OpCode::GT ? "setg" : q.op == OpCode::LE ? "setle" : "setge";
                    I(std::string(cc) + " al");
                } else {
                    const std::string u = rt == Ty::Float ? "ucomiss" : "ucomisd";
                    switch (q.op) {
                        case OpCode::EQ: I(u + " xmm0, xmm1"); I("sete al"); I("setnp cl"); I("and al, cl"); break;
                        case OpCode::NEQ: I(u + " xmm0, xmm1"); I("setne al"); I("setp cl"); I("or al, cl"); break;
                        case OpCode::LT: I(u + " xmm1, xmm0"); I("seta al"); break;
                        case OpCode::GT: I(u + " xmm0, xmm1"); I("seta al"); break;
                        case OpCode::LE: I(u + " xmm1, xmm0"); I("setae al"); break;
                        default: I(u + " xmm0, xmm1"); I("setae al"); break;
                    }
                }
                I("movzx eax, al");
                store(q.result);
                break;
            }
            case OpCode::AND: case OpCode::OR:
                load(q.arg1, Ty::Bool, 0);
                load(q.arg2, Ty::Bool, 1);
                I(q.op == OpCode::AND ? "and eax, ecx" : "or eax, ecx");
                store(q.result);
                break;
            case OpCode::NOT:
                load(q.arg1, Ty::Bool, 0);
                I("xor eax, 1");
                store(q.result);
                break;
            case OpCode::NEG: {
                Ty a = typeOf(q.arg1);
                Ty rt = promote(a, a);
                load(q.arg1, rt, 0);
                if (rt == Ty::Int) {
                    I("neg eax");
                } else {
                    const char* sfx = rt == Ty::Float ? "ss" : "sd";
                    I("xorps xmm1, xmm1");
                    I(std::string("sub") + sfx + " xmm1, xmm0");
                    I("movaps xmm0, xmm1");
                }
                store(q.result);
                break;
            }
            case OpCode::PARAM: pending_.push_back(q.arg1); break;
            case OpCode::CALL: emitCall(q); break;
            case OpCode::RETURN:
                if (!q.arg1.empty()) {
                    if (f.ret == Ty::Void) fail("return con valor en funcion void: " + f.name);
                    load(q.arg1, f.ret, 0);
                }
                I("jmp " + endLbl);
                break;
            case OpCode::PRINT: emitPrint(q); break;
            case OpCode::READ: emitRead(q); break;
            case OpCode::POP_PARAM: break;
            default: fail("operacion TAC no soportada: " + q.print());
        }
    }

    static std::string asmString(const std::string& s) {
        std::string r = "\t.ascii \"";
        for (unsigned char c : s) {
            if (c == '"' || c == '\\') { r += '\\'; r += static_cast<char>(c); }
            else if (c >= 32 && c < 127) r += static_cast<char>(c);
            else { char b[8]; std::snprintf(b, sizeof b, "\\%03o", c); r += b; }
        }
        return r + "\"\n\t.byte 0\n";
    }

public:
    std::string generateAll(const std::vector<Quadruple>& tac) {
        split(tac);
        for (auto& f : funcs_) {
            f.ret = tyFromText(f.retText);
            if (f.ret == Ty::Struct) fail("retorno de struct por valor no soportado: " + f.name);
            for (const auto& p : f.ptext) {
                Ty t = tyFromText(p);
                if (t == Ty::Struct) fail("parametro struct por valor no soportado en " + f.name);
                if (t == Ty::Void) fail("parametro void en " + f.name);
                f.ptypes.push_back(t);
            }
        }
        Func* mainFn = findFunc("main");
        if (!mainFn) fail("el programa no define la funcion main");
        if (mainFn->ret != Ty::Int) fail("main debe devolver int");
        if (!mainFn->ptypes.empty()) fail("main no admite parametros en esta version");

        layout(init_);  // primero: registra los globales
        for (auto& f : funcs_) layout(f);

        t_ << "\t.intel_syntax noprefix\n\t.text\n";
        emitFunction(init_);
        for (auto& f : funcs_) emitFunction(f);

        // punto de entrada
        t_ << "\n\t.globl main\n";
        L("main");
        I("push rbp");
        I("mov rbp, rsp");
        I("sub rsp, 32");
        if (win_) {
            I("mov ecx, 65001");  // consola en UTF-8
            callC("SetConsoleOutputCP");
        }
        I("call cg_init_globals");
        I("call " + findFunc("main")->label());
        I("leave");
        I("ret");

        std::ostringstream out;
        out << "# Generado por compiler_app - x86-64 (" << (win_ ? "Windows / ABI Microsoft x64" : "Linux / System V") << ")\n";
        out << t_.str();
        out << "\n\t" << (win_ ? ".section .rdata,\"dr\"" : ".section .rodata") << "\n";
        for (size_t i = 0; i < strPool_.size(); ++i) out << ".LC" << i << ":\n" << asmString(strPool_[i]);
        out << "\t.p2align 3\n";
        for (size_t i = 0; i < fpPool_.size(); ++i) out << ".LFP" << i << ":\n" << fpPool_[i].substr(1) << "\n";
        out << "\n\t.bss\n\t.p2align 4\n";
        for (const auto& g : globalOrder_) out << globals_.at(g).symbol << ":\n\t.zero 8\n";
        for (int i = 0; i < sbufCount_; ++i) out << ".Lsbuf" << i << ":\n\t.zero 256\n";
        if (!win_) out << "\n\t.section .note.GNU-stack,\"\",@progbits\n";
        return out.str();
    }
};

}  // namespace

std::string NativeBackend::generate(const std::vector<Quadruple>& tac) const {
    Gen g(target_ == TargetOS::Windows);
    return g.generateAll(tac);
}
