// Tela: renderiza a interface em texto e confere o que aparece, e dirige a
// App com eventos de teclado, sem terminal.

#include "app.h"
#include "fixtures.h"
#include "i18n.h"
#include "support.h"
#include "view.h"

#include <catch2/catch_test_macros.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/terminal.hpp>
#include <memory>
#include <set>

using namespace ftxui;
using octo::Kind;
using octo::Status;

namespace {

octo::Package installed(const std::string &name, const std::string &version, std::int64_t size,
                        Kind kind = Kind::Formula)
{
    octo::Package p;
    p.name = p.fullName = name;
    p.kind = kind;
    p.tap = kind == Kind::Cask ? "homebrew/cask" : "homebrew/core";
    p.version = version;
    p.installedVersion = version;
    p.size = size;
    p.installedOn = 1760000000;
    p.installed = true;
    p.comment = name + " comment";
    p.status = Status::Installed;
    return p;
}

// Na ordem do brew: fórmulas e depois casks.
std::vector<octo::Package> sample()
{
    octo::Package curl = installed("curl", "8.9.1", 4 * 1024 * 1024);
    curl.newVersion = "8.10.0";
    curl.status = Status::Outdated;
    curl.dependencies = {"brotli"};
    octo::Package brotli = installed("brotli", "1.2.0", 1024 * 1024);
    brotli.automatic = true;
    octo::Package wget = installed("wget", "1.24.5", 25 * 1024 * 1024);
    wget.pinned = true;
    wget.newVersion = "1.25.0_1";
    wget.status = Status::Pinned;
    wget.dependencies = {"brotli"};
    return {curl, brotli, wget, installed("alacritty", "0.17.0", 14 * 1024 * 1024, Kind::Cask)};
}

tui::Listing listing()
{
    tui::Listing l;
    l.installed = sample();
    return l;
}

octo::Details curlDetails()
{
    octo::Details d = octo::parseDetails(kCurlInfo);
    d.files = {"/opt/homebrew/Cellar/curl/8.9.1/bin/curl", "/opt/homebrew/Cellar/curl/8.9.1/lib/libcurl.4.dylib"};
    return d;
}

std::string draw(const tui::ViewState &s, int width = 120, int height = 40)
{
    auto screen = Screen::Create(Dimension::Fixed(width), Dimension::Fixed(height));
    Render(screen, tui::render(s, text("")));
    // Sem as sequências de cor e estilo, que o FTXUI intercala no texto.
    const std::string raw = screen.ToString();
    std::string out;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\x1b' && i + 1 < raw.size() && raw[i + 1] == '[') {
            i += 2;
            while (i < raw.size() && !(raw[i] >= '@' && raw[i] <= '~'))
                ++i;
            continue;
        }
        out += raw[i];
    }
    return out;
}

bool has(const std::string &haystack, const std::string &needle)
{
    return haystack.find(needle) != std::string::npos;
}

// A tecla de aplicar na barra: a notação da Apple no macOS, a do mc no Linux.
#ifdef __APPLE__
const char kApplyKey[] = "⌥M";
#else
const char kApplyKey[] = "M-m";
#endif

// CHECK de cada texto de uma lista, dizendo qual faltou.
#define CHECK_INFO(out, t)                                                                              \
    do {                                                                                                \
        INFO(t);                                                                                        \
        CHECK(has(out, t));                                                                             \
    } while (false)

tui::Options options(const TempDir &dir, const std::string &brew)
{
    tui::Options o;
    o.brewPath = brew;
    // O sudo falso: "sudo -n true" passa (credenciais em cache), a menos que sudo.rc exista.
    o.sudoPath = dir.file("sudo");
    // Sem rede nos testes: o curl é o curl.sh de cada teste (ou nenhum).
    o.curlPath = dir.file("curl.sh");
    writeExecutable(o.sudoPath, "#!/bin/sh\n[ -f '" + dir.file("sudo.rc") + "' ] && exit 1\nexit 0\n");
    o.iconMode = "ascii";
    o.language = "en_US";
    o.settingsPath = dir.file("config");
    return o;
}

// App com a lista de sample(), sem brew: nada aqui chama o brew.
struct AppFixture {
    TempDir dir;
    tui::App app;
    AppFixture()
        : app(options(dir, dir.file("brew-nao-usado")))
    {
        app.applyListing(listing());
    }
    std::string selected()
    {
        const tui::ViewState s = app.viewState();
        return s.hasCurrent ? s.current.key() : std::string();
    }
};

// App com o brew falso dos fixtures: a listagem, os tamanhos, o repositório
// e os detalhes passam pelo núcleo de verdade (sem tela, tudo roda na hora).
struct BrewFixture {
    FakeBrew brew;
    std::unique_ptr<tui::App> app;
    // unpinned: o wget sem o pin, para que o brotli possa sair com os dependentes.
    explicit BrewFixture(bool unpinned = false)
    {
        if (unpinned) {
            std::string json = kInstalledJson;
            const std::string pinned = "\"pinned\": true";
            json.replace(json.find(pinned), pinned.size(), "\"pinned\": false");
            writeFile(brew.dir.file("installed.json"), json);
        }
        brew.writeApiCache();
        // Algo no Cellar e no Caskroom, para os tamanhos.
        (void)std::system(("mkdir -p '" + brew.dir.file("Cellar/curl/8.9.1") + "' '"
                           + brew.dir.file("Caskroom/alacritty/0.17.0") + "'").c_str());
        writeFile(brew.dir.file("Cellar/curl/8.9.1/curl"), std::string(2048, 'x'));
        writeFile(brew.dir.file("Caskroom/alacritty/0.17.0/a"), std::string(100, 'x'));
        app = std::make_unique<tui::App>(options(brew.dir, brew.path));
        app->refresh();
    }
    void select(const std::string &key)
    {
        app->handleEvent(Event::Home);
        for (int i = 0; i < 20 && app->viewState().current.key() != key; ++i)
            app->handleEvent(Event::ArrowDown);
        REQUIRE(app->viewState().current.key() == key);
    }
    std::string screen() { return draw(app->viewState()); }
    const tui::Dialog &dialog() const { return app->dialog(); }
    std::string snapshots() const { return brew.dir.file("snapshots"); }
};

} // namespace

TEST_CASE("tela mostra colunas, linhas e estado", "[view]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    const std::string out = draw(f.app.viewState());
    for (const char *t : {"OctoBrew — Homebrew", "[# Installed packages]", "Name ^", "Type", "Version", "Size",
                          "Installed on", "curl", "brotli", "alacritty", "Formula", "Cask", "4.00 MiB", "25.00 MiB",
                          "Ready.", "4 package(s)", "2 outdated", "1 i Info", "2 f Files", "3 a Actions",
                          "4 > Output", "5 n News", "6 ? Help", "F1", "F5", "F8", "F9", "F10", "+/-", kApplyKey})
        CHECK_INFO(out, t);
    // Ordem pelo nome; brotli é dependência ("d"), curl desatualizado ("U"), wget fixado ("P").
    CHECK(out.find("alacritty") < out.find("brotli"));
    CHECK(has(out, " d brotli"));
    CHECK(has(out, " U curl"));
    CHECK(has(out, " P wget"));
    // Aba Informação do pacote selecionado (alacritty), ainda sem os detalhes.
    CHECK(has(out, "Status:"));
    CHECK(has(out, "Loading…"));
}

TEST_CASE("aba Informação de uma fórmula, com os caveats", "[view]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.handleEvent(Event::ArrowDown);   // brotli
    f.app.handleEvent(Event::ArrowDown);   // curl
    REQUIRE(f.selected() == "f:curl");
    octo::Details d = curlDetails();
    d.requiredBy = {"wget"};
    f.app.applyDetails(f.app.currentDetailsKey(), d, "");
    tui::ViewState s = f.app.viewState();
    std::string out = draw(s);
    for (const char *t : {"Outdated (version 8.10.0 is available)", "Type:", "Formula", "Installed version:",
                          "8.9.1", "Tap:", "homebrew/core", "https://curl.se", "License:", "Keg-only:",
                          "provided by macos", "Installed size:", "4.00 MiB", "2 f Files (2)"})
        CHECK_INFO(out, t);
    // Dependências e caveats ficam abaixo: aparecem rolando a aba (Tab e setas).
    CHECK_FALSE(has(out, "Caveats"));
    s.contentFocused = true;
    s.contentScroll = 11;
    out = draw(s);
    for (const char *t : {"Depends on:", "libnghttp2", "Build dependencies:", "pkgconf", "Required by:", "wget",
                          "Caveats", "curl is keg-only", "  echo 'export PATH="})
        CHECK_INFO(out, t);
}

