#include "app.h"
#include "i18n.h"
#include "process.h"
#include "theme.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <unistd.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/terminal.hpp>

using namespace ftxui;

namespace tui {

namespace {

// Teclas que o FTXUI entrega como sequência crua.
const std::string kCtrlE = "\x05";
const std::string kCtrlK = "\x0b";
const std::string kCtrlL = "\x0c";
const std::string kCtrlP = "\x10";
const std::string kCtrlQ = "\x11";
const std::string kCtrlR = "\x12";
const std::string kCtrlU = "\x15";
const std::string kAltA = "\x1b" "a";
const std::string kAltB = "\x1b" "b";
const std::string kAltC = "\x1b" "c";
const std::string kAltD = "\x1b" "d";
const std::string kAltH = "\x1b" "h";
const std::string kAltI = "\x1b" "i";
const std::string kAltJ = "\x1b" "j";
const std::string kAltM = "\x1b" "m";
const std::string kAltT = "\x1b" "t";
const std::string kAltZ = "\x1b" "z";
const std::string kAltComma = "\x1b" ",";
const std::string kCtrlF = "\x06";
const std::string kCtrlG = "\x07";
const std::string kShiftF3 = "\x1b[1;2R";   // xterm; o FTXUI não tem um evento próprio

enum Menus { FileMenu = 0, SearchMenu, ActionsMenu, BrewMenu, HelpMenu };

// Linhas fixas da tela: título, separadores, cabeçalho, abas, status e teclas.
constexpr int kFixedLines = 9;
// Itens de uma lista mostrados no diálogo da transação; a lista completa vai à Saída.
constexpr size_t kDialogItems = 10;

// Remove o último caractere UTF-8 (Backspace no campo de texto).
void popUtf8(std::string &s)
{
    while (!s.empty()) {
        const unsigned char c = static_cast<unsigned char>(s.back());
        s.pop_back();
        if ((c & 0xC0) != 0x80)
            break;
    }
}

// O cache das notícias fica ao lado do arquivo de preferências.
std::string newsCachePath(const std::string &settingsPath)
{
    const size_t slash = settingsPath.rfind('/');
    return (slash == std::string::npos ? std::string(".") : settingsPath.substr(0, slash)) + "/news.xml";
}

std::string join(const std::vector<std::string> &v, const std::string &sep)
{
    std::string out;
    for (size_t i = 0; i < v.size(); ++i)
        out += (i ? sep : std::string()) + v[i];
    return out;
}

// Até kDialogItems itens, um por linha, e "… e mais N".
std::string bulletList(const std::vector<std::string> &items)
{
    std::string out;
    for (size_t i = 0; i < items.size() && i < kDialogItems; ++i)
        out += "  • " + items[i] + "\n";
    if (items.size() > kDialogItems)
        out += "  " + i18n::trf("… and %1 more (see the Output tab)", {std::to_string(items.size() - kDialogItems)})
               + "\n";
    return out;
}

std::string trimmed(const std::string &s)
{
    const size_t b = s.find_first_not_of(" \t");
    return b == std::string::npos ? std::string() : s.substr(b, s.find_last_not_of(" \t") - b + 1);
}

std::vector<std::string> names(const std::vector<octo::Package> &packages)
{
    std::vector<std::string> out;
    for (const auto &p : packages)
        out.push_back(p.kind == octo::Kind::Cask ? p.fullName + " (cask)" : p.fullName);
    return out;
}

std::vector<std::string> caskTokens(const std::vector<octo::Package> &packages)
{
    std::vector<std::string> out;
    for (const auto &p : packages)
        if (p.kind == octo::Kind::Cask)
            out.push_back(p.fullName);
    return out;
}

std::string detailsKey(const octo::Package &p)
{
    return (p.installed ? "I:" : "R:") + p.key();
}

} // namespace

App::App(const Options &options)
    : m_settings(options.settingsPath)
    , m_mgr(options.brewPath)
    , m_iconMode(iconModeFromString(options.iconMode.empty() ? m_settings.value("icons", "nerd")
                                                             : options.iconMode))
    , m_remoteView(options.remote)
    , m_sudoPath(options.sudoPath.empty() ? std::string("/usr/bin/sudo") : options.sudoPath)
    , m_newsFeed(newsCachePath(m_settings.path()), options.curlPath)
{
    const std::string lang = options.language.empty() ? m_settings.value("language", "system")
                                                      : options.language;
    if (lang == "system")
        i18n::initFromEnvironment();
    else
        i18n::setLocale(lang);
    m_sort.column = m_settings.intValue("sort.column", model::Name);
    if (m_sort.column < -1 || m_sort.column >= model::ColumnCount)
        m_sort.column = model::Name;
    m_sort.ascending = m_settings.value("sort.order", "asc") != "desc";
    m_themeId = theme(options.theme.empty() ? m_settings.value("theme", "system") : options.theme).id;
    m_searchMode = m_settings.value("search", "name") == "description" ? model::SearchMode::Description
                                                                       : model::SearchMode::Name;
    m_placeholder = filterPlaceholder();
    m_status = i18n::tr("Ready.");
}

App::~App()
{
    m_cancelSizes = true;
    joinWorkers();
}

void App::joinWorkers()
{
    if (m_servicesWorker.joinable())
        m_servicesWorker.join();
    if (m_newsWorker.joinable())
        m_newsWorker.join();
    if (m_worker.joinable())
        m_worker.join();
    if (m_detailWorker.joinable())
        m_detailWorker.join();
}

void App::log(const std::string &message)
{
    char ts[16];
    const std::time_t now = std::time(nullptr);
    std::tm tm {};
    ::localtime_r(&now, &tm);
    std::strftime(ts, sizeof ts, "%H:%M:%S", &tm);
    m_log.push_back(std::string("[") + ts + "] " + message);
    if (m_log.size() > 2000)
        m_log.erase(m_log.begin());
}

// Lista em segundo plano: a tela continua respondendo enquanto o brew roda.
// Depois, na mesma thread, mede o que cada instalado ocupa em disco.
void App::refresh()
{
    if (m_busy)
        return;
    m_busy = true;
    m_error = false;
    const bool withRemote = m_remoteView;
    m_status = withRemote ? i18n::tr("Listing repository packages…") : i18n::tr("Listing installed packages…");
    const auto list = [this, withRemote] {
        Listing l;
        try {
            l.installed = m_mgr.listInstalled();
            if (withRemote) {
                l.remote = m_mgr.listRemote(&l.warning);
                l.hasRemote = true;
            }
        } catch (const std::exception &e) {
            l.error = e.what();
        }
        return l;
    };
    const auto measure = [this](const std::vector<octo::Package> &installed) {
        try {
            return m_mgr.sizes(installed, &m_cancelSizes);
        } catch (const std::exception &) {
            return std::map<std::string, std::int64_t>();   // sem os tamanhos, a lista continua
        }
    };
    if (!m_screen) {   // testes
        const Listing l = list();
        applyListing(l);
        if (l.error.empty())
            applySizes(measure(l.installed));
        return;
    }
    // Um cálculo de tamanhos ainda em andamento para logo: a nova lista o refaz.
    m_cancelSizes = true;
    if (m_worker.joinable())
        m_worker.join();
    m_cancelSizes = false;
    const int generation = ++m_generation;
    m_worker = std::thread([this, list, measure, generation] {
        const Listing l = list();
        const bool measuring = l.error.empty() && !l.installed.empty();
        m_screen->Post([this, l, measuring, generation] {
            if (generation != m_generation)
                return;
            applyListing(l);
            m_sizing = measuring;
        });
        m_screen->PostEvent(Event::Custom);
        if (!measuring)
            return;
        const auto sizes = measure(l.installed);
        const bool cancelled = m_cancelSizes;
        m_screen->Post([this, sizes, cancelled, generation] {
            if (generation != m_generation)
                return;
            m_sizing = false;
            if (!cancelled)
                applySizes(sizes);
        });
        m_screen->PostEvent(Event::Custom);
    });
}

void App::applyListing(const Listing &l)
{
    m_busy = false;
    m_details.clear();
    m_detailErrors.clear();
    if (!l.error.empty()) {
        m_error = true;
        m_status = i18n::trf("ERROR: %1", {l.error});
        log(m_status);
        // Sem o catálogo do repositório, volta aos instalados.
        if (m_remoteView && !m_hasRemote && !l.installed.empty())
            m_remoteView = false;
        if (l.installed.empty())
            return;
    }
    m_installed = l.installed;
    // Os tamanhos da medição anterior ficam até a nova terminar.
    for (auto &p : m_installed) {
        const auto it = m_sizes.find(p.key());
        if (it != m_sizes.end())
            p.size = it->second;
    }
    std::vector<const octo::Package *> outdated;
    for (const auto &p : m_installed)
        if (!p.newVersion.empty())
            outdated.push_back(&p);
    m_outdated = static_cast<int>(outdated.size());
    if (l.hasRemote) {
        m_remote = l.remote;
        m_hasRemote = true;
    }
    rebuild();

    if (l.error.empty() && m_operationError.empty()) {
        m_status = i18n::tr("Ready.");
        m_error = false;
    } else if (l.error.empty()) {
        // A listagem depois de uma operação que falhou não esconde o erro.
        m_status = i18n::trf("ERROR: %1", {m_operationError});
        m_error = true;
    }
    int formulae = 0, casks = 0;
    for (const auto &p : m_installed)
        ++(p.kind == octo::Kind::Cask ? casks : formulae);
    log(i18n::trf("%1 package(s) installed: %2 formula(e) and %3 cask(s).",
                  {std::to_string(m_installed.size()), std::to_string(formulae), std::to_string(casks)}));
    if (l.hasRemote)
        log(i18n::trf("%1 package(s) in the repository.", {std::to_string(m_remote.size())}));
    if (!l.warning.empty())
        log(i18n::trf("WARNING: %1", {l.warning}));
    if (!outdated.empty()) {
        log(i18n::trf("Outdated packages (%1):", {std::to_string(outdated.size())}));
        for (const auto *p : outdated)
            log("  " + p->fullName + "  " + p->installedVersion + " -> " + p->newVersion
                + (p->pinned ? "  [" + i18n::tr("Pinned") + "]" : std::string()));
    }
}

void App::applySizes(const std::map<std::string, std::int64_t> &sizes)
{
    m_sizes = sizes;
    std::int64_t total = 0;
    for (auto &p : m_installed) {
        const auto it = m_sizes.find(p.key());
        p.size = it == m_sizes.end() ? 0 : it->second;
        total += p.size;
    }
    if (!m_installed.empty())
        log(i18n::trf("Disk usage of the installed packages: %1", {octo::formatSize(total)}));
    rebuild();
}

void App::rebuild()
{
    m_all = (m_remoteView && m_hasRemote) ? octo::mergeRemote(m_remote, m_installed) : m_installed;
    m_rowsDirty = true;
}

void App::toggleView()
{
    if (m_busy) {
        m_status = i18n::tr("Wait for the current operation to finish.");
        return;
    }
    m_remoteView = !m_remoteView;
    if (m_remoteView && !m_hasRemote) {
        refresh();
        return;
    }
    rebuild();
}

const std::vector<int> &App::rows()
{
    if (m_rowsDirty || m_rowsFilter != m_filter || !(m_rowsSort == m_sort) || m_rowsMode != m_searchMode
        || m_rowsCategory != m_category || m_rowsLeaves != m_leavesOnly) {
        m_rows = model::visibleRows(m_all, m_filter, m_sort, m_searchMode, m_category, m_leavesOnly);
        m_rowsCategory = m_category;
        m_rowsLeaves = m_leavesOnly;
        m_rowsFilter = m_filter;
        m_rowsSort = m_sort;
        m_rowsMode = m_searchMode;
        m_rowsDirty = false;
    }
    return m_rows;
}

void App::layout()
{
    if (!m_screen)
        return;
    const auto size = Terminal::Size();
    const int height = size.dimy;
    // Sobra espaço para o indicador de posição ("12/345") da aba com foco.
    m_contentWidth = std::max(20, size.dimx - 9);
    const int free = std::max(2, height - kFixedLines);
    switch (m_layout) {
    case Layout::ListOnly:   // sem a barra de abas, a aba e um separador
        m_tableHeight = std::max(1, free + 2);
        m_contentHeight = 3;
        break;
    case Layout::TabsOnly:   // sem o cabeçalho, a lista e um separador
        m_contentHeight = std::max(3, free + 2);
        m_tableHeight = 1;
        break;
    default:
        m_contentHeight = std::max(3, std::min(16, free * 2 / 5));
        m_tableHeight = std::max(1, free - m_contentHeight);
        break;
    }
}

const octo::Package *App::current()
{
    const auto &r = rows();
    const int sel = model::rowToSelect(m_all, r, m_selectedKey);
    return sel < 0 ? nullptr : &m_all[static_cast<size_t>(r[static_cast<size_t>(sel)])];
}

std::string App::currentDetailsKey()
{
    const octo::Package *p = current();
    return p ? detailsKey(*p) : std::string();
}

octo::Details App::fetchDetails(const octo::Package &p, std::string *error)
{
    octo::Details d;
    try {
        d = m_mgr.details(p);
    } catch (const std::exception &e) {
        *error = e.what();
    }
    return d;
}

void App::requestDetails()
{
    if (!m_screen || m_detailBusy)
        return;
    const octo::Package *p = current();
    if (!p)
        return;
    const std::string key = detailsKey(*p);
    if (m_details.count(key) || m_detailErrors.count(key))
        return;
    m_detailBusy = true;
    if (m_detailWorker.joinable())
        m_detailWorker.join();
    const octo::Package pkg = *p;
    // "Requerido por" vem da lista de instalados, que só a thread da interface lê.
    const std::vector<std::string> requiredBy = octo::dependents(m_installed, pkg.fullName);
    m_detailWorker = std::thread([this, key, pkg, requiredBy] {
        std::string error;
        octo::Details d = fetchDetails(pkg, &error);
        d.requiredBy = requiredBy;
        m_screen->Post([this, key, d, error] { applyDetails(key, d, error); });
        m_screen->PostEvent(Event::Custom);
    });
}

void App::loadDetails()
{
    const octo::Package *p = current();
    if (!p)
        return;
    std::string error;
    octo::Details d = fetchDetails(*p, &error);
    d.requiredBy = octo::dependents(m_installed, p->fullName);
    applyDetails(detailsKey(*p), d, error);
}

void App::applyDetails(const std::string &key, const octo::Details &details, const std::string &error)
{
    m_detailBusy = false;
    if (m_details.size() > 256) {
        m_details.clear();
        m_detailErrors.clear();
    }
    if (error.empty())
        m_details[key] = details;
    else
        m_detailErrors[key] = error;
}

ViewState App::viewState()
{
    layout();
    ViewState s;
    const auto &r = rows();
    // A seleção acompanha o pacote, não a linha: sobrevive a filtro, ordem e atualização.
    const int sel = model::rowToSelect(m_all, r, m_selectedKey);
    m_selectedKey = sel >= 0 ? m_all[static_cast<size_t>(r[static_cast<size_t>(sel)])].key() : std::string();
    // Outro pacote (seta, filtro, ordem ou atualização): a aba volta ao início.
    if (m_selectedKey != m_shownKey) {
        m_shownKey = m_selectedKey;
        m_contentScroll = 0;
    }
    const int count = static_cast<int>(r.size());
    m_offset = model::scrollOffset(sel, m_offset, m_tableHeight, count);
    for (int i = m_offset; i < count && i < m_offset + m_tableHeight; ++i)
        s.rows.push_back(m_all[static_cast<size_t>(r[static_cast<size_t>(i)])]);
    s.firstRow = m_offset;
    s.rowCount = count;
    s.selected = sel >= m_offset && sel < m_offset + m_tableHeight ? sel - m_offset : -1;
    if (sel >= 0) {
        s.hasCurrent = true;
        s.current = m_all[static_cast<size_t>(r[static_cast<size_t>(sel)])];
        const std::string key = detailsKey(s.current);
        const auto d = m_details.find(key);
        if (d != m_details.end()) {
            s.detailsReady = true;
            s.details = d->second;
        }
        const auto e = m_detailErrors.find(key);
        if (e != m_detailErrors.end())
            s.detailsError = e->second;
    }
    s.sort = m_sort;
    s.searchMode = m_searchMode;
    s.category = m_category;
    s.leavesOnly = m_leavesOnly;
    s.services = m_services;
    s.servicesLoaded = m_servicesLoaded;
    s.servicesLoading = m_servicesBusy;
    s.servicesError = m_servicesError;
    s.categoryChooserOpen = m_categoryChooser;
    s.categories = m_categoryList;
    s.categoryIndex = m_categoryIndex;
    s.remoteView = m_remoteView && m_hasRemote;
    s.tab = m_tab;
    s.contentFocused = m_contentFocused;
    s.contentScroll = m_contentScroll;
    s.contentHeight = m_contentHeight;
    s.contentWidth = m_contentWidth;
    s.layout = m_layout;
    // Só com as abas na tela, as setas rolam a aba.
    if (m_layout == Layout::TabsOnly)
        s.contentFocused = true;
    s.log = m_log;
    s.status = m_status;
    s.busy = m_busy;
    s.sizing = m_sizing;
    s.error = m_error;
    s.totalPackages = static_cast<int>(m_all.size());
    s.outdatedCount = m_outdated;
    s.iconMode = m_iconMode;
    s.sortChooserOpen = m_sortChooser;
    s.sortChooserIndex = m_sortChooserIndex;
    s.toInstall = m_toInstall;
    s.toRemove = m_toRemove;
    s.dialog = m_dialog;
    s.themeId = m_themeId;
    s.news = m_news.items;
    s.newsLoading = m_newsBusy;
    s.newsFromCache = m_news.fromCache;
    s.newsError = m_news.error;
    s.newsUnseen = m_newsUnseen;
    s.findOpen = m_findOpen;
    s.findText = m_findText;
    if (m_findOpen && !m_findText.empty()) {
        const std::vector<int> found = findLines(tabText(s), m_findText);
        s.findCount = static_cast<int>(found.size());
        if (!found.empty()) {
            m_findIndex = ((m_findIndex % s.findCount) + s.findCount) % s.findCount;
            s.findMatchIndex = m_findIndex;
            s.findMatch = found[static_cast<size_t>(m_findIndex)];
        }
    }
    return s;
}

void App::setSort(model::SortSpec sort)
{
    m_sort = sort;
    m_settings.setValue("sort.column", m_sort.column);
    m_settings.setValue("sort.order", m_sort.ascending ? "asc" : "desc");
    m_settings.save();
}

void App::move(int delta)
{
    if (m_contentFocused || m_layout == Layout::TabsOnly) {
        scrollContent(delta);
        return;
    }
    const auto &r = rows();
    if (r.empty())
        return;
    int i = model::rowToSelect(m_all, r, m_selectedKey) + delta;
    i = std::max(0, std::min(i, static_cast<int>(r.size()) - 1));
    m_selectedKey = m_all[static_cast<size_t>(r[static_cast<size_t>(i)])].key();
}

void App::scrollContent(int delta)
{
    const int last = std::max(0, m_boxes.contentLines - m_contentHeight);
    m_contentScroll = std::max(0, std::min(m_contentScroll + delta, last));
}

void App::setTab(int tab)
{
    m_tab = tab;
    if (tab == NewsTab) {
        m_newsUnseen = false;
        if (!m_newsStarted)
            loadNews();
    }
    if (tab == ServicesTab && !m_servicesLoaded)
        loadServices();
    m_contentScroll = 0;
    if (tab == OutputTab && m_contentFocused)
        m_contentScroll = std::max(0, static_cast<int>(m_log.size()) - m_contentHeight);
}

void App::quit()
{
    // Uma listagem ou prévia pode ser abandonada (a saída espera o brew
    // terminar); uma transação, não.
    if (m_inOperation) {
        m_status = i18n::tr("Wait for the current operation to finish.");
        return;
    }
    m_cancelSizes = true;
    if (m_screen)
        m_screen->Exit();
}

// ---------------------------------------------------------------------------
// Marcas e transações (aba Ações do OctoPkg)
// ---------------------------------------------------------------------------

void App::markInstall()
{
    const octo::Package *p = current();
    if (!p)
        return;
    if (p->pinned) {
        m_status = i18n::trf("%1 is pinned.", {p->name});
        return;
    }
    // Instalar um instalado só faz sentido se ele está desatualizado (o brew
    // install o atualiza).
    if (p->installed && p->newVersion.empty() && !m_toInstall.count(p->key())) {
        m_status = i18n::trf("%1 is already installed and up to date.", {p->name});
        return;
    }
    // Como no OctoPkg, + de novo desmarca; instalar e remover se excluem.
    if (!m_toInstall.erase(p->key())) {
        m_toInstall.insert(p->key());
        m_toRemove.erase(p->key());
    }
}

void App::markRemove()
{
    const octo::Package *p = current();
    if (!p)
        return;
    if (!p->installed) {
        m_status = i18n::tr("Only installed packages can be removed.");
        return;
    }
    if (octo::isForbidden(*p)) {
        m_status = i18n::trf("%1 cannot be removed.", {p->name});
        return;
    }
    if (p->pinned) {
        m_status = i18n::trf("%1 is pinned.", {p->name});
        return;
    }
    if (!m_toRemove.erase(p->key())) {
        m_toRemove.insert(p->key());
        m_toInstall.erase(p->key());
    }
}

void App::clearMarks()
{
    m_toInstall.clear();
    m_toRemove.clear();
}

std::vector<octo::Package> App::packagesFor(const std::set<std::string> &keys) const
{
    // Pela ordem: os instalados, depois o repositório (um marcado na visão
    // Repositório continua valendo na visão Instalados).
    std::vector<octo::Package> out;
    std::set<std::string> found;
    for (const auto *list : {&m_installed, &m_remote})
        for (const auto &p : *list)
            if (keys.count(p.key()) && found.insert(p.key()).second)
                out.push_back(p);
    return out;
}

std::string App::snapshotDir() const
{
    const std::string &path = m_settings.path();
    const size_t slash = path.rfind('/');
    return (slash == std::string::npos ? std::string(".") : path.substr(0, slash)) + "/snapshots";
}

DialogField App::snapshotField(bool checked, const std::string &file) const
{
    // O Boot Environment do OctoPkg, no Homebrew: o estado instalado num Brewfile.
    DialogField f;
    f.kind = DialogField::Check;
    f.label = i18n::tr("Save a snapshot (Brewfile) before the transaction");
    f.checked = checked;
    f.hint = i18n::trf("File: %1", {file});
    return f;
}

void App::actApply()
{
    if (refuseWhileBusy())
        return;
    if (m_toInstall.empty() && m_toRemove.empty()) {
        showMessage(i18n::tr("Actions"), i18n::tr("No packages marked. Use + to install and - to remove."));
        return;
    }
    const std::vector<octo::Package> rem = packagesFor(m_toRemove);
    const std::vector<octo::Package> ins = packagesFor(m_toInstall);
    // O brew não remove o que outros instalados usam: eles vão juntos, como
    // no "pkg remove -R" do OctoPkg, e o diálogo vira Aviso.
    const std::vector<std::string> remKeys(m_toRemove.begin(), m_toRemove.end());
    const auto closure = octo::dependentsClosure(m_installed, remKeys);
    const std::vector<octo::Package> deps = packagesFor(std::set<std::string>(closure.begin(), closure.end()));
    for (const auto &p : deps)
        if (octo::isForbidden(p) || p.pinned) {
            showMessage(i18n::tr("Error"), i18n::trf("%1 depends on a package marked for removal and cannot be removed.",
                                                     {p.fullName}));
            return;
        }
    runTask(i18n::tr("Collecting transaction data…"), [this, rem, ins, deps]() -> Op {
        octo::TransactionPreview install;
        if (!ins.empty())
            install = m_mgr.previewInstall(ins);
        std::vector<octo::Package> all = rem;
        all.insert(all.end(), deps.begin(), deps.end());
        all.insert(all.end(), ins.begin(), ins.end());
        const std::vector<std::string> sudo = m_mgr.casksNeedingSudo(caskTokens(all));
        return [this, install, rem, deps, ins, sudo] { confirmTransaction(install, rem, deps, ins, sudo); };
    });
}

void App::confirmTransaction(const octo::TransactionPreview &install, const std::vector<octo::Package> &rem,
                             const std::vector<octo::Package> &deps, const std::vector<octo::Package> &ins,
                             const std::vector<std::string> &sudoCasks)
{
    std::vector<octo::Package> removal = rem;
    removal.insert(removal.end(), deps.begin(), deps.end());
    const std::vector<std::string> retrieved = install.items();
    const std::vector<std::string> removed = names(removal);

    std::string message;
    if (!retrieved.empty()) {
        message += retrieved.size() == 1 ? i18n::tr("The following package needs to be retrieved")
                                         : i18n::trf("The following %1 packages need to be retrieved",
                                                     {std::to_string(retrieved.size())});
        message += "\n" + bulletList(retrieved) + "\n";
    }
    if (!removed.empty()) {
        message += removed.size() == 1 ? i18n::tr("The following package will be removed")
                                       : i18n::trf("The following %1 packages will be removed",
                                                   {std::to_string(removed.size())});
        message += "\n" + bulletList(removed);
        if (!deps.empty())
            message += i18n::trf("(%1 of them depend on the marked packages)", {std::to_string(deps.size())}) + "\n";
        message += "\n";
    }
    if (retrieved.empty() && removed.empty())
        message += i18n::tr("Nothing to do: the marked packages are already in this state.") + "\n\n";
    const bool terminal = !sudoCasks.empty() && !sudoReady();
    if (!sudoCasks.empty())
        message += i18n::trf("Needs sudo: %1", {join(sudoCasks, ", ")})
                   + (terminal ? "\n" + i18n::tr("The screen will be suspended while sudo asks for your password.")
                               : std::string())
                   + "\n\n";
    message += i18n::tr("Confirm?");

    log(i18n::tr("Transaction:"));
    for (const auto &i : retrieved)
        log("  + " + i);
    for (const auto &i : removed)
        log("  - " + i);
    for (const auto &w : install.warnings)
        log(i18n::trf("WARNING: %1", {w}));

    // Como no OctoPkg: "Aviso" quando a remoção leva mais pacotes do que os marcados.
    const bool danger = !deps.empty();
    const std::string file = snapshotDir() + "/" + octo::snapshotName(std::time(nullptr));
    // O OctoPkg desmarca o Boot Environment na remoção.
    std::vector<DialogField> fields{snapshotField(!ins.empty(), file)};
    showForm(danger ? i18n::tr("Warning") : i18n::tr("Confirmation"), message, fields, danger,
             [this, removal, ins, file, terminal](const std::vector<DialogField> &f) {
                 const bool withSnapshot = !f.empty() && f[0].checked;
                 std::vector<std::string> commands;
                 if (withSnapshot)
                     commands.push_back(m_mgr.commandLine({"bundle", "dump", "--file=" + file, "--force"}));
                 for (const auto &a : octo::Manager::uninstallArgs(removal))
                     commands.push_back(m_mgr.commandLine(a));
                 for (const auto &a : octo::Manager::installArgs(ins))
                     commands.push_back(m_mgr.commandLine(a));
                 runOperation(
                     ins.empty() ? i18n::tr("Removing selected packages...")
                                 : i18n::tr("Installing selected packages..."),
                     join(commands, "; "),
                     [this, withSnapshot, file, removal, ins](const octo::OutputFn &out, bool interactive) {
                         // Sem shell: se o snapshot falhar, nada é removido nem instalado.
                         if (withSnapshot)
                             m_mgr.snapshot(file, out);
                         if (!removal.empty())
                             m_mgr.uninstall(removal, out, interactive);
                         if (!ins.empty())
                             m_mgr.install(ins, out, interactive);
                     },
                     [this, removal, ins, withSnapshot, file] {
                         for (const auto &p : removal)
                             m_toRemove.erase(p.key());
                         for (const auto &p : ins)
                             m_toInstall.erase(p.key());
                         std::string summary;
                         if (withSnapshot)
                             summary += i18n::trf("Snapshot: %1", {file}) + "\n";
                         if (!ins.empty())
                             summary += i18n::trf("Installed: %1", {join(names(ins), ", ")}) + "\n";
                         if (!removal.empty())
                             summary += i18n::trf("Removed: %1", {join(names(removal), ", ")}) + "\n";
                         showMessage(i18n::tr("Command finished OK!"), summary + i18n::tr("Details in the Output tab."));
                     },
                     terminal);
             });
}

void App::actUpgrade()
{
    if (refuseWhileBusy())
        return;
    std::map<std::string, std::string> casks;   // os desatualizados que são casks, pelo nome
    for (const auto &p : m_installed)
        if (p.kind == octo::Kind::Cask)
            casks[p.name] = p.fullName;
    runTask(i18n::tr("Collecting transaction data…"), [this, casks]() -> Op {
        const octo::TransactionPreview p = m_mgr.previewUpgrade();
        std::vector<std::string> tokens;
        for (const auto &i : p.items()) {
            const auto it = casks.find(octo::itemName(i));
            if (it != casks.end())
                tokens.push_back(it->second);
        }
        const std::vector<std::string> sudo = m_mgr.casksNeedingSudo(tokens);
        return [this, p, sudo] {
            for (const auto &w : p.warnings)
                log(i18n::trf("WARNING: %1", {w}));
            if (p.count() == 0) {
                showMessage(i18n::tr("System upgrade"), i18n::tr("There are no new updates available!"));
                return;
            }
            const std::vector<std::string> items = p.items();
            std::string message = items.size() == 1 ? i18n::tr("The following package needs to be retrieved")
                                                    : i18n::trf("The following %1 packages need to be retrieved",
                                                                {std::to_string(items.size())});
            message += "\n" + bulletList(items) + "\n";
            const bool terminal = !sudo.empty() && !sudoReady();
            if (!sudo.empty())
                message += i18n::trf("Needs sudo: %1", {join(sudo, ", ")})
                           + (terminal ? "\n" + i18n::tr("The screen will be suspended while sudo asks for your password.")
                                       : std::string())
                           + "\n\n";
            message += i18n::tr("Pinned packages are not upgraded.") + "\n\n" + i18n::tr("Confirm?");
            log(i18n::tr("Transaction:"));
            for (const auto &i : items)
                log("  + " + i);
            const std::string file = snapshotDir() + "/" + octo::snapshotName(std::time(nullptr));
            showForm(i18n::tr("System upgrade"), message, {snapshotField(true, file)}, false,
                     [this, file, terminal](const std::vector<DialogField> &f) {
                         const bool withSnapshot = !f.empty() && f[0].checked;
                         runOperation(i18n::tr("Installing updates..."),
                                      (withSnapshot ? m_mgr.commandLine({"bundle", "dump", "--file=" + file, "--force"}) + "; "
                                                    : std::string())
                                          + m_mgr.commandLine({"upgrade"}),
                                      [this, withSnapshot, file](const octo::OutputFn &out, bool interactive) {
                                          if (withSnapshot)
                                              m_mgr.snapshot(file, out);
                                          m_mgr.upgrade(out, interactive);
                                      },
                                      [this, withSnapshot, file] {
                                          showMessage(i18n::tr("Command finished OK!"),
                                                      (withSnapshot ? i18n::trf("Snapshot: %1", {file}) + "\n"
                                                                    : std::string())
                                                          + i18n::tr("Details in the Output tab."));
                                      },
                                      terminal);
                     });
        };
    });
}

void App::actUpdate()
{
    runOperation(i18n::tr("Updating Homebrew and the package list…"), m_mgr.commandLine({"update"}),
                 [this](const octo::OutputFn &out, bool) { m_mgr.update(out); },
                 // O cache da API mudou: a visão Repositório relê na próxima vez.
                 [this] { m_hasRemote = m_remoteView; });
}

void App::actClean()
{
    if (refuseWhileBusy())
        return;
    runTask(i18n::tr("Collecting transaction data…"), [this]() -> Op {
        const octo::TransactionPreview p = m_mgr.previewCleanup();
        return [this, p] {
            if (p.count() == 0) {
                showMessage(i18n::tr("Clean the local cache"), i18n::tr("Nothing to clean."));
                return;
            }
            log(i18n::trf("brew cleanup would remove %1 item(s):", {std::to_string(p.count())}));
            for (const auto &i : p.items())
                log("  - " + i);
            std::string message = i18n::trf("brew cleanup would remove %1 item(s) (old versions and downloads; see the Output tab).",
                                             {std::to_string(p.count())});
            if (!p.freed.empty())
                message += "\n" + i18n::trf("It would free approximately %1.", {p.freed});
            showConfirm(i18n::tr("Clean the local cache"), message + "\n\n" + i18n::tr("Confirm?"), false, [this] {
                runOperation(i18n::tr("Cleaning the local cache..."), m_mgr.commandLine({"cleanup", "--prune=all"}),
                             [this](const octo::OutputFn &out, bool) { m_mgr.cleanup(out); }, nullptr);
            });
        };
    });
}

void App::actAutoremove()
{
    if (refuseWhileBusy())
        return;
    runTask(i18n::tr("Collecting transaction data…"), [this]() -> Op {
        const octo::TransactionPreview p = m_mgr.previewAutoremove();
        return [this, p] {
            if (p.count() == 0) {
                showMessage(i18n::tr("Remove unneeded dependencies"), i18n::tr("There are no unneeded dependencies."));
                return;
            }
            const std::vector<std::string> items = p.items();
            const std::string message = i18n::trf("The following %1 packages will be removed",
                                                  {std::to_string(items.size())})
                                        + "\n" + bulletList(items) + "\n"
                                        + i18n::tr("They were installed as dependencies and nothing uses them anymore.")
                                        + "\n\n" + i18n::tr("Confirm?");
            log(i18n::tr("Transaction:"));
            for (const auto &i : items)
                log("  - " + i);
            showConfirm(i18n::tr("Remove unneeded dependencies"), message, false, [this] {
                runOperation(i18n::tr("Removing unneeded dependencies…"), m_mgr.commandLine({"autoremove"}),
                             [this](const octo::OutputFn &out, bool) { m_mgr.autoremove(out); }, nullptr);
            });
        };
    });
}

void App::actPin(bool pin)
{
    const octo::Package *p = current();
    if (!p || !p->installed) {
        m_status = i18n::tr("Select an installed package.");
        return;
    }
    if (p->pinned == pin) {
        m_status = pin ? i18n::trf("%1 is pinned.", {p->name}) : i18n::trf("%1 is not pinned.", {p->name});
        return;
    }
    const octo::Package pkg = *p;
    runOperation(pin ? i18n::tr("Pinning…") : i18n::tr("Unpinning…"),
                 m_mgr.commandLine(octo::Manager::pinArgs(pkg, pin)),
                 [this, pkg, pin](const octo::OutputFn &out, bool) { m_mgr.pin(pkg, pin, out); }, nullptr);
}

void App::actRestore()
{
    if (refuseWhileBusy())
        return;
    const std::string dir = snapshotDir();
    const std::vector<std::string> snapshots = octo::listSnapshots(dir);
    if (snapshots.empty()) {
        showMessage(i18n::tr("Restore a snapshot"), i18n::trf("No snapshots in %1 yet.", {dir}));
        return;
    }
    DialogField f;
    f.kind = DialogField::Choice;
    f.label = i18n::tr("Snapshot:");
    f.choices = snapshots;
    f.hint = i18n::tr("Installs what is missing from it; nothing is removed or upgraded.");
    showForm(i18n::tr("Restore a snapshot"), i18n::trf("Snapshots in %1", {dir}), {f}, false,
             [this, dir, snapshots](const std::vector<DialogField> &fields) {
                 const std::string file = dir + "/" + snapshots[static_cast<size_t>(fields[0].choice)];
                 // Um cask do Brewfile pode pedir o sudo.
                 bool casks = false;
                 std::ifstream in(file);
                 for (std::string line; std::getline(in, line);)
                     casks = casks || line.rfind("cask ", 0) == 0;
                 runOperation(i18n::tr("Restoring the snapshot…"),
                              m_mgr.commandLine({"bundle", "install", "--file=" + file, "--no-upgrade"}),
                              [this, file](const octo::OutputFn &out, bool interactive) {
                                  m_mgr.restore(file, out, interactive);
                              },
                              [this, file] {
                                  showMessage(i18n::tr("Command finished OK!"),
                                              i18n::trf("Restored: %1", {file}) + "\n" + i18n::tr("Details in the Output tab."));
                              },
                              casks && !sudoReady());
             });
    m_dialog.focus = 0;
}

// ---------------------------------------------------------------------------
// Fase 4: serviços, taps, folhas, doctor e Brewfiles
// ---------------------------------------------------------------------------

std::string App::expandHome(const std::string &path)
{
    const std::string p = trimmed(path);
    if (p == "~" || p.rfind("~/", 0) == 0) {
        const char *home = std::getenv("HOME");
        return std::string(home ? home : "") + p.substr(1);
    }
    return p;
}

void App::loadServices()
{
    // Como as notícias: em segundo plano, e a aba mostra "Carregando…".
    if (m_servicesBusy)
        return;
    m_servicesBusy = true;
    const auto load = [this](std::string *error) {
        try {
            return m_mgr.listServices();
        } catch (const std::exception &e) {
            *error = e.what();
        }
        return std::vector<octo::Service>();
    };
    const auto apply = [this](const std::vector<octo::Service> &list, const std::string &error) {
        m_servicesBusy = false;
        m_servicesLoaded = true;
        m_servicesError = error;
        if (error.empty())
            m_services = list;
        else
            log(i18n::trf("WARNING: %1", {error}));
    };
    if (!m_screen) {   // testes
        std::string error;
        const auto list = load(&error);
        apply(list, error);
        return;
    }
    if (m_servicesWorker.joinable())
        m_servicesWorker.join();
    m_servicesWorker = std::thread([this, load, apply] {
        std::string error;
        const auto list = load(&error);
        m_screen->Post([apply, list, error] { apply(list, error); });
        m_screen->PostEvent(Event::Custom);
    });
}

void App::actService(const std::string &verb)
{
    const octo::Package *p = current();
    if (!p || !p->installed || p->kind != octo::Kind::Formula) {
        m_status = i18n::tr("Select an installed formula.");
        return;
    }
    if (!p->hasService) {
        m_status = i18n::trf("%1 has no service.", {p->name});
        return;
    }
    const std::string name = p->name;
    const std::string status = verb == "start"  ? i18n::trf("Starting the service %1…", {name})
                               : verb == "stop" ? i18n::trf("Stopping the service %1…", {name})
                                                : i18n::trf("Restarting the service %1…", {name});
    runOperation(status, m_mgr.commandLine({"services", verb, name}),
                 [this, verb, name](const octo::OutputFn &out, bool) { m_mgr.service(verb, name, out); },
                 [this] {
                     m_servicesLoaded = false;
                     loadServices();
                 });
}

void App::toggleLeaves()
{
    m_leavesOnly = !m_leavesOnly;
    m_status = m_leavesOnly ? i18n::tr("Only leaves: installed formulae that no other installed package needs.")
                            : i18n::tr("All packages.");
}

void App::actAddTap()
{
    if (refuseWhileBusy())
        return;
    DialogField name;
    name.kind = DialogField::Text;
    name.label = i18n::tr("Tap (user/repo):");
    name.hint = i18n::tr("From GitHub: user/homebrew-repo.");
    DialogField trust;
    trust.kind = DialogField::Check;
    trust.label = i18n::tr("Trust its formulae and casks (brew trust --tap)");
    trust.hint = i18n::tr("Homebrew only loads packages from trusted third-party taps.");
    showForm(i18n::tr("Add a tap"), std::string(), {name, trust}, false, [this](const std::vector<DialogField> &f) {
        const std::string tap = trimmed(f[0].text);
        if (!octo::validTapName(tap)) {
            showMessage(i18n::tr("Error"), i18n::trf("Invalid tap name: %1 (use user/repo)", {tap}));
            return;
        }
        const bool trust = f[1].checked;
        runOperation(i18n::trf("Adding the tap %1…", {tap}),
                     m_mgr.commandLine({"tap", tap}) + (trust ? "; " + m_mgr.commandLine({"trust", "--tap", tap}) : std::string()),
                     [this, tap, trust](const octo::OutputFn &out, bool) { m_mgr.tap(tap, trust, out); },
                     // Os pacotes do tap entram na visão Repositório na próxima leitura.
                     [this, tap] {
                         m_hasRemote = m_remoteView;
                         showMessage(i18n::tr("Command finished OK!"), i18n::trf("Tap added: %1", {tap}) + "\n"
                                                                           + i18n::tr("Its packages are in the repository view (i)."));
                     });
    });
    m_dialog.focus = 0;
}

void App::actRemoveTap()
{
    if (refuseWhileBusy())
        return;
    runTask(i18n::tr("Listing the taps…"), [this]() -> Op {
        std::vector<octo::Tap> taps;
        for (auto &t : m_mgr.listTaps())
            if (!t.official)
                taps.push_back(std::move(t));
        return [this, taps] {
            if (taps.empty()) {
                showMessage(i18n::tr("Remove a tap"), i18n::tr("There are no third-party taps."));
                return;
            }
            DialogField f;
            f.kind = DialogField::Choice;
            f.label = i18n::tr("Tap:");
            for (const auto &t : taps)
                f.choices.push_back(t.name + "  (" + std::to_string(t.formulae.size() + t.casks.size()) + ")"
                                    + (t.trusted ? "" : "  " + i18n::tr("untrusted")));
            showForm(i18n::tr("Remove a tap"), std::string(), {f}, false, [this, taps](const std::vector<DialogField> &fields) {
                const octo::Tap tap = taps[static_cast<size_t>(fields[0].choice)];
                // O brew recusa o untap com pacotes do tap instalados: melhor dizer quais.
                std::vector<std::string> used;
                for (const auto &p : m_installed)
                    if (p.tap == tap.name)
                        used.push_back(p.fullName);
                if (!used.empty()) {
                    showMessage(i18n::tr("Remove a tap"), i18n::trf("These packages from %1 are installed; remove them first:", {tap.name})
                                                              + "\n" + bulletList(used));
                    return;
                }
                showConfirm(i18n::tr("Remove a tap"), i18n::trf("Remove the tap %1?", {tap.name}), true, [this, tap] {
                    runOperation(i18n::trf("Removing the tap %1…", {tap.name}), m_mgr.commandLine({"untap", tap.name}),
                                 [this, tap](const octo::OutputFn &out, bool) { m_mgr.untap(tap.name, out); },
                                 [this] { m_hasRemote = m_remoteView; });
                });
            });
            m_dialog.focus = 0;
        };
    });
}

void App::actDoctor()
{
    if (refuseWhileBusy())
        return;
    runTask(i18n::tr("Checking the system (brew doctor)…"), [this]() -> Op {
        const octo::DoctorReport r = m_mgr.doctor();
        return [this, r] {
            log("brew doctor");
            appendOutput(r.output + "\n");
            if (r.ready) {
                showMessage(i18n::tr("Check the system"), i18n::tr("Your system is ready to brew."));
                return;
            }
            if (r.warnings.empty()) {
                showMessage(i18n::tr("Check the system"),
                            i18n::tr("brew doctor reported problems.") + "\n" + i18n::tr("Details in the Output tab."));
                return;
            }
            showMessage(i18n::tr("Check the system"),
                        i18n::trf("brew doctor found %1 warning(s):", {std::to_string(r.warnings.size())}) + "\n"
                            + bulletList(r.warnings) + "\n" + i18n::tr("Details in the Output tab."));
        };
    });
}

void App::actExportBrewfile()
{
    if (refuseWhileBusy())
        return;
    DialogField f;
    f.kind = DialogField::Text;
    f.label = i18n::tr("File:");
    f.text = "~/Brewfile";
    f.hint = i18n::tr("Taps, formulae and casks installed now (brew bundle dump).");
    showForm(i18n::tr("Export a Brewfile"), std::string(), {f}, false, [this](const std::vector<DialogField> &fields) {
        const std::string file = expandHome(fields[0].text);
        if (file.empty())
            return;
        const auto run = [this, file] {
            runOperation(i18n::tr("Exporting the Brewfile…"), m_mgr.commandLine({"bundle", "dump", "--file=" + file, "--force"}),
                         [this, file](const octo::OutputFn &out, bool) { m_mgr.snapshot(file, out); },
                         [this, file] { showMessage(i18n::tr("Command finished OK!"), i18n::trf("Brewfile saved: %1", {file})); });
        };
        if (::access(file.c_str(), F_OK) == 0)
            showConfirm(i18n::tr("Export a Brewfile"), i18n::trf("%1 already exists. Replace it?", {file}), true, run);
        else
            run();
    });
    m_dialog.focus = 0;
}

void App::actImportBrewfile()
{
    if (refuseWhileBusy())
        return;
    DialogField f;
    f.kind = DialogField::Text;
    f.label = i18n::tr("File:");
    f.text = "~/Brewfile";
    f.hint = i18n::tr("Installs what is missing from it; nothing is removed or upgraded.");
    showForm(i18n::tr("Install from a Brewfile"), std::string(), {f}, false, [this](const std::vector<DialogField> &fields) {
        const std::string file = expandHome(fields[0].text);
        // A prévia: o que falta, pelo brew bundle check.
        runTask(i18n::tr("Collecting transaction data…"), [this, file]() -> Op {
            const std::vector<std::string> missing = m_mgr.bundleCheck(file);
            return [this, file, missing] {
                if (missing.empty()) {
                    showMessage(i18n::tr("Install from a Brewfile"), i18n::trf("Everything in %1 is already installed.", {file}));
                    return;
                }
                bool casks = false;
                for (const auto &m : missing)
                    casks = casks || m.rfind("Cask ", 0) == 0;
                const bool terminal = casks && !sudoReady();
                log(i18n::tr("Transaction:"));
                for (const auto &m : missing)
                    log("  + " + m);
                const std::string message = i18n::trf("The following %1 packages need to be retrieved", {std::to_string(missing.size())})
                                            + "\n" + bulletList(missing) + "\n"
                                            + (terminal ? i18n::tr("The screen will be suspended while sudo asks for your password.") + "\n\n"
                                                        : std::string())
                                            + i18n::tr("Confirm?");
                showConfirm(i18n::tr("Install from a Brewfile"), message, false, [this, file, terminal] {
                    runOperation(i18n::tr("Installing from the Brewfile…"),
                                 m_mgr.commandLine({"bundle", "install", "--file=" + file, "--no-upgrade"}),
                                 [this, file](const octo::OutputFn &out, bool interactive) { m_mgr.restore(file, out, interactive); },
                                 [this, file] {
                                     showMessage(i18n::tr("Command finished OK!"),
                                                 i18n::trf("Installed from: %1", {file}) + "\n" + i18n::tr("Details in the Output tab."));
                                 },
                                 terminal);
                });
            };
        });
    });
    m_dialog.focus = 0;
}

void App::actAbout()
{
    std::string brewVersion;
    try {
        brewVersion = m_mgr.version();
    } catch (const std::exception &) {
        brewVersion = "Homebrew ?";
    }
    showMessage(i18n::tr("About"), std::string("octobrew-tui ") + OCTOBREW_TUI_VERSION
                                        + "\n" + brewVersion + " · FTXUI"
                                        + "\n" + i18n::tr("Based on OctoPkg, © Alexandre Albuquerque Arnt")
                                        + "\n" + i18n::tr("and on octopkg-tui")
                                        + "\nGPL-2.0-or-later"
                                        + "\nhttps://github.com/cleberalves40/octobrew-tui");
}

// ---------------------------------------------------------------------------
// Preferências, temas e busca nas abas
// ---------------------------------------------------------------------------

void App::setSearchMode(model::SearchMode mode)
{
    // Como o menu Busca do OctoPkg; a escolha fica salva.
    m_searchMode = mode;
    m_placeholder = filterPlaceholder();
    m_settings.setValue("search", mode == model::SearchMode::Description ? "description" : "name");
    m_settings.save();
    m_status = mode == model::SearchMode::Description ? i18n::tr("Search by description (name and description).")
                                                      : i18n::tr("Search by name.");
}

void App::nextLayout()
{
    // Como o F10 (lista) e o F11 (abas) do OctoPkg, numa tecla só.
    switch (m_layout) {
    case Layout::Normal:
        m_layout = Layout::ListOnly;
        m_status = i18n::tr("Package list maximized (F11: tabs).");
        break;
    case Layout::ListOnly:
        m_layout = Layout::TabsOnly;
        m_status = i18n::tr("Tabs maximized (F11: back to both).");
        break;
    default:
        m_layout = Layout::Normal;
        m_status = i18n::tr("Package list and tabs.");
        break;
    }
}

void App::openCategoryChooser()
{
    // Os taps da visão atual (Instalados ou Repositório), com o atual em foco.
    m_categoryList = model::categories(m_all);
    m_categoryIndex = 0;
    for (size_t i = 0; i < m_categoryList.size(); ++i)
        if (m_categoryList[i].first == m_category)
            m_categoryIndex = static_cast<int>(i) + 1;
    m_categoryChooser = true;
}

void App::setCategory(const std::string &category)
{
    m_category = category;
    m_status = category.empty() ? i18n::tr("All taps.") : i18n::trf("Tap: %1", {category});
}

bool App::handleCategoryEvent(const Event &event)
{
    const int last = static_cast<int>(m_categoryList.size());
    if (event == Event::Escape || event == Event::F8) {
        m_categoryChooser = false;
    } else if (event == Event::ArrowUp) {
        m_categoryIndex = std::max(0, m_categoryIndex - 1);
    } else if (event == Event::ArrowDown) {
        m_categoryIndex = std::min(last, m_categoryIndex + 1);
    } else if (event == Event::PageUp) {
        m_categoryIndex = std::max(0, m_categoryIndex - 15);
    } else if (event == Event::PageDown) {
        m_categoryIndex = std::min(last, m_categoryIndex + 15);
    } else if (event == Event::Home) {
        m_categoryIndex = 0;
    } else if (event == Event::End) {
        m_categoryIndex = last;
    } else if (event == Event::Return) {
        setCategory(m_categoryIndex == 0 ? std::string()
                                         : m_categoryList[static_cast<size_t>(m_categoryIndex - 1)].first);
        m_categoryChooser = false;
    } else if (last > 0 && event.is_character() && event.character().size() == 1) {
        // Uma letra pula para o próximo tap que começa com ela.
        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(event.character()[0])));
        for (int step = 1; step <= last; ++step) {
            const int i = (m_categoryIndex - 1 + step) % last;
            if (!m_categoryList[static_cast<size_t>(i)].first.empty()
                && m_categoryList[static_cast<size_t>(i)].first[0] == c) {
                m_categoryIndex = i + 1;
                break;
            }
        }
    }
    return true;
}

