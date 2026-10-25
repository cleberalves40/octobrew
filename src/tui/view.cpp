#include "view.h"
#include "i18n.h"

#include <cctype>

using namespace ftxui;

namespace tui {

namespace {

// Tema da tela sendo desenhada, montado no começo de render() (sempre na
// thread da interface): evita passá-lo a cada função de desenho.
Theme g_theme = theme("system");

const Theme &T()
{
    return g_theme;
}

// Seleção e destaque: no tema do sistema, vídeo reverso.
Decorator highlight()
{
    if (T().system)
        return inverted;
    return bgcolor(T().accent) | color(T().accentText);
}

// Texto secundário.
Decorator faint()
{
    if (T().system)
        return dim;
    return color(T().dim);
}

Element sep()
{
    return T().system ? separator() : separator() | color(T().border);
}

Element sepLight()
{
    return T().system ? separatorLight() : separatorLight() | color(T().border);
}

// Larguras fixas das colunas; o nome ocupa o restante.
constexpr int kIconWidth = 3;
constexpr int kTypeWidth = 9;
constexpr int kVersionWidth = 20;
constexpr int kSizeWidth = 12;
constexpr int kDateWidth = 17;

Element cell(Element e, int width)
{
    return e | size(WIDTH, EQUAL, width);
}

Element maybeReflect(Element e, Box *box)
{
    return box ? e | reflect(*box) : e;
}

std::string sortMark(const ViewState &s, int column)
{
    if (s.sort.column != column)
        return std::string();
    const IconSet &ic = icons(s.iconMode);
    return std::string(" ") + (s.sort.ascending ? ic.sortAscending : ic.sortDescending);
}

const char *stateIcon(IconMode mode, const octo::Package &p)
{
    const IconSet &ic = icons(mode);
    switch (p.status) {
    case octo::Status::Pinned:       return ic.pinned;
    case octo::Status::Outdated:     return ic.outdated;
    case octo::Status::Newer:        return ic.newer;
    case octo::Status::NotInstalled: return ic.notInstalled;
    default:                         return p.automatic ? ic.dependency : ic.installed;
    }
}

Color stateColor(const octo::Package &p)
{
    switch (p.status) {
    case octo::Status::Pinned:       return T().magenta;
    case octo::Status::Outdated:     return T().yellow;
    case octo::Status::Newer:        return T().cyan;
    case octo::Status::NotInstalled: return T().foreground;
    default:                         return T().green;
    }
}

Element header(const ViewState &s, HitBoxes *boxes)
{
    const auto title = [&](int col) { return text(columnTitle(col) + sortMark(s, col)); };
    Box *b = boxes ? boxes->header.data() : nullptr;
    const std::string iconMark = sortMark(s, model::StateIcon);
    return hbox({
               maybeReflect(cell(text(iconMark.empty() ? " " : iconMark), kIconWidth),
                            b ? &b[model::StateIcon] : nullptr),
               maybeReflect(title(model::Name) | flex, b ? &b[model::Name] : nullptr),
               text(" "),   // nomes longos são cortados antes do tipo, sem encostar nele
               maybeReflect(cell(title(model::Type), kTypeWidth), b ? &b[model::Type] : nullptr),
               maybeReflect(cell(title(model::Version), kVersionWidth), b ? &b[model::Version] : nullptr),
               maybeReflect(cell(title(model::Size) | align_right, kSizeWidth), b ? &b[model::Size] : nullptr),
               text("  "),
               maybeReflect(cell(title(model::InstalledOn), kDateWidth), b ? &b[model::InstalledOn] : nullptr),
           })
        | bold;
}

Element row(const ViewState &s, size_t i, HitBoxes *boxes)
{
    const octo::Package &p = s.rows[i];
    const IconSet &ic = icons(s.iconMode);
    const std::string key = p.key();
    Element icon;
    if (s.toInstall.count(key))
        icon = text(std::string(" ") + ic.toInstall) | color(T().green) | bold;
    else if (s.toRemove.count(key))
        icon = text(std::string(" ") + ic.toRemove) | color(T().red) | bold;
    else {
        icon = text(std::string(" ") + stateIcon(s.iconMode, p));
        if (p.installed)
            icon = icon | color(stateColor(p));
    }
    // Enquanto os tamanhos são calculados, os instalados mostram "…".
    std::string size;
    if (p.size > 0)
        size = octo::formatSize(p.size);
    else if (p.installed && s.sizing)
        size = "…";
    // Na visão Repositório, como no OctoPkg, a versão é a disponível.
    Element line = hbox({
        cell(icon, kIconWidth),
        text(p.name) | flex,
        text(" "),
        cell(text(kindText(p.kind)), kTypeWidth),
        cell(text(p.version), kVersionWidth),
        cell(text(size) | align_right, kSizeWidth),
        text("  "),
        cell(text(octo::formatTime(p.installedOn)), kDateWidth),
    });
    if (!p.installed && !s.toInstall.count(key))
        line = line | faint();
    if (static_cast<int>(i) == s.selected)
        line = line | (s.contentFocused ? Decorator(underlined) : highlight());
    if (boxes)
        line = line | reflect(boxes->rows[i]);
    return line;
}

Element table(const ViewState &s, HitBoxes *boxes)
{
    if (boxes)
        boxes->rows.assign(s.rows.size(), Box{});
    Elements lines;
    for (size_t i = 0; i < s.rows.size(); ++i)
        lines.push_back(row(s, i, boxes));
    if (lines.empty())
        lines.push_back(text(s.busy ? std::string() : i18n::tr("No packages.")) | faint());
    return vbox({header(s, boxes), sepLight(), vbox(std::move(lines)) | flex | yframe});
}

size_t columns(const std::string &s);

Element tabs(const ViewState &s, HitBoxes *boxes)
{
    const IconSet &ic = icons(s.iconMode);
    std::string files = i18n::tr("Files");
    if (s.detailsReady && s.details.installed && !s.details.filesTruncated)
        files += " (" + std::to_string(s.details.files.size()) + ")";
    std::string actions = i18n::tr("Actions");
    const size_t marked = s.toInstall.size() + s.toRemove.size();
    if (marked)
        actions += " (" + std::to_string(marked) + ")";
    const std::string names[TabCount] = {
        i18n::tr("Info"),
        files,
        actions,
        i18n::tr("Output"),
        i18n::tr("News") + (s.newsUnseen ? " *" : ""),
        i18n::tr("Help"),
        i18n::tr("Services"),
    };
    const char *iconOf[TabCount] = {ic.info, ic.files, ic.actions, ic.output, ic.news, ic.help, ic.services};
    // Com as sete abas, a barra passa das 80 colunas. Num terminal estreito, os
    // ícones saem; se ainda não couber (os nomes em português, a contagem dos
    // arquivos), as abas perdem o nome, da última para a primeira, até caber,
    // ficando o número e a contagem ("2 (832)"). A aba atual sempre tem o nome.
    const size_t available = static_cast<size_t>(s.contentWidth + 9);
    const auto extraOf = [](const std::string &n) {   // o que vem depois do nome: "(832)", "*"
        const size_t at = n.find(" (") != std::string::npos ? n.find(" (") : n.find(" *");
        return at == std::string::npos ? std::string() : n.substr(at + 1);
    };
    std::string labels[TabCount];
    size_t width = 0;
    for (int t = 0; t < TabCount; ++t) {
        labels[t] = names[t];
        width += 5 + columns(names[t]);   // " 1 nome " (4 colunas além do nome) e o espaço entre as abas
    }
    if (width + 2 * TabCount <= available) {
        for (int t = 0; t < TabCount; ++t)
            labels[t] = std::string(iconOf[t]) + " " + names[t];
    } else {
        for (int t = TabCount - 1; t >= 0 && width > available; --t) {
            if (t == s.tab)
                continue;
            const std::string shorter = extraOf(names[t]);
            width -= columns(labels[t]) - columns(shorter) + (shorter.empty() ? 1 : 0);
            labels[t] = shorter;
        }
    }
    Elements items;
    for (int t = 0; t < TabCount; ++t) {
        Element e = text(std::string(" ") + tabKey(t) + (labels[t].empty() ? "" : " " + labels[t]) + " ");
        e = t == s.tab ? e | highlight() | bold : e | faint();
        if (boxes)
            e = e | reflect(boxes->tabs[t]);
        items.push_back(e);
        items.push_back(text(" "));
    }
    return hbox(std::move(items));
}

constexpr int kLabelWidth = 20;

Element field(const std::string &label, const std::string &value)
{
    return hbox({text(label + ":") | bold | size(WIDTH, EQUAL, kLabelWidth), text(value)});
}

// Lista ("Depende de", "Conflita com"...): a primeira linha com o rótulo.
void listField(Elements &out, const std::string &label, const std::vector<std::string> &items)
{
    if (items.empty())
        return;
    for (size_t i = 0; i < items.size(); ++i)
        out.push_back(i == 0 ? field(label, items[i]) : hbox({text("") | size(WIDTH, EQUAL, kLabelWidth),
                                                             text(items[i])}));
}

Elements infoLines(const ViewState &s)
{
    Elements out;
    if (!s.hasCurrent) {
        out.push_back(text(i18n::tr("Select a package to see details.")) | faint());
        return out;
    }
    const octo::Package &p = s.current;
    out.push_back(hbox({text(p.name) | bold, text("  " + p.comment)}));
    out.push_back(hbox({text(i18n::tr("Status") + ":") | bold | size(WIDTH, EQUAL, kLabelWidth),
                        text(statusText(p)) | color(stateColor(p))}));
    out.push_back(field(i18n::tr("Type"), kindText(p.kind)));
    // O serviço vem da aba Serviços, não dos detalhes: aparece logo.
    if (p.hasService) {
        std::string status = i18n::tr("yes (tab 7)");
        for (const auto &sv : s.services)
            if (sv.name == p.name)
                status = serviceStatusText(sv.status) + (sv.user.empty() ? std::string() : " (" + sv.user + ")");
        out.push_back(field(i18n::tr("Service"), status));
    }
    if (!s.detailsError.empty()) {
        out.push_back(text(i18n::trf("ERROR: %1", {s.detailsError})) | color(T().red));
        return out;
    }
    if (!s.detailsReady) {
        out.push_back(text(i18n::tr("Loading…")) | faint());
        return out;
    }
    const octo::Details &d = s.details;
    if (p.installed && !p.installedVersion.empty() && p.installedVersion != d.version)
        out.push_back(field(i18n::tr("Installed version"), p.installedVersion));
    out.push_back(field(i18n::tr("Version"), d.version));
    if (!d.names.empty())
        out.push_back(field(i18n::tr("Application"), d.names.front()));
    out.push_back(field(i18n::tr("Tap"), d.tap));
    out.push_back(field(i18n::tr("Homepage"), d.homepage));
    if (!d.license.empty())
        out.push_back(field(i18n::tr("License"), d.license));
    if (!d.kegOnly.empty())
        out.push_back(field(i18n::tr("Keg-only"), d.kegOnly));
    if (d.autoUpdates)
        out.push_back(field(i18n::tr("Auto-updates"), i18n::tr("yes (brew upgrade skips it)")));
    if (!d.deprecated.empty())
        out.push_back(hbox({text(i18n::tr("Deprecated") + ":") | bold | size(WIDTH, EQUAL, kLabelWidth),
                            text(d.deprecated) | color(T().yellow)}));
    if (!d.disabled.empty())
        out.push_back(hbox({text(i18n::tr("Disabled") + ":") | bold | size(WIDTH, EQUAL, kLabelWidth),
                            text(d.disabled) | color(T().red)}));
    if (p.size > 0)
        out.push_back(field(i18n::tr("Installed size"), octo::formatSize(p.size)));
    if (d.installedOn > 0)
        out.push_back(field(i18n::tr("Installed on"), octo::formatTime(d.installedOn)));
    listField(out, i18n::tr("Depends on"), d.dependencies);
    listField(out, i18n::tr("Build dependencies"), d.buildDependencies);
    listField(out, i18n::tr("Required by"), d.requiredBy);
    listField(out, i18n::tr("Conflicts with"), d.conflicts);
    listField(out, i18n::tr("Artifacts"), d.artifacts);
    if (!d.caveats.empty()) {
        // Os caveats do brew (o que fazer depois de instalar), como o pkg-message.
        out.push_back(text(""));
        out.push_back(text(i18n::tr("Caveats")) | bold | color(T().yellow));
        for (const auto &l : wrapText(d.caveats, s.contentWidth))
            out.push_back(text(l));
    }
    return out;
}

Elements filesLines(const ViewState &s)
{
    Elements out;
    if (!s.hasCurrent)
        out.push_back(text(i18n::tr("Select a package to see details.")) | faint());
    else if (!s.current.installed)
        out.push_back(text(i18n::tr("Files are listed for installed packages only.")) | faint());
    else if (!s.detailsError.empty())
        out.push_back(text(i18n::trf("ERROR: %1", {s.detailsError})) | color(T().red));
    else if (!s.detailsReady)
        out.push_back(text(i18n::tr("Loading…")) | faint());
    else if (s.details.filesTruncated)
        out.push_back(text(i18n::tr("This package has too many files to be listed.")) | faint());
    else if (s.details.files.empty())
        out.push_back(text(i18n::tr("No files.")) | faint());
    else
        for (const auto &f : s.details.files)
            out.push_back(text(f));
    return out;
}

// Atalhos da aba Ajuda: (tecla, descrição); a busca (Ctrl+F) usa o mesmo texto.
// No Mac, as teclas F pedem o fn: cada uma tem uma alternativa.
std::vector<std::pair<std::string, std::string>> helpEntries()
{
    std::vector<std::pair<std::string, std::string>> entries{
        {"F5  Ctrl+R", i18n::tr("Refresh the package list")},
        {"↑ ↓ PgUp PgDn Home End", i18n::tr("Move")},
        {"Ctrl+L  /", i18n::tr("Find a package in the package list")},
        {"F7  Alt+D", i18n::tr("Search by name or by description")},
        {"F8  t", i18n::tr("Filter by tap (homebrew/core, homebrew/cask...)")},
        {"l", i18n::tr("Only leaves: installed formulae that nothing else needs (brew leaves)")},
        {"Esc  Enter  Ctrl+P", i18n::tr("Go back to the package list")},
        {"i  Alt+I", i18n::tr("Switch between installed and repository packages")},
        {"+  Insert", i18n::tr("Mark the selected package for installation")},
        {"-  Delete", i18n::tr("Mark the selected package for removal")},
        {"Alt+M", i18n::tr("Apply: install and remove the marked packages")},
        {"Ctrl+E", i18n::tr("Cancel: clear the marks")},
        {"Ctrl+K", i18n::tr("Update Homebrew and the package list (brew update)")},
        {"Ctrl+U", i18n::tr("Install updates (brew upgrade)")},
        {"Ctrl+F", i18n::tr("Find text in the Files, Output, News and Help tabs (F3: next, Shift+F3: previous)")},
        {"Ctrl+G", i18n::tr("Get the latest Homebrew news (News tab)")},
        {"Alt+,", i18n::tr("Preferences: theme, language and icons")},
        {"Alt+T", i18n::tr("Next theme")},
        {"F9  Alt+A Alt+B Alt+C Alt+H Alt+J", i18n::tr("Menus File, Search, Actions, Homebrew and Help")},
        {"F6  s", i18n::tr("Choose the sort column (again: reverse; (brew order): original order)")},
        {"1-7  Alt+1…Alt+7", i18n::tr("Tabs Info, Files, Actions, Output, News, Help and Services")},
        {"F1  ?", i18n::tr("Show this help page")},
        {"Enter", i18n::tr("Show the Info tab of the selected package")},
        {"Tab", i18n::tr("Scroll the tab with the arrow keys")},
        {"F11  Alt+Z", i18n::tr("Maximize the package list, then the tabs, then back to both")},
        {"Ctrl+Q  F10  q", i18n::tr("Exit")},
        {"", i18n::tr("Click a column header to sort")},
    };
#ifdef __APPLE__
    entries.push_back({"", i18n::tr("On a Mac keyboard, F keys may need fn; for ⌥ (Option), enable \"Use Option as Meta key\" in the terminal.")});
#endif
    // Na notação do teclado do sistema, na tela e na busca.
    for (auto &e : entries) {
        e.first = keyLabel(e.first);
        e.second = keyText(e.second);
    }
    return entries;
}

Elements helpLines()
{
    Elements out;
    for (const auto &e : helpEntries()) {
        if (e.first.empty())
            out.push_back(text(e.second) | faint());
        else
            // A descrição longa é cortada; a coluna das teclas fica com as 24 colunas.
            out.push_back(hbox({text(e.first) | bold | size(WIDTH, EQUAL, 24), text(e.second) | flex}));
    }
    return out;
}

Elements actionsLines(const ViewState &s)
{
    Elements out;
    if (s.toInstall.empty() && s.toRemove.empty()) {
        out.push_back(text(i18n::tr("No packages marked. Use + to install and - to remove.")) | faint());
        return out;
    }
    const IconSet &ic = icons(s.iconMode);
    const auto section = [&](const std::string &title, const std::set<std::string> &keys, const char *icon,
                             Color c) {
        if (keys.empty())
            return;
        out.push_back(text(title + " (" + std::to_string(keys.size()) + ")") | bold);
        for (const auto &k : keys)
            out.push_back(hbox({text(std::string("  ") + icon + " ") | color(c), text(packageLabel(k))}));
    };
    section(i18n::tr("To be installed"), s.toInstall, ic.toInstall, T().green);
    section(i18n::tr("To be removed"), s.toRemove, ic.toRemove, T().red);
    out.push_back(text(keyText(i18n::tr("Alt+M: apply   Ctrl+E: cancel"))) | faint());
    return out;
}

// Linhas da aba Notícias: o tipo (para o estilo) e o texto, o mesmo para o
// desenho e para a busca. A descrição é quebrada na largura da aba
// (contentWidth), para que as linhas desenhadas sejam as que a busca conta.
enum NewsLine { NewsHeader, NewsTitle, NewsLink, NewsBody, NewsNote, NewsBlank };

std::vector<std::pair<NewsLine, std::string>> newsEntries(const ViewState &s)
{
    std::vector<std::pair<NewsLine, std::string>> out;
    out.push_back({NewsHeader, i18n::trf("%1 news", {s.newsSource})});
    if (s.newsLoading)
        out.push_back({NewsNote, i18n::trf("Searching for %1 latest news...", {s.newsSource})});
    if (!s.newsError.empty())
        out.push_back({NewsNote, i18n::trf("WARNING: %1", {s.newsError})
                                     + (s.newsFromCache && !s.news.empty() ? " " + i18n::tr("(showing the saved news)")
                                                                           : std::string())});
    if (s.news.empty() && !s.newsLoading)
        out.push_back({NewsNote, keyText(i18n::tr("No news yet. Ctrl+G gets the latest news."))});
    for (const auto &n : s.news) {
        const bool untitled = n.title.empty();
        const std::string title = untitled ? n.description : n.title;
        out.push_back({NewsBlank, std::string()});
        out.push_back({NewsTitle, n.date.empty() ? title : title + "  —  " + n.date});
        if (!n.link.empty())
            out.push_back({NewsLink, n.link});
        if (!untitled)
            for (const auto &l : wrapText(n.description, s.contentWidth))
                if (!l.empty())
                    out.push_back({NewsBody, l});
    }
    return out;
}

Elements newsLines(const ViewState &s)
{
    Elements out;
    for (const auto &e : newsEntries(s)) {
        switch (e.first) {
        case NewsHeader: out.push_back(text(e.second) | bold); break;
        case NewsTitle:  out.push_back(text(e.second) | bold | color(T().cyan)); break;
        case NewsLink:   out.push_back(text(e.second) | underlined | faint()); break;
        case NewsNote:   out.push_back(text(e.second) | faint()); break;
        case NewsBlank:  out.push_back(text("")); break;
        default:         out.push_back(text(e.second)); break;
        }
    }
    return out;
}

// Aba Serviços: uma linha por serviço, o mesmo texto para o desenho e a busca.
constexpr size_t kServiceName = 24;
constexpr size_t kServiceStatus = 12;
constexpr size_t kServiceUser = 12;

std::string pad(const std::string &s, size_t width)
{
    size_t cols = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80)
            ++cols;
    return cols >= width ? s + " " : s + std::string(width - cols, ' ');
}

std::vector<std::string> serviceEntries(const ViewState &s)
{
    std::vector<std::string> out;
    if (s.servicesLoading && !s.servicesLoaded)
        out.push_back(i18n::tr("Loading…"));
    if (!s.servicesError.empty())
        out.push_back(i18n::trf("ERROR: %1", {s.servicesError}));
    if (s.servicesLoaded && s.services.empty())
        out.push_back(i18n::tr("No installed formula has a service."));
    for (const auto &sv : s.services)
        out.push_back(pad(sv.name, kServiceName) + pad(serviceStatusText(sv.status), kServiceStatus)
                      + pad(sv.user, kServiceUser)
                      + (sv.exitCode > 0 ? i18n::trf("(exit code %1)", {std::to_string(sv.exitCode)}) + "  " : std::string())
                      + sv.file);
    return out;
}

Elements servicesLines(const ViewState &s)
{
    Elements out;
    const std::vector<std::string> entries = serviceEntries(s);
    const size_t first = entries.size() - s.services.size();
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i < first) {
            out.push_back(text(entries[i]) | (s.servicesError.empty() ? faint() : color(T().red)));
            continue;
        }
        const std::string &status = s.services[i - first].status;
        Color c = T().foreground;
        if (status == "started" || status == "scheduled")
            c = T().green;
        else if (status == "error")
            c = T().red;
        Element e = text(entries[i]) | color(c);
        out.push_back(status == "none" || status == "stopped" ? e | faint() : e);
    }
    if (!s.services.empty())
        out.push_back(text(keyText(i18n::tr("Homebrew menu (Alt+H): start, stop or restart the service of the selected formula.")))
                      | faint());
    return out;
}