TEST_CASE("aba Informação de um cask, com os artifacts", "[view]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    REQUIRE(f.selected() == "c:alacritty");
    octo::Details d = octo::parseDetails(kKittyInfo);
    d.autoUpdates = true;
    f.app.applyDetails(f.app.currentDetailsKey(), d, "");
    tui::ViewState s = f.app.viewState();
    s.contentFocused = true;
    s.contentScroll = 3;
    const std::string out = draw(s);
    for (const char *t : {"Cask", "Application:", "Auto-updates:", "yes (brew upgrade skips it)", "Deprecated:",
                          "unmaintained", "Conflicts with:", "kitty@nightly", "Artifacts:", "App: kitty.app",
                          "Command wrapper: kitty"})
        CHECK_INFO(out, t);
}

TEST_CASE("aba Arquivos e erro nos detalhes", "[view]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::ArrowDown);   // curl
    f.app.applyDetails(f.app.currentDetailsKey(), curlDetails(), "");
    f.app.handleEvent(Event::Character('2'));
    std::string out = draw(f.app.viewState());
    CHECK(has(out, "/opt/homebrew/Cellar/curl/8.9.1/lib/libcurl.4.dylib"));

    f.app.handleEvent(Event::ArrowDown);   // wget, sem detalhes
    f.app.applyDetails(f.app.currentDetailsKey(), {}, "boom");
    out = draw(f.app.viewState());
    CHECK(has(out, "ERROR: boom"));
}

TEST_CASE("ícones Nerd Font no modo padrão", "[view]")
{
    tui::ViewState s;
    s.rows = sample();
    s.selected = 0;
    s.iconMode = tui::IconMode::Nerd;
    const std::string out = draw(s);
    CHECK(has(out, ""));   // nf-fa-check_circle: instalado
    CHECK(has(out, ""));   // nf-fa-check_circle_o: dependência
    CHECK(has(out, ""));   // nf-fa-thumb_tack: fixado
    CHECK(has(out, ""));   // nf-fa-folder_open: aba Arquivos
}

TEST_CASE("tela em português", "[view]")
{
    i18n::setLocale("pt_BR");
    tui::ViewState s;
    s.rows = sample();
    s.selected = 0;
    s.hasCurrent = true;
    s.current = s.rows[2];
    s.iconMode = tui::IconMode::Ascii;
    const std::string out = draw(s);
    for (const char *t : {"Nome", "Tipo", "Versão", "Tamanho", "Instalado em", "Informação", "Arquivos", "Saída",
                          "Ajuda", "Instalados", "Fórmula", "Fixado (a versão 1.25.0_1 está disponível)",
                          "Carregando…", "Ações", "Marcar", "Aplicar"})
        CHECK_INFO(out, t);
    i18n::setLocale("en_US");
}

TEST_CASE("enquanto os tamanhos são calculados", "[view]")
{
    i18n::setLocale("en_US");
    tui::ViewState s;
    s.rows = sample();
    s.rows[0].size = 0;
    s.sizing = true;
    s.iconMode = tui::IconMode::Ascii;
    const std::string out = draw(s);
    CHECK(has(out, "Calculating sizes…"));
    CHECK(has(out, "…  20"));   // o "…" na coluna do tamanho, antes da data
}

TEST_CASE("caveats quebram na largura da aba, com o recuo", "[view]")
{
    const auto lines = tui::wrapText("one two three four five six\n\n  indented words go here and wrap", 20);
    REQUIRE(lines.size() >= 4);
    CHECK(lines[0] == "one two three four");
    CHECK(lines[1] == "five six");
    CHECK(lines[2].empty());
    CHECK(lines[3].rfind("  indented", 0) == 0);
    for (const auto &l : lines)
        CHECK(l.size() <= 20);
    CHECK(lines.back().rfind("  ", 0) == 0);
}

TEST_CASE("F6 (ou s) escolhe a coluna e Enter ordena, e a ordem é salva", "[app]")
{
    AppFixture f;
    REQUIRE(f.app.handleEvent(Event::F6));
    CHECK(f.app.viewState().sortChooserOpen);
    for (int i = 0; i < 3; ++i)   // Nome → Tipo → Versão → Tamanho
        f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);
    tui::ViewState s = f.app.viewState();
    CHECK_FALSE(s.sortChooserOpen);
    CHECK(s.sort == model::SortSpec{model::Size, true});
    CHECK(s.rows.front().name == "brotli");
    CHECK(f.app.settings().intValue("sort.column", -1) == model::Size);

    // De novo na mesma coluna: inverte. "s" faz o mesmo que F6 (sem o fn do Mac).
    f.app.handleEvent(Event::Character('s'));
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().sort == model::SortSpec{model::Size, false});
    CHECK(f.app.viewState().rows.front().name == "wget");
    // "(ordem do brew)": volta à ordem original.
    f.app.handleEvent(Event::F6);
    for (int i = 0; i < 8; ++i)
        f.app.handleEvent(Event::ArrowUp);
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().rows.front().name == "curl");
    // Pelo tipo: as fórmulas, depois os casks.
    f.app.handleEvent(Event::F6);
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().sort == model::SortSpec{model::Type, true});
    CHECK(f.app.viewState().rows.back().name == "alacritty");
}

TEST_CASE("seleção sobrevive à ordenação e ao filtro", "[app]")
{
    AppFixture f;
    CHECK(f.selected() == "c:alacritty");
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::ArrowDown);
    CHECK(f.selected() == "f:curl");
    f.app.handleEvent(Event::F6);
    for (int i = 0; i < 3; ++i)
        f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::Return);   // por tamanho
    CHECK(f.selected() == "f:curl");
    f.app.filterText() = "wg";          // curl some: seleciona a primeira
    CHECK(f.selected() == "f:wget");
    f.app.filterText().clear();
    CHECK(f.selected() == "f:wget");
}

TEST_CASE("janela da tabela rola com a seleção", "[app]")
{
    AppFixture f;
    tui::Listing l;
    for (int i = 0; i < 100; ++i)
        l.installed.push_back(installed("p" + std::string(i < 10 ? "0" : "") + std::to_string(i), "1", i));
    f.app.applyListing(l);
    f.app.setTableHeight(10);
    tui::ViewState s = f.app.viewState();
    CHECK(s.rows.size() == 10);
    CHECK(s.rowCount == 100);
    f.app.handleEvent(Event::End);
    s = f.app.viewState();
    CHECK(s.firstRow == 90);
    CHECK(s.rows.back().name == "p99");
    CHECK(s.selected == 9);
    f.app.handleEvent(Event::PageUp);
    s = f.app.viewState();
    CHECK(s.current.name == "p90");
    CHECK(s.firstRow == 90);
    f.app.handleEvent(Event::ArrowUp);
    s = f.app.viewState();
    CHECK(s.firstRow == 89);
    CHECK(s.selected == 0);
}

TEST_CASE("atalhos de busca, abas e foco", "[app]")
{
    AppFixture f;
    CHECK(f.app.handleEvent(Event::Character('/')));
    CHECK(f.app.filterMode());
    CHECK_FALSE(f.app.handleEvent(Event::Character('x')));   // vai para o campo de filtro
    f.app.handleEvent(Event::Escape);
    CHECK_FALSE(f.app.filterMode());
    f.app.handleEvent(Event::Special("\x0c"));                 // Ctrl+L, como no OctoPkg
    CHECK(f.app.filterMode());
    f.app.handleEvent(Event::Special("\x10"));                 // Ctrl+P volta à lista
    CHECK_FALSE(f.app.filterMode());

    f.app.handleEvent(Event::Character('2'));
    CHECK(f.app.viewState().tab == tui::FilesTab);
    f.app.handleEvent(Event::Special("\x1b" "4"));             // Alt+4: Saída
    CHECK(f.app.viewState().tab == tui::OutputTab);
    f.app.handleEvent(Event::Special("\x1b" "6"));             // Alt+6: Ajuda
    CHECK(f.app.viewState().tab == tui::HelpTab);
    f.app.handleEvent(Event::Character('3'));                  // Ações
    CHECK(f.app.viewState().tab == tui::ActionsTab);
    f.app.handleEvent(Event::Special("\x1b" "5"));             // Notícias
    CHECK(f.app.viewState().tab == tui::NewsTab);
    f.app.handleEvent(Event::Return);                          // Enter: aba Informação
    CHECK(f.app.viewState().tab == tui::InfoTab);
    f.app.handleEvent(Event::F1);
    CHECK(f.app.viewState().tab == tui::HelpTab);
    f.app.handleEvent(Event::Character('1'));
    f.app.handleEvent(Event::Character('?'));                  // ? faz o F1 sem o fn
    CHECK(f.app.viewState().tab == tui::HelpTab);

    // Tab: as setas rolam a aba em vez de mover a seleção.
    f.app.handleEvent(Event::Tab);
    CHECK(f.app.viewState().contentFocused);
    const std::string before = f.selected();
    (void)draw(f.app.viewState());
    f.app.handleEvent(Event::ArrowDown);
    CHECK(f.selected() == before);
    f.app.handleEvent(Event::Escape);
    CHECK_FALSE(f.app.viewState().contentFocused);
}