std::string App::filterPlaceholder() const
{
    return m_searchMode == model::SearchMode::Description ? i18n::tr("Filter by description…")
                                                          : i18n::tr("Filter by name…");
}

void App::nextThemeKey()
{
    m_themeId = nextTheme(m_themeId).id;
    m_settings.setValue("theme", m_themeId);
    m_settings.save();
    const Theme t = theme(m_themeId);
    m_status = i18n::trf("Theme: %1", {t.system ? i18n::tr("System") : std::string(t.label)});
}

void App::actPreferences()
{
    // As mesmas preferências do octopkg-tui: tema, idioma e ícones.
    DialogField themeField;
    themeField.kind = DialogField::Choice;
    themeField.label = i18n::tr("Theme:");
    const auto &list = themes();
    for (size_t i = 0; i < list.size(); ++i) {
        themeField.choices.push_back(list[i].system ? i18n::tr("System (terminal colors)") : list[i].label);
        if (m_themeId == list[i].id)
            themeField.choice = static_cast<int>(i);
    }
    DialogField lang;
    lang.kind = DialogField::Choice;
    lang.label = i18n::tr("Language:");
    const std::vector<std::string> langIds{"system", "en_US", "pt_BR", "es_ES"};
    lang.choices = {i18n::tr("System default"), "English", "Português (Brasil)", "Español"};
    const std::string savedLang = m_settings.value("language", "system");
    for (size_t i = 0; i < langIds.size(); ++i)
        if (savedLang == langIds[i])
            lang.choice = static_cast<int>(i);
    DialogField iconsField;
    iconsField.kind = DialogField::Choice;
    iconsField.label = i18n::tr("Icons:");
    const std::vector<IconMode> modes{IconMode::Nerd, IconMode::Unicode, IconMode::Ascii};
    iconsField.choices = {"Nerd Fonts", "Unicode", "ASCII"};
    for (size_t i = 0; i < modes.size(); ++i)
        if (m_iconMode == modes[i])
            iconsField.choice = static_cast<int>(i);
    iconsField.hint = i18n::tr("Nerd Fonts need a Nerd Font in the terminal.");

    showForm(i18n::tr("Preferences"), std::string(), {themeField, lang, iconsField}, false,
             [this, langIds, modes](const std::vector<DialogField> &f) {
                 m_themeId = themes()[static_cast<size_t>(f[0].choice)].id;
                 const std::string language = langIds[static_cast<size_t>(f[1].choice)];
                 m_iconMode = modes[static_cast<size_t>(f[2].choice)];
                 if (language == "system")
                     i18n::initFromEnvironment();
                 else
                     i18n::setLocale(language);
                 m_placeholder = filterPlaceholder();   // o texto do campo segue o idioma
                 m_settings.setValue("theme", m_themeId);
                 m_settings.setValue("language", language);
                 m_settings.setValue("icons", iconModeName(m_iconMode));
                 m_status = m_settings.save() ? i18n::tr("Preferences saved.")
                                              : i18n::trf("Could not save %1", {m_settings.path()});
             });
    // O foco começa no primeiro campo, não no OK.
    m_dialog.focus = 0;
}

