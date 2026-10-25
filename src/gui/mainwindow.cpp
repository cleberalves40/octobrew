// SPDX-License-Identifier: GPL-2.0-or-later

#include "mainwindow.h"
#include "listmodel.h"
#include "packagemodel.h"
#include "pages.h"
#include "strings.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString qs(const std::string &s)
{
    return QString::fromStdString(s);
}

// As preferências ficam em $XDG_CONFIG_HOME/octobrew/config (ou ~/.config),
// separadas das do octobrew-tui.
QString defaultSettingsPath()
{
    const QByteArray xdg = qgetenv("XDG_CONFIG_HOME");
    if (!xdg.isEmpty())
        return QString::fromLocal8Bit(xdg) + "/octobrew/config";
    return QString::fromLocal8Bit(qgetenv("HOME")) + "/.config/octobrew/config";
}

constexpr int kDetailsDelayMs = 150;   // entre as setas, não pede o brew info de cada linha

} // namespace

template <typename R>
void MainWindow::runAsync(std::function<R()> work, std::function<void(const R &)> done)
{
    if (m_options.synchronous) {
        done(work());
        return;
    }
    auto *watcher = new QFutureWatcher<R>(this);
    m_pending.append(watcher);
    connect(watcher, &QFutureWatcher<R>::finished, this, [this, watcher, done] {
        m_pending.removeOne(watcher);
        const R result = watcher->result();
        watcher->deleteLater();
        done(result);
    });
    watcher->setFuture(QtConcurrent::run(work));
}

MainWindow::MainWindow(const WindowOptions &options, QWidget *parent)
    : QMainWindow(parent)
    , m_options(options)
    , m_settings((options.settingsPath.isEmpty() ? defaultSettingsPath() : options.settingsPath).toStdString())
    , m_mgr(options.brewPath.toStdString())
    , m_remoteView(options.remote)
{
    setWindowTitle(QStringLiteral("OctoBrew"));
    setWindowIcon(QIcon(QStringLiteral(":/images/octopi_green-big.png")));
    m_model = new PackageModel(this);
    m_detailsTimer = new QTimer(this);
    m_detailsTimer->setSingleShot(true);
    m_detailsTimer->setInterval(kDetailsDelayMs);
    connect(m_detailsTimer, &QTimer::timeout, this, &MainWindow::showDetails);
    buildUi();
    buildActions();
    restoreState();
    setStatus(ui::tr("Ready."));
    m_info->setHtml(pages::emptyInfoHtml());
}

MainWindow::~MainWindow()
{
    m_cancelSizes = true;
    for (QFutureWatcherBase *w : m_pending)
        w->waitForFinished();
}