TEST_CASE("Saída lista os desatualizados; erro mantém a última lista", "[app]")
{
    AppFixture f;
    tui::ViewState s = f.app.viewState();
    bool curl = false, wget = false, summary = false;
    for (const auto &l : s.log) {
        curl = curl || has(l, "curl  8.9.1 -> 8.10.0");
        wget = wget || has(l, "wget  1.24.5 -> 1.25.0_1  [Pinned]");
        summary = summary || has(l, "4 package(s) installed: 3 formula(e) and 1 cask(s).");
    }
    CHECK(curl);
    CHECK(wget);
    CHECK(summary);

    tui::Listing bad;
    bad.error = "boom";
    f.app.applyListing(bad);
    s = f.app.viewState();
    CHECK(s.error);
    CHECK(s.status == "ERROR: boom");
    CHECK(s.log.back().find("ERROR: boom") != std::string::npos);
    CHECK(s.totalPackages == 4);
}

// --- Com o brew falso: o caminho completo ---

TEST_CASE("listagem pelo brew, com os tamanhos em disco", "[app][brew]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    tui::ViewState s = f.app->viewState();
    CHECK(s.totalPackages == 6);
    CHECK(s.status == "Ready.");
    CHECK_FALSE(s.sizing);
    std::string out = f.screen();
    for (const char *t : {"foo", "docker", "2.00 KiB", "100.00 Bytes", "6 package(s)", "3 outdated"})
        CHECK_INFO(out, t);
    bool disk = false;
    for (const auto &l : s.log)
        disk = disk || has(l, "Disk usage of the installed packages: 2.10 KiB");
    CHECK(disk);
}

TEST_CASE("detalhes pelo brew, com o Requerido por", "[app][brew][details]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    f.select("f:curl");
    f.app->loadDetails();
    tui::ViewState s = f.app->viewState();
    REQUIRE(s.detailsReady);
    CHECK(s.details.homepage == "https://curl.se");
    CHECK(s.details.files.size() == 2);
    CHECK(s.details.requiredBy.empty());

    // brotli: o brew falso não tem o info dele; o erro aparece na aba.
    f.select("f:brotli");
    f.app->loadDetails();
    s = f.app->viewState();
    CHECK(has(s.detailsError, "No available formula with the name \"brotli\""));
    CHECK(has(f.screen(), "ERROR: No available formula"));
}

TEST_CASE("visão Repositório pelo cache da API", "[app][brew][remote]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    f.app->handleEvent(Event::Character('i'));
    tui::ViewState s = f.app->viewState();
    CHECK(s.remoteView);
    CHECK(s.totalPackages == 9);
    const std::string out = f.screen();
    for (const char *t : {"[@ Repository]", " - vim", " - kitty", " N alacritty", "9 package(s)"})
        CHECK_INFO(out, t);
    // Fórmula e cask de mesmo nome: duas linhas, e a seleção distingue.
    f.select("c:docker");
    CHECK(f.app->viewState().current.installed);
    CHECK(f.app->viewState().current.status == Status::Outdated);
    f.select("f:docker");
    CHECK_FALSE(f.app->viewState().current.installed);
    // O tamanho do instalado vale também nesta visão.
    f.select("f:curl");
    CHECK(f.app->viewState().current.size == 2048);
    CHECK(f.app->viewState().current.version == "8.10.0");

    // Alt+I volta aos instalados sem chamar o brew de novo.
    const size_t calls = f.brew.calls().size();
    f.app->handleEvent(Event::Special("\x1b" "i"));
    CHECK_FALSE(f.app->viewState().remoteView);
    CHECK(f.brew.calls().size() == calls);
}

TEST_CASE("sem o cache da API, a visão Repositório avisa e volta aos instalados", "[app][brew][remote]")
{
    i18n::setLocale("en_US");
    FakeBrew brew;   // sem writeApiCache()
    TempDir dir;
    tui::App app(options(dir, brew.path));
    app.refresh();
    app.handleEvent(Event::Character('i'));
    const tui::ViewState s = app.viewState();
    CHECK_FALSE(s.remoteView);
    CHECK(s.error);
    CHECK(has(s.status, "brew update"));
    CHECK(s.totalPackages == 6);
}

TEST_CASE("menus Arquivo, Busca e Ajuda, e o Sobre", "[app][menu]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    f.app->handleEvent(Event::F9);
    REQUIRE(f.app->dialog().kind == tui::Dialog::Menu);
    std::string out = f.screen();
    const std::vector<std::string> expected{"File", "Search", "Actions", "Help", "Refresh the package list",
                                            "Update Homebrew", tui::keyLabel("Ctrl+K"), "Install updates",
                                            "Clean the local cache", "Remove unneeded dependencies",
                                            "Restore a snapshot…", "Preferences…", "Exit"};
    for (const auto &t : expected)
        CHECK_INFO(out, t);
    // Instalados/Repositório pelo menu.
    f.app->handleEvent(Event::ArrowDown);
    f.app->handleEvent(Event::Return);
    CHECK(f.app->viewState().remoteView);

    f.app->handleEvent(Event::Special("\x1b" "j"));   // Alt+J: Ajuda
    REQUIRE(f.app->dialog().menu == 4);               // Arquivo, Busca, Ações, Homebrew, Ajuda
    f.app->handleEvent(Event::ArrowDown);
    f.app->handleEvent(Event::Return);
    REQUIRE(f.app->dialog().kind == tui::Dialog::Message);
    CHECK(f.app->dialog().title == "About");
    CHECK(has(f.app->dialog().message, std::string("octobrew-tui ") + OCTOBREW_TUI_VERSION));
    CHECK(has(f.app->dialog().message, "Homebrew 7.0.10"));
    f.app->handleEvent(Event::Return);
    CHECK(f.app->dialog().kind == tui::Dialog::None);
}

// --- Temas, preferências e busca nas abas (portados do octopkg-tui) ---

namespace {

// A tela com as sequências de cor, para conferir as cores do tema.
std::string drawRaw(const tui::ViewState &s, int width = 120, int height = 40)
{
    Terminal::SetColorSupport(Terminal::Color::TrueColor);
    auto screen = Screen::Create(Dimension::Fixed(width), Dimension::Fixed(height));
    Render(screen, tui::render(s, text("")));
    return screen.ToString();
}

} // namespace

TEST_CASE("os temas do bectl-gui, em ciclo", "[theme]")
{
    const auto &list = tui::themes();
    REQUIRE(list.size() == 15);
    CHECK(std::string(list.front().id) == "system");
    std::set<std::string> ids;
    for (const auto &t : list)
        ids.insert(t.id);
    CHECK(ids.size() == list.size());
    CHECK(std::string(tui::theme("nao-existe").id) == "system");
    CHECK(std::string(tui::nextTheme("system").id) == "octopkg");
    CHECK(std::string(tui::nextTheme("one_dark").id) == "system");
}

TEST_CASE("tema pinta o fundo, o texto e a seleção", "[theme][view]")
{
    i18n::setLocale("en_US");
    tui::ViewState s;
    s.rows = sample();
    s.selected = 0;
    s.iconMode = tui::IconMode::Ascii;
    s.themeId = "nord";
    const std::string raw = drawRaw(s);
    CHECK(raw.find("48;2;46;52;64") != std::string::npos);    // fundo #2e3440
    CHECK(raw.find("38;2;216;222;233") != std::string::npos); // texto #d8dee9
    CHECK(raw.find("48;2;129;161;193") != std::string::npos); // seleção #81a1c1
    CHECK(draw(s).find("Theme: Nord") != std::string::npos);

    s.themeId = "system";
    const std::string plain = drawRaw(s);
    CHECK(plain.find("48;2;") == std::string::npos);          // sem cores próprias
}

TEST_CASE("Alt+T troca o tema e o salva", "[theme][app]")
{
    AppFixture f;
    CHECK(f.app.viewState().themeId == "system");
    f.app.handleEvent(Event::Special("\x1b" "t"));
    CHECK(f.app.viewState().themeId == "octopkg");
    CHECK(f.app.settings().value("theme") == "octopkg");
    for (int i = 0; i < 14; ++i)
        f.app.handleEvent(Event::Special("\x1b" "t"));
    CHECK(f.app.viewState().themeId == "system");
}