void App::openFind()
{
    // Como o Ctrl+F do OctoPkg: nas abas Arquivos, Saída, Notícias e Ajuda, e na de Serviços.
    if (m_tab != FilesTab && m_tab != OutputTab && m_tab != NewsTab && m_tab != HelpTab && m_tab != ServicesTab) {
        m_status = i18n::tr("Find works in the Files, Output, News and Help tabs.");
        return;
    }
    m_findOpen = true;
    m_findText.clear();
    m_findIndex = 0;
    m_contentFocused = false;
}

// ---------------------------------------------------------------------------
// Notícias (aba 5, Ctrl+G), como no OctoPkg
// ---------------------------------------------------------------------------

void App::loadNews()
{
    // Na primeira visita: as do cache na hora, e as novas em segundo plano.
    m_newsStarted = true;
    m_news = m_newsFeed.cached();
    refreshNews();
}

void App::refreshNews()
{
    m_newsStarted = true;
    if (m_newsBusy)
        return;
    log(i18n::trf("Searching for %1 latest news...", {octo::kNewsSource}));
    m_newsBusy = true;
    if (!m_screen) {   // testes
        applyNews(m_newsFeed.refresh());
        return;
    }
    if (m_newsWorker.joinable())
        m_newsWorker.join();
    m_newsWorker = std::thread([this] {
        const octo::NewsResult r = m_newsFeed.refresh();
        m_screen->Post([this, r] { applyNews(r); });
        m_screen->PostEvent(Event::Custom);
    });
}