void MainWindow::buildUi()
{
    // A lista, com o desenho do OctoPkg: uma linha por pacote, o ícone do estado
    // na primeira coluna e a ordem pelo clique no cabeçalho.
    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);
    m_table->setAlternatingRowColors(true);
    m_table->setIconSize(QSize(16, 16));
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->verticalHeader()->setDefaultSectionSize(m_table->fontMetrics().height() + 6);
    QHeaderView *header = m_table->horizontalHeader();
    header->setHighlightSections(false);
    header->setSectionResizeMode(model::Name, QHeaderView::Stretch);
    header->setSectionResizeMode(model::StateIcon, QHeaderView::Fixed);
    header->resizeSection(model::StateIcon, 28);
    header->resizeSection(model::Type, 80);
    header->resizeSection(model::Version, 150);
    header->resizeSection(model::Size, 100);
    header->resizeSection(model::InstalledOn, 140);
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &MainWindow::selectionChanged);
    connect(m_model, &QAbstractItemModel::modelReset, this, &MainWindow::updateCounts);

    // À esquerda, os taps, no lugar dos grupos do OctoPkg.
    m_taps = new QListWidget;
    m_taps->setMinimumWidth(150);
    connect(m_taps, &QListWidget::currentRowChanged, this, &MainWindow::applyFilter);

    m_info = new QTextBrowser;
    m_info->setOpenExternalLinks(true);
    m_files = new QPlainTextEdit;
    m_files->setReadOnly(true);
    m_files->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output = new QPlainTextEdit;
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output->setMaximumBlockCount(5000);
    m_help = new QTextBrowser;
    m_tabs = new QTabWidget;
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(m_info, QIcon(QStringLiteral(":/images/find.png")), ui::tr("Info"));
    m_tabs->addTab(m_files, ui::tr("Files"));
    m_tabs->addTab(m_output, QIcon(QStringLiteral(":/images/terminal.png")), ui::tr("Output"));
    m_tabs->addTab(m_help, ui::tr("Help"));

    m_horizontal = new QSplitter(Qt::Horizontal);
    m_horizontal->addWidget(m_taps);
    m_horizontal->addWidget(m_table);
    m_horizontal->setStretchFactor(1, 1);
    m_horizontal->setSizes({170, 830});
    m_vertical = new QSplitter(Qt::Vertical);
    m_vertical->addWidget(m_horizontal);
    m_vertical->addWidget(m_tabs);
    m_vertical->setStretchFactor(0, 3);
    m_vertical->setStretchFactor(1, 2);
    setCentralWidget(m_vertical);

    // A barra de status do OctoPkg: a mensagem e as contagens.
    m_statusLabel = new QLabel;
    m_sizingLabel = new QLabel;
    m_countLabel = new QLabel;
    m_outdatedLabel = new QLabel;
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_sizingLabel);
    statusBar()->addPermanentWidget(m_countLabel);
    statusBar()->addPermanentWidget(m_outdatedLabel);

    m_search = new QLineEdit;
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(320);
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::applyFilter);
    m_viewCombo = new QComboBox;
    m_viewCombo->addItem(QIcon(QStringLiteral(":/images/installed.png")), ui::tr("Installed packages"));
    m_viewCombo->addItem(QIcon(QStringLiteral(":/images/cloud.png")), ui::tr("Repository"));
    connect(m_viewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if ((index == 1) != m_remoteView)
            toggleView();
    });
    resize(1100, 760);
}

