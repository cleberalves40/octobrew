// Modelo da lista: filtro, ordenação, seleção e rolagem.

#include "listmodel.h"

#include <catch2/catch_test_macros.hpp>

using model::SortSpec;
using octo::Status;

namespace {

octo::Package pkg(const std::string &name, const std::string &version, std::int64_t size, std::int64_t when,
                  Status status)
{
    octo::Package p;
    p.name = name;
    p.version = version;
    p.size = size;
    p.installedOn = when;
    p.status = status;
    p.installed = status != Status::NotInstalled;
    return p;
}

std::vector<octo::Package> sample()
{
    // Na ordem do brew.
    return {
        pkg("zsh", "5.9", 9000000, 300, Status::Installed),
        pkg("Curl", "8.10.0", 4000000, 100, Status::Outdated),
        pkg("pkg", "2.3.1", 25000000, 200, Status::Pinned),
        pkg("vim", "9.1.0", 40000000, 0, Status::NotInstalled),
    };
}

using Names = std::vector<std::string>;

Names names(const std::vector<octo::Package> &all, const std::vector<int> &rows)
{
    Names out;
    for (int i : rows)
        out.push_back(all[static_cast<size_t>(i)].name);
    return out;
}

Names sorted(SortSpec sort, const std::string &filter = std::string())
{
    const auto all = sample();
    return names(all, model::visibleRows(all, filter, sort));
}

} // namespace

TEST_CASE("sem ordenação, mantém a ordem do brew", "[model]")
{
    CHECK(sorted({-1, true}) == Names{"zsh", "Curl", "pkg", "vim"});
}

TEST_CASE("ordem padrão é pelo nome, sem diferenciar maiúsculas", "[model]")
{
    CHECK(SortSpec{}.column == model::Name);
    CHECK(sorted({}) == Names{"Curl", "pkg", "vim", "zsh"});
    CHECK(sorted({model::Name, false}) == Names{"zsh", "vim", "pkg", "Curl"});
}

TEST_CASE("filtro pelo nome, sem diferenciar maiúsculas", "[model]")
{
    CHECK(sorted({-1, true}, "CU") == Names{"Curl"});
    CHECK(sorted({-1, true}, "s") == Names{"zsh"});
    CHECK(sorted({-1, true}, "nada").empty());
}

TEST_CASE("tamanho, data e estado", "[model]")
{
    CHECK(sorted({model::Size, true}) == Names{"Curl", "zsh", "pkg", "vim"});
    CHECK(sorted({model::InstalledOn, false}) == Names{"zsh", "pkg", "Curl", "vim"});
    // Ordem do PackageStatus do OctoPkg: fixado, instalado, não instalado, desatualizado.
    CHECK(sorted({model::StateIcon, true}) == Names{"pkg", "zsh", "vim", "Curl"});
}

TEST_CASE("versões comparam por partes numéricas", "[model]")
{
    std::vector<octo::Package> all{pkg("a", "1.10", 0, 0, Status::Installed), pkg("b", "1.9", 0, 0, Status::Installed),
                                   pkg("c", "1.9_1", 0, 0, Status::Installed),
                                   pkg("d", "1.9a", 0, 0, Status::Installed),
                                   pkg("e", "01.2", 0, 0, Status::Installed)};
    CHECK(names(all, model::visibleRows(all, "", {model::Version, true})) == Names{"e", "b", "d", "c", "a"});
}

TEST_CASE("clique no cabeçalho: crescente, decrescente, sem ordem", "[model]")
{
    SortSpec s{-1, true};
    s = model::nextOnHeaderClick(s, model::Size);
    CHECK(s == SortSpec{model::Size, true});
    s = model::nextOnHeaderClick(s, model::Size);
    CHECK(s == SortSpec{model::Size, false});
    s = model::nextOnHeaderClick(s, model::Size);
    CHECK(s == SortSpec{-1, true});
    CHECK(model::nextOnHeaderClick({model::Name, false}, model::Version) == SortSpec{model::Version, true});
}

TEST_CASE("seleção acompanha o pacote", "[model]")
{
    const auto all = sample();
    const auto rows = model::visibleRows(all, "", {});
    CHECK(model::rowToSelect(all, rows, "f:vim") == 2);
    CHECK(model::rowToSelect(all, rows, "f:sumiu") == 0);
    CHECK(model::rowToSelect(all, {}, "f:vim") == -1);
}

TEST_CASE("fórmula e cask de mesmo nome são linhas diferentes", "[model]")
{
    std::vector<octo::Package> all{pkg("docker", "28.0.4", 0, 0, Status::NotInstalled),
                                   pkg("docker", "4.40.0", 0, 0, Status::Installed)};
    all[1].kind = octo::Kind::Cask;
    const auto rows = model::visibleRows(all, "", {model::Type, true});
    CHECK(model::rowToSelect(all, rows, "c:docker") == 1);
    CHECK(model::rowToSelect(all, rows, "f:docker") == 0);
    // Tipo decrescente: os casks primeiro.
    CHECK(model::visibleRows(all, "", {model::Type, false}).front() == 1);
}