TEST_CASE("Preferências: tema, idioma e ícones, aplicados e salvos", "[app][preferences]")
{
    AppFixture f;
    f.app.handleEvent(Event::Special("\x1b" ","));   // Alt+,
    REQUIRE(f.app.viewState().dialog.kind == tui::Dialog::Form);
    const tui::Dialog d = f.app.viewState().dialog;
    CHECK(d.title == "Preferences");
    REQUIRE(d.fields.size() == 3);
    CHECK(d.fields[0].choices.size() == 15);
    CHECK(d.focus == 0);

    for (int i = 0; i < 12; ++i)   // Tema: system → … → nord
        f.app.handleEvent(Event::ArrowRight);
    f.app.handleEvent(Event::Tab);                   // Idioma
    f.app.handleEvent(Event::ArrowRight);            // en_US
    f.app.handleEvent(Event::ArrowRight);            // pt_BR
    f.app.handleEvent(Event::Tab);                   // Ícones
    f.app.handleEvent(Event::ArrowLeft);             // ASCII → Unicode
    f.app.handleEvent(Event::Return);

    const tui::ViewState s = f.app.viewState();
    CHECK(s.dialog.kind == tui::Dialog::None);
    CHECK(s.themeId == "nord");
    CHECK(s.iconMode == tui::IconMode::Unicode);
    CHECK(i18n::locale() == "pt_BR");
    CHECK(s.status == "Preferências salvas.");
    // Relido do arquivo, numa nova App.
    tui::Options o;
    o.brewPath = f.dir.file("brew-nao-usado");
    o.settingsPath = f.dir.file("config");
    tui::App again(o);
    CHECK(again.viewState().themeId == "nord");
    CHECK(again.viewState().iconMode == tui::IconMode::Unicode);
    i18n::setLocale("en_US");
}

TEST_CASE("Ctrl+F busca nas abas Arquivos, Saída e Ajuda", "[app][find]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::ArrowDown);   // curl
    octo::Details d = curlDetails();
    d.files = {"/opt/homebrew/Cellar/curl/8.9.1/bin/curl", "/opt/homebrew/Cellar/curl/8.9.1/include/curl/curl.h",
               "/opt/homebrew/Cellar/curl/8.9.1/lib/libcurl.4.dylib",
               "/opt/homebrew/Cellar/curl/8.9.1/share/man/man1/curl.1"};
    f.app.applyDetails(f.app.currentDetailsKey(), d, "");

    // Na aba Informação, não há onde buscar.
    f.app.handleEvent(Event::Special("\x06"));
    CHECK_FALSE(f.app.viewState().findOpen);
    CHECK(f.app.viewState().status == "Find works in the Files, Output, News and Help tabs.");

    f.app.handleEvent(Event::Character('2'));
    f.app.handleEvent(Event::Special("\x06"));
    REQUIRE(f.app.viewState().findOpen);
    for (char c : std::string("LIB/"))   // sem diferenciar maiúsculas
        f.app.handleEvent(Event::Character(c));
    tui::ViewState s = f.app.viewState();
    CHECK(s.findCount == 1);
    CHECK(s.findMatch == 2);
    CHECK(draw(s).find("1/1") != std::string::npos);
    for (int i = 0; i < 4; ++i)
        f.app.handleEvent(Event::Backspace);
    for (char c : std::string("/curl."))
        f.app.handleEvent(Event::Character(c));
    s = f.app.viewState();
    CHECK(s.findCount == 2);   // curl.h e curl.1
    f.app.handleEvent(Event::F3);
    CHECK(f.app.viewState().findMatch == 3);
    f.app.handleEvent(Event::Special("\x1b[1;2R"));   // Shift+F3
    CHECK(f.app.viewState().findMatch == 1);
    // q é texto da busca, não sair.
    CHECK(f.app.handleEvent(Event::Character('q')));
    CHECK(f.app.viewState().findText == "/curl.q");
    f.app.handleEvent(Event::Escape);
    CHECK_FALSE(f.app.viewState().findOpen);

    // Na Ajuda, encontra o próprio atalho.
    f.app.handleEvent(Event::F1);
    f.app.handleEvent(Event::Special("\x06"));
    for (char c : tui::keyLabel("Ctrl+F"))   // "⌃F" no macOS: os bytes do UTF-8 entram um a um
        f.app.handleEvent(Event::Character(std::string(1, c)));
    CHECK(f.app.viewState().findCount >= 1);
}

TEST_CASE("F7 (ou Alt+D) alterna a busca por nome e por descrição, e a escolha fica salva", "[app][search]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.filterText() = "comment";   // sample(): comment = nome + " comment"
    CHECK(f.app.viewState().rowCount == 0);

    f.app.handleEvent(Event::F7);
    tui::ViewState s = f.app.viewState();
    CHECK(s.searchMode == model::SearchMode::Description);
    CHECK(s.status == "Search by description (name and description).");
    CHECK(f.app.settings().value("search") == "description");
    CHECK(s.rowCount == 4);
    f.app.filterText() = "wget comment";
    s = f.app.viewState();
    REQUIRE(s.rowCount == 1);
    CHECK(s.current.name == "wget");
    CHECK(draw(s).find("Description:") != std::string::npos);

    f.app.handleEvent(Event::Special("\x1b" "d"));   // Alt+D: o F7 sem o fn
    CHECK(f.app.viewState().searchMode == model::SearchMode::Name);
    CHECK(f.app.viewState().rowCount == 0);
    CHECK(f.app.settings().value("search") == "name");
}

TEST_CASE("menu Busca, com o modo atual marcado", "[app][search][menu]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.handleEvent(Event::Special("\x1b" "b"));   // Alt+B
    REQUIRE(f.app.viewState().dialog.kind == tui::Dialog::Menu);
    CHECK(f.app.viewState().dialog.menu == 1);
    CHECK(draw(f.app.viewState()).find("• By name") != std::string::npos);
    f.app.handleEvent(Event::ArrowDown);   // Por descrição
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().searchMode == model::SearchMode::Description);
    f.app.handleEvent(Event::Special("\x1b" "b"));
    CHECK(draw(f.app.viewState()).find("• By description") != std::string::npos);
    f.app.handleEvent(Event::Escape);
}

TEST_CASE("F8 (ou t) filtra pelo tap, que combina com o filtro", "[app][category]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    tui::Listing l = listing();
    l.installed[1].tap = "someone/tap";   // brotli
    f.app.applyListing(l);
    f.app.handleEvent(Event::F8);
    tui::ViewState s = f.app.viewState();
    REQUIRE(s.categoryChooserOpen);
    CHECK(s.categoryIndex == 0);   // (todos), o atual
    CHECK(s.categories.size() == 3);
    std::string out = draw(s);
    for (const char *t : {" Tap ", "• (all taps)", "homebrew/cask", "homebrew/core", "someone/tap", "1/4"})
        CHECK_INFO(out, t);

    f.app.handleEvent(Event::Character('s'));   // salta para someone/tap
    CHECK(f.app.viewState().categoryIndex == 3);
    f.app.handleEvent(Event::Character('h'));   // e para homebrew/cask
    CHECK(f.app.viewState().categoryIndex == 1);
    f.app.handleEvent(Event::Character('h'));   // homebrew/core
    f.app.handleEvent(Event::Return);
    s = f.app.viewState();
    CHECK_FALSE(s.categoryChooserOpen);
    CHECK(s.category == "homebrew/core");
    CHECK(s.status == "Tap: homebrew/core");
    CHECK(s.rowCount == 2);   // curl e wget
    CHECK(draw(s).find("[# Installed packages · homebrew/core]") != std::string::npos);
    f.app.filterText() = "wg";
    CHECK(f.app.viewState().rowCount == 1);
    f.app.filterText().clear();

    // De novo, pelo t: o atual vem marcado; Home e Enter voltam a todos.
    f.app.handleEvent(Event::Character('t'));
    s = f.app.viewState();
    CHECK(s.categoryIndex == 2);
    f.app.handleEvent(Event::Home);
    f.app.handleEvent(Event::Return);
    CHECK(f.app.viewState().category.empty());
    CHECK(f.app.viewState().rowCount == 4);

    // Sem pacotes, a lista de taps fica vazia e não quebra.
    AppFixture empty;
    empty.app.applyListing(tui::Listing{});
    empty.app.handleEvent(Event::F8);
    REQUIRE(empty.app.viewState().categoryChooserOpen);
    empty.app.handleEvent(Event::Character('x'));
    empty.app.handleEvent(Event::Escape);
    CHECK_FALSE(empty.app.viewState().categoryChooserOpen);
}

TEST_CASE("F11 e Alt+Z: só a lista, só as abas, e as duas", "[app][layout]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.handleEvent(Event::ArrowDown);
    f.app.handleEvent(Event::ArrowDown);   // curl
    f.app.applyDetails(f.app.currentDetailsKey(), curlDetails(), "");

    f.app.handleEvent(Event::F11);
    tui::ViewState s = f.app.viewState();
    CHECK(s.layout == tui::Layout::ListOnly);
    std::string out = draw(s);
    CHECK(has(out, "brotli"));
    CHECK_FALSE(has(out, "1 i Info"));    // sem as abas
    f.app.handleEvent(Event::ArrowDown);  // as setas movem a seleção
    CHECK(f.app.viewState().current.name == "wget");
    f.app.handleEvent(Event::ArrowUp);

    f.app.handleEvent(Event::Special("\x1b" "z"));   // Alt+Z
    s = f.app.viewState();
    CHECK(s.layout == tui::Layout::TabsOnly);
    CHECK(s.contentFocused);
    out = draw(s);
    CHECK(has(out, "https://curl.se"));
    CHECK_FALSE(has(out, "Name ^"));      // sem a lista
    f.app.handleEvent(Event::ArrowDown);
    CHECK(f.app.viewState().current.name == "curl");

    f.app.handleEvent(Event::F11);
    CHECK(f.app.viewState().layout == tui::Layout::Normal);
    CHECK_FALSE(f.app.viewState().contentFocused);
}