void MainWindow::buildActions()
{
    // No macOS, o Qt mostra o Ctrl como ⌘ (o Command), que numa GUI chega ao
    // programa; o Alt é o ⌥ (Option).
    const auto add = [this](const QString &name, const QString &text, const QList<QKeySequence> &keys,
                            std::function<void()> slot) {
        auto *a = new QAction(text, this);
        a->setShortcuts(keys);
        connect(a, &QAction::triggered, this, slot);
        m_actions.insert(name, a);
        addAction(a);   // os atalhos valem mesmo sem o menu aberto
        return a;
    };
    QAction *refreshAction = add("refresh", ui::tr("Refresh the package list"),
                                 {QKeySequence(Qt::Key_F5), QKeySequence(QStringLiteral("Ctrl+R"))}, [this] { refresh(); });
    refreshAction->setIcon(QIcon(QStringLiteral(":/images/refresh.png")));
    refreshAction->setIconText(ui::tr("Refresh"));
    add("toggleView", ui::tr("Switch between installed and repository packages"), {QKeySequence(QStringLiteral("Ctrl+I"))},
        [this] { toggleView(); });
    QAction *quit = add("quit", ui::tr("Exit"), {QKeySequence::Quit}, [this] { close(); });
    quit->setMenuRole(QAction::QuitRole);

    add("findPackage", ui::tr("Find a package in the package list"), {QKeySequence(QStringLiteral("Ctrl+L"))}, [this] {
        m_search->setFocus();
        m_search->selectAll();
    });
    QAction *description = add("searchDescription", ui::tr("Search by description"), {QKeySequence(Qt::Key_F7)},
                               [this] { setSearchMode(m_actions.value("searchDescription")->isChecked()); });
    description->setCheckable(true);
    description->setIconText(ui::tr("Description"));
    QAction *leaves = add("leaves", ui::tr("Only leaves (brew leaves)"), {QKeySequence(QStringLiteral("Ctrl+Shift+L"))},
                          [this] { applyFilter(); });
    leaves->setCheckable(true);
    leaves->setIcon(QIcon(QStringLiteral(":/images/unrequired.png")));
    leaves->setIconText(ui::tr("Leaves"));

    // As abas, com os números do OctoPkg.
    add("tabInfo", ui::tr("Info"), {QKeySequence(QStringLiteral("Alt+1"))}, [this] { setTab(InfoTab); });
    add("tabFiles", ui::tr("Files"), {QKeySequence(QStringLiteral("Alt+2"))}, [this] { setTab(FilesTab); });
    add("tabOutput", ui::tr("Output"), {QKeySequence(QStringLiteral("Alt+4"))}, [this] { setTab(OutputTab); });
    add("tabHelp", ui::tr("Help"), {QKeySequence(QStringLiteral("Alt+6")), QKeySequence(Qt::Key_F1)},
        [this] { setTab(HelpTab); });
    QAction *aboutAction = add("about", ui::tr("About"), {}, [this] { about(); });
    aboutAction->setMenuRole(QAction::AboutRole);
    QAction *aboutQt = add("aboutQt", ui::tr("About Qt"), {}, [] { QApplication::aboutQt(); });
    aboutQt->setMenuRole(QAction::AboutQtRole);

    QMenu *file = menuBar()->addMenu(ui::tr("File"));
    file->addAction(m_actions["refresh"]);
    file->addAction(m_actions["toggleView"]);
    file->addSeparator();
    file->addAction(quit);
    QMenu *search = menuBar()->addMenu(ui::tr("Search"));
    search->addAction(m_actions["findPackage"]);
    search->addAction(description);
    search->addSeparator();
    search->addAction(leaves);
    QMenu *view = menuBar()->addMenu(ui::tr("View"));
    for (const char *t : {"tabInfo", "tabFiles", "tabOutput", "tabHelp"})
        view->addAction(m_actions[t]);
    QMenu *help = menuBar()->addMenu(ui::tr("Help"));
    help->addAction(m_actions["tabHelp"]);
    help->addAction(aboutAction);
    help->addAction(aboutQt);

    QToolBar *bar = addToolBar(ui::tr("Main toolbar"));
    bar->setObjectName(QStringLiteral("mainToolBar"));
    bar->setMovable(false);
    bar->addWidget(m_viewCombo);
    bar->addAction(refreshAction);
    bar->addSeparator();
    bar->addAction(leaves);
    // Na barra, as alternâncias com o nome ao lado: só o ícone não diz o que fazem.
    for (QAction *a : {leaves, description})
        if (auto *button = qobject_cast<QToolButton *>(bar->widgetForAction(a)))
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    auto *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(spacer);
    bar->addAction(description);
    bar->addWidget(m_search);
    setUnifiedTitleAndToolBarOnMac(true);

    // A aba Ajuda: os atalhos de cada ação, na notação do sistema (⌘R, ⌥1…).
    QList<QPair<QString, QString>> shortcuts;
    for (const char *name : {"refresh", "toggleView", "findPackage", "searchDescription", "leaves", "tabInfo", "tabFiles",
                             "tabOutput", "tabHelp", "quit"}) {
        const QAction *a = m_actions[name];
        QStringList keys;
        for (const QKeySequence &k : a->shortcuts())
            keys << k.toString(QKeySequence::NativeText);
        shortcuts.append({keys.join(QStringLiteral("  ")), a->text()});
    }
    m_help->setHtml(pages::helpHtml(shortcuts));
}

void MainWindow::restoreState()
{
    int column = m_settings.intValue("sort.column", model::Name);
    if (column < 0 || column >= model::ColumnCount)
        column = model::Name;
    const Qt::SortOrder order = m_settings.value("sort.order", "asc") == "desc" ? Qt::DescendingOrder : Qt::AscendingOrder;
    m_table->horizontalHeader()->setSortIndicator(column, order);
    m_table->setSortingEnabled(true);
    m_model->sort(column, order);
    connect(m_table->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int col, Qt::SortOrder o) {
        m_settings.setValue("sort.column", col);
        m_settings.setValue("sort.order", o == Qt::AscendingOrder ? "asc" : "desc");
        m_settings.save();
    });
    const bool description = m_settings.value("search", "name") == "description";
    m_actions["searchDescription"]->setChecked(description);
    setSearchMode(description);
    const QByteArray geometry = QByteArray::fromBase64(QByteArray::fromStdString(m_settings.value("window.geometry")));
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    const QByteArray split = QByteArray::fromBase64(QByteArray::fromStdString(m_settings.value("window.splitter")));
    if (!split.isEmpty())
        m_vertical->restoreState(split);
    m_viewCombo->blockSignals(true);
    m_viewCombo->setCurrentIndex(m_remoteView ? 1 : 0);
    m_viewCombo->blockSignals(false);
}

