#include "Toolchain.hpp"

#include <cstdlib>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef NATIVE_DEFAULT_CC
#define NATIVE_DEFAULT_CC ""
#endif

namespace {

std::string quote(const std::string& s) { return "\"" + s + "\""; }

// cmd.exe necesita comillas extra alrededor de toda la linea cuando empieza con comillas.
std::string shellLine(const std::string& cmd) {
#ifdef _WIN32
    return "\"" + cmd + "\"";
#else
    return cmd;
#endif
}

int runShell(const std::string& cmd) {
    int rc = std::system(shellLine(cmd).c_str());
#ifndef _WIN32
    if (rc != -1 && WIFEXITED(rc)) rc = WEXITSTATUS(rc);
#endif
    return rc;
}

bool driverWorks(const std::string& cc) {
#ifdef _WIN32
    const char* sink = " >nul 2>&1";
#else
    const char* sink = " >/dev/null 2>&1";
#endif
    return runShell(quote(cc) + " --version" + sink) == 0;
}

}  // namespace

BuildResult buildExecutable(const std::string& asmPath, const std::string& exePath,
                            const std::string& ccHint) {
    std::vector<std::string> candidates;
    if (!ccHint.empty()) candidates.push_back(ccHint);
    if (const char* env = std::getenv("CC")) {
        if (*env) candidates.push_back(env);
    }
    if (std::string(NATIVE_DEFAULT_CC).size()) candidates.push_back(NATIVE_DEFAULT_CC);
    for (const char* c : {"gcc", "x86_64-w64-mingw32-gcc", "clang", "cc"}) candidates.push_back(c);

    BuildResult result;
    for (const auto& cc : candidates) {
        if (!driverWorks(cc)) continue;
        result.compiler = cc;
        const std::string cmd = quote(cc) + " " + quote(asmPath) + " -o " + quote(exePath);
        if (runShell(cmd) == 0) {
            result.ok = true;
            return result;
        }
        result.message = "El ensamblado/enlace con '" + cc + "' fallo (revisa el mensaje de arriba y el archivo " + asmPath + ").";
        return result;
    }
    result.message =
        "No se encontro un ensamblador/enlazador (gcc de MinGW-w64, clang o similar).\n"
        "  - Instala MinGW-w64 (o usa el que trae CLion) y agrega su carpeta bin al PATH, o\n"
        "  - indica el driver con --cc <ruta>/gcc.exe\n"
        "El ensamblador generado quedo en " + asmPath + " (se puede ensamblar a mano: gcc " + asmPath + " -o programa.exe).";
    return result;
}

int runExecutable(const std::string& exePath) {
    std::string path = exePath;
    if (path.find('/') == std::string::npos && path.find('\\') == std::string::npos) {
#ifdef _WIN32
        path = ".\\" + path;
#else
        path = "./" + path;
#endif
    }
    return runShell(quote(path));
}
