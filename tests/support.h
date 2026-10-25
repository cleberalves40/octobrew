// Utilitários dos testes: diretório temporário e brew falso.
#ifndef OCTOBREW_TUI_TEST_SUPPORT_H
#define OCTOBREW_TUI_TEST_SUPPORT_H

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

struct TempDir {
    std::string path;
    TempDir()
    {
        char tmpl[] = "/tmp/octobrew-tui-test.XXXXXX";
        path = ::mkdtemp(tmpl);
    }
    ~TempDir()
    {
        const std::string cmd = "rm -rf '" + path + "'";
        (void)std::system(cmd.c_str());
    }
    std::string file(const std::string &name) const { return path + "/" + name; }
};

inline void writeExecutable(const std::string &path, const std::string &content)
{
    std::ofstream(path) << content;
    ::chmod(path.c_str(), 0755);
}

inline std::string readFile(const std::string &path)
{
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline std::vector<std::string> readLines(const std::string &path)
{
    std::vector<std::string> out;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line))
        if (!line.empty())
            out.push_back(line);
    return out;
}

/**
 * brew falso: registra cada chamada numa linha do log, com os argumentos
 * separados por "|", e o ambiente (HOMEBREW_NO_AUTO_UPDATE, HOMEBREW_NO_COLOR,
 * LC_ALL, HOMEBREW_NO_ASK, HOMEBREW_NO_AUTOREMOVE e SUDO_ASKPASS) em log +
 * ".env"; depois executa body, um trecho de sh com "$@" disponível.
 */
inline std::string fakeBrew(const TempDir &dir, const std::string &log, const std::string &body)
{
    const std::string path = dir.file("brew");
    writeExecutable(path, "#!/bin/sh\n"
                          "( IFS='|'; echo \"$*\" ) >> '" + log + "'\n"
                          "echo \"$HOMEBREW_NO_AUTO_UPDATE|$HOMEBREW_NO_COLOR|$LC_ALL|$HOMEBREW_NO_ASK|$HOMEBREW_NO_AUTOREMOVE|$SUDO_ASKPASS\" >> '" + log + ".env'\n"
                          + body + "\n");
    return path;
}

inline void writeFile(const std::string &path, const std::string &content)
{
    std::ofstream(path) << content;
}

#endif