void MainWindow::saveState()
{
    m_settings.setValue("window.geometry", saveGeometry().toBase64().toStdString());
    m_settings.setValue("window.splitter", m_vertical->saveState().toBase64().toStdString());
    m_settings.save();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveState();
    m_cancelSizes = true;
    QMainWindow::closeEvent(event);
}

QAction *MainWindow::action(const QString &name) const
{
    return m_actions.value(name);
}

QString MainWindow::statusText() const
{
    return m_status;
}

QString MainWindow::countText() const
{
    return m_countLabel->text();
}

void MainWindow::log(const QString &message)
{
    m_output->appendPlainText(QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message));
}

void MainWindow::setStatus(const QString &message, bool error)
{
    m_status = message;
    m_statusError = error;
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet(error ? QStringLiteral("color: #c03030") : QString());
}

void MainWindow::updateCounts()
{
    m_countLabel->setText(ui::trf("%1 package(s)", {QString::number(m_model->packages().size())}));
    const int outdated = m_model->outdatedCount();
    m_outdatedLabel->setText(outdated > 0 ? ui::trf("%1 outdated", {QString::number(outdated)}) : QString());
    m_outdatedLabel->setStyleSheet(QStringLiteral("color: #c08000"));
}

// ---------------------------------------------------------------------------
// Listagem, em segundo plano
// ---------------------------------------------------------------------------

void MainWindow::refresh()
{
    if (m_busy)
        return;
    m_busy = true;
    const bool withRemote = m_remoteView;
    setStatus(withRemote ? ui::tr("Listing repository packages…") : ui::tr("Listing installed packages…"));
    // Uma nova listagem para um cálculo de tamanhos em andamento.
    m_cancelSizes = true;
    const int generation = ++m_generation;
    runAsync<Listing>(
        [this, withRemote] {
            Listing l;
            try {
                l.installed = m_mgr.listInstalled();
                if (withRemote) {
                    std::string warning;
                    l.remote = m_mgr.listRemote(&warning);
                    l.warning = qs(warning);
                    l.hasRemote = true;
                }
            } catch (const std::exception &e) {
                l.error = QString::fromUtf8(e.what());
            }
            return l;
        },
        [this, generation](const Listing &l) { applyListing(l, generation); });
}

void MainWindow::applyListing(const Listing &l, int generation)
{
    if (generation != m_generation)
        return;
    m_busy = false;
    m_details.clear();
    m_detailErrors.clear();
    if (!l.error.isEmpty()) {
        setStatus(ui::trf("ERROR: %1", {l.error}), true);
        log(m_status);
        if (m_remoteView && !m_hasRemote && !l.installed.empty()) {
            m_remoteView = false;   // sem o catálogo do repositório, volta aos instalados
            m_viewCombo->blockSignals(true);
            m_viewCombo->setCurrentIndex(0);
            m_viewCombo->blockSignals(false);
        }
        if (l.installed.empty())
            return;
    }
    m_installed = l.installed;
    for (auto &p : m_installed) {   // os tamanhos da medição anterior ficam até a nova
        const auto it = m_sizes.find(p.key());
        if (it != m_sizes.end())
            p.size = it->second;
    }
    if (l.hasRemote) {
        m_remote = l.remote;
        m_hasRemote = true;
    }
    rebuildView();
    if (l.error.isEmpty())
        setStatus(ui::tr("Ready."));
    int formulae = 0, casks = 0;
    for (const auto &p : m_installed)
        ++(p.kind == octo::Kind::Cask ? casks : formulae);
    log(ui::trf("%1 package(s) installed: %2 formula(e) and %3 cask(s).",
                {QString::number(m_installed.size()), QString::number(formulae), QString::number(casks)}));
    if (l.hasRemote)
        log(ui::trf("%1 package(s) in the repository.", {QString::number(m_remote.size())}));
    if (!l.warning.isEmpty())
        log(ui::trf("WARNING: %1", {l.warning}));
    if (!l.error.isEmpty() || m_installed.empty())
        return;

    // Depois da lista, o espaço em disco de cada instalado.
    m_cancelSizes = false;
    m_sizing = true;
    m_model->setSizing(true);
    m_sizingLabel->setText(ui::tr("Calculating sizes…"));
    const std::vector<octo::Package> installed = m_installed;
    runAsync<std::map<std::string, std::int64_t>>(
        [this, installed] {
            try {
                return m_mgr.sizes(installed, &m_cancelSizes);
            } catch (const std::exception &) {
                return std::map<std::string, std::int64_t>();   // sem os tamanhos, a lista continua
            }
        },
        [this, generation](const std::map<std::string, std::int64_t> &sizes) { applySizes(sizes, generation); });
}

