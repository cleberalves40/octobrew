#include "app.h"

#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

void usage()
{
    std::cout << "uso: octobrew-tui [--remote] [--icons=nerd|unicode|ascii] [--lang=en_US|pt_BR|es_ES]\n"
                 "                   [--theme=TEMA] [--config=ARQUIVO] [--brew=EXECUTÁVEL] [--version]\n"
                 "\n"
                 "Interface de terminal para o Homebrew, derivada do OctoPkg e do octopkg-tui.\n"
                 "--remote começa na lista do repositório (o cache da API do Homebrew). Os\n"
                 "ícones \"nerd\" (padrão) exigem uma fonte Nerd no terminal, como a JetBrainsMono\n"
                 "Nerd Font Mono (brew install --cask font-jetbrains-mono-nerd-font). Temas:\n"
                 "system, octopkg, frappe, latte, mocha, macchiato, solarized_dark,\n"
                 "solarized_light, selenized_dark, selenized_light, dracula, tokyo_night, nord,\n"
                 "gruvbox, one_dark. As operações rodam como o seu usuário; casks que instalam\n"
                 "pacotes .pkg pedem o sudo, com a tela suspensa enquanto ele pede a senha.\n";
}

bool option(const char *arg, const char *name, std::string *value)
{
    const size_t n = std::strlen(name);
    if (std::strncmp(arg, name, n) != 0 || arg[n] != '=')
        return false;
    *value = arg + n + 1;
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    tui::Options options;
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!std::strcmp(a, "--help") || !std::strcmp(a, "-h")) {
            usage();
            return 0;
        }
        if (!std::strcmp(a, "--version") || !std::strcmp(a, "-V")) {
            std::cout << "octobrew-tui " << OCTOBREW_TUI_VERSION << '\n';
            return 0;
        }
        if (!std::strcmp(a, "--remote")) {
            options.remote = true;
            continue;
        }
        if (option(a, "--icons", &options.iconMode) || option(a, "--lang", &options.language)
            || option(a, "--config", &options.settingsPath) || option(a, "--brew", &options.brewPath)
            || option(a, "--theme", &options.theme))
            continue;
        std::cerr << "octobrew-tui: opção desconhecida: " << a << "\n\n";
        usage();
        return 2;
    }
    // O Homebrew se recusa a rodar como root; melhor dizer logo por quê.
    if (::geteuid() == 0) {
        std::cerr << "octobrew-tui: o Homebrew não roda como root; use o seu usuário.\n";
        return 1;
    }
    tui::App app(options);
    return app.run();
}