Elements outputLines(const ViewState &s)
{
    Elements out;
    for (const auto &l : s.log)
        out.push_back(text(l));
    return out;
}

Element tabContent(const ViewState &s, HitBoxes *boxes)
{
    Elements lines;
    switch (s.tab) {
    case FilesTab:  lines = filesLines(s); break;
    case ActionsTab: lines = actionsLines(s); break;
    case OutputTab: lines = outputLines(s); break;
    case NewsTab:   lines = newsLines(s); break;
    case HelpTab:   lines = helpLines(); break;
    case ServicesTab: lines = servicesLines(s); break;
    default:        lines = infoLines(s); break;
    }
    // Ctrl+F: as linhas com o texto ficam destacadas; a do resultado atual, mais.
    if (!s.findText.empty())
        for (int i : findLines(tabText(s), s.findText)) {
            if (i >= static_cast<int>(lines.size()))
                break;
            lines[static_cast<size_t>(i)] = i == s.findMatch ? lines[static_cast<size_t>(i)] | highlight() | bold
                                                             : lines[static_cast<size_t>(i)] | color(T().yellow);
        }
    const int count = static_cast<int>(lines.size());
    if (boxes)
        boxes->contentLines = count;
    const int height = s.findOpen ? std::max(1, s.contentHeight - 1) : s.contentHeight;
    // A Saída mostra o fim, a menos que o usuário a role ou busque nela.
    int first = s.contentScroll;
    if (s.tab == OutputTab && !s.contentFocused && !s.findOpen)
        first = count - height;
    first = std::max(0, std::min(first, count - 1));
    Elements shown(lines.begin() + std::min(first, count), lines.end());
    Element body = vbox(std::move(shown));
    if (s.contentFocused)
        body = hbox({body | flex, text(std::to_string(std::min(first + 1, count)) + "/" + std::to_string(count))
                                      | faint()});
    body = body | size(HEIGHT, EQUAL, height) | yframe;
    if (!s.findOpen)
        return body;
    std::string result;
    if (s.findText.empty())
        result = std::string();
    else if (s.findCount == 0)
        result = i18n::tr("Not found");
    else
        result = std::to_string(s.findMatch < 0 ? 0 : 1 + s.findMatchIndex) + "/" + std::to_string(s.findCount);
    Element bar = hbox({text(i18n::tr("Find:") + " ") | bold, text(s.findText), text("█") | blink,
                        text("  " + result) | (s.findCount == 0 && !s.findText.empty() ? color(T().red) : faint()),
                        filler(), text(keyText(i18n::tr("Enter/F3: next  Shift+F3: previous  Esc: close"))) | faint()});
    return vbox({body, bar});
}

