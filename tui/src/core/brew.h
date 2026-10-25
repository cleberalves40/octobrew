// SPDX-License-Identifier: GPL-2.0-or-later
//
// Integração com o Homebrew: a contraparte do pkg.h do octopkg-tui (o porte
// das classes Package e UnixCommand do OctoPkg), lendo o JSON do brew em vez
// da saída do pkg(8).
// OctoPkg: Copyright (C) 2015 Alexandre Albuquerque Arnt.

#ifndef OCTOBREW_TUI_BREW_H
#define OCTOBREW_TUI_BREW_H

#include <atomic>
#include <cstdint>
#include <ctime>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace octo {

/** Fórmula (Cellar) ou cask (Caskroom: aplicativos, fontes, pacotes .pkg). */
enum class Kind { Formula = 0, Cask };

/**
 * A ordem do PackageStatus do OctoPkg, que é a ordem da coluna de estado;
 * o bloqueio do pkg é o pin do brew.
 */
enum class Status { Pinned = 0, Installed, NotInstalled, Outdated, Newer };

struct Package {
    std::string name;               // fórmula ou token do cask ("curl", "alacritty")
    std::string fullName;           // com o tap, fora dos oficiais ("user/tap/foo"); vai aos comandos
    Kind kind = Kind::Formula;
    std::string version;            // disponível, na visão Repositório; instalada, na visão Instalados
    std::string tap;                // "homebrew/core", "homebrew/cask", "user/tap"
    std::string comment;            // desc
    std::int64_t size = 0;          // tamanho em disco, calculado depois da listagem; 0: desconhecido
    std::int64_t installedOn = 0;   // UNIX; 0 se não instalado
    bool installed = false;
    bool pinned = false;
    bool automatic = false;         // instalado como dependência (não installed_on_request)
    int requiredBy = 0;             // quantos instalados dependem dele diretamente
    std::string installedVersion;
    std::string newVersion;         // versão disponível, se desatualizado
    std::vector<std::string> dependencies;   // diretas, de execução (só instalados)
    std::vector<std::string> paths;          // casks instalados: onde os artifacts foram parar
    bool hasService = false;                 // a fórmula define um serviço (brew services)
    Status status = Status::NotInstalled;

    /**
     * Identidade na lista: fórmula e cask podem ter o mesmo nome ("docker"),
     * e taps diferentes, a mesma fórmula. "f:curl", "c:alacritty".
     */
    std::string key() const;
};

/** Aba Informação e aba Arquivos. */
struct Details {
    std::string name;
    std::string fullName;
    Kind kind = Kind::Formula;
    std::string version;
    std::string tap;
    std::string homepage;
    std::string license;
    std::string description;
    std::string caveats;
    std::string kegOnly;                     // motivo; vazio se não for keg-only
    std::string deprecated;                  // motivo, se obsoleto
    std::string disabled;                    // motivo, se desativado
    bool autoUpdates = false;                // cask que se atualiza sozinho
    std::vector<std::string> names;          // casks: nomes do aplicativo
    std::vector<std::string> dependencies;
    std::vector<std::string> buildDependencies;
    std::vector<std::string> requiredBy;     // instalados que dependem dele (a App preenche)
    std::vector<std::string> conflicts;
    std::vector<std::string> artifacts;      // casks: "App: Alacritty.app"
    std::vector<std::string> files;          // só instalados
    bool filesTruncated = false;
    bool installed = false;
    std::int64_t installedOn = 0;
};

/**
 * Prévia de uma transação: a saída do "brew install --dry-run", do "brew
 * upgrade --dry-run", do "brew autoremove --dry-run" ou do "brew cleanup
 * --dry-run", como o OctoPkg lê a do pkg -n.
 */
struct TransactionPreview {
    struct Section {
        std::string title;               // "Would install 2 formulae", "Would upgrade 1 outdated package"
        std::string verb;                // install, upgrade, autoremove, remove, reinstall
        std::vector<std::string> items;  // como o brew as imprime, sem o alinhamento: "curl 8.9.1 -> 8.10.0"
    };
    std::vector<Section> sections;
    std::string freed;                   // cleanup: "6.2GB", de "would free approximately 6.2GB"
    std::vector<std::string> warnings;   // "Warning: ..." (casks desativados, pins...), sem o prefixo

    int count() const;
    /** Itens das seções com o verbo (vazio: todas). */
    std::vector<std::string> items(const std::string &verb = std::string()) const;
};

/** Um serviço do "brew services list --json" (launchd no macOS, systemd no Linux). */
struct Service {
    std::string name;
    std::string status;     // none, started, stopped, scheduled, error, unknown
    std::string user;       // quem o iniciou; vazio se não está carregado
    std::string file;       // o plist ou a unit
    int exitCode = -1;      // -1: sem código
};

/** Um tap do "brew tap-info --json --installed". */
struct Tap {
    std::string name;       // "user/repo"
    bool official = false;  // homebrew/core, homebrew/cask
    bool trusted = false;   // brew trust --tap (brew 6+); os oficiais sempre
    std::vector<std::string> formulae;   // nomes completos ("user/repo/foo")
    std::vector<std::string> casks;
};

