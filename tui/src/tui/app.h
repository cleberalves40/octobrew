#ifndef OCTOBREW_TUI_APP_H
#define OCTOBREW_TUI_APP_H

#include "brew.h"
#include "icons.h"
#include "listmodel.h"
#include "news.h"
#include "settings.h"
#include "view.h"

#include <atomic>
#include <functional>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
// FTXUI 7 define ScreenInteractive como apelido de ftxui::App; o cabeçalho
// real evita conflito com uma declaração antecipada.
#include <ftxui/component/screen_interactive.hpp>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace tui {

struct Options {
    std::string brewPath;        // vazio: o brew do Homebrew instalado
    std::string language;        // vazio: preferência salva ou ambiente
    std::string iconMode;        // vazio: preferência salva ou "nerd"
    std::string settingsPath;    // vazio: ~/.config/octobrew-tui/config
    bool remote = false;         // começa na visão Repositório
    std::string theme;           // vazio: preferência salva ou "system"
    std::string sudoPath;        // vazio: /usr/bin/sudo (só o "sudo -n true", para saber se pede senha)
    std::string curlPath;        // vazio: /usr/bin/curl, para o feed de notícias
};

/** Resultado de uma listagem, aplicado na thread da interface. */
struct Listing {
    std::vector<octo::Package> installed;   // com o estado (desatualizado, fixado) já marcado
    bool hasRemote = false;
    std::vector<octo::Package> remote;
    std::string error;
    std::string warning;         // falha só na lista do repositório
};

class App
{
public:
    explicit App(const Options &options);
    ~App();

    /** Laço da interface em tela cheia; devolve o código de saída. */
    int run();

    // Para os testes: o estado que a tela mostraria e o tratamento de eventos.
    // Sem tela (run() não chamado), listagens, prévias e operações rodam na hora.
    ViewState viewState();
    bool handleEvent(const ftxui::Event &event);
    void applyListing(const Listing &listing);
    void applySizes(const std::map<std::string, std::int64_t> &sizes);
    void applyDetails(const std::string &key, const octo::Details &details, const std::string &error);
    /** Chave do cache de detalhes do pacote selecionado ("I:f:curl" ou "R:c:kitty"). */
    std::string currentDetailsKey();
    /** Carrega na hora os detalhes do selecionado (os testes; a tela usa uma thread). */
    void loadDetails();
    bool filterMode() const { return m_filterMode; }
    std::string &filterText() { return m_filter; }
    const octo::Settings &settings() const { return m_settings; }
    void setTableHeight(int rows) { m_tableHeight = rows; }
    const Dialog &dialog() const { return m_dialog; }
    void refresh();

private:
    using Op = std::function<void()>;
    using FormOp = std::function<void(const std::vector<DialogField> &)>;
    /** Uma operação: recebe para onde mandar a saída e se o brew tem o terminal. */
    using Work = std::function<void(const octo::OutputFn &, bool interactive)>;
    /** Trabalho fora da interface (uma prévia); devolve o que aplicar nela. */
    using Task = std::function<Op()>;

    // Ações do OctoPkg, com o brew.
    void markInstall();
    void markRemove();
    void clearMarks();
    void actApply();
    void confirmTransaction(const octo::TransactionPreview &install, const std::vector<octo::Package> &remove,
                            const std::vector<octo::Package> &dependents, const std::vector<octo::Package> &toInstall,
                            const std::vector<std::string> &sudoCasks);
    void actUpgrade();
    void actUpdate();
    void actClean();
    void actAutoremove();
    void actPin(bool pin);
    void actRestore();
    /** Campo "criar um snapshot antes", o Boot Environment do octopkg-tui. */
    DialogField snapshotField(bool checked, const std::string &file) const;
    std::string snapshotDir() const;
    std::vector<octo::Package> packagesFor(const std::set<std::string> &keys) const;

    /** Prévia em segundo plano, com o status "Collecting transaction data…". */
    void runTask(const std::string &status, const Task &task);
    /**
     * Executa work. Sem senha a pedir, roda em segundo plano, com a saída na
     * aba Saída; se um cask precisar do sudo e ele pedir a senha
     * (needsTerminal), devolve o terminal enquanto isso. Depois, onSuccess e
     * uma nova listagem.
     */
    void runOperation(const std::string &status, const std::string &command, const Work &work, const Op &onSuccess,
                      bool needsTerminal = false);
    void finishOperation(const std::string &error, const Op &onSuccess);
    /** O sudo roda sem pedir senha agora (credenciais em cache ou NOPASSWD)? */
    bool sudoReady() const;
    void appendOutput(const std::string &chunk);
    bool refuseWhileBusy();

    void actAbout();
    void actPreferences();
    void nextThemeKey();
    void setSearchMode(model::SearchMode mode);
    void nextLayout();
    void openCategoryChooser();
    void setCategory(const std::string &category);
    bool handleCategoryEvent(const ftxui::Event &event);
    std::string filterPlaceholder() const;
    void openFind();
    void loadNews();
    // Fase 4: o que só o Homebrew tem.
    void loadServices();
    void actService(const std::string &verb);
    void actAddTap();
    void actRemoveTap();
    void actDoctor();
    void actExportBrewfile();
    void actImportBrewfile();
    void toggleLeaves();
    /** "~/Brewfile" → "$HOME/Brewfile". */
    static std::string expandHome(const std::string &path);
    void refreshNews();
    void applyNews(const octo::NewsResult &result);
    void findStep(int delta);
    bool handleFindEvent(const ftxui::Event &event);
    void openMenu(int menu);
    void openSortChooser();