// --- v0.2.0: marcas, transações, diálogos e menus ---

TEST_CASE("+ e - marcam, como no OctoPkg, e Ctrl+E desfaz", "[app][actions]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    // + num instalado em dia não faz nada; no desatualizado (curl), marca.
    f.select("f:brotli");
    f.app->handleEvent(Event::Character('+'));
    CHECK(f.app->viewState().toInstall.empty());
    CHECK(f.app->viewState().status == "brotli is already installed and up to date.");
    f.select("f:curl");
    f.app->handleEvent(Event::Character('+'));
    CHECK(f.app->viewState().toInstall == std::set<std::string>{"f:curl"});
    // Fixado: nem + nem -.
    f.select("f:wget");
    f.app->handleEvent(Event::Character('+'));
    f.app->handleEvent(Event::Character('-'));
    CHECK(f.app->viewState().status == "wget is pinned.");
    // - no cask; de novo, desmarca; Delete faz o mesmo.
    f.select("c:alacritty");
    f.app->handleEvent(Event::Character('-'));
    f.app->handleEvent(Event::Character('-'));
    CHECK(f.app->viewState().toRemove.empty());
    f.app->handleEvent(Event::Delete);
    CHECK(f.app->viewState().toRemove == std::set<std::string>{"c:alacritty"});
    // + e - se excluem.
    f.select("f:curl");
    f.app->handleEvent(Event::Character('-'));
    CHECK(f.app->viewState().toRemove.count("f:curl"));
    CHECK(f.app->viewState().toInstall.empty());

    std::string out = f.screen();
    for (const char *t : {" x alacritty", " x curl", "-2", "3 a Actions (2)"})
        CHECK_INFO(out, t);
    f.app->handleEvent(Event::Character('3'));
    out = f.screen();
    for (const std::string &t : {std::string("To be removed (2)"), std::string("alacritty (cask)"), std::string("curl"),
                                 tui::keyText("Alt+M: apply")})
        CHECK_INFO(out, t);

    // Na visão Repositório, - num não instalado é recusado, e + marca.
    f.app->handleEvent(Event::Character('i'));
    f.select("f:vim");
    f.app->handleEvent(Event::Character('-'));
    CHECK(f.app->viewState().status == "Only installed packages can be removed.");
    f.app->handleEvent(Event::Insert);
    CHECK(f.app->viewState().toInstall == std::set<std::string>{"f:vim"});
    CHECK(has(f.screen(), " + vim"));

    f.app->handleEvent(Event::Special("\x05"));   // Ctrl+E
    CHECK(f.app->viewState().toInstall.empty());
    CHECK(f.app->viewState().toRemove.empty());
}

TEST_CASE("Alt+M mostra a prévia, e OK tira o snapshot, remove e instala", "[app][actions]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    f.brew.preview("install", kInstallCaskDryRun);
    f.app->handleEvent(Event::Character('i'));
    f.select("c:kitty");
    f.app->handleEvent(Event::Character('+'));
    f.select("c:alacritty");
    f.app->handleEvent(Event::Character('-'));
    f.app->handleEvent(Event::Special("\x1b" "m"));   // Alt+M
    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    CHECK(f.dialog().title == "Confirmation");
    CHECK_FALSE(f.dialog().danger);
    const std::string msg = f.dialog().message;
    CHECK(has(msg, "The following package needs to be retrieved\n  • kitty"));
    CHECK(has(msg, "The following package will be removed\n  • alacritty (cask)"));
    REQUIRE(f.dialog().fields.size() == 1);
    CHECK(f.dialog().fields[0].checked);   // instala algo: o snapshot vem marcado
    CHECK(has(f.dialog().fields[0].hint, f.snapshots()));
    CHECK(has(f.screen(), "[x] Save a snapshot (Brewfile) before the transaction"));
    CHECK(f.brew.operations().empty());   // até aqui, só a prévia

    f.app->handleEvent(Event::Return);   // OK
    const auto ops = f.brew.operations();
    REQUIRE(ops.size() == 3);
    CHECK(ops[0].rfind("bundle|dump|--file=" + f.snapshots() + "/", 0) == 0);
    CHECK(ops[1] == "uninstall|--cask|alacritty");
    CHECK(ops[2] == "install|--no-ask|--cask|kitty");
    CHECK(octo::listSnapshots(f.snapshots()).size() == 1);
    CHECK(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().title == "Command finished OK!");
    CHECK(has(f.dialog().message, "Installed: kitty (cask)"));
    tui::ViewState s = f.app->viewState();
    CHECK(s.toInstall.empty());
    CHECK(s.toRemove.empty());
    CHECK(s.tab == tui::OutputTab);   // a Saída acompanha a transação
    bool command = false, output = false;
    for (const auto &l : s.log) {
        command = command || has(l, "brew uninstall --cask alacritty; brew install --cask kitty");
        output = output || has(l, "executando install");
    }
    CHECK(command);
    CHECK(output);
    // As operações rodam sem pergunta, sem autoremove e sem o terminal.
    bool env = false;
    for (const auto &e : readLines(f.brew.log + ".env"))
        env = env || has(e, "|1|1|/usr/bin/false");
    CHECK(env);
}

TEST_CASE("remoção que leva dependentes: Aviso, foco em Cancelar, sem snapshot", "[app][actions]")
{
    i18n::setLocale("en_US");
    BrewFixture f(true);
    f.select("f:brotli");
    f.app->handleEvent(Event::Character('-'));
    f.app->handleEvent(Event::Special("\x1b" "m"));
    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    CHECK(f.dialog().title == "Warning");
    CHECK(f.dialog().danger);
    CHECK(f.dialog().focus == 2);   // Cancelar (o campo do snapshot é o 0)
    CHECK_FALSE(f.dialog().fields[0].checked);
    CHECK(has(f.dialog().message, "The following 4 packages will be removed"));
    CHECK(has(f.dialog().message, "(3 of them depend on the marked packages)"));
    CHECK(has(f.dialog().message, "docker (cask)"));
    // Enter no Cancelar não faz nada.
    f.app->handleEvent(Event::Return);
    CHECK(f.brew.operations().empty());
    CHECK(f.app->viewState().toRemove.size() == 1);

    f.app->handleEvent(Event::Special("\x1b" "m"));
    f.app->handleEvent(Event::TabReverse);   // OK
    f.app->handleEvent(Event::Return);
    CHECK(f.brew.operations() == std::vector<std::string>{"uninstall|--cask|docker",
                                                          "uninstall|--formula|brotli|curl|wget"});
}

TEST_CASE("dependente fixado ou protegido impede a remoção", "[app][actions]")
{
    i18n::setLocale("en_US");
    BrewFixture f;   // o wget, fixado, depende do brotli
    f.select("f:brotli");
    f.app->handleEvent(Event::Character('-'));
    f.app->handleEvent(Event::Special("\x1b" "m"));
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().title == "Error");
    CHECK(f.dialog().message == "wget depends on a package marked for removal and cannot be removed.");
    CHECK(f.brew.operations().empty());
}

TEST_CASE("falhas: sem marcas, prévia que falha e operação que falha", "[app][actions]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    f.app->handleEvent(Event::Special("\x1b" "m"));
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().message == "No packages marked. Use + to install and - to remove.");
    f.app->handleEvent(Event::Return);

    f.select("f:curl");
    f.app->handleEvent(Event::Character('+'));
    f.brew.failNext("install-dry");
    f.app->handleEvent(Event::Special("\x1b" "m"));
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().title == "Error");
    CHECK(f.dialog().message == "install-dry falhou");
    f.app->handleEvent(Event::Return);

    ::unlink(f.brew.dir.file("install-dry.rc").c_str());
    f.brew.failNext("install");
    f.app->handleEvent(Event::Special("\x1b" "m"));
    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    f.app->handleEvent(Event::ArrowUp);          // do OK para o campo do snapshot
    f.app->handleEvent(Event::Character(' '));   // sem o snapshot
    CHECK_FALSE(f.dialog().fields[0].checked);
    f.app->handleEvent(Event::Return);
    CHECK(f.dialog().title == "Error");
    CHECK(f.dialog().message == "install falhou");
    const tui::ViewState s = f.app->viewState();
    CHECK(s.error);
    CHECK(s.status == "ERROR: install falhou");   // a nova listagem não esconde o erro
    CHECK(s.toInstall.size() == 1);               // as marcas ficam
    CHECK(f.brew.operations() == std::vector<std::string>{"install|--no-ask|--formula|curl"});
}