/** O "brew doctor": sai com 1 quando há avisos, o que não é uma falha. */
struct DoctorReport {
    bool ready = false;                  // saiu com 0 ("Your system is ready to brew.")
    std::vector<std::string> warnings;   // os títulos dos "Warning:" e "Error:"; pode ficar vazio sem ready
    std::string output;                  // tudo, para a aba Saída
};

/** Função que recebe a saída de uma operação à medida que ela roda. */
using OutputFn = std::function<void(const std::string &chunk)>;

// --- Leitura do JSON (funções puras, testadas sem o brew) ---

/** "brew info --json=v2 --installed": fórmulas e casks instalados, com o estado. */
std::vector<Package> parseInstalled(const std::string &json);
/**
 * O cache da API do Homebrew: o "packages.<tag>.jws.json" do brew 5 ou mais
 * novo (um objeto com "formulae" e "casks") ou o "formula.jws.json" e o
 * "cask.jws.json" do brew 4 (um array). Com ou sem o envelope JWS ("payload").
 */
std::vector<Package> parseApi(const std::string &json);
/** "brew info --json=v2 --formula|--cask nome": o primeiro registro. */
Details parseDetails(const std::string &json);

/**
 * Visão Repositório: os pacotes da API, com o estado dos instalados, mais os
 * instalados que a API não tem (taps de terceiros).
 */
std::vector<Package> mergeRemote(const std::vector<Package> &remote, const std::vector<Package> &installed);
/** Instalados que dependem diretamente de fullName (o "Requerido por"). */
std::vector<std::string> dependents(const std::vector<Package> &installed, const std::string &fullName);

/** Versão com a revisão do brew ("3.4" e 1 → "3.4_1"). */
std::string formulaVersion(const std::string &stable, int revision);
/**
 * Compara versões por partes numéricas e de texto ("1.10" > "1.9"), como o
 * rpmvercmp do OctoPkg. -1, 0 ou 1.
 */
int compareVersions(const std::string &a, const std::string &b);

/** Tamanho legível, como o Package::kbytesToSize do OctoPkg ("12.34 MiB"). */
std::string formatSize(std::int64_t bytes);
/** Data e hora local "AAAA-MM-DD HH:MM"; vazio para 0. */
std::string formatTime(std::int64_t unixTime);

/**
 * Nome aceito como argumento do brew: não vazio, sem "-" inicial (seria lido
 * como opção) e só com letras, dígitos e "@+-._/".
 */
bool validPackageName(const std::string &name);

// --- Transações (funções puras) ---

TransactionPreview parseDryRun(const std::string &output, const std::string &errors = std::string());
/** Nome do pacote num item da prévia ("curl 8.9.1 -> 8.10.0" → "curl"). */
std::string itemName(const std::string &item);

/**
 * O que a remoção de keys (Package::key()) leva junto: os instalados que
 * dependem deles, direta ou indiretamente, fora os próprios keys. O brew
 * recusa a remoção enquanto houver dependentes, então eles precisam ir
 * juntos, como o "pkg remove -R" do OctoPkg.
 */
std::vector<std::string> dependentsClosure(const std::vector<Package> &installed, const std::vector<std::string> &keys);

/**
 * Pacotes que o octobrew-tui não deixa remover, como o Package::isForbidden do
 * OctoPkg: o FTXUI, de que o próprio programa depende, e o próprio programa.
 */
bool isForbidden(const Package &p);

/**
 * O cask (um registro do "brew info --json=v2 --cask") pede o sudo para
 * instalar ou remover: instaladores .pkg e passos de desinstalação no sistema
 * (pkgutil, launchctl, kext, scripts, arquivos fora do usuário).
 */
bool caskNeedsSudo(const std::string &infoJson, const std::string &token);

// --- Fase 4: serviços, taps, doctor e Brewfiles (funções puras) ---

std::vector<Service> parseServices(const std::string &json);
std::vector<Tap> parseTaps(const std::string &json);
/**
 * Os pacotes dos taps de terceiros, que o cache da API não tem: só o nome e o
 * tap (a versão e a descrição vêm do brew info, nos detalhes).
 */
std::vector<Package> tapPackages(const std::vector<Tap> &taps);
/** exitStatus: o código do brew doctor; 0 é "pronto". */
DoctorReport parseDoctor(const std::string &output, int exitStatus);
/** O que falta de um Brewfile, no "brew bundle check --verbose": "Formula figlet", "Cask iterm2". */
std::vector<std::string> parseBundleCheck(const std::string &output);
/** "user/repo", como o brew tap aceita (sem URL). */
bool validTapName(const std::string &name);

/** Nome de um snapshot antes da transação, como o do Boot Environment: "AAMMDD-hhmmss.Brewfile". */
std::string snapshotName(std::time_t when);
/** Os snapshots em dir, do mais novo ao mais antigo (só os nomes). */
std::vector<std::string> listSnapshots(const std::string &dir);

