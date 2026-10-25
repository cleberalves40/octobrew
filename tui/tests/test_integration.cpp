// Integração contra o Homebrew real: as mesmas consultas e prévias (dry-run)
// que a interface faz, no Cellar, no Caskroom e no cache da API do sistema,
// sem mudar nada. A transação de verdade (instalar e remover o figlet) só roda
// com OCTOBREW_TEST_TRANSACTIONS=1, como no CI.
//
// OCTOBREW_TEST_BREW aponta o brew; sem a variável, os testes são ignorados.
// tests/macos-e2e.sh a define.

#include "brew.h"
#include "news.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <unistd.h>

namespace {

bool enabled()
{
    const char *cmd = std::getenv("OCTOBREW_TEST_BREW");
    return cmd && *cmd;
}

const octo::Manager &mgr()
{
    static octo::Manager m(std::getenv("OCTOBREW_TEST_BREW") ? std::getenv("OCTOBREW_TEST_BREW") : "");
    return m;
}

const std::vector<octo::Package> &installed()
{
    static const std::vector<octo::Package> list = mgr().listInstalled();
    return list;
}

#define REQUIRE_BREW()                                                                                  \
    if (!enabled())                                                                                     \
    SKIP("OCTOBREW_TEST_BREW não definido; execute via tests/macos-e2e.sh")

} // namespace

TEST_CASE("lista os instalados, com o estado de cada um", "[integration]")
{
    REQUIRE_BREW();
    const auto &list = installed();
    // Os testes compilam com o ftxui e o catch2 do Homebrew.
    REQUIRE_FALSE(list.empty());
    bool ftxui = false;
    for (const auto &p : list) {
        CHECK_FALSE(p.name.empty());
        CHECK(p.installed);
        CHECK_FALSE(p.installedVersion.empty());
        CHECK(octo::validPackageName(p.fullName));
        CHECK(p.status != octo::Status::NotInstalled);
        if (p.key() == "f:ftxui") {
            ftxui = true;
            CHECK(p.tap == "homebrew/core");
            CHECK(p.installedOn > 0);
        }
    }
    CHECK(ftxui);
}

TEST_CASE("detalhes e arquivos de um instalado", "[integration]")
{
    REQUIRE_BREW();
    for (const auto &p : installed()) {
        if (p.key() != "f:ftxui")
            continue;
        const octo::Details d = mgr().details(p);
        CHECK(d.name == "ftxui");
        CHECK_FALSE(d.homepage.empty());
        CHECK_FALSE(d.files.empty());
        bool header = false;
        for (const auto &f : d.files)
            header = header || f.find("/include/ftxui/") != std::string::npos;
        CHECK(header);
        return;
    }
    FAIL("ftxui não está instalado");
}

TEST_CASE("lê o cache da API e junta com os instalados", "[integration]")
{
    REQUIRE_BREW();
    const auto remote = mgr().listRemote();
    // O homebrew-core tem milhares de fórmulas, e o homebrew-cask, milhares de casks.
    CHECK(remote.size() > 5000);
    bool cask = false;
    for (const auto &p : remote)
        cask = cask || p.kind == octo::Kind::Cask;
    CHECK(cask);
    const auto merged = octo::mergeRemote(remote, installed());
    int installedCount = 0;
    for (const auto &p : merged)
        installedCount += p.installed ? 1 : 0;
    CHECK(installedCount == static_cast<int>(installed().size()));
}

TEST_CASE("mede o espaço em disco dos instalados", "[integration]")
{
    REQUIRE_BREW();
    const auto sizes = mgr().sizes(installed());
    CHECK(sizes.count("f:ftxui"));
    CHECK(sizes.at("f:ftxui") > 100 * 1024);
}

TEST_CASE("versão do Homebrew", "[integration]")
{
    REQUIRE_BREW();
    CHECK(mgr().version().rfind("Homebrew ", 0) == 0);
}

// --- v0.2.0: prévias (sempre) e uma transação de verdade (só no CI) ---