void MainWindow::applySizes(const std::map<std::string, std::int64_t> &sizes, int generation)
{
    if (generation != m_generation)
        return;
    m_sizing = false;
    m_model->setSizing(false);
    m_sizingLabel->clear();
    if (m_cancelSizes)
        return;
    m_sizes = sizes;
    std::int64_t total = 0;
    for (auto &p : m_installed) {
        const auto it = m_sizes.find(p.key());
        p.size = it == m_sizes.end() ? 0 : it->second;
        total += p.size;
    }
    log(ui::trf("Disk usage of the installed packages: %1", {qs(octo::formatSize(total))}));
    rebuildView();
}

void MainWindow::toggleView()
{
    if (m_busy) {
        setStatus(ui::tr("Wait for the current operation to finish."));
        return;
    }
    m_remoteView = !m_remoteView;
    m_viewCombo->blockSignals(true);
    m_viewCombo->setCurrentIndex(m_remoteView ? 1 : 0);
    m_viewCombo->blockSignals(false);
    if (m_remoteView && !m_hasRemote) {
        refresh();
        return;
    }
    rebuildView();
}

void MainWindow::rebuildView()
{
    // A seleção acompanha o pacote, não a linha.
    const QString key = currentKey();
    m_model->setPackages(m_remoteView && m_hasRemote ? octo::mergeRemote(m_remote, m_installed) : m_installed);
    rebuildTaps();
    if (!selectKey(key) && m_model->rowCount() > 0)
        m_table->selectRow(0);
}

void MainWindow::rebuildTaps()
{
    const QString currentTap = m_taps->currentItem() ? m_taps->currentItem()->data(Qt::UserRole).toString() : QString();
    m_taps->blockSignals(true);
    m_taps->clear();
    const auto taps = model::categories(m_model->packages());
    auto *all = new QListWidgetItem(ui::tr("(all taps)") + QStringLiteral("  ") + QString::number(m_model->packages().size()));
    all->setData(Qt::UserRole, QString());
    m_taps->addItem(all);
    int select = 0;
    for (size_t i = 0; i < taps.size(); ++i) {
        auto *item = new QListWidgetItem(qs(taps[i].first) + QStringLiteral("  ") + QString::number(taps[i].second));
        item->setData(Qt::UserRole, qs(taps[i].first));
        m_taps->addItem(item);
        if (qs(taps[i].first) == currentTap)
            select = static_cast<int>(i) + 1;
    }
    m_taps->setCurrentRow(select);
    m_taps->blockSignals(false);
    applyFilter();
}

void MainWindow::applyFilter()
{
    const QString key = currentKey();
    const QString tap = m_taps->currentItem() ? m_taps->currentItem()->data(Qt::UserRole).toString() : QString();
    const bool description = m_actions.value("searchDescription") && m_actions["searchDescription"]->isChecked();
    const bool leaves = m_actions.value("leaves") && m_actions["leaves"]->isChecked();
    m_model->setFilter(m_search->text(), description ? model::SearchMode::Description : model::SearchMode::Name, tap,
                       leaves);
    if (!selectKey(key) && m_model->rowCount() > 0)
        m_table->selectRow(0);
    if (m_model->rowCount() == 0)
        selectionChanged();
}

void MainWindow::setSearchMode(bool description)
{
    // Como o menu Busca do OctoPkg; a escolha fica salva.
    m_search->setPlaceholderText(description ? ui::tr("Filter by description…") : ui::tr("Filter by name…"));
    m_settings.setValue("search", description ? "description" : "name");
    m_settings.save();
    applyFilter();
}

// ---------------------------------------------------------------------------
// Seleção e detalhes
// ---------------------------------------------------------------------------

const octo::Package *MainWindow::current() const
{
    const QModelIndex index = m_table->currentIndex();
    return index.isValid() ? m_model->packageAt(index.row()) : nullptr;
}

QString MainWindow::currentKey() const
{
    const octo::Package *p = current();
    return p ? qs(p->key()) : QString();
}

