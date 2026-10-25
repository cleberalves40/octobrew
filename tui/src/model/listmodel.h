#ifndef OCTOBREW_TUI_LISTMODEL_H
#define OCTOBREW_TUI_LISTMODEL_H

#include "brew.h"

#include <string>
#include <utility>
#include <vector>

/**
 * Filtro, ordenação e seleção da lista, independentes da interface, com as
 * colunas do OctoPkg e a do tipo (fórmula ou cask). Testável sem desenhar a
 * tela.
 */
namespace model {

enum Column { StateIcon = 0, Name, Type, Version, Size, InstalledOn, ColumnCount };

struct SortSpec {
    int column = Name;       // -1: ordem do brew
    bool ascending = true;

    bool operator==(const SortSpec &o) const { return column == o.column && ascending == o.ascending; }
};

/**
 * Onde o filtro procura, como o menu Busca do OctoPkg: no nome, ou no nome e
 * na descrição do pacote (o PackageModel::ctn_PACKAGE_DESCRIPTION_FILTER_NO_COLUMN
 * do OctoPkg, que junta os dois).
 */
enum class SearchMode { Name = 0, Description };

/** Clique no cabeçalho, como no bectl-tui: crescente, decrescente e sem ordem. */
SortSpec nextOnHeaderClick(SortSpec current, int column);

/**
 * Índices de all que passam no filtro (parte do nome, ou do nome e da
 * descrição, sem diferenciar maiúsculas), na ordem pedida. Ordenação
 * estável; a do estado segue a ordem do OctoPkg (fixado, instalado, não
 * instalado, desatualizado, mais novo).
 */
std::vector<int> visibleRows(const std::vector<octo::Package> &all, const std::string &filter, SortSpec sort,
                             SearchMode mode = SearchMode::Name, const std::string &tap = std::string(),
                             bool leavesOnly = false);

/**
 * Uma "folha", como no "brew leaves": fórmula instalada de que nenhum outro
 * instalado depende. Junto com o filtro (l), mostra o que dá para remover
 * sem levar nada junto.
 */
bool isLeaf(const octo::Package &p);

/**
 * O "grupo" do pacote para o filtro F8: o tap ("homebrew/core",
 * "homebrew/cask", "user/tap"), como a categoria do port no octopkg-tui.
 */
std::string category(const octo::Package &p);
/** Os taps presentes em all, em ordem alfabética, com quantos pacotes cada um tem. */
std::vector<std::pair<std::string, int>> categories(const std::vector<octo::Package> &all);

/**
 * Posição a selecionar após atualizar, filtrar ou ordenar: a do pacote que
 * estava selecionado (pela Package::key()), se ainda visível; senão a
 * primeira (ou -1, lista vazia).
 */
int rowToSelect(const std::vector<octo::Package> &all, const std::vector<int> &rows, const std::string &previousKey);

/** Primeira linha da janela de height linhas para que selected fique visível. */
int scrollOffset(int selected, int offset, int height, int count);

} // namespace model

#endif