void App::applyNews(const octo::NewsResult &result)
{
    m_newsBusy = false;
    m_news = result;
    if (!result.error.empty())
        log(i18n::trf("WARNING: %1", {result.error}));
    else
        log(i18n::trf("%1 news: %2 item(s).", {octo::kNewsSource, std::to_string(result.items.size())}));
    if (result.updated && m_tab != NewsTab)
        m_newsUnseen = true;
}

void App::findStep(int delta)
{
    m_findIndex += delta;
    // Rola até o resultado, no meio da aba se der.
    const ViewState s = viewState();
    if (s.findMatch >= 0)
        m_contentScroll = std::max(0, s.findMatch - std::max(1, m_contentHeight - 1) / 2);
}

bool App::handleFindEvent(const Event &event)
{
    const std::string &raw = event.input();
    if (event == Event::Escape || raw == kCtrlF) {
        m_findOpen = false;
        m_findText.clear();
        return true;
    }
    if (event == Event::Return || event == Event::F3 || event == Event::ArrowDown) {
        findStep(1);
        return true;
    }
    if (raw == kShiftF3 || event == Event::ArrowUp) {
        findStep(-1);
        return true;
    }
    if (event == Event::Backspace) {
        popUtf8(m_findText);
        m_findIndex = 0;
        findStep(0);
        return true;
    }
    if (event.is_character()) {
        m_findText += event.character();
        m_findIndex = 0;
        findStep(0);
        return true;
    }
    if (event.is_mouse())
        return handleMouse(event);
    return true;   // as demais teclas não saem da busca por engano
}