TEST_CASE("cask que pede o sudo: avisa e, sem a senha em cache, usa o terminal", "[app][actions][sudo]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    std::string info = kPkgCaskInfo;
    info.replace(info.find("\"zoom\", \"full_token\": \"zoom\""), 28, "\"alacritty\", \"full_token\": \"alacritty\"");
    writeFile(f.brew.dir.file("alacritty.json"), info);
    writeFile(f.brew.dir.file("sudo.rc"), "1");   // o sudo pediria a senha
    f.select("c:alacritty");
    f.app->handleEvent(Event::Character('-'));
    f.app->handleEvent(Event::Special("\x1b" "m"));
    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    CHECK(has(f.dialog().message, "Needs sudo: alacritty"));
    CHECK(has(f.dialog().message, "The screen will be suspended while sudo asks for your password."));
    f.app->handleEvent(Event::Return);
    CHECK(f.brew.operations() == std::vector<std::string>{"uninstall|--cask|alacritty"});
    // Com o terminal, o brew roda sem o SUDO_ASKPASS: o sudo pode pedir a senha.
    const auto env = readLines(f.brew.log + ".env");
    bool interactive = false;
    for (const auto &e : env)
        interactive = interactive || (has(e, "|1|1|") && e.back() == '|');
    CHECK(interactive);
}

TEST_CASE("Ctrl+U atualiza o sistema; Ctrl+K atualiza o Homebrew", "[app][actions]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    f.app->handleEvent(Event::Special("\x15"));   // Ctrl+U, sem nada a atualizar
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().message == "There are no new updates available!");
    f.app->handleEvent(Event::Return);

    f.brew.preview("upgrade", kUpgradeDryRun);
    f.app->handleEvent(Event::Special("\x15"));
    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    CHECK(f.dialog().title == "System upgrade");
    CHECK(has(f.dialog().message, "The following 2 packages need to be retrieved"));
    CHECK(has(f.dialog().message, "curl 8.9.1 -> 8.10.0"));
    CHECK(has(f.dialog().message, "Pinned packages are not upgraded."));
    CHECK(f.dialog().fields[0].checked);
    f.app->handleEvent(Event::Return);
    auto ops = f.brew.operations();
    REQUIRE(ops.size() == 2);
    CHECK(ops[0].rfind("bundle|dump|", 0) == 0);
    CHECK(ops[1] == "upgrade|--no-ask");
    f.app->handleEvent(Event::Return);

    f.app->handleEvent(Event::Special("\x0b"));   // Ctrl+K
    CHECK(f.brew.operations().back() == "update");
    CHECK(f.app->viewState().status == "Ready.");
}

TEST_CASE("menus: limpar o cache, remover as dependências desnecessárias, fixar", "[app][menu][actions]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    const auto file = [&](int downs) {
        f.app->handleEvent(Event::F9);
        for (int i = 0; i < downs; ++i)
            f.app->handleEvent(Event::ArrowDown);
        f.app->handleEvent(Event::Return);
    };
    // Atualizar, Instalados/Repositório, (sep), Atualizar o Homebrew, Instalar as atualizações, Limpar
    f.brew.preview("cleanup", kCleanupDryRun);
    file(4);
    REQUIRE(f.dialog().kind == tui::Dialog::Confirm);
    CHECK(has(f.dialog().message, "brew cleanup would remove 2 item(s)"));
    CHECK(has(f.dialog().message, "It would free approximately 6.2MB."));
    f.app->handleEvent(Event::Character('y'));
    CHECK(f.brew.operations().back() == "cleanup|--prune=all");

    file(5);   // Remover as dependências desnecessárias, sem nenhuma
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().message == "There are no unneeded dependencies.");
    f.app->handleEvent(Event::Return);
    f.brew.preview("autoremove", kAutoremoveDryRun);
    file(5);
    REQUIRE(f.dialog().kind == tui::Dialog::Confirm);
    CHECK(has(f.dialog().message, "The following 2 packages will be removed\n  • libgpg-error\n  • lzlib"));
    f.app->handleEvent(Event::Character('n'));   // não
    CHECK(f.brew.operations().back() == "cleanup|--prune=all");
    file(5);
    f.app->handleEvent(Event::Return);           // Sim (o foco)
    CHECK(f.brew.operations().back() == "autoremove");

    // Ações → Fixar e Liberar, no pacote selecionado.
    const auto actions = [&](int downs) {
        f.app->handleEvent(Event::Special("\x1b" "c"));
        REQUIRE(f.dialog().menu == 2);
        for (int i = 0; i < downs; ++i)
            f.app->handleEvent(Event::ArrowDown);
        f.app->handleEvent(Event::Return);
    };
    f.select("f:curl");
    actions(4);   // Marcar, Marcar, (sep), Aplicar, Cancelar, (sep), Fixar
    CHECK(f.brew.operations().back() == "pin|--formula|curl");
    f.select("f:wget");
    actions(4);
    CHECK(f.app->viewState().status == "wget is pinned.");
    actions(5);   // Liberar
    CHECK(f.brew.operations().back() == "unpin|--formula|wget");
}

TEST_CASE("restaurar um snapshot", "[app][menu][actions]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    const auto restore = [&] {
        f.app->handleEvent(Event::F9);
        for (int i = 0; i < 6; ++i)   // … Remover as dependências, (sep), Restaurar
            f.app->handleEvent(Event::ArrowDown);
        f.app->handleEvent(Event::Return);
    };
    restore();
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(has(f.dialog().message, "No snapshots in " + f.snapshots()));
    f.app->handleEvent(Event::Return);

    (void)std::system(("mkdir -p '" + f.snapshots() + "'").c_str());
    writeFile(f.snapshots() + "/261020-080000.Brewfile", "brew \"curl\"\n");
    writeFile(f.snapshots() + "/261024-090000.Brewfile", "cask \"kitty\"\n");
    restore();
    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    CHECK(f.dialog().fields[0].choices == std::vector<std::string>{"261024-090000.Brewfile", "261020-080000.Brewfile"});
    f.app->handleEvent(Event::ArrowRight);   // o mais antigo
    f.app->handleEvent(Event::Tab);          // OK
    f.app->handleEvent(Event::Return);
    CHECK(f.brew.operations().back()
          == "bundle|install|--file=" + f.snapshots() + "/261020-080000.Brewfile|--no-upgrade");
    CHECK(f.dialog().title == "Command finished OK!");
}

TEST_CASE("diálogo da transação em português", "[view][actions]")
{
    BrewFixture f;
    i18n::setLocale("pt_BR");   // depois da App, que começa em en_US (a opção --lang dos testes)
    f.select("f:curl");
    f.app->handleEvent(Event::Character('+'));
    f.app->handleEvent(Event::Special("\x1b" "m"));
    const std::string out = f.screen();
    for (const char *t : {"Confirmação", "Confirma?", "[x] Salvar um snapshot (Brewfile) antes da transação",
                          "[ OK ]", "[ Cancelar ]", "Marcar", "Aplicar"})
        CHECK_INFO(out, t);
    i18n::setLocale("en_US");
}

// --- v0.3.0: aba Notícias ---

TEST_CASE("aba Notícias: cache na hora, Ctrl+G baixa de novo, e a busca funciona", "[app][news]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    writeFile(f.dir.file("feed.xml"), kAtomFeed);
    writeExecutable(f.dir.file("curl.sh"), "#!/bin/sh\necho \"$*\" >> '" + f.dir.file("curl.log") + "'\n"
                                           "while [ $# -gt 1 ]; do [ \"$1\" = -o ] && out=$2; shift; done\n"
                                           "cp '" + f.dir.file("feed.xml") + "' \"$out\"\n");
    f.app.handleEvent(Event::Character('5'));
    tui::ViewState s = f.app.viewState();
    CHECK(s.tab == tui::NewsTab);
    REQUIRE(s.news.size() == 2);   // sem tela, o download roda na hora
    CHECK(readLines(f.dir.file("curl.log")).size() == 1);
    const std::string out = draw(s);
    for (const char *t : {"Homebrew news", "Homebrew 7.0.0  —  2026-09-13", "https://brew.sh/2026/09/13/homebrew-7.0.0/",
                          "proud to announce Homebrew 7.0.0", "new git signing key  —  2025-01-01"})
        CHECK_INFO(out, t);
    // A descrição é quebrada na largura da aba.
    s.contentWidth = 40;
    for (const auto &l : tui::tabText(s))
        if (l.rfind("https://", 0) != 0 && l.find("  —  ") == std::string::npos)
            CHECK(l.size() <= 42);

    // Voltar à aba não baixa de novo; Ctrl+G, sim.
    f.app.handleEvent(Event::Character('1'));
    f.app.handleEvent(Event::Character('5'));
    CHECK(readLines(f.dir.file("curl.log")).size() == 1);
    f.app.handleEvent(Event::Special("\x07"));   // Ctrl+G
    CHECK(readLines(f.dir.file("curl.log")).size() == 2);

    f.app.handleEvent(Event::Special("\x06"));   // Ctrl+F
    for (char c : std::string("sandboxing"))
        f.app.handleEvent(Event::Character(c));
    CHECK(f.app.viewState().findCount == 1);
    f.app.handleEvent(Event::Escape);
    // O cache ficou ao lado das preferências.
    CHECK(has(readFile(f.dir.file("news.xml")), "homebrew-7.0.0"));
}