    void showMessage(const std::string &title, const std::string &message);
    void showConfirm(const std::string &title, const std::string &message, bool danger, const Op &onYes);
    void showForm(const std::string &title, const std::string &message, std::vector<DialogField> fields,
                  bool danger, FormOp onAccept);
    bool handleDialogEvent(const ftxui::Event &event);
    const octo::Package *current();

    void toggleView();
    void requestDetails();
    octo::Details fetchDetails(const octo::Package &p, std::string *error);
    void log(const std::string &message);
    void setSort(model::SortSpec sort);
    void move(int delta);
    void scrollContent(int delta);
    void setTab(int tab);
    bool handleMouse(const ftxui::Event &event);
    void quit();
    const std::vector<int> &rows();
    void rebuild();
    void layout();
    void joinWorkers();

    octo::Settings m_settings;
    octo::Manager m_mgr;
    IconMode m_iconMode;

    std::vector<octo::Package> m_installed;    // com o estado de cada um
    std::vector<octo::Package> m_remote;
    std::map<std::string, std::int64_t> m_sizes;   // por Package::key()
    bool m_hasRemote = false;
    bool m_remoteView = false;
    std::vector<octo::Package> m_all;          // a visão atual
    int m_outdated = 0;

    // Linhas filtradas e ordenadas, refeitas só quando algo muda.
    std::vector<int> m_rows;
    std::string m_rowsFilter;
    model::SortSpec m_rowsSort{-2, true};
    model::SearchMode m_rowsMode = model::SearchMode::Name;
    std::string m_rowsCategory;
    bool m_rowsLeaves = false;
    bool m_rowsDirty = true;

    std::string m_filter;
    std::string m_placeholder;                 // do campo de filtro, conforme o modo de busca
    model::SortSpec m_sort;
    model::SearchMode m_searchMode = model::SearchMode::Name;
    std::string m_category;                    // F8 (o tap); vazio: todos
    bool m_leavesOnly = false;                 // l: só as folhas
    bool m_categoryChooser = false;
    std::vector<std::pair<std::string, int>> m_categoryList;
    int m_categoryIndex = 0;
    std::string m_selectedKey;                 // Package::key() do selecionado
    std::string m_shownKey;                    // pacote da última tela, para zerar a rolagem
    int m_offset = 0;
    int m_tableHeight = 1000;
    int m_contentHeight = 12;
    int m_contentWidth = 96;
    Layout m_layout = Layout::Normal;
    int m_tab = InfoTab;
    bool m_contentFocused = false;
    int m_contentScroll = 0;
    bool m_filterMode = false;
    bool m_started = false;                    // primeira listagem já disparada
    bool m_sortChooser = false;
    int m_sortChooserIndex = 0;
    std::string m_themeId;
    bool m_findOpen = false;                   // Ctrl+F
    std::string m_findText;
    int m_findIndex = 0;                       // resultado atual, entre os encontrados

    std::set<std::string> m_toInstall;         // Package::key() dos marcados com +
    std::set<std::string> m_toRemove;          // e com -
    std::string m_partial;                     // linha da Saída ainda sem "\n"
    std::string m_operationError;              // fica no status até a próxima ação
    bool m_inOperation = false;                // uma operação do brew: não dá para sair
    std::string m_sudoPath;

    octo::NewsFeed m_newsFeed;
    octo::NewsResult m_news;
    bool m_newsStarted = false;                // a aba já foi aberta
    bool m_newsUnseen = false;
    std::atomic<bool> m_newsBusy{false};
    std::thread m_newsWorker;

    std::vector<octo::Service> m_services;
    bool m_servicesLoaded = false;
    std::string m_servicesError;
    std::atomic<bool> m_servicesBusy{false};
    std::thread m_servicesWorker;

    Dialog m_dialog;
    Op m_onAccept;                             // Confirm
    FormOp m_onForm;                           // Form
    std::vector<std::vector<Op>> m_menuActions;

    std::map<std::string, octo::Details> m_details;   // cache, por currentDetailsKey()
    std::map<std::string, std::string> m_detailErrors;
    std::atomic<bool> m_detailBusy{false};
    std::thread m_detailWorker;

    std::vector<std::string> m_log;
    std::string m_status;
    bool m_error = false;
    std::atomic<bool> m_busy{false};
    bool m_sizing = false;                     // calculando os tamanhos em disco
    std::atomic<bool> m_cancelSizes{false};    // nova listagem ou saída: o cálculo para
    int m_generation = 0;                      // de cada listagem; resultados antigos são ignorados
    std::thread m_worker;

    ftxui::ScreenInteractive *m_screen = nullptr;
    HitBoxes m_boxes;
    ftxui::Component m_input;
};

} // namespace tui

#endif