Element statusBar(const ViewState &s)
{
    const IconSet &ic = icons(s.iconMode);
    std::string left = s.status;
    if (s.busy)
        left = std::string(ic.busy) + " " + left;
    else if (s.error)
        left = std::string(ic.error) + " " + left;
    Element l = text(left);
    if (s.error)
        l = l | color(T().red);
    Elements right;
    if (!s.toInstall.empty())
        right.push_back(text("+" + std::to_string(s.toInstall.size()) + "  ") | color(T().green));
    if (!s.toRemove.empty())
        right.push_back(text("-" + std::to_string(s.toRemove.size()) + "  ") | color(T().red));
    if (s.sizing)
        right.push_back(text(i18n::tr("Calculating sizes…") + "  ") | faint());
    right.push_back(text(i18n::trf("Theme: %1", {T().system ? i18n::tr("System") : std::string(T().label)}) + "  ")
                    | faint());
    right.push_back(text(i18n::trf("%1 package(s)", {std::to_string(s.totalPackages)})));
    if (s.outdatedCount > 0)
        right.push_back(text("  " + i18n::trf("%1 outdated", {std::to_string(s.outdatedCount)}))
                        | color(T().yellow));
    Elements all{l | flex};
    for (auto &e : right)
        all.push_back(e);
    return hbox(std::move(all));
}