TEST_CASE("aba Notícias sem rede mostra o aviso; o menu Arquivo busca de novo", "[app][news]")
{
    i18n::setLocale("en_US");
    AppFixture f;   // curl.sh não existe: sem rede
    f.app.handleEvent(Event::Special("\x1b" "5"));
    tui::ViewState s = f.app.viewState();
    CHECK(s.news.empty());
    CHECK_FALSE(s.newsError.empty());
    std::string out = draw(s);
    CHECK(has(out, "WARNING: Failed to start"));
    CHECK(has(out, tui::keyText("No news yet. Ctrl+G gets the latest news.")));

    // Pelo menu Arquivo, de outra aba: o menu leva à aba e baixa o feed.
    writeFile(f.dir.file("feed.xml"), kAtomFeed);
    writeExecutable(f.dir.file("curl.sh"), "#!/bin/sh\nwhile [ $# -gt 1 ]; do [ \"$1\" = -o ] && out=$2; shift; done\n"
                                           "cp '" + f.dir.file("feed.xml") + "' \"$out\"\n");
    f.app.handleEvent(Event::Character('1'));
    f.app.handleEvent(Event::F9);
    for (int i = 0; i < 7; ++i)   // … Restaurar um snapshot, Buscar as últimas notícias
        f.app.handleEvent(Event::ArrowDown);
    CHECK(has(draw(f.app.viewState()), "Get the latest news"));
    f.app.handleEvent(Event::Return);
    s = f.app.viewState();
    CHECK(s.tab == tui::NewsTab);   // o menu leva à aba
    CHECK(s.news.size() == 2);
    CHECK(s.newsError.empty());
}

// --- v0.4.0: serviços, folhas, taps, doctor e Brewfiles ---

namespace {

// Abre o menu Homebrew (Alt+H) e escolhe o item downs setas abaixo.
// Serviços, Iniciar, Parar, Reiniciar, (sep), Adicionar tap, Remover tap,
// (sep), Exportar Brewfile, Instalar de Brewfile, (sep), brew doctor.
void brewMenu(BrewFixture &f, int downs)
{
    f.app->handleEvent(Event::Special("\x1b" "h"));
    REQUIRE(f.dialog().kind == tui::Dialog::Menu);
    REQUIRE(f.dialog().menu == 3);
    for (int i = 0; i < downs; ++i)
        f.app->handleEvent(Event::ArrowDown);
    f.app->handleEvent(Event::Return);
}

void type(BrewFixture &f, const std::string &text, size_t erase = 0)
{
    for (size_t i = 0; i < erase; ++i)
        f.app->handleEvent(Event::Backspace);
    for (char c : text)
        f.app->handleEvent(Event::Character(c));
}

} // namespace

TEST_CASE("aba Serviços (7) e o serviço na aba Informação", "[app][services]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    writeFile(f.brew.dir.file("services.json"), kServicesJson);
    f.app->handleEvent(Event::Character('7'));
    tui::ViewState s = f.app->viewState();
    CHECK(s.tab == tui::ServicesTab);
    REQUIRE(s.services.size() == 3);
    std::string out = f.screen();
    for (const char *t : {"7 s Services", "foo", "Started", "cleber", "php", "Not running", "postgresql@17", "Error",
                          "(exit code 78)"})
        CHECK_INFO(out, t);
    CHECK(has(out, tui::keyText("Homebrew menu (Alt+H)")));
    // Ctrl+F busca na aba.
    f.app->handleEvent(Event::Special("\x06"));
    type(f, "postgres");
    CHECK(f.app->viewState().findCount == 1);
    f.app->handleEvent(Event::Escape);

    f.select("f:someone/tap/foo");
    f.app->handleEvent(Event::Character('1'));
    out = f.screen();
    CHECK(has(out, "Service:"));
    CHECK(has(out, "Started (cleber)"));
}

TEST_CASE("iniciar, parar e reiniciar o serviço da fórmula selecionada", "[app][services]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    f.select("f:curl");
    brewMenu(f, 1);   // Iniciar
    CHECK(f.app->viewState().status == "curl has no service.");
    f.select("c:alacritty");
    brewMenu(f, 1);
    CHECK(f.app->viewState().status == "Select an installed formula.");
    CHECK(f.brew.operations().empty());

    f.select("f:someone/tap/foo");
    brewMenu(f, 1);
    brewMenu(f, 2);
    brewMenu(f, 3);
    CHECK(f.brew.operations() == std::vector<std::string>{"services|start|foo", "services|stop|foo", "services|restart|foo"});
    // Depois de cada um, a lista dos serviços é relida.
    int lists = 0;
    for (const auto &c : f.brew.calls())
        lists += c == "services|list|--json";
    CHECK(lists == 3);
}

TEST_CASE("l mostra só as folhas, como o brew leaves", "[app][leaves]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    CHECK(f.app->viewState().rowCount == 6);
    f.app->handleEvent(Event::Character('l'));
    tui::ViewState s = f.app->viewState();
    CHECK(s.leavesOnly);
    CHECK(s.rowCount == 3);   // curl, wget e foo: o brotli é dependência, e os casks não contam
    CHECK(has(f.screen(), "[# Installed packages · leaves]"));
    CHECK(s.status == "Only leaves: installed formulae that no other installed package needs.");
    // Pelo menu Busca, marcado.
    f.app->handleEvent(Event::Special("\x1b" "b"));
    CHECK(has(f.screen(), "• Only leaves (brew leaves)"));
    f.app->handleEvent(Event::Escape);
    f.app->handleEvent(Event::Character('l'));
    CHECK(f.app->viewState().rowCount == 6);
}

TEST_CASE("adicionar um tap, com a confiança, e os pacotes dele no repositório", "[app][taps]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    brewMenu(f, 4);   // Adicionar um tap
    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    CHECK(f.dialog().title == "Add a tap");
    type(f, "not a tap");
    f.app->handleEvent(Event::Return);
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(has(f.dialog().message, "Invalid tap name: not a tap"));
    f.app->handleEvent(Event::Return);
    CHECK(f.brew.operations().empty());

    brewMenu(f, 4);
    type(f, " someone/tap ");
    f.app->handleEvent(Event::Tab);               // a confiança
    f.app->handleEvent(Event::Character(' '));
    f.app->handleEvent(Event::Return);
    CHECK(f.brew.operations() == std::vector<std::string>{"tap|someone/tap", "trust|--tap|someone/tap"});
    CHECK(f.dialog().title == "Command finished OK!");
    f.app->handleEvent(Event::Return);

    writeFile(f.brew.dir.file("taps.json"), kTapsJson);
    f.app->handleEvent(Event::Character('i'));
    f.select("f:someone/tap/bar");
    CHECK_FALSE(f.app->viewState().current.installed);
    CHECK(f.app->viewState().totalPackages == 11);   // 8 da API, foo (instalado), bar e baz-app
}

TEST_CASE("remover um tap: recusa com pacotes dele instalados", "[app][taps]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    brewMenu(f, 5);   // Remover um tap, sem nenhum de terceiros
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().message == "There are no third-party taps.");
    f.app->handleEvent(Event::Return);

    writeFile(f.brew.dir.file("taps.json"),
              std::string(kTapsJson).insert(std::string(kTapsJson).rfind(']'),
                                            R"(, {"name": "other/tap", "official": false, "trusted": true, "formula_names": ["other/tap/x"], "cask_tokens": []})"));
    brewMenu(f, 5);
    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    CHECK(f.dialog().fields[0].choices == std::vector<std::string>{"someone/tap  (3)  untrusted", "other/tap  (1)"});
    f.app->handleEvent(Event::Return);   // someone/tap: o foo dele está instalado
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(has(f.dialog().message, "These packages from someone/tap are installed; remove them first:\n  • someone/tap/foo"));
    f.app->handleEvent(Event::Return);

    brewMenu(f, 5);
    f.app->handleEvent(Event::ArrowRight);   // other/tap
    f.app->handleEvent(Event::Return);
    REQUIRE(f.dialog().kind == tui::Dialog::Confirm);
    CHECK(f.dialog().danger);
    CHECK(f.dialog().focus == 1);            // "Não"
    f.app->handleEvent(Event::Character('y'));
    CHECK(f.brew.operations() == std::vector<std::string>{"untap|other/tap"});
}