// ---------------------------------------------------------------------------
// Execução: prévias e operações
// ---------------------------------------------------------------------------

bool App::refuseWhileBusy()
{
    if (!m_busy)
        return false;
    m_status = i18n::tr("Wait for the current operation to finish.");
    return true;
}

void App::runTask(const std::string &status, const Task &task)
{
    if (refuseWhileBusy())
        return;
    const auto attempt = [task](std::string *error) -> Op {
        try {
            return task();
        } catch (const std::exception &e) {
            *error = e.what();
        }
        return nullptr;
    };
    const auto apply = [this](const Op &then, const std::string &error) {
        m_busy = false;
        m_status = i18n::tr("Ready.");
        if (!error.empty()) {
            m_error = true;
            m_status = i18n::trf("ERROR: %1", {error});
            log(m_status);
            showMessage(i18n::tr("Error"), error);
            return;
        }
        m_error = false;
        if (then)
            then();
    };
    if (!m_screen) {   // testes
        std::string error;
        const Op then = attempt(&error);
        apply(then, error);
        return;
    }
    m_busy = true;
    m_error = false;
    m_status = status;
    log(status);
    m_cancelSizes = true;   // um cálculo de tamanhos em andamento para logo
    if (m_worker.joinable())
        m_worker.join();
    m_cancelSizes = false;
    m_sizing = false;
    ++m_generation;
    m_worker = std::thread([this, attempt, apply] {
        std::string error;
        const Op then = attempt(&error);
        m_screen->Post([apply, then, error] { apply(then, error); });
        m_screen->PostEvent(Event::Custom);
    });
}