Element keyBar()
{
    const auto key = [](const char *k, const std::string &label) {
        return hbox({text(k) | bold, text(label + " ") | highlight()});
    };
    // Como no Midnight Commander (e no bectl-tui): teclas de função numeradas.
    return hbox({key("F1", i18n::tr("Help")), text(" "), key("F5", i18n::tr("Refresh")), text(" "),
                 key("F6", i18n::tr("Sort")), text(" "), key("F8", i18n::tr("Tap")), text(" "),
                 key("F9", i18n::tr("Menu")), text(" "), key("F10", i18n::tr("Exit")), text(" "),
                 key("+/-", i18n::tr("Mark")), text(" "),
#ifdef __APPLE__
                 key("⌥M", i18n::tr("Apply")),
#else
                 key("M-m", i18n::tr("Apply")),   // como no mc
#endif
                 filler()});
}

Element button(const std::string &label, bool focused, bool danger)
{
    Element b = text("[ " + label + " ]");
    if (focused)
        b = b | highlight() | bold;
    if (danger && focused)
        b = b | color(T().red);
    return b;
}

Elements paragraph(const std::string &message)
{
    Elements lines;
    size_t start = 0;
    for (;;) {
        const size_t nl = message.find('\n', start);
        lines.push_back(text(message.substr(start, nl == std::string::npos ? std::string::npos : nl - start)));
        if (nl == std::string::npos)
            break;
        start = nl + 1;
    }
    return lines;
}

