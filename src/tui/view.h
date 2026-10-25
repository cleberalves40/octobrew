#ifndef OCTOBREW_TUI_VIEW_H
#define OCTOBREW_TUI_VIEW_H

#include "brew.h"
#include "icons.h"
#include "listmodel.h"
#include "news.h"
#include "theme.h"

#include <array>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <set>
#include <string>
#include <vector>

namespace tui {

/**
 * As abas do OctoPkg, com as teclas dele: Alt+1 Informação, Alt+2 Arquivos,
 * Alt+3 Ações, Alt+4 Saída, Alt+5 Notícias e Alt+6 Ajuda; e a do Homebrew,
 * Alt+7 Serviços (brew services).
 */
enum Tab { InfoTab = 0, FilesTab, ActionsTab, OutputTab, NewsTab, HelpTab, ServicesTab, TabCount };

/**
 * Disposição da tela, como o F10/F11 do OctoPkg (maximizar a lista ou as
 * abas); aqui, F11 ou Alt+Z passa de uma à outra.
 */
enum class Layout { Normal = 0, ListOnly, TabsOnly };

/** Tecla da aba ('1' a '7'). */
char tabKey(int tab);

struct DialogField {
    enum Kind { Text, Check, Choice } kind = Text;
    std::string label;
    std::string text;                    // Text
    bool checked = false;                // Check
    std::vector<std::string> choices;    // Choice
    int choice = 0;
    std::string hint;                    // linha de ajuda abaixo do campo
};

/**
 * Diálogo modal, como no bectl-tui: formulário (a confirmação de uma
 * transação, com a opção do snapshot, e as Preferências), confirmação
 * (Sim/Não), mensagem, ou os menus (F9).
 */
struct Dialog {
    enum Kind { None, Form, Confirm, Message, Menu } kind = None;
    std::string title;
    std::string message;                 // Form, Confirm e Message
    std::vector<DialogField> fields;
    // Form: índice do campo, ou fields.size() = OK e fields.size() + 1 =
    // Cancelar. Confirm: 0 = Sim, 1 = Não.
    int focus = 0;
    bool danger = false;                 // destrutivo: em vermelho, foco em Não/Cancelar
    // Menu: títulos, itens (rótulo, atalho) e posição.
    std::vector<std::string> menus;
    std::vector<std::vector<std::pair<std::string, std::string>>> items;
    int menu = 0;
    int item = 0;
};

/** Tudo o que a tela mostra; montado pela App e desenhado por render(). */
struct ViewState {
    std::vector<octo::Package> rows;   // só as linhas da janela da tabela
    int firstRow = 0;                  // posição de rows[0] na lista filtrada
    int rowCount = 0;                  // linhas da lista filtrada
    int selected = -1;                 // índice em rows; -1 fora da janela
    bool hasCurrent = false;
    octo::Package current;             // pacote selecionado
    bool detailsReady = false;         // details corresponde a current
    octo::Details details;
    std::string detailsError;
    model::SortSpec sort;
    model::SearchMode searchMode = model::SearchMode::Name;
    std::string category;              // filtro por tap; vazio: todos
    bool leavesOnly = false;           // l: só as folhas (brew leaves)
    bool categoryChooserOpen = false;  // F8
    std::vector<std::pair<std::string, int>> categories;   // os taps da visão, com a contagem
    int categoryIndex = 0;             // 0 = todos, 1… = categories
    bool remoteView = false;
    int tab = InfoTab;
    bool contentFocused = false;       // Tab: as setas rolam a aba
    int contentScroll = 0;
    int contentHeight = 12;
    int contentWidth = 96;             // largura útil das abas (os caveats quebram nela)
    Layout layout = Layout::Normal;
    std::vector<std::string> log;      // aba Saída, com horário
    std::string status;
    bool busy = false;
    bool sizing = false;               // calculando os tamanhos em disco
    bool error = false;
    int totalPackages = 0;
    int outdatedCount = 0;
    IconMode iconMode = IconMode::Nerd;
    bool sortChooserOpen = false;
    int sortChooserIndex = 0;          // 0 = ordem do brew, 1… = colunas
    std::set<std::string> toInstall;   // marcados com +, como no OctoPkg (Package::key())
    std::set<std::string> toRemove;    // marcados com -
    Dialog dialog;
    std::string themeId = "system";
    // Busca na aba (Ctrl+F): o texto e a linha do resultado atual.
    bool findOpen = false;
    std::string findText;
    int findMatch = -1;                // linha do resultado atual; -1 sem resultado
    int findMatchIndex = 0;            // posição dele entre os resultados
    int findCount = 0;
    // Aba Notícias.
    std::string newsSource = octo::kNewsSource;
    std::vector<octo::NewsItem> news;
    bool newsLoading = false;
    bool newsFromCache = false;
    bool newsUnseen = false;           // há notícias novas que ainda não foram vistas
    std::string newsError;
    // Aba Serviços.
    std::vector<octo::Service> services;
    bool servicesLoaded = false;
    bool servicesLoading = false;
    std::string servicesError;
};

/** Posições na tela, para o mouse, e o tamanho do conteúdo da aba. */
struct HitBoxes {
    std::array<ftxui::Box, model::ColumnCount> header{};
    std::vector<ftxui::Box> rows;      // de ViewState::rows
    std::array<ftxui::Box, TabCount> tabs{};
    int contentLines = 0;              // linhas da aba atual, para limitar a rolagem
};

/**
 * Uma coluna de atalhos ("Ctrl+L  /", "F9  Alt+A Alt+B") como o teclado do
 * sistema a mostra. No macOS, a notação dos menus da Apple: ⌃L, ⌥A, ⇧F3, fn↑
 * (PgUp), fn↓, fn← (Home), fn→ (End), ⌦ (o Delete, fn⌫), ↩ (Enter) e ⇥ (Tab),
 * sem o Insert, que o teclado da Apple não tem. No Linux, como está. ⌘ não
 * aparece: o terminal não repassa o Command ao programa.
 */
std::string keyLabel(const std::string &keys);
/** Um texto com atalhos ("Alt+M: apply   Ctrl+E: cancel"): no macOS, Ctrl+, Alt+ e Shift+ viram ⌃, ⌥ e ⇧. */
std::string keyText(const std::string &text);

/** Rótulo da coluna. */
std::string columnTitle(int column);
/** O estado de um serviço, traduzido ("started" → "Started"). */
std::string serviceStatusText(const std::string &status);
/** "Formula" ou "Cask", traduzido. */
std::string kindText(octo::Kind kind);
/** Um pacote pela chave, para as listas ("f:curl" → "curl", "c:kitty" → "kitty (cask)"). */
std::string packageLabel(const std::string &key);
/** Estado do pacote por extenso ("Outdated (version 2.0 is available)"). */
std::string statusText(const octo::Package &p);

/**
 * Texto das linhas das abas em que Ctrl+F busca (Arquivos, Saída, Notícias, Ajuda e Serviços),
 * uma por linha desenhada; vazio nas outras.
 */
std::vector<std::string> tabText(const ViewState &state);
/** Índices das linhas de lines que contêm needle, sem diferenciar maiúsculas. */
std::vector<int> findLines(const std::vector<std::string> &lines, const std::string &needle);
/** text quebrado por palavras em linhas de até width colunas (as quebras de linha ficam). */
std::vector<std::string> wrapText(const std::string &text, int width);

/** Tela completa. filter é o campo de filtro (componente Input ou texto). */
ftxui::Element render(const ViewState &state, ftxui::Element filter, HitBoxes *boxes = nullptr);

} // namespace tui

#endif