bool App::sudoReady() const
{
    // "sudo -n true": sai com 0 se o sudo não vai pedir a senha agora.
    const auto r = octo::runProcess({m_sudoPath, "-n", "true"}, 10000);
    return r.started && !r.timedOut && r.status == 0;
}

void App::appendOutput(const std::string &chunk)
{
    // Linha a linha; "\r" (barras de progresso) recomeça a linha atual.
    for (char c : chunk) {
        if (c == '\n') {
            m_log.push_back("  " + m_partial);
            m_partial.clear();
        } else if (c == '\r') {
            m_partial.clear();
        } else {
            m_partial += c;
        }
    }
    while (m_log.size() > 2000)
        m_log.erase(m_log.begin());
}

void App::runOperation(const std::string &status, const std::string &command, const Work &work,
                       const Op &onSuccess, bool needsTerminal)
{
    if (refuseWhileBusy())
        return;
    m_operationError.clear();
    log(command);
    // Como no OctoPkg, a aba Saída acompanha a transação.
    m_tab = OutputTab;
    m_contentFocused = false;
    m_partial.clear();
    const auto attempt = [work](const octo::OutputFn &out, bool interactive) {
        try {
            work(out, interactive);
        } catch (const std::exception &e) {
            return std::string(e.what());
        }
        return std::string();
    };

    if (!m_screen) {   // testes
        finishOperation(attempt([this](const std::string &c) { appendOutput(c); }, needsTerminal), onSuccess);
        return;
    }

    // Uma operação para o cálculo dos tamanhos: a lista é refeita depois.
    m_cancelSizes = true;
    if (m_worker.joinable())
        m_worker.join();
    m_cancelSizes = false;
    m_sizing = false;
    ++m_generation;

    if (needsTerminal) {
        // O sudo pede a senha no terminal: a tela é suspensa enquanto isso, e
        // a saída aparece no terminal e depois na aba Saída.
        std::string error, captured;
        m_screen->WithRestoredIO([&] {
            std::printf("\noctobrew-tui — %s\n%s\n\n", i18n::trf("Running: %1", {command}).c_str(),
                        i18n::tr("sudo may ask for your password.").c_str());
            std::fflush(stdout);
            error = attempt(
                [&captured](const std::string &c) {
                    std::fwrite(c.data(), 1, c.size(), stdout);
                    std::fflush(stdout);
                    captured += c;
                },
                true);
        })();
        appendOutput(captured);
        finishOperation(error, onSuccess);
        return;
    }

    m_busy = true;
    m_inOperation = true;
    m_error = false;
    m_status = status;
    m_worker = std::thread([this, attempt, onSuccess] {
        const std::string error = attempt(
            [this](const std::string &c) {
                m_screen->Post([this, c] { appendOutput(c); });
                m_screen->PostEvent(Event::Custom);
            },
            false);
        m_screen->Post([this, error, onSuccess] { finishOperation(error, onSuccess); });
        m_screen->PostEvent(Event::Custom);
    });
}