bool MainWindow::selectKey(const QString &key)
{
    const int row = key.isEmpty() ? -1 : m_model->rowOf(key);
    if (row < 0)
        return false;
    m_table->selectRow(row);
    m_table->scrollTo(m_model->index(row, model::Name));
    return true;
}

QString MainWindow::detailsKey(const octo::Package &p) const
{
    return (p.installed ? QStringLiteral("I:") : QStringLiteral("R:")) + qs(p.key());
}

void MainWindow::selectionChanged()
{
    if (m_options.synchronous)
        showDetails();
    else
        m_detailsTimer->start();
}

void MainWindow::loadDetails()
{
    m_detailsTimer->stop();
    showDetails();
}

void MainWindow::showDetails()
{
    const octo::Package *p = current();
    if (!p) {
        m_info->setHtml(pages::emptyInfoHtml());
        m_files->clear();
        m_tabs->setTabText(FilesTab, ui::tr("Files"));
        return;
    }
    const octo::Package pkg = *p;
    const QString key = detailsKey(pkg);
    const auto render = [this, pkg, key] {
        const auto d = m_details.find(key);
        const octo::Details *details = d == m_details.end() ? nullptr : &d.value();
        m_info->setHtml(pages::infoHtml(pkg, details, m_detailErrors.value(key)));
        QString filesTitle = ui::tr("Files");
        if (!pkg.installed)
            m_files->setPlainText(ui::tr("Files are listed for installed packages only."));
        else if (!m_detailErrors.value(key).isEmpty())
            m_files->setPlainText(ui::trf("ERROR: %1", {m_detailErrors.value(key)}));
        else if (!details)
            m_files->setPlainText(ui::tr("Loading…"));
        else if (details->filesTruncated)
            m_files->setPlainText(ui::tr("This package has too many files to be listed."));
        else {
            QStringList files;
            for (const auto &f : details->files)
                files << qs(f);
            m_files->setPlainText(files.isEmpty() ? ui::tr("No files.") : files.join('\n'));
            filesTitle += QStringLiteral(" (%1)").arg(files.size());
        }
        m_tabs->setTabText(FilesTab, filesTitle);
    };
    render();
    if (m_details.contains(key) || m_detailErrors.contains(key) || m_detailsPending == key)
        return;
    m_detailsPending = key;
    // "Requerido por" vem da lista de instalados, que só a interface lê.
    const std::vector<std::string> requiredBy = octo::dependents(m_installed, pkg.fullName);
    runAsync<DetailsResult>(
        [this, pkg, requiredBy] {
            DetailsResult r;
            try {
                r.details = m_mgr.details(pkg);
                r.details.requiredBy = requiredBy;
            } catch (const std::exception &e) {
                r.error = QString::fromUtf8(e.what());
            }
            return r;
        },
        [this, key, render](const DetailsResult &r) {
            if (m_detailsPending == key)
                m_detailsPending.clear();
            if (m_details.size() > 256) {
                m_details.clear();
                m_detailErrors.clear();
            }
            if (r.error.isEmpty())
                m_details.insert(key, r.details);
            else
                m_detailErrors.insert(key, r.error);
            const octo::Package *now = current();
            if (now && detailsKey(*now) == key)
                render();
        });
}

void MainWindow::setTab(int tab)
{
    m_tabs->setCurrentIndex(tab);
}

void MainWindow::about()
{
    QString brew;
    try {
        brew = qs(m_mgr.version());
    } catch (const std::exception &) {
        brew = QStringLiteral("Homebrew ?");
    }
    QMessageBox::about(this, ui::tr("About"),
                       QStringLiteral("<h3>OctoBrew %1</h3><p>%2</p><p>%3 · Qt %4</p><p>%5<br>%6</p>"
                                      "<p>GPL-2.0-or-later<br><a href='https://github.com/cleberalves40/octobrew'>"
                                      "github.com/cleberalves40/octobrew</a></p>")
                           .arg(QStringLiteral(OCTOBREW_VERSION), ui::tr("A graphical front end for Homebrew.").toHtmlEscaped(),
                                brew.toHtmlEscaped(), QString::fromLatin1(qVersion()),
                                ui::tr("Based on OctoPkg, © Alexandre Albuquerque Arnt").toHtmlEscaped(),
                                ui::tr("and on octobrew-tui").toHtmlEscaped()));
}