Element formDialog(const Dialog &d)
{
    Elements rows;
    if (!d.message.empty()) {
        rows = paragraph(d.message);
        rows.push_back(text(""));
    }
    for (size_t i = 0; i < d.fields.size(); ++i) {
        const DialogField &f = d.fields[i];
        const bool focused = static_cast<int>(i) == d.focus;
        Element value;
        switch (f.kind) {
        case DialogField::Text:
            value = hbox({text(f.text), focused ? text("█") | blink : text("")}) | size(WIDTH, GREATER_THAN, 30)
                    | borderLight;
            break;
        case DialogField::Check:
            value = text(std::string(f.checked ? "[x] " : "[ ] ") + f.label);
            break;
        case DialogField::Choice:
            value = text("< " + (f.choices.empty() ? std::string() : f.choices[f.choice]) + " >")
                    | size(WIDTH, GREATER_THAN, 30);
            break;
        }
        if (focused && f.kind != DialogField::Text)
            value = value | highlight();
        if (f.kind == DialogField::Check)
            rows.push_back(value);
        else
            rows.push_back(hbox({text(f.label) | size(WIDTH, EQUAL, 28), value}));
        if (!f.hint.empty())
            rows.push_back(text("    " + f.hint) | faint());
    }
    const int ok = static_cast<int>(d.fields.size());
    rows.push_back(text(""));
    rows.push_back(hbox({filler(), button(i18n::tr("OK"), d.focus == ok, d.danger), text("  "),
                         button(i18n::tr("Cancel"), d.focus == ok + 1, false)}));
    rows.push_back(text(i18n::tr("Tab / ↑ ↓: field   Space: check   ← →: choose   Enter: confirm   Esc: cancel")) | faint());
    Element w = window(text(" " + d.title + " ") | bold, vbox(std::move(rows)));
    return d.danger ? w | color(T().red) : w;
}