TEST_CASE("brew doctor: pronto, ou os avisos com os detalhes na Saída", "[app][doctor]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    brewMenu(f, 8);   // Verificar o sistema
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().message == "Your system is ready to brew.");
    f.app->handleEvent(Event::Return);

    writeFile(f.brew.dir.file("doctor.txt"), kDoctorOutput);
    brewMenu(f, 8);
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(has(f.dialog().message, "brew doctor found 2 warning(s):\n  • Some installed casks are deprecated or disabled."));
    bool detail = false;
    for (const auto &l : f.app->viewState().log)
        detail = detail || has(l, "You should find replacements for the following casks:");
    CHECK(detail);
    CHECK(f.brew.operations().empty());   // o doctor só lê
    f.app->handleEvent(Event::Return);

    // Sai com 1 sem um "Warning:" reconhecível: o diálogo manda para a Saída.
    writeFile(f.brew.dir.file("doctor.txt"), "Algo que o brew passou a imprimir.\n");
    brewMenu(f, 8);
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().message == "brew doctor reported problems.\nDetails in the Output tab.");
}

TEST_CASE("exportar e instalar de um Brewfile", "[app][bundle]")
{
    i18n::setLocale("en_US");
    BrewFixture f;
    const std::string file = f.brew.dir.file("meu.Brewfile");
    brewMenu(f, 6);   // Exportar
    REQUIRE(f.dialog().kind == tui::Dialog::Form);
    CHECK(f.dialog().fields[0].text == "~/Brewfile");
    type(f, file, 10);   // troca o ~/Brewfile pelo arquivo do teste
    f.app->handleEvent(Event::Return);
    CHECK(f.brew.operations() == std::vector<std::string>{"bundle|dump|--file=" + file + "|--force"});
    CHECK(f.dialog().message == "Brewfile saved: " + file);
    f.app->handleEvent(Event::Return);
    // De novo: o arquivo existe, e o diálogo pergunta antes de trocar.
    brewMenu(f, 6);
    type(f, file, 10);
    f.app->handleEvent(Event::Return);
    REQUIRE(f.dialog().kind == tui::Dialog::Confirm);
    CHECK(f.dialog().message == file + " already exists. Replace it?");
    f.app->handleEvent(Event::Escape);
    CHECK(f.brew.operations().size() == 1);

    brewMenu(f, 7);   // Instalar de um Brewfile: nada falta
    type(f, file, 10);
    f.app->handleEvent(Event::Return);
    REQUIRE(f.dialog().kind == tui::Dialog::Message);
    CHECK(f.dialog().message == "Everything in " + file + " is already installed.");
    f.app->handleEvent(Event::Return);

    writeFile(f.brew.dir.file("bundle-check.txt"), kBundleCheckOutput);
    brewMenu(f, 7);
    type(f, file, 10);
    f.app->handleEvent(Event::Return);
    REQUIRE(f.dialog().kind == tui::Dialog::Confirm);
    CHECK(has(f.dialog().message, "The following 2 packages need to be retrieved\n  • Cask iterm2\n  • Formula figlet"));
    f.app->handleEvent(Event::Character('y'));
    CHECK(f.brew.operations().back() == "bundle|install|--file=" + file + "|--no-upgrade");
}

TEST_CASE("num terminal de 80 colunas, as abas perdem os ícones e cabem", "[view][layout]")
{
    i18n::setLocale("en_US");
    tui::ViewState s;
    s.rows = sample();
    s.iconMode = tui::IconMode::Ascii;
    s.toInstall = {"f:curl", "f:wget"};
    s.contentWidth = 71;   // 80 colunas, menos o indicador de posição
    std::string out = draw(s, 80);
    for (const char *t : {" 1 Info ", " 3 Actions (2) ", " 7 Services "})
        CHECK_INFO(out, t);
    CHECK_FALSE(has(out, "1 i Info"));
    s.contentWidth = 111;
    out = draw(s, 120);
    CHECK(has(out, "1 i Info"));
    CHECK(has(out, "7 s Services"));
}

// --- v1.0.1: a notação do teclado da Apple ---

TEST_CASE("atalhos na notação do teclado do sistema", "[view][keys]")
{
#ifdef __APPLE__
    CHECK(tui::keyLabel("Ctrl+L  /") == "⌃L  /");
    CHECK(tui::keyLabel("F9  Alt+A Alt+B Alt+C Alt+H Alt+J") == "F9  ⌥A ⌥B ⌥C ⌥H ⌥J");
    CHECK(tui::keyLabel("1-7  Alt+1…Alt+7") == "1-7  ⌥1…⌥7");
    CHECK(tui::keyLabel("↑ ↓ PgUp PgDn Home End") == "↑ ↓ fn↑ fn↓ fn← fn→");
    CHECK(tui::keyLabel("+  Insert") == "+");   // o teclado da Apple não tem Insert
    CHECK(tui::keyLabel("-  Delete") == "-  ⌦");
    CHECK(tui::keyLabel("Esc  Enter  Ctrl+P") == "Esc  ↩  ⌃P");
    CHECK(tui::keyLabel("Tab") == "⇥");
    CHECK(tui::keyText("Enter/F3: next  Shift+F3: previous") == "Enter/F3: next  ⇧F3: previous");
    CHECK(tui::keyText("Alt+M: apply   Ctrl+E: cancel") == "⌥M: apply   ⌃E: cancel");
#else
    // No Linux, os nomes do teclado de PC, como estão.
    CHECK(tui::keyLabel("Ctrl+L  /") == "Ctrl+L  /");
    CHECK(tui::keyLabel("+  Insert") == "+  Insert");
    CHECK(tui::keyText("Alt+M: apply") == "Alt+M: apply");
#endif
    CHECK(tui::keyLabel("F5") == "F5");
    CHECK(tui::keyLabel("").empty());
    CHECK(tui::keyText("sem atalhos") == "sem atalhos");
}

TEST_CASE("Ajuda, menus e barra de teclas na notação do sistema", "[view][keys]")
{
    i18n::setLocale("en_US");
    AppFixture f;
    f.app.handleEvent(Event::F1);
    // A aba Ajuda inteira (a tela só mostra o começo), mais a barra de teclas.
    std::string out = draw(f.app.viewState(), 140, 60);
    for (const auto &l : tui::tabText(f.app.viewState()))
        out += "\n" + l;
    f.app.handleEvent(Event::F9);
    const std::string menu = draw(f.app.viewState(), 140, 60);
#ifdef __APPLE__
    for (const char *t : {"F5  ⌃R", "⌃L  /", "F7  ⌥D", "-  ⌦", "⌥,", "↑ ↓ fn↑ fn↓ fn← fn→", "⌥MApply",
                          "for ⌥ (Option), enable \"Use Option as Meta key\""})
        CHECK_INFO(out, t);
    for (const char *t : {"Ctrl+", "Alt+", "Insert", "PgUp"})
        CHECK_FALSE(has(out, t));
    CHECK(has(menu, "⌃K"));
    CHECK(has(menu, "⌥,"));
    CHECK_FALSE(has(menu, "Ctrl+"));
#else
    for (const char *t : {"F5  Ctrl+R", "+  Insert", "M-mApply"})
        CHECK_INFO(out, t);
    CHECK_FALSE(has(out, "Option"));   // a nota do teclado do Mac fica só no macOS
    CHECK(has(menu, "Ctrl+K"));
#endif
}

TEST_CASE("em português, com muitos arquivos, as abas perdem o nome do fim para o começo", "[view][layout]")
{
    i18n::setLocale("pt_BR");
    tui::ViewState s;
    s.iconMode = tui::IconMode::Ascii;
    s.contentWidth = 71;   // 80 colunas
    s.tab = tui::HelpTab;
    s.hasCurrent = true;
    s.current.installed = true;
    s.detailsReady = true;
    s.details.installed = true;
    s.details.files.assign(832, "f");
    std::string out = draw(s, 80);
    // A aba atual e as primeiras ficam com o nome; a última vira só o número.
    for (const char *t : {" 1 Informação ", " 2 Arquivos (832) ", " 6 Ajuda ", " 7 "})
        CHECK_INFO(out, t);
    CHECK_FALSE(has(out, "Serviços"));
    // Na aba Serviços, é ela que fica com o nome, e outra perde o dela.
    s.tab = tui::ServicesTab;
    out = draw(s, 80);
    CHECK(has(out, " 7 Serviços "));
    CHECK_FALSE(has(out, " 6 Ajuda "));   // o "Ajuda" da barra de teclas (F1Ajuda) continua
    i18n::setLocale("en_US");
}

TEST_CASE("na Ajuda, uma descrição longa não empurra a coluna das teclas", "[view][layout]")
{
    i18n::setLocale("pt_BR");
    tui::ViewState s;
    s.iconMode = tui::IconMode::Ascii;
    s.tab = tui::HelpTab;
    s.contentWidth = 71;
    s.contentHeight = 30;
    const std::string out = draw(s, 80, 50);
    // "l" tem a descrição mais longa: ela começa na coluna 24, como as outras.
    const size_t line = out.find("\nl ");
    REQUIRE(line != std::string::npos);
    CHECK(out.compare(line + 1 + 24, std::string("Só ").size(), "Só ") == 0);
    i18n::setLocale("en_US");
}