void App::finishOperation(const std::string &error, const Op &onSuccess)
{
    m_busy = false;
    m_inOperation = false;
    if (!m_partial.empty())
        appendOutput("\n");
    if (!error.empty()) {
        m_operationError = error;
        m_error = true;
        m_status = i18n::trf("ERROR: %1", {error});
        log(m_status);
        showMessage(i18n::tr("Error"), error);
        refresh();   // a transação pode ter feito parte do trabalho
        return;
    }
    m_error = false;
    m_status = i18n::tr("Ready.");
    log(i18n::tr("Command finished OK!"));
    if (onSuccess)
        onSuccess();
    refresh();
}

// ---------------------------------------------------------------------------
// Diálogos e menus
// ---------------------------------------------------------------------------

void App::showMessage(const std::string &title, const std::string &message)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Message;
    m_dialog.title = title;
    m_dialog.message = message;
}

void App::showConfirm(const std::string &title, const std::string &message, bool danger, const Op &onYes)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Confirm;
    m_dialog.title = title;
    m_dialog.message = message;
    m_dialog.danger = danger;
    m_dialog.focus = danger ? 1 : 0;   // destrutivo: "Não" por padrão
    m_onAccept = onYes;
}

void App::showForm(const std::string &title, const std::string &message, std::vector<DialogField> fields,
                   bool danger, FormOp onAccept)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Form;
    m_dialog.title = title;
    m_dialog.message = message;
    m_dialog.fields = std::move(fields);
    m_dialog.danger = danger;
    // O foco começa no botão: OK, ou Cancelar se for destrutivo.
    m_dialog.focus = static_cast<int>(m_dialog.fields.size()) + (danger ? 1 : 0);
    m_onForm = std::move(onAccept);
}

void App::openMenu(int menu)
{
    m_dialog = Dialog{};
    m_dialog.kind = Dialog::Menu;
    m_dialog.menus = {i18n::tr("File"), i18n::tr("Search"), i18n::tr("Actions"), "Homebrew", i18n::tr("Help")};
    const bool byName = m_searchMode == model::SearchMode::Name;
    const auto sep = std::make_pair(std::string(), std::string());
    m_dialog.items = {
        {{i18n::tr("Refresh the package list"), "F5"},
         {i18n::tr("Switch between installed and repository packages"), "i"},
         sep,
         {i18n::tr("Update Homebrew"), "Ctrl+K"},
         {i18n::tr("Install updates"), "Ctrl+U"},
         {i18n::tr("Clean the local cache"), ""},
         {i18n::tr("Remove unneeded dependencies"), ""},
         sep,
         {i18n::tr("Restore a snapshot…"), ""},
         {i18n::tr("Get the latest news"), "Ctrl+G"},
         sep,
         {i18n::tr("Preferences…"), "Alt+,"},
         {i18n::tr("Next theme"), "Alt+T"},
         sep,
         {i18n::tr("Exit"), "Ctrl+Q"}},
        {{std::string(byName ? "• " : "  ") + i18n::tr("By name"), "F7"},
         {std::string(byName ? "  " : "• ") + i18n::tr("By description"), "F7"},
         sep,
         {i18n::tr("Filter by tap…"), "F8"},
         {i18n::tr("All taps"), ""},
         sep,
         {i18n::tr("Find a package in the package list"), "Ctrl+L"},
         {i18n::tr("Find text in the tab"), "Ctrl+F"},
         sep,
         {std::string(m_leavesOnly ? "• " : "  ") + i18n::tr("Only leaves (brew leaves)"), "l"}},
        {{i18n::tr("Mark for installation"), "+"},
         {i18n::tr("Mark for removal"), "-"},
         sep,
         {i18n::tr("Apply"), "Alt+M"},
         {i18n::tr("Cancel"), "Ctrl+E"},
         sep,
         {i18n::tr("Pin"), ""},
         {i18n::tr("Unpin"), ""}},
        {{i18n::tr("Services"), "7"},
         {i18n::tr("Start the service"), ""},
         {i18n::tr("Stop the service"), ""},
         {i18n::tr("Restart the service"), ""},
         sep,
         {i18n::tr("Add a tap…"), ""},
         {i18n::tr("Remove a tap…"), ""},
         sep,
         {i18n::tr("Export a Brewfile…"), ""},
         {i18n::tr("Install from a Brewfile…"), ""},
         sep,
         {i18n::tr("Check the system (brew doctor)"), ""}},
        {{i18n::tr("Keyboard shortcuts"), "F1"}, {i18n::tr("About"), ""}},
    };
    m_menuActions = {
        {[this] { refresh(); }, [this] { toggleView(); }, nullptr, [this] { actUpdate(); },
         [this] { actUpgrade(); }, [this] { actClean(); }, [this] { actAutoremove(); }, nullptr,
         [this] { actRestore(); }, [this] { setTab(NewsTab); refreshNews(); }, nullptr, [this] { actPreferences(); }, [this] { nextThemeKey(); }, nullptr,
         [this] { quit(); }},
        {[this] { setSearchMode(model::SearchMode::Name); },
         [this] { setSearchMode(model::SearchMode::Description); }, nullptr,
         [this] { openCategoryChooser(); }, [this] { setCategory(std::string()); }, nullptr,
         [this] { m_filterMode = true; m_contentFocused = false; }, [this] { openFind(); }, nullptr,
         [this] { toggleLeaves(); }},
        {[this] { markInstall(); }, [this] { markRemove(); }, nullptr, [this] { actApply(); },
         [this] { clearMarks(); }, nullptr, [this] { actPin(true); }, [this] { actPin(false); }},
        {[this] { setTab(ServicesTab); }, [this] { actService("start"); }, [this] { actService("stop"); },
         [this] { actService("restart"); }, nullptr, [this] { actAddTap(); }, [this] { actRemoveTap(); }, nullptr,
         [this] { actExportBrewfile(); }, [this] { actImportBrewfile(); }, nullptr, [this] { actDoctor(); }},
        {[this] { setTab(HelpTab); }, [this] { actAbout(); }},
    };
    m_dialog.menu = std::max(0, std::min(menu, static_cast<int>(m_dialog.menus.size()) - 1));
}

void App::openSortChooser()
{
    m_sortChooser = true;
    m_sortChooserIndex = m_sort.column + 1;
}