Element confirmDialog(const Dialog &d)
{
    Elements rows = paragraph(d.message);
    rows.push_back(text(""));
    rows.push_back(hbox({filler(), button(i18n::tr("Yes"), d.focus == 0, d.danger), text("  "),
                         button(i18n::tr("No"), d.focus == 1, false)}));
    Element w = window(text(" " + d.title + " ") | bold, vbox(std::move(rows)));
    return d.danger ? w | color(T().red) : w;
}

Element messageDialog(const Dialog &d)
{
    Elements rows = paragraph(d.message);
    rows.push_back(text(""));
    rows.push_back(hbox({filler(), button(i18n::tr("OK"), true, false)}));
    return window(text(" " + d.title + " ") | bold, vbox(std::move(rows)));
}

Element menuDialog(const Dialog &d)
{
    Elements titles;
    for (size_t m = 0; m < d.menus.size(); ++m) {
        Element t = text(" " + d.menus[m] + " ");
        titles.push_back(static_cast<int>(m) == d.menu ? t | highlight() | bold : t);
    }
    Elements items;
    if (d.menu >= 0 && d.menu < static_cast<int>(d.items.size())) {
        const auto &list = d.items[d.menu];
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].first.empty()) {
                items.push_back(sepLight());
                continue;
            }
            Element e = hbox({text(" " + list[i].first) | flex, text("  " + keyLabel(list[i].second) + " ") | faint()});
            items.push_back(static_cast<int>(i) == d.item ? e | highlight() : e);
        }
    }
    return vbox({hbox(std::move(titles)), window(text(""), vbox(std::move(items)) | size(WIDTH, GREATER_THAN, 40))});
}