TEST_CASE("janela da tabela segue a seleção", "[model]")
{
    CHECK(model::scrollOffset(0, 0, 10, 100) == 0);
    CHECK(model::scrollOffset(9, 0, 10, 100) == 0);
    CHECK(model::scrollOffset(10, 0, 10, 100) == 1);
    CHECK(model::scrollOffset(50, 0, 10, 100) == 41);
    CHECK(model::scrollOffset(45, 41, 10, 100) == 41);   // dentro da janela: não rola
    CHECK(model::scrollOffset(5, 41, 10, 100) == 5);
    CHECK(model::scrollOffset(99, 0, 10, 100) == 90);
    CHECK(model::scrollOffset(3, 90, 10, 5) == 0);       // a lista encolheu
    CHECK(model::scrollOffset(-1, 7, 10, 0) == 0);
}

TEST_CASE("ordena 40 mil pacotes", "[model][performance]")
{
    std::vector<octo::Package> all;
    for (int i = 0; i < 40000; ++i)
        all.push_back(pkg("pkg" + std::to_string((i * 7919) % 40000), "1." + std::to_string(i % 97), i, i,
                          Status::NotInstalled));
    const auto rows = model::visibleRows(all, "", {model::Name, true});
    REQUIRE(rows.size() == all.size());
    CHECK(all[static_cast<size_t>(rows.front())].name == "pkg0");
    CHECK(model::visibleRows(all, "pkg3999", {model::Version, false}).size() == 11);
}

TEST_CASE("busca por descrição: nome ou comentário", "[model][search]")
{
    std::vector<octo::Package> all = sample();
    all[0].comment = "The Z shell";
    all[1].comment = "Command line tool for transferring data with URLs";
    all[2].comment = "Package manager";
    all[3].comment = "Improved version of the vi editor";
    using model::SearchMode;
    // Por nome, "editor" não acha nada; por descrição, acha o vim.
    CHECK(model::visibleRows(all, "editor", {}, SearchMode::Name).empty());
    CHECK(names(all, model::visibleRows(all, "EDITOR", {}, SearchMode::Description)) == Names{"vim"});
    // O nome continua valendo no modo descrição.
    CHECK(names(all, model::visibleRows(all, "zsh", {}, SearchMode::Description)) == Names{"zsh"});
    // "a" está nos comentários do curl e do pkg; não em "The Z shell" nem no do vim.
    CHECK(names(all, model::visibleRows(all, "a", {model::Name, true}, SearchMode::Description))
          == Names{"Curl", "pkg"});
}

TEST_CASE("o tap e o filtro por ele", "[model][category]")
{
    std::vector<octo::Package> all = sample();
    all[0].tap = "homebrew/core";
    all[1].tap = "homebrew/core";
    all[2].tap = "someone/tap";
    all[3].tap = "homebrew/cask";
    all[3].kind = octo::Kind::Cask;
    octo::Package nano = pkg("nano", "8.2", 1, 1, Status::NotInstalled);
    nano.tap = "homebrew/cask";
    nano.comment = "Nano's ANOther editor";
    all.push_back(nano);
    CHECK(model::category(all[3]) == "homebrew/cask");
    CHECK(model::category(octo::Package{}).empty());
    using Cats = std::vector<std::pair<std::string, int>>;
    CHECK(model::categories(all) == Cats{{"homebrew/cask", 2}, {"homebrew/core", 2}, {"someone/tap", 1}});
    using model::SearchMode;
    CHECK(names(all, model::visibleRows(all, "", {}, SearchMode::Name, "homebrew/cask")) == Names{"nano", "vim"});
    CHECK(names(all, model::visibleRows(all, "v", {}, SearchMode::Name, "homebrew/cask")) == Names{"vim"});
    CHECK(names(all, model::visibleRows(all, "another", {}, SearchMode::Description, "homebrew/cask")) == Names{"nano"});
    CHECK(model::visibleRows(all, "", {}, SearchMode::Name, "outro/tap").empty());
}

TEST_CASE("folhas: fórmulas instaladas de que nada depende, como o brew leaves", "[model][leaves]")
{
    std::vector<octo::Package> all = sample();   // zsh, Curl, pkg instalados; vim não
    all[1].requiredBy = 2;                        // Curl: dependência de outros
    octo::Package cask = pkg("kitty", "0.49", 0, 0, Status::Installed);
    cask.kind = octo::Kind::Cask;
    all.push_back(cask);
    CHECK(model::isLeaf(all[0]));
    CHECK_FALSE(model::isLeaf(all[1]));
    CHECK_FALSE(model::isLeaf(all[3]));   // não instalado
    CHECK_FALSE(model::isLeaf(all[4]));   // cask
    CHECK(names(all, model::visibleRows(all, "", {}, model::SearchMode::Name, "", true)) == Names{"pkg", "zsh"});
    CHECK(names(all, model::visibleRows(all, "z", {}, model::SearchMode::Name, "", true)) == Names{"zsh"});
}