bool App::handleDialogEvent(const Event &event)
{
    Dialog &d = m_dialog;
    switch (d.kind) {
    case Dialog::Message:
        if (event == Event::Return || event == Event::Escape || event == Event::Character(' '))
            d = Dialog{};
        return true;

    case Dialog::Confirm:
        if (event == Event::ArrowLeft || event == Event::ArrowRight || event == Event::Tab
            || event == Event::TabReverse)
            d.focus = 1 - d.focus;
        else if (event == Event::Escape || event == Event::Character('n') || event == Event::Character('N'))
            d = Dialog{};
        else if (event == Event::Return || event == Event::Character('y') || event == Event::Character('s')
                 || event == Event::Character('Y') || event == Event::Character('S')) {
            const bool yes = event != Event::Return || d.focus == 0;
            const Op cb = m_onAccept;
            d = Dialog{};
            if (yes && cb)
                cb();
        }
        return true;

    case Dialog::Form: {
        const int total = static_cast<int>(d.fields.size()) + 2;   // campos + OK + Cancelar
        if (event == Event::Escape) {
            d = Dialog{};
            return true;
        }
        if (event == Event::Tab || event == Event::ArrowDown) {
            d.focus = (d.focus + 1) % total;
            return true;
        }
        if (event == Event::TabReverse || event == Event::ArrowUp) {
            d.focus = (d.focus + total - 1) % total;
            return true;
        }
        if (event == Event::Return) {
            if (d.focus == total - 1) {   // Cancelar
                d = Dialog{};
                return true;
            }
            const std::vector<DialogField> fields = d.fields;
            const FormOp cb = m_onForm;
            d = Dialog{};
            if (cb)
                cb(fields);
            return true;
        }
        if (d.focus >= static_cast<int>(d.fields.size()))
            return true;
        DialogField &f = d.fields[static_cast<size_t>(d.focus)];
        switch (f.kind) {
        case DialogField::Text:
            if (event == Event::Backspace)
                popUtf8(f.text);
            else if (event.is_character())
                f.text += event.character();
            break;
        case DialogField::Check:
            if (event == Event::Character(' '))
                f.checked = !f.checked;
            break;
        case DialogField::Choice:
            if (!f.choices.empty() && (event == Event::ArrowRight || event == Event::Character(' ')))
                f.choice = (f.choice + 1) % static_cast<int>(f.choices.size());
            else if (!f.choices.empty() && event == Event::ArrowLeft)
                f.choice = (f.choice + static_cast<int>(f.choices.size()) - 1) % static_cast<int>(f.choices.size());
            break;
        }
        return true;
    }

    case Dialog::Menu: {
        const int menus = static_cast<int>(d.menus.size());
        const auto &items = d.items[static_cast<size_t>(d.menu)];
        const auto step = [&](int delta) {
            const int n = static_cast<int>(items.size());
            for (int i = 0; i < n; ++i) {
                d.item = (d.item + delta + n) % n;
                if (!items[static_cast<size_t>(d.item)].first.empty())
                    break;   // separadores não são selecionáveis
            }
        };
        if (event == Event::Escape || event == Event::F9 || event == Event::F10)
            d = Dialog{};
        else if (event == Event::ArrowLeft) {
            d.menu = (d.menu + menus - 1) % menus;
            d.item = 0;
        } else if (event == Event::ArrowRight) {
            d.menu = (d.menu + 1) % menus;
            d.item = 0;
        } else if (event == Event::ArrowUp)
            step(-1);
        else if (event == Event::ArrowDown)
            step(1);
        else if (event == Event::Return) {
            const Op action = m_menuActions[static_cast<size_t>(d.menu)][static_cast<size_t>(d.item)];
            d = Dialog{};
            if (action)
                action();
        }
        return true;
    }

    case Dialog::None:
        break;
    }
    return false;
}

bool App::handleMouse(const Event &event)
{
    Event copy = event;   // mouse() não é const em todas as versões do FTXUI
    Mouse mouse = copy.mouse();
    if (mouse.button == Mouse::WheelUp) {
        move(-1);
        return true;
    }
    if (mouse.button == Mouse::WheelDown) {
        move(1);
        return true;
    }
    if (mouse.button != Mouse::Left || mouse.motion != Mouse::Pressed)
        return true;
    for (int c = 0; c < model::ColumnCount; ++c)
        if (m_boxes.header[c].Contain(mouse.x, mouse.y)) {
            setSort(model::nextOnHeaderClick(m_sort, c));
            return true;
        }
    for (int t = 0; t < TabCount; ++t)
        if (m_boxes.tabs[t].Contain(mouse.x, mouse.y)) {
            setTab(t);
            return true;
        }
    const auto &r = rows();
    for (size_t i = 0; i < m_boxes.rows.size(); ++i) {
        const size_t index = static_cast<size_t>(m_offset) + i;
        if (index < r.size() && m_boxes.rows[i].Contain(mouse.x, mouse.y)) {
            m_selectedKey = m_all[static_cast<size_t>(r[index])].key();
            m_contentFocused = false;
            return true;
        }
    }
    return true;
}

bool App::handleEvent(const Event &event)
{
    // Diálogos e menus têm o teclado só para eles.
    if (m_dialog.kind != Dialog::None) {
        if (event.is_mouse())
            return true;
        return handleDialogEvent(event);
    }

    if (m_findOpen)
        return handleFindEvent(event);
    if (m_categoryChooser)
        return handleCategoryEvent(event);

    // Escolha da coluna de ordenação (F6 ou s).
    if (m_sortChooser) {
        if (event == Event::ArrowUp || event == Event::Character('k'))
            m_sortChooserIndex = std::max(0, m_sortChooserIndex - 1);
        else if (event == Event::ArrowDown || event == Event::Character('j'))
            m_sortChooserIndex = std::min(static_cast<int>(model::ColumnCount), m_sortChooserIndex + 1);
        else if (event == Event::Return) {
            const int column = m_sortChooserIndex - 1;
            if (column < 0)
                setSort({-1, true});
            else if (column == m_sort.column)
                setSort({column, !m_sort.ascending});
            else
                setSort({column, true});
            m_sortChooser = false;
        } else if (event == Event::Escape || event == Event::F6 || event == Event::Character('s'))
            m_sortChooser = false;
        return true;
    }

    const std::string &raw = event.input();

    // Campo de filtro: Esc, Enter ou Ctrl+P devolvem o foco à lista; o resto vai ao Input.
    if (m_filterMode) {
        if (event == Event::Escape || event == Event::Return || raw == kCtrlP) {
            m_filterMode = false;
            return true;
        }
        if (event.is_mouse())
            return handleMouse(event);
        return false;
    }

    if (event.is_mouse())
        return handleMouse(event);

    // No teclado do Mac, as teclas F pedem o fn: cada uma tem uma alternativa.
    if (event == Event::F5 || raw == kCtrlR) {
        m_operationError.clear();
        if (m_tab == ServicesTab)
            loadServices();
        refresh();
        return true;
    }
    if (event == Event::F6 || event == Event::Character('s')) {
        openSortChooser();
        return true;
    }
    if (raw == kCtrlL || event == Event::Character('/')) {
        m_filterMode = true;
        m_contentFocused = false;
        return true;
    }
    if (raw == kCtrlQ || event == Event::F10 || event == Event::Character('q')) {
        quit();
        return true;
    }
    if (raw == kAltI || event == Event::Character('i')) {
        toggleView();
        return true;
    }
    if (event == Event::F9 || raw == kAltA) {
        openMenu(FileMenu);
        return true;
    }
    if (raw == kAltB) {
        openMenu(SearchMenu);
        return true;
    }
    if (raw == kAltC) {
        openMenu(ActionsMenu);
        return true;
    }
    if (raw == kAltH) {
        openMenu(BrewMenu);
        return true;
    }
    if (event == Event::Character('l')) {
        toggleLeaves();
        return true;
    }
    if (raw == kAltJ) {
        openMenu(HelpMenu);
        return true;
    }
    // Marcas do OctoPkg: + instalar, - remover (e Insert/Delete).
    if (event == Event::Character('+') || event == Event::Insert) {
        markInstall();
        return true;
    }
    if (event == Event::Character('-') || event == Event::Delete) {
        markRemove();
        return true;
    }
    if (raw == kCtrlE) {
        clearMarks();
        return true;
    }
    // Ctrl+M é Enter no terminal: aplicar fica em Alt+M.
    if (raw == kAltM) {
        actApply();
        return true;
    }
    if (raw == kCtrlK) {
        actUpdate();
        return true;
    }
    if (raw == kCtrlU) {
        actUpgrade();
        return true;
    }
    if (event == Event::F11 || raw == kAltZ) {
        nextLayout();
        return true;
    }
    if (event == Event::F8 || event == Event::Character('t')) {
        openCategoryChooser();
        return true;
    }
    if (event == Event::F7 || raw == kAltD) {
        setSearchMode(m_searchMode == model::SearchMode::Name ? model::SearchMode::Description
                                                              : model::SearchMode::Name);
        return true;
    }
    if (raw == kCtrlF) {
        openFind();
        return true;
    }
    if (raw == kCtrlG) {
        setTab(NewsTab);
        if (m_newsStarted && !m_newsBusy)
            refreshNews();
        return true;
    }
    if (raw == kAltT) {
        nextThemeKey();
        return true;
    }
    if (raw == kAltComma) {
        actPreferences();
        return true;
    }
    if (event == Event::F1 || event == Event::Character('?')) {
        setTab(HelpTab);
        return true;
    }
    for (char digit = '1'; digit <= '7'; ++digit) {
        const std::string d(1, digit);
        // Os números das abas do OctoPkg; Alt+n chega como ESC n.
        if (raw != d && raw != "\x1b" + d)
            continue;
        for (int t = 0; t < TabCount; ++t)
            if (tabKey(t) == digit)
                setTab(t);
        return true;
    }
    if (event == Event::Tab || event == Event::TabReverse) {
        m_contentFocused = !m_contentFocused;
        setTab(m_tab);
        return true;
    }
    if (raw == kCtrlP || event == Event::Escape) {
        m_contentFocused = false;
        return true;
    }
    if (event == Event::Return) {
        setTab(InfoTab);
        m_contentFocused = false;
        return true;
    }
    const int page = (m_contentFocused || m_layout == Layout::TabsOnly) ? m_contentHeight : m_tableHeight;
    if (event == Event::ArrowUp || event == Event::Character('k'))
        move(-1);
    else if (event == Event::ArrowDown || event == Event::Character('j'))
        move(1);
    else if (event == Event::PageUp)
        move(-std::max(1, page - 1));
    else if (event == Event::PageDown)
        move(std::max(1, page - 1));
    else if (event == Event::Home)
        move(-1000000);
    else if (event == Event::End)
        move(1000000);
    return true;
}

int App::run()
{
    auto screen = ScreenInteractive::Fullscreen();
    m_screen = &screen;

    InputOption option;
    option.multiline = false;
    // O texto de ajuda do campo acompanha o modo de busca (F7).
    m_input = Input(&m_filter, &m_placeholder, option);

    auto renderer = Renderer(m_input, [this] {
        // A primeira listagem parte da primeira renderização, já dentro do
        // laço: no FTXUI 6, uma tarefa enviada com Post() antes de Loop() é
        // descartada, e a lista nunca aparecia.
        if (!m_started) {
            m_started = true;
            refresh();
        }
        const ViewState state = viewState();
        Element filter = m_filterMode ? m_input->Render()
                                      : (m_filter.empty() ? text(m_placeholder) | dim
                                                          : text(m_filter));
        Element e = render(state, filter, &m_boxes);
        requestDetails();
        return e;
    });
    auto root = CatchEvent(renderer, [this](Event event) { return handleEvent(event); });

    screen.Loop(root);

    m_cancelSizes = true;
    joinWorkers();
    m_screen = nullptr;
    return 0;
}

} // namespace tui