// F8: lista rolável dos taps, com o atual marcado.
Element categoryChooser(const ViewState &s)
{
    constexpr int kVisible = 16;
    const int total = static_cast<int>(s.categories.size()) + 1;
    const int first = std::max(0, std::min(s.categoryIndex - kVisible / 2, total - kVisible));
    int all = 0;
    for (const auto &c : s.categories)
        all += c.second;
    Elements items;
    for (int i = first; i < total && i < first + kVisible; ++i) {
        const bool isAll = i == 0;
        const std::string name = isAll ? i18n::tr("(all taps)") : s.categories[static_cast<size_t>(i - 1)].first;
        const int count = isAll ? all : s.categories[static_cast<size_t>(i - 1)].second;
        const bool current = isAll ? s.category.empty() : s.category == name;
        Element e = hbox({text(std::string(current ? " • " : "   ") + name) | flex,
                          text(" " + std::to_string(count) + " ") | faint()});
        if (i == s.categoryIndex)
            e = e | highlight();
        items.push_back(e);
    }
    const std::string pos = std::to_string(s.categoryIndex + 1) + "/" + std::to_string(total);
    items.push_back(text(i18n::tr("Enter: choose   a-z: jump   Esc: close") + "   " + pos) | faint());
    return window(text(" " + i18n::tr("Tap") + " "), vbox(std::move(items)) | size(WIDTH, GREATER_THAN, 36))
           | clear_under | center;
}

Element sortChooser(const ViewState &s)
{
    Elements items;
    for (int i = 0; i <= model::ColumnCount; ++i) {
        const std::string label = i == 0 ? i18n::tr("(brew order)") : columnTitle(i - 1) + sortMark(s, i - 1);
        Element e = text(" " + label + " ");
        if (i == s.sortChooserIndex)
            e = e | highlight();
        items.push_back(e);
    }
    return window(text(" " + i18n::tr("Sort by") + " "), vbox(std::move(items))) | clear_under | center;
}

// Largura em colunas de um texto UTF-8 (um caractere por coluna).
size_t columns(const std::string &s)
{
    size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80)
            ++n;
    return n;
}

} // namespace

std::string keyText(const std::string &text)
{
#ifdef __APPLE__
    std::string out = text;
    for (const auto &r : {std::make_pair(std::string("Ctrl+"), std::string("⌃")),
                          std::make_pair(std::string("Alt+"), std::string("⌥")),
                          std::make_pair(std::string("Shift+"), std::string("⇧"))})
        for (size_t pos = 0; (pos = out.find(r.first, pos)) != std::string::npos; pos += r.second.size())
            out.replace(pos, r.first.size(), r.second);
    return out;
#else
    return text;
#endif
}

std::string keyLabel(const std::string &keys)
{
#ifdef __APPLE__
    // Palavra a palavra, mantendo os espaços que separam as alternativas.
    static const std::vector<std::pair<std::string, std::string>> names{
        {"PgUp", "fn↑"}, {"PgDn", "fn↓"}, {"Home", "fn←"}, {"End", "fn→"},
        {"Delete", "⌦"}, {"Enter", "↩"}, {"Tab", "⇥"}};
    std::vector<std::string> words;
    size_t start = 0;
    for (;;) {
        const size_t space = keys.find(' ', start);
        std::string w = keys.substr(start, space == std::string::npos ? std::string::npos : space - start);
        for (const auto &n : names)
            if (w == n.first)
                w = n.second;
        if (w != "Insert")
            words.push_back(w);
        if (space == std::string::npos)
            break;
        start = space + 1;
    }
    std::string out;
    for (size_t i = 0; i < words.size(); ++i)
        out += (i ? " " : "") + words[i];
    out.erase(out.find_last_not_of(' ') + 1);
    return keyText(out);
#else
    return keys;
#endif
}

std::vector<std::string> wrapText(const std::string &text, int width)
{
    const size_t w = static_cast<size_t>(std::max(20, width));
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos)
            nl = text.size();
        const std::string para = text.substr(start, nl - start);
        // O recuo do parágrafo continua nas linhas quebradas (os caveats usam).
        const size_t indentEnd = para.find_first_not_of(' ');
        const std::string indent = indentEnd == std::string::npos ? std::string() : para.substr(0, indentEnd);
        std::string line;
        size_t pos = indentEnd == std::string::npos ? para.size() : indentEnd;
        bool any = false;
        while (pos < para.size()) {
            size_t end = para.find(' ', pos);
            if (end == std::string::npos)
                end = para.size();
            const std::string word = para.substr(pos, end - pos);
            if (!word.empty()) {
                if (any && columns(indent + line) + 1 + columns(word) > w) {
                    out.push_back(indent + line);
                    line.clear();
                    any = false;
                }
                line += (any ? " " : "") + word;
                any = true;
            }
            pos = end + 1;
        }
        out.push_back(any ? indent + line : std::string());
        start = nl + 1;
    }
    return out;
}

std::vector<std::string> tabText(const ViewState &s)
{
    std::vector<std::string> out;
    switch (s.tab) {
    case FilesTab:
        // Só quando a aba mostra a lista (pacote instalado, detalhes carregados).
        if (s.hasCurrent && s.current.installed && s.detailsReady && s.detailsError.empty()
            && !s.details.filesTruncated)
            out = s.details.files;
        break;
    case OutputTab:
        out = s.log;
        break;
    case NewsTab:
        for (const auto &e : newsEntries(s))
            out.push_back(e.second);
        break;
    case ServicesTab:
        out = serviceEntries(s);
        break;
    case HelpTab:
        for (const auto &e : helpEntries())
            out.push_back(e.first.empty() ? e.second : e.first + "  " + e.second);
        break;
    default:
        break;
    }
    return out;
}