/**
 * Bytes em disco da árvore em path, sem seguir links simbólicos (o tamanho
 * aparente dos arquivos, como o flatsize do pkg). cancel, se dado, interrompe.
 */
std::int64_t treeSize(const std::string &path, const std::atomic<bool> *cancel = nullptr);

class Manager
{
public:
    /** brewPath: executável no lugar do brew; vazio: o do Homebrew instalado ou o do PATH. */
    explicit Manager(std::string brewPath = std::string());

    std::vector<Package> listInstalled() const;
    /**
     * Do cache da API, sem rede e sem o Ruby do brew, mais os pacotes dos taps
     * de terceiros (brew tap-info). Se só o tap-info falhar, warning diz por
     * quê, e a lista fica com os da API.
     */
    std::vector<Package> listRemote(std::string *warning = nullptr) const;
    /** brew info, mais brew list nos instalados (a aba Arquivos). */
    Details details(const Package &p) const;
    /** Tamanho de cada instalado, por Package::key(); os que não deram para medir ficam de fora. */
    std::map<std::string, std::int64_t> sizes(const std::vector<Package> &installed,
                                              const std::atomic<bool> *cancel = nullptr) const;
    /** "Homebrew 7.0.10" */
    std::string version() const;

    // --- Prévias (dry-run): não mudam nada ---
    TransactionPreview previewInstall(const std::vector<Package> &packages) const;
    TransactionPreview previewUpgrade() const;
    TransactionPreview previewAutoremove() const;
    TransactionPreview previewCleanup() const;
    /** Os casks de tokens que pedem o sudo (uma chamada ao brew info). */
    std::vector<std::string> casksNeedingSudo(const std::vector<std::string> &tokens) const;

    // --- Operações ---
    // interactive: o brew roda com o terminal (a tela suspensa), e o sudo
    // pode pedir a senha. Sem ele, um sudo que precise de senha falha na
    // hora (SUDO_ASKPASS=/usr/bin/false) em vez de disputar a tela.
    void install(const std::vector<Package> &packages, const OutputFn &out = nullptr, bool interactive = false) const;
    void uninstall(const std::vector<Package> &packages, const OutputFn &out = nullptr, bool interactive = false) const;
    void upgrade(const OutputFn &out = nullptr, bool interactive = false) const;
    void update(const OutputFn &out = nullptr) const;
    void cleanup(const OutputFn &out = nullptr) const;
    void autoremove(const OutputFn &out = nullptr) const;
    void pin(const Package &p, bool pin, const OutputFn &out = nullptr) const;
    /** brew bundle dump: o estado instalado, num Brewfile. */
    void snapshot(const std::string &file, const OutputFn &out = nullptr) const;
    /** brew bundle install: instala o que falta do Brewfile, sem atualizar o resto. */
    void restore(const std::string &file, const OutputFn &out = nullptr, bool interactive = false) const;

    // --- Fase 4 ---
    std::vector<Service> listServices() const;
    /** brew services start|stop|restart nome (do usuário, sem sudo). */
    void service(const std::string &verb, const std::string &name, const OutputFn &out = nullptr) const;
    std::vector<Tap> listTaps() const;
    /** brew tap user/repo e, com trust, brew trust --tap user/repo. */
    void tap(const std::string &name, bool trust, const OutputFn &out = nullptr) const;
    void untap(const std::string &name, const OutputFn &out = nullptr) const;
    DoctorReport doctor() const;
    /** brew bundle check --verbose: o que falta do Brewfile (vazio: nada). */
    std::vector<std::string> bundleCheck(const std::string &file) const;

    /** Linha de comando de uma operação, para a aba Saída ("brew install --formula curl"). */
    std::string commandLine(const std::vector<std::string> &args) const;

    // Os argumentos de cada operação (para a aba Saída e os testes).
    static std::vector<std::vector<std::string>> installArgs(const std::vector<Package> &packages);
    static std::vector<std::vector<std::string>> uninstallArgs(const std::vector<Package> &packages);
    static std::vector<std::string> pinArgs(const Package &p, bool pin);

    struct Paths {
        std::string cellar;     // brew --cellar
        std::string caskroom;   // brew --caskroom
        std::string cache;      // brew --cache
    };
    /** Os diretórios do Homebrew, perguntados ao brew uma vez. */
    Paths paths() const;

    const std::string &brewPath() const { return m_brewPath; }

    /** Limite da aba Arquivos, como no OctoPkg (2 MiB de saída). */
    static constexpr std::size_t kMaxFilesOutput = 2097152;

private:
    std::string query(const std::vector<std::string> &args) const;
    TransactionPreview dryRun(const std::vector<std::string> &args) const;
    void run(const std::vector<std::string> &args, const OutputFn &out, bool interactive) const;

    std::string m_brewPath;
    mutable std::mutex m_pathsMutex;
    mutable bool m_hasPaths = false;
    mutable Paths m_paths;
};

} // namespace octo

#endif