TEST_CASE("prévias do brew real, sem mudar nada", "[integration][transaction]")
{
    REQUIRE_BREW();
    // O figlet é pequeno e sem dependências; instalado ou não, a prévia funciona.
    octo::Package figlet;
    figlet.name = figlet.fullName = "figlet";
    const auto install = mgr().previewInstall({figlet});
    bool present = false;
    for (const auto &p : installed())
        present = present || p.key() == "f:figlet";
    if (!present) {
        REQUIRE(install.count() >= 1);
        CHECK(octo::itemName(install.items("install").front()) == "figlet");
    }
    // Upgrade, autoremove e cleanup: o formato tem de ser lido, mesmo vazio.
    CHECK_NOTHROW(mgr().previewUpgrade());
    CHECK_NOTHROW(mgr().previewAutoremove());
    CHECK_NOTHROW(mgr().previewCleanup());
}

TEST_CASE("instala, fixa, libera e remove o figlet", "[integration][transaction]")
{
    REQUIRE_BREW();
    // Muda o sistema: só com OCTOBREW_TEST_TRANSACTIONS=1 (o CI, num runner descartável).
    const char *allow = std::getenv("OCTOBREW_TEST_TRANSACTIONS");
    if (!allow || std::string(allow) != "1")
        SKIP("OCTOBREW_TEST_TRANSACTIONS=1 não definido: a transação muda o sistema");
    for (const auto &p : mgr().listInstalled())
        if (p.key() == "f:figlet")
            SKIP("o figlet já está instalado; o teste não mexe no que é do usuário");

    octo::Package figlet;
    figlet.name = figlet.fullName = "figlet";
    std::string out;
    const octo::OutputFn collect = [&out](const std::string &c) { out += c; };
    mgr().install({figlet}, collect);
    const auto find = [](const std::string &key) {
        for (const auto &p : mgr().listInstalled())
            if (p.key() == key)
                return p;
        throw std::runtime_error("não instalado: " + key);
    };
    octo::Package p = find("f:figlet");
    CHECK_FALSE(p.automatic);
    mgr().pin(p, true, collect);
    CHECK(find("f:figlet").status == octo::Status::Pinned);
    mgr().pin(p, false, collect);
    CHECK(find("f:figlet").status == octo::Status::Installed);
    mgr().uninstall({p}, collect);
    bool still = false;
    for (const auto &q : mgr().listInstalled())
        still = still || q.key() == "f:figlet";
    CHECK_FALSE(still);
}

// --- v0.3.0: notícias ---

TEST_CASE("baixa o feed do blog do Homebrew com o curl", "[integration][news]")
{
    REQUIRE_BREW();
    char tmpl[] = "/tmp/octobrew-news.XXXXXX";
    const std::string dir = ::mkdtemp(tmpl);
    const octo::NewsFeed feed(dir + "/news.xml");
    const octo::NewsResult r = feed.refresh();
    (void)std::system(("rm -rf '" + dir + "'").c_str());
    INFO(r.error);
    REQUIRE(r.error.empty());
    REQUIRE_FALSE(r.items.empty());
    CHECK(r.items.front().link.rfind("https://brew.sh/", 0) == 0);
    CHECK(r.items.front().date.size() == 10);   // AAAA-MM-DD
}

// --- v0.4.0: serviços, taps, doctor e Brewfiles (só leitura) ---

TEST_CASE("serviços, taps, doctor e bundle check do brew real", "[integration][services][taps]")
{
    REQUIRE_BREW();
    CHECK_NOTHROW(mgr().listServices());
    const auto taps = mgr().listTaps();
    bool core = false;
    for (const auto &t : taps)
        core = core || (t.name == "homebrew/core" && t.official && t.trusted);
    CHECK(core);
    const octo::DoctorReport d = mgr().doctor();
    // Pronto, ou com o que o doctor imprimiu (o texto dos avisos muda entre versões).
    INFO(d.output);
    CHECK((d.ready || !d.output.empty()));
    // Um Brewfile com o ftxui, que os testes usam: nada falta.
    char tmpl[] = "/tmp/octobrew-bundle.XXXXXX";
    const std::string dir = ::mkdtemp(tmpl);
    { std::FILE *f = std::fopen((dir + "/Brewfile").c_str(), "w"); std::fputs("brew \"ftxui\"\n", f); std::fclose(f); }
    const auto missing = mgr().bundleCheck(dir + "/Brewfile");
    (void)std::system(("rm -rf '" + dir + "'").c_str());
    CHECK(missing.empty());
}