std::vector<int> findLines(const std::vector<std::string> &lines, const std::string &needle)
{
    const auto lower = [](std::string x) {
        for (auto &c : x)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return x;
    };
    std::vector<int> out;
    if (needle.empty())
        return out;
    const std::string n = lower(needle);
    for (size_t i = 0; i < lines.size(); ++i)
        if (lower(lines[i]).find(n) != std::string::npos)
            out.push_back(static_cast<int>(i));
    return out;
}

char tabKey(int tab)
{
    static const char keys[TabCount] = {'1', '2', '3', '4', '5', '6', '7'};
    return tab >= 0 && tab < TabCount ? keys[tab] : '?';
}

std::string columnTitle(int column)
{
    switch (column) {
    case model::StateIcon:   return i18n::tr("Status");
    case model::Name:        return i18n::tr("Name");
    case model::Type:        return i18n::tr("Type");
    case model::Version:     return i18n::tr("Version");
    case model::Size:        return i18n::tr("Size");
    case model::InstalledOn: return i18n::tr("Installed on");
    default:                 return std::string();
    }
}

std::string packageLabel(const std::string &key)
{
    if (key.rfind("c:", 0) == 0)
        return key.substr(2) + " (cask)";
    return key.rfind("f:", 0) == 0 ? key.substr(2) : key;
}

std::string serviceStatusText(const std::string &status)
{
    if (status == "started")
        return i18n::tr("Started");
    if (status == "stopped")
        return i18n::tr("Stopped");
    if (status == "scheduled")
        return i18n::tr("Scheduled");
    if (status == "error")
        return i18n::tr("Error");
    if (status == "none")
        return i18n::tr("Not running");
    return status;
}

std::string kindText(octo::Kind kind)
{
    return kind == octo::Kind::Cask ? i18n::tr("Cask") : i18n::tr("Formula");
}

std::string statusText(const octo::Package &p)
{
    switch (p.status) {
    case octo::Status::Pinned:
        return p.newVersion.empty() ? i18n::tr("Pinned")
                                    : i18n::tr("Pinned") + " " + i18n::trf("(version %1 is available)", {p.newVersion});
    case octo::Status::Outdated:
        return i18n::tr("Outdated") + " " + i18n::trf("(version %1 is available)", {p.newVersion});
    case octo::Status::Newer:
        return i18n::tr("Newer than the repository") + i18n::trf(" (newer installed version is %1)",
                                                                 {p.installedVersion});
    case octo::Status::NotInstalled:
        return i18n::tr("Not installed");
    default:
        return p.automatic ? i18n::tr("Installed as a dependency") : i18n::tr("Installed");
    }
}

Element render(const ViewState &s, Element filter, HitBoxes *boxes)
{
    g_theme = theme(s.themeId);
    const IconSet &ic = icons(s.iconMode);
    const std::string view = s.remoteView ? std::string(ic.remoteView) + " " + i18n::tr("Repository")
                                          : std::string(ic.installedView) + " " + i18n::tr("Installed packages");
    Elements body;
    body.push_back(hbox({text(" OctoBrew — Homebrew ") | bold, text("[" + view + (s.category.empty() ? std::string() : " · " + s.category)
                   + (s.leavesOnly ? " · " + i18n::tr("leaves") : std::string()) + "]") | color(T().cyan), filler(),
              text(std::string(ic.filter) + " "),
              s.searchMode == model::SearchMode::Description ? text(i18n::tr("Description") + ": ") | faint()
                                                             : text(""),
              filter | size(WIDTH, EQUAL, 30)}));
    body.push_back(sep());
    // F11: só a lista, só as abas, ou as duas.
    if (s.layout != Layout::TabsOnly)
        body.push_back(table(s, boxes) | flex);
    if (s.layout == Layout::Normal)
        body.push_back(sep());
    if (s.layout != Layout::ListOnly) {
        body.push_back(tabs(s, boxes));
        Element content = tabContent(s, boxes);
        body.push_back(s.layout == Layout::TabsOnly ? content | flex : content);
    }
    if (boxes && s.layout == Layout::ListOnly)
        boxes->tabs = {};   // sem abas na tela: nenhum clique cai nelas
    if (boxes && s.layout == Layout::TabsOnly) {
        boxes->rows.clear();
        boxes->header = {};
    }
    body.push_back(sep());
    body.push_back(statusBar(s));
    body.push_back(keyBar());
    Element main = vbox(std::move(body));
    // Fora do tema do sistema, o fundo e o texto do tema valem para a tela
    // inteira; os diálogos ficam sobre o painel.
    const auto paint = [](Element e, bool panel) {
        if (T().system)
            return e;
        return e | bgcolor(panel ? T().panel : T().background) | color(T().foreground);
    };
    main = paint(main, false);
    if (s.sortChooserOpen)
        return dbox({main, paint(sortChooser(s), true)});
    if (s.categoryChooserOpen)
        return dbox({main, paint(categoryChooser(s), true)});
    switch (s.dialog.kind) {
    case Dialog::Form:    return dbox({main, paint(formDialog(s.dialog), true) | clear_under | center});
    case Dialog::Confirm: return dbox({main, paint(confirmDialog(s.dialog), true) | clear_under | center});
    case Dialog::Message: return dbox({main, paint(messageDialog(s.dialog), true) | clear_under | center});
    case Dialog::Menu:    return dbox({main, paint(menuDialog(s.dialog), true) | clear_under});
    case Dialog::None:    break;
    }
    return main;
}

} // namespace tui
