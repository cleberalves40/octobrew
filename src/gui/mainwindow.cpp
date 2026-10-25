// SPDX-License-Identifier: GPL-2.0-or-later

#include "mainwindow.h"
#include "askpass.h"
#include "listmodel.h"
#include "packagemodel.h"
#include "pages.h"
#include "texts.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableView>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <ctime>

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
// Itens de uma lista mostrados no diálogo; a lista completa vai à Saída.
constexpr int kDialogItems = 25;

QString join(const std::vector<std::string> &v, const QString &sep)
{
    QStringList out;
    for (const auto &s : v)
        out << qs(s);
    return out.join(sep);
}

QStringList names(const std::vector<octo::Package> &packages)
{
    QStringList out;
    for (const auto &p : packages)
        out << (p.kind == octo::Kind::Cask ? qs(p.fullName) + " (cask)" : qs(p.fullName));
    return out;
}

// Uma seção do diálogo: o título e os itens (os primeiros kDialogItems).
QString section(const QString &title, const QStringList &items)
{
    if (items.isEmpty())
        return QString();
    QString html = QStringLiteral("<p><b>%1</b></p><ul>").arg(title.toHtmlEscaped());
    for (int i = 0; i < items.size() && i < kDialogItems; ++i)
        html += QStringLiteral("<li>%1</li>").arg(items[i].toHtmlEscaped());
    if (items.size() > kDialogItems)
        html += QStringLiteral("<li><i>%1</i></li>")
                    .arg(ui::trf("… and %1 more (see the Output tab)", {QString::number(items.size() - kDialogItems)}).toHtmlEscaped());
    return html + QStringLiteral("</ul>");
}

QStringList toQ(const std::vector<std::string> &v)
{
    QStringList out;
    for (const auto &s : v)
        out << qs(s);
    return out;
}

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
    , m_newsFeed(QFileInfo(QString::fromStdString(m_settings.path())).absolutePath().toStdString() + "/news.xml",
                 options.curlPath.toStdString())
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
    updateMarks();
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
    m_actionsView = new QTreeWidget;
    m_actionsView->setHeaderHidden(true);
    m_actionsView->setRootIsDecorated(true);
    m_newsView = new QTextBrowser;
    m_newsView->setOpenExternalLinks(true);
    // A aba Serviços: a tabela do brew services e os botões do serviço selecionado.
    m_servicesView = new QTableWidget(0, 5);
    m_servicesView->setHorizontalHeaderLabels({ui::tr("Name"), ui::tr("Status"), ui::tr("User"), ui::tr("Exit code"), ui::tr("File")});
    m_servicesView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_servicesView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_servicesView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_servicesView->verticalHeader()->hide();
    m_servicesView->setShowGrid(false);
    m_servicesView->setWordWrap(false);
    m_servicesView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_servicesView->verticalHeader()->setDefaultSectionSize(m_servicesView->fontMetrics().height() + 6);
    // As colunas no tamanho do conteúdo; a do arquivo vai até o fim.
    for (int c = 0; c < 4; ++c)
        m_servicesView->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    m_servicesView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    auto *servicesPage = new QWidget;
    auto *servicesLayout = new QVBoxLayout(servicesPage);
    servicesLayout->setContentsMargins(0, 0, 0, 0);
    auto *servicesBar = new QToolBar;
    servicesLayout->addWidget(servicesBar);
    servicesLayout->addWidget(m_servicesView);
    for (const auto &[verb, label] : {std::make_pair("start", "Start the service"), std::make_pair("stop", "Stop the service"),
                                      std::make_pair("restart", "Restart the service")}) {
        const QString v = QString::fromLatin1(verb);
        servicesBar->addAction(ui::tr(label), this, [this, v] { service(v); });
    }
    m_tabs = new QTabWidget;
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(m_info, QIcon(QStringLiteral(":/images/find.png")), ui::tr("Info"));
    m_tabs->addTab(m_files, ui::tr("Files"));
    m_tabs->addTab(m_actionsView, QIcon(QStringLiteral(":/images/toinstall.png")), ui::tr("Actions"));
    m_tabs->addTab(m_output, QIcon(QStringLiteral(":/images/terminal.png")), ui::tr("Output"));
    m_tabs->addTab(m_newsView, ui::tr("News"));
    m_tabs->addTab(m_help, ui::tr("Help"));
    m_tabs->addTab(servicesPage, ui::tr("Services"));
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int tab) {
        // Como no octobrew-tui: as notícias e os serviços só na primeira visita.
        if (tab == NewsTab) {
            m_newsUnseen = false;
            setTabTitles();
            if (!m_newsStarted)
                loadNews();
        }
        if (tab == ServicesTab && !m_servicesLoaded)
            loadServices();
    });

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
    m_progress = new QProgressBar;
    m_progress->setRange(0, 0);   // sem fim conhecido: a barra que vai e volta
    m_progress->setMaximumWidth(140);
    m_progress->setVisible(false);
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_progress);
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
                                 {QKeySequence(Qt::Key_F5), QKeySequence(QStringLiteral("Ctrl+R"))}, [this] {
                                     if (m_tabs->currentIndex() == ServicesTab)
                                         loadServices();
                                     refresh();
                                 });
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

    // As ações do OctoPkg, com o brew. As marcas só valem com o foco na lista:
    // o + e o - não podem ser roubados do campo de busca.
    const auto mark = [this, &add](const QString &name, const QString &text, const QList<QKeySequence> &keys,
                                   std::function<void()> slot, const QString &icon) {
        QAction *a = add(name, text, keys, std::move(slot));
        removeAction(a);
        a->setShortcutContext(Qt::WidgetShortcut);
        m_table->addAction(a);
        a->setIcon(QIcon(icon));
        return a;
    };
    mark("markInstall", ui::tr("Mark for installation"), {QKeySequence(Qt::Key_Plus), QKeySequence(Qt::Key_Insert)},
         [this] { markInstall(); }, QStringLiteral(":/images/toinstall.png"));
    mark("markRemove", ui::tr("Mark for removal"), {QKeySequence(Qt::Key_Minus), QKeySequence(Qt::Key_Delete)},
         [this] { markRemove(); }, QStringLiteral(":/images/toremove.png"));
    // Aplicar: o Ctrl+M do OctoPkg. No macOS, o ⌘M é minimizar: fica o ⌥M, como no octobrew-tui.
#ifdef Q_OS_MACOS
    const QList<QKeySequence> applyKeys{QKeySequence(QStringLiteral("Alt+M"))};
#else
    const QList<QKeySequence> applyKeys{QKeySequence(QStringLiteral("Ctrl+M")), QKeySequence(QStringLiteral("Alt+M"))};
#endif
    QAction *applyAction = add("apply", ui::tr("Apply"), applyKeys, [this] { apply(); });
    applyAction->setIcon(QIcon(QStringLiteral(":/images/octopi_green.png")));
    applyAction->setIconText(ui::tr("Apply"));
    add("clearMarks", ui::tr("Cancel: clear the marks"), {QKeySequence(QStringLiteral("Ctrl+E"))}, [this] { clearMarks(); });
    QAction *upgradeAction = add("upgrade", ui::tr("Install updates"), {QKeySequence(QStringLiteral("Ctrl+U"))},
                                 [this] { upgrade(); });
    upgradeAction->setIcon(QIcon(QStringLiteral(":/images/octopi_yellow.png")));
    add("update", ui::tr("Update Homebrew"), {QKeySequence(QStringLiteral("Ctrl+K"))}, [this] { update(); });
    add("cleanup", ui::tr("Clean the local cache"), {}, [this] { cleanup(); });
    add("autoremove", ui::tr("Remove unneeded dependencies"), {}, [this] { autoremove(); });
    add("pin", ui::tr("Pin"), {}, [this] { pin(true); });
    add("unpin", ui::tr("Unpin"), {}, [this] { pin(false); });
    add("restore", ui::tr("Restore a snapshot…"), {}, [this] { restoreSnapshot(); });
    add("refreshNews", ui::tr("Get the latest news"), {QKeySequence(QStringLiteral("Ctrl+G"))}, [this] {
        setTab(NewsTab);
        if (m_newsStarted)
            refreshNews();
    });
    add("serviceStart", ui::tr("Start the service"), {}, [this] { service(QStringLiteral("start")); });
    add("serviceStop", ui::tr("Stop the service"), {}, [this] { service(QStringLiteral("stop")); });
    add("serviceRestart", ui::tr("Restart the service"), {}, [this] { service(QStringLiteral("restart")); });
    add("addTap", ui::tr("Add a tap…"), {}, [this] { addTap(); });
    add("removeTap", ui::tr("Remove a tap…"), {}, [this] { removeTap(); });
    add("exportBrewfile", ui::tr("Export a Brewfile…"), {}, [this] { exportBrewfile(); });
    add("importBrewfile", ui::tr("Install from a Brewfile…"), {}, [this] { importBrewfile(); });
    add("doctor", ui::tr("Check the system (brew doctor)"), {}, [this] { doctor(); });

    // As abas, com os números do OctoPkg.
    add("tabInfo", ui::tr("Info"), {QKeySequence(QStringLiteral("Alt+1"))}, [this] { setTab(InfoTab); });
    add("tabFiles", ui::tr("Files"), {QKeySequence(QStringLiteral("Alt+2"))}, [this] { setTab(FilesTab); });
    add("tabActions", ui::tr("Actions"), {QKeySequence(QStringLiteral("Alt+3"))}, [this] { setTab(ActionsTab); });
    add("tabOutput", ui::tr("Output"), {QKeySequence(QStringLiteral("Alt+4"))}, [this] { setTab(OutputTab); });
    add("tabNews", ui::tr("News"), {QKeySequence(QStringLiteral("Alt+5"))}, [this] { setTab(NewsTab); });
    add("tabServices", ui::tr("Services"), {QKeySequence(QStringLiteral("Alt+7"))}, [this] { setTab(ServicesTab); });
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
    for (const char *a : {"update", "upgrade", "cleanup", "autoremove"})
        file->addAction(m_actions[a]);
    file->addSeparator();
    file->addAction(m_actions["restore"]);
    file->addAction(m_actions["refreshNews"]);
    file->addSeparator();
    file->addAction(quit);
    QMenu *search = menuBar()->addMenu(ui::tr("Search"));
    search->addAction(m_actions["findPackage"]);
    search->addAction(description);
    search->addSeparator();
    search->addAction(leaves);
    QMenu *actionsMenu = menuBar()->addMenu(ui::tr("Actions"));
    for (const char *a : {"markInstall", "markRemove", "", "apply", "clearMarks", "", "pin", "unpin"}) {
        if (*a)
            actionsMenu->addAction(m_actions[a]);
        else
            actionsMenu->addSeparator();
    }
    QMenu *brew = menuBar()->addMenu(QStringLiteral("Homebrew"));
    for (const char *a : {"tabServices", "serviceStart", "serviceStop", "serviceRestart", "", "addTap", "removeTap", "",
                          "exportBrewfile", "importBrewfile", "", "doctor"}) {
        if (*a)
            brew->addAction(m_actions[a]);
        else
            brew->addSeparator();
    }
    QMenu *view = menuBar()->addMenu(ui::tr("View"));
    for (const char *t : {"tabInfo", "tabFiles", "tabActions", "tabOutput", "tabNews", "tabHelp", "tabServices"})
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
    bar->addAction(applyAction);
    bar->addAction(upgradeAction);
    bar->addSeparator();
    bar->addAction(leaves);
    // Na barra, as alternâncias com o nome ao lado: só o ícone não diz o que fazem.
    for (QAction *a : {leaves, description, applyAction})
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
    for (const char *name : {"refresh", "toggleView", "findPackage", "searchDescription", "leaves", "markInstall",
                             "markRemove", "apply", "clearMarks", "upgrade", "update", "refreshNews", "tabInfo", "tabFiles",
                             "tabActions", "tabOutput", "tabNews", "tabHelp", "tabServices", "quit"}) {
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
    // Uma listagem pode ser abandonada; uma transação, não.
    if (m_inOperation) {
        setStatus(ui::tr("Wait for the current operation to finish."));
        event->ignore();
        return;
    }
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
    if (l.error.isEmpty() && m_operationError.isEmpty())
        setStatus(ui::tr("Ready."));
    else if (l.error.isEmpty())   // a listagem depois de uma operação que falhou não esconde o erro
        setStatus(ui::trf("ERROR: %1", {m_operationError}), true);
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
        QString service;
        for (const auto &sv : m_services)
            if (sv.name == pkg.name)
                service = pages::serviceStatusText(sv.status) + (sv.user.empty() ? QString() : " (" + qs(sv.user) + ")");
        m_info->setHtml(pages::infoHtml(pkg, details, m_detailErrors.value(key), service));
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

// ---------------------------------------------------------------------------
// Marcas e transações (a aba Ações do OctoPkg)
// ---------------------------------------------------------------------------

ConfirmAnswer MainWindow::confirm(const ConfirmRequest &request)
{
    if (m_confirm)
        return m_confirm(request);
    TransactionDialog dialog(request, this);
    dialog.exec();
    return dialog.answer();
}

void MainWindow::notice(const QString &title, const QString &text, bool error)
{
    if (m_notice) {
        m_notice(title, text, error);
        return;
    }
    if (error)
        QMessageBox::critical(this, title, text);
    else
        QMessageBox::information(this, title, text);
}

bool MainWindow::refuseWhileBusy()
{
    if (!m_busy)
        return false;
    setStatus(ui::tr("Wait for the current operation to finish."));
    return true;
}

bool MainWindow::askpassAvailable() const
{
    return !qEnvironmentVariableIsEmpty("SUDO_ASKPASS");
}

QString MainWindow::snapshotDir() const
{
    const QString path = qs(m_settings.path());
    return QFileInfo(path).absolutePath() + QStringLiteral("/snapshots");
}

QString MainWindow::newSnapshotFile() const
{
    return snapshotDir() + "/" + qs(octo::snapshotName(std::time(nullptr)));
}

std::vector<octo::Package> MainWindow::packagesFor(const QSet<QString> &keys) const
{
    // Os instalados, depois o repositório: uma marca da visão Repositório vale na outra.
    std::vector<octo::Package> out;
    QSet<QString> found;
    for (const auto *list : {&m_installed, &m_remote})
        for (const auto &p : *list) {
            const QString key = qs(p.key());
            if (keys.contains(key) && !found.contains(key)) {
                found.insert(key);
                out.push_back(p);
            }
        }
    return out;
}

void MainWindow::updateMarks()
{
    m_model->setMarks(m_toInstall, m_toRemove);
    m_actionsView->clear();
    const auto fill = [this](const QString &title, const QSet<QString> &keys, const QString &icon) {
        if (keys.isEmpty())
            return;
        auto *top = new QTreeWidgetItem(m_actionsView, {QStringLiteral("%1 (%2)").arg(title).arg(keys.size())});
        top->setIcon(0, QIcon(icon));
        for (const auto &p : packagesFor(keys))
            new QTreeWidgetItem(top, {p.kind == octo::Kind::Cask ? qs(p.fullName) + " (cask)" : qs(p.fullName)});
        top->setExpanded(true);
    };
    fill(ui::tr("To be installed"), m_toInstall, QStringLiteral(":/images/toinstall.png"));
    fill(ui::tr("To be removed"), m_toRemove, QStringLiteral(":/images/toremove.png"));
    if (m_toInstall.isEmpty() && m_toRemove.isEmpty())
        new QTreeWidgetItem(m_actionsView, {ui::tr("No packages marked. Use + to install and - to remove.")});
    const int marked = static_cast<int>(m_toInstall.size() + m_toRemove.size());
    m_tabs->setTabText(ActionsTab, marked ? QStringLiteral("%1 (%2)").arg(ui::tr("Actions")).arg(marked) : ui::tr("Actions"));
    setTabTitles();
    m_actions["apply"]->setEnabled(marked > 0);
}

void MainWindow::markInstall()
{
    const octo::Package *p = current();
    if (!p)
        return;
    const QString key = qs(p->key());
    if (p->pinned) {
        setStatus(ui::trf("%1 is pinned.", {qs(p->name)}));
        return;
    }
    // Instalar um instalado só faz sentido se ele está desatualizado (o brew install o atualiza).
    if (p->installed && p->newVersion.empty() && !m_toInstall.contains(key)) {
        setStatus(ui::trf("%1 is already installed and up to date.", {qs(p->name)}));
        return;
    }
    // Como no OctoPkg, + de novo desmarca; instalar e remover se excluem.
    if (!m_toInstall.remove(key)) {
        m_toInstall.insert(key);
        m_toRemove.remove(key);
    }
    updateMarks();
}

void MainWindow::markRemove()
{
    const octo::Package *p = current();
    if (!p)
        return;
    const QString key = qs(p->key());
    if (!p->installed) {
        setStatus(ui::tr("Only installed packages can be removed."));
        return;
    }
    if (octo::isForbidden(*p)) {
        setStatus(ui::trf("%1 cannot be removed.", {qs(p->name)}));
        return;
    }
    if (p->pinned) {
        setStatus(ui::trf("%1 is pinned.", {qs(p->name)}));
        return;
    }
    if (!m_toRemove.remove(key)) {
        m_toRemove.insert(key);
        m_toInstall.remove(key);
    }
    updateMarks();
}

void MainWindow::clearMarks()
{
    m_toInstall.clear();
    m_toRemove.clear();
    updateMarks();
}

void MainWindow::apply()
{
    if (refuseWhileBusy())
        return;
    if (m_toInstall.isEmpty() && m_toRemove.isEmpty()) {
        notice(ui::tr("Actions"), ui::tr("No packages marked. Use + to install and - to remove."));
        return;
    }
    const std::vector<octo::Package> rem = packagesFor(m_toRemove);
    const std::vector<octo::Package> ins = packagesFor(m_toInstall);
    // O brew não remove o que outros instalados usam: eles vão juntos, como no
    // "pkg remove -R" do OctoPkg, e o diálogo vira Aviso.
    std::vector<std::string> remKeys;
    for (const auto &k : m_toRemove)
        remKeys.push_back(k.toStdString());
    const auto closure = octo::dependentsClosure(m_installed, remKeys);
    QSet<QString> closureKeys;
    for (const auto &k : closure)
        closureKeys.insert(qs(k));
    const std::vector<octo::Package> deps = packagesFor(closureKeys);
    for (const auto &p : deps)
        if (octo::isForbidden(p) || p.pinned) {
            notice(ui::tr("Error"), ui::trf("%1 depends on a package marked for removal and cannot be removed.", {qs(p.fullName)}), true);
            return;
        }
    struct Preview {
        octo::TransactionPreview install;
        std::vector<std::string> sudo;
    };
    m_busy = true;
    setStatus(ui::tr("Collecting transaction data…"));
    runAsync<Result<Preview>>(
        [this, rem, ins, deps] {
            Result<Preview> r;
            try {
                if (!ins.empty())
                    r.value.install = m_mgr.previewInstall(ins);
                std::vector<std::string> casks;
                for (const auto *list : {&rem, &deps, &ins})
                    for (const auto &p : *list)
                        if (p.kind == octo::Kind::Cask)
                            casks.push_back(p.fullName);
                r.value.sudo = m_mgr.casksNeedingSudo(casks);
            } catch (const std::exception &e) {
                r.error = QString::fromUtf8(e.what());
            }
            return r;
        },
        [this, rem, ins, deps](const Result<Preview> &r) {
            m_busy = false;
            if (!r.error.isEmpty()) {
                setStatus(ui::trf("ERROR: %1", {r.error}), true);
                log(m_status);
                notice(ui::tr("Error"), r.error, true);
                return;
            }
            setStatus(ui::tr("Ready."));
            std::vector<octo::Package> removal = rem;
            removal.insert(removal.end(), deps.begin(), deps.end());
            const QStringList retrieved = toQ(r.value.install.items());
            const QStringList removed = names(removal);

            ConfirmRequest req;
            req.danger = !deps.empty();
            req.title = req.danger ? ui::tr("Warning") : ui::tr("Confirmation");
            req.text = ui::tr("Confirm?");
            req.details = section(retrieved.size() == 1 ? ui::tr("The following package needs to be retrieved")
                                                        : ui::trf("The following %1 packages need to be retrieved",
                                                                  {QString::number(retrieved.size())}),
                                  retrieved)
                          + section(removed.size() == 1 ? ui::tr("The following package will be removed")
                                                        : ui::trf("The following %1 packages will be removed",
                                                                  {QString::number(removed.size())}),
                                    removed);
            if (!deps.empty())
                req.details += QStringLiteral("<p style='color:#c03030'>%1</p>")
                                   .arg(ui::trf("(%1 of them depend on the marked packages)", {QString::number(deps.size())}).toHtmlEscaped());
            if (retrieved.isEmpty() && removed.isEmpty())
                req.details += QStringLiteral("<p>%1</p>").arg(ui::tr("Nothing to do: the marked packages are already in this state.").toHtmlEscaped());
            if (!r.value.sudo.empty())
                req.details += QStringLiteral("<p>%1</p>").arg(ui::trf("Needs sudo: %1", {join(r.value.sudo, QStringLiteral(", "))}).toHtmlEscaped());
            // O Boot Environment do OctoPkg: marcado ao instalar, desmarcado ao remover.
            req.snapshot = true;
            req.snapshotChecked = !ins.empty();
            req.snapshotFile = newSnapshotFile();
            log(ui::tr("Transaction:"));
            for (const auto &i : retrieved)
                log("  + " + i);
            for (const auto &i : removed)
                log("  - " + i);
            for (const auto &w : r.value.install.warnings)
                log(ui::trf("WARNING: %1", {qs(w)}));
            const ConfirmAnswer answer = confirm(req);
            if (!answer.accepted)
                return;

            const QString file = req.snapshotFile;
            const bool withSnapshot = answer.snapshot;
            QStringList commands;
            if (withSnapshot)
                commands << qs(m_mgr.commandLine({"bundle", "dump", "--file=" + file.toStdString(), "--force"}));
            for (const auto &a : octo::Manager::uninstallArgs(removal))
                commands << qs(m_mgr.commandLine(a));
            for (const auto &a : octo::Manager::installArgs(ins))
                commands << qs(m_mgr.commandLine(a));
            const bool interactive = askpassAvailable();
            runOperation(ins.empty() ? ui::tr("Removing selected packages...") : ui::tr("Installing selected packages..."),
                         commands.join(QStringLiteral("; ")),
                         [this, withSnapshot, file, removal, ins, interactive](const octo::OutputFn &out) {
                             // Sem shell: se o snapshot falhar, nada é removido nem instalado.
                             if (withSnapshot)
                                 m_mgr.snapshot(file.toStdString(), out);
                             if (!removal.empty())
                                 m_mgr.uninstall(removal, out, interactive);
                             if (!ins.empty())
                                 m_mgr.install(ins, out, interactive);
                         },
                         [this, removal, ins, withSnapshot, file] {
                             for (const auto &p : removal)
                                 m_toRemove.remove(qs(p.key()));
                             for (const auto &p : ins)
                                 m_toInstall.remove(qs(p.key()));
                             updateMarks();
                             QString summary;
                             if (withSnapshot)
                                 summary += ui::trf("Snapshot: %1", {file}) + "\n";
                             if (!ins.empty())
                                 summary += ui::trf("Installed: %1", {names(ins).join(QStringLiteral(", "))}) + "\n";
                             if (!removal.empty())
                                 summary += ui::trf("Removed: %1", {names(removal).join(QStringLiteral(", "))}) + "\n";
                             notice(ui::tr("Command finished OK!"), summary + ui::tr("Details in the Output tab."));
                         });
        });
}

void MainWindow::upgrade()
{
    if (refuseWhileBusy())
        return;
    QMap<QString, QString> casks;   // os casks instalados, pelo nome
    for (const auto &p : m_installed)
        if (p.kind == octo::Kind::Cask)
            casks.insert(qs(p.name), qs(p.fullName));
    struct Preview {
        octo::TransactionPreview upgrade;
        std::vector<std::string> sudo;
    };
    m_busy = true;
    setStatus(ui::tr("Collecting transaction data…"));
    runAsync<Result<Preview>>(
        [this, casks] {
            Result<Preview> r;
            try {
                r.value.upgrade = m_mgr.previewUpgrade();
                std::vector<std::string> tokens;
                for (const auto &i : r.value.upgrade.items()) {
                    const QString name = qs(octo::itemName(i));
                    if (casks.contains(name))
                        tokens.push_back(casks.value(name).toStdString());
                }
                r.value.sudo = m_mgr.casksNeedingSudo(tokens);
            } catch (const std::exception &e) {
                r.error = QString::fromUtf8(e.what());
            }
            return r;
        },
        [this](const Result<Preview> &r) {
            m_busy = false;
            if (!r.error.isEmpty()) {
                setStatus(ui::trf("ERROR: %1", {r.error}), true);
                notice(ui::tr("Error"), r.error, true);
                return;
            }
            setStatus(ui::tr("Ready."));
            for (const auto &w : r.value.upgrade.warnings)
                log(ui::trf("WARNING: %1", {qs(w)}));
            if (r.value.upgrade.count() == 0) {
                notice(ui::tr("System upgrade"), ui::tr("There are no new updates available!"));
                return;
            }
            const QStringList items = toQ(r.value.upgrade.items());
            ConfirmRequest req;
            req.title = ui::tr("System upgrade");
            req.text = ui::tr("Confirm?");
            req.details = section(items.size() == 1 ? ui::tr("The following package needs to be retrieved")
                                                    : ui::trf("The following %1 packages need to be retrieved",
                                                              {QString::number(items.size())}),
                                  items)
                          + QStringLiteral("<p>%1</p>").arg(ui::tr("Pinned packages are not upgraded.").toHtmlEscaped());
            if (!r.value.sudo.empty())
                req.details += QStringLiteral("<p>%1</p>").arg(ui::trf("Needs sudo: %1", {join(r.value.sudo, QStringLiteral(", "))}).toHtmlEscaped());
            req.snapshot = true;
            req.snapshotChecked = true;
            req.snapshotFile = newSnapshotFile();
            log(ui::tr("Transaction:"));
            for (const auto &i : items)
                log("  + " + i);
            const ConfirmAnswer answer = confirm(req);
            if (!answer.accepted)
                return;
            const QString file = req.snapshotFile;
            const bool withSnapshot = answer.snapshot;
            const bool interactive = askpassAvailable();
            runOperation(ui::tr("Installing updates..."),
                         (withSnapshot ? qs(m_mgr.commandLine({"bundle", "dump", "--file=" + file.toStdString(), "--force"})) + "; "
                                       : QString())
                             + qs(m_mgr.commandLine({"upgrade"})),
                         [this, withSnapshot, file, interactive](const octo::OutputFn &out) {
                             if (withSnapshot)
                                 m_mgr.snapshot(file.toStdString(), out);
                             m_mgr.upgrade(out, interactive);
                         },
                         [this, withSnapshot, file] {
                             notice(ui::tr("Command finished OK!"),
                                    (withSnapshot ? ui::trf("Snapshot: %1", {file}) + "\n" : QString())
                                        + ui::tr("Details in the Output tab."));
                         });
        });
}

void MainWindow::update()
{
    runOperation(ui::tr("Updating Homebrew and the package list…"), qs(m_mgr.commandLine({"update"})),
                 [this](const octo::OutputFn &out) { m_mgr.update(out); },
                 // O cache da API mudou: a visão Repositório relê na próxima vez.
                 [this] { m_hasRemote = m_remoteView; });
}

void MainWindow::cleanup()
{
    if (refuseWhileBusy())
        return;
    m_busy = true;
    setStatus(ui::tr("Collecting transaction data…"));
    runAsync<Result<octo::TransactionPreview>>(
        [this] {
            Result<octo::TransactionPreview> r;
            try {
                r.value = m_mgr.previewCleanup();
            } catch (const std::exception &e) {
                r.error = QString::fromUtf8(e.what());
            }
            return r;
        },
        [this](const Result<octo::TransactionPreview> &r) {
            m_busy = false;
            setStatus(r.error.isEmpty() ? ui::tr("Ready.") : ui::trf("ERROR: %1", {r.error}), !r.error.isEmpty());
            if (!r.error.isEmpty()) {
                notice(ui::tr("Error"), r.error, true);
                return;
            }
            if (r.value.count() == 0) {
                notice(ui::tr("Clean the local cache"), ui::tr("Nothing to clean."));
                return;
            }
            log(ui::trf("brew cleanup would remove %1 item(s):", {QString::number(r.value.count())}));
            for (const auto &i : r.value.items())
                log("  - " + qs(i));
            ConfirmRequest req;
            req.title = ui::tr("Clean the local cache");
            req.text = ui::tr("Confirm?");
            req.details = section(ui::trf("brew cleanup would remove %1 item(s):", {QString::number(r.value.count())}),
                                  toQ(r.value.items()));
            if (!r.value.freed.empty())
                req.details += QStringLiteral("<p><b>%1</b></p>").arg(ui::trf("It would free approximately %1.", {qs(r.value.freed)}).toHtmlEscaped());
            if (!confirm(req).accepted)
                return;
            runOperation(ui::tr("Cleaning the local cache..."), qs(m_mgr.commandLine({"cleanup", "--prune=all"})),
                         [this](const octo::OutputFn &out) { m_mgr.cleanup(out); }, nullptr);
        });
}

void MainWindow::autoremove()
{
    if (refuseWhileBusy())
        return;
    m_busy = true;
    setStatus(ui::tr("Collecting transaction data…"));
    runAsync<Result<octo::TransactionPreview>>(
        [this] {
            Result<octo::TransactionPreview> r;
            try {
                r.value = m_mgr.previewAutoremove();
            } catch (const std::exception &e) {
                r.error = QString::fromUtf8(e.what());
            }
            return r;
        },
        [this](const Result<octo::TransactionPreview> &r) {
            m_busy = false;
            setStatus(r.error.isEmpty() ? ui::tr("Ready.") : ui::trf("ERROR: %1", {r.error}), !r.error.isEmpty());
            if (!r.error.isEmpty()) {
                notice(ui::tr("Error"), r.error, true);
                return;
            }
            if (r.value.count() == 0) {
                notice(ui::tr("Remove unneeded dependencies"), ui::tr("There are no unneeded dependencies."));
                return;
            }
            const QStringList items = toQ(r.value.items());
            ConfirmRequest req;
            req.title = ui::tr("Remove unneeded dependencies");
            req.text = ui::tr("Confirm?");
            req.details = section(ui::trf("The following %1 packages will be removed", {QString::number(items.size())}), items)
                          + QStringLiteral("<p>%1</p>").arg(ui::tr("They were installed as dependencies and nothing uses them anymore.").toHtmlEscaped());
            log(ui::tr("Transaction:"));
            for (const auto &i : items)
                log("  - " + i);
            if (!confirm(req).accepted)
                return;
            runOperation(ui::tr("Removing unneeded dependencies…"), qs(m_mgr.commandLine({"autoremove"})),
                         [this](const octo::OutputFn &out) { m_mgr.autoremove(out); }, nullptr);
        });
}

void MainWindow::pin(bool pinIt)
{
    const octo::Package *p = current();
    if (!p || !p->installed) {
        setStatus(ui::tr("Select an installed package."));
        return;
    }
    if (p->pinned == pinIt) {
        setStatus(pinIt ? ui::trf("%1 is pinned.", {qs(p->name)}) : ui::trf("%1 is not pinned.", {qs(p->name)}));
        return;
    }
    const octo::Package pkg = *p;
    runOperation(pinIt ? ui::tr("Pinning…") : ui::tr("Unpinning…"), qs(m_mgr.commandLine(octo::Manager::pinArgs(pkg, pinIt))),
                 [this, pkg, pinIt](const octo::OutputFn &out) { m_mgr.pin(pkg, pinIt, out); }, nullptr);
}

void MainWindow::restoreSnapshot(const QString &chosen)
{
    if (refuseWhileBusy())
        return;
    QString file = chosen;
    if (file.isEmpty()) {
        const auto list = octo::listSnapshots(snapshotDir().toStdString());
        if (list.empty()) {
            notice(ui::tr("Restore a snapshot"), ui::trf("No snapshots in %1 yet.", {snapshotDir()}));
            return;
        }
        const auto index = choose(ui::tr("Restore a snapshot"), ui::trf("Snapshots in %1", {snapshotDir()}), toQ(list));
        if (!index || *index < 0)
            return;
        file = snapshotDir() + "/" + qs(list[static_cast<size_t>(*index)]);
    }
    ConfirmRequest req;
    req.title = ui::tr("Restore a snapshot");
    req.text = ui::tr("Confirm?");
    req.details = QStringLiteral("<p>%1</p><p>%2</p>")
                      .arg(file.toHtmlEscaped(), ui::tr("Installs what is missing from it; nothing is removed or upgraded.").toHtmlEscaped());
    if (!confirm(req).accepted)
        return;
    const bool interactive = askpassAvailable();
    runOperation(ui::tr("Restoring the snapshot…"),
                 qs(m_mgr.commandLine({"bundle", "install", "--file=" + file.toStdString(), "--no-upgrade"})),
                 [this, file, interactive](const octo::OutputFn &out) { m_mgr.restore(file.toStdString(), out, interactive); },
                 [this, file] {
                     notice(ui::tr("Command finished OK!"), ui::trf("Restored: %1", {file}) + "\n" + ui::tr("Details in the Output tab."));
                 });
}

// ---------------------------------------------------------------------------
// Execução das operações, com a saída ao vivo na aba Saída
// ---------------------------------------------------------------------------

void MainWindow::appendOutput(const QString &chunk)
{
    // Linha a linha; "\r" (as barras de progresso do brew) recomeça a linha atual.
    for (const QChar c : chunk) {
        if (c == '\n') {
            m_output->appendPlainText(QStringLiteral("  ") + m_partial);
            m_partial.clear();
        } else if (c == '\r') {
            m_partial.clear();
        } else {
            m_partial += c;
        }
    }
}

void MainWindow::runOperation(const QString &status, const QString &command, Work work, std::function<void()> onSuccess)
{
    if (refuseWhileBusy())
        return;
    m_operationError.clear();
    log(command);
    // Como no OctoPkg, a aba Saída acompanha a transação.
    setTab(OutputTab);
    m_partial.clear();
    m_busy = true;
    m_inOperation = true;
    m_cancelSizes = true;
    ++m_generation;   // a medição de tamanhos em andamento não vale mais
    setStatus(status);
    m_progress->setVisible(true);
    for (const char *a : {"apply", "upgrade", "update", "cleanup", "autoremove", "pin", "unpin", "restore", "refresh"})
        m_actions[a]->setEnabled(false);
    const bool sync = m_options.synchronous;
    QPointer<MainWindow> self(this);
    runAsync<QString>(
        [this, work, sync, self] {
            // A saída chega na thread do trabalho e vai para a interface pela fila de eventos.
            const octo::OutputFn out = [this, sync, self](const std::string &chunk) {
                const QString text = QString::fromStdString(chunk);
                if (sync)
                    appendOutput(text);
                else
                    QMetaObject::invokeMethod(self, [self, text] { if (self) self->appendOutput(text); }, Qt::QueuedConnection);
            };
            try {
                work(out);
            } catch (const std::exception &e) {
                return QString::fromUtf8(e.what());
            }
            return QString();
        },
        [this, onSuccess](const QString &error) { finishOperation(error, onSuccess); });
}

void MainWindow::finishOperation(const QString &error, const std::function<void()> &onSuccess)
{
    m_busy = false;
    m_inOperation = false;
    m_progress->setVisible(false);
    for (const char *a : {"upgrade", "update", "cleanup", "autoremove", "pin", "unpin", "restore", "refresh"})
        m_actions[a]->setEnabled(true);
    updateMarks();   // o Aplicar volta se ainda houver marcas
    if (!m_partial.isEmpty())
        appendOutput(QStringLiteral("\n"));
    if (!error.isEmpty()) {
        m_operationError = error;
        setStatus(ui::trf("ERROR: %1", {error}), true);
        log(m_status);
        notice(ui::tr("Error"), error, true);
        refresh();   // a transação pode ter feito parte do trabalho
        return;
    }
    setStatus(ui::tr("Ready."));
    log(ui::tr("Command finished OK!"));
    if (onSuccess)
        onSuccess();
    refresh();
}

// ---------------------------------------------------------------------------
// Fase 3: as notícias, os serviços, os taps, o doctor e os Brewfiles
// ---------------------------------------------------------------------------

std::optional<int> MainWindow::choose(const QString &title, const QString &label, const QStringList &items)
{
    if (m_choose)
        return m_choose(title, label, items);
    bool ok = false;
    const QString item = QInputDialog::getItem(this, title, label, items, 0, false, &ok);
    if (!ok)
        return std::nullopt;
    return static_cast<int>(items.indexOf(item));
}

QString MainWindow::askFile(bool save, const QString &title, const QString &suggested)
{
    if (m_file)
        return m_file(save, title, suggested);
    // O diálogo nativo do sistema; o de gravar já pergunta antes de trocar um arquivo.
    const QString filter = ui::tr("Brewfiles (Brewfile *.Brewfile);;All files (*)");
    return save ? QFileDialog::getSaveFileName(this, title, suggested, filter)
                : QFileDialog::getOpenFileName(this, title, suggested, filter);
}

std::optional<MainWindow::TapRequest> MainWindow::askTap()
{
    if (m_tap)
        return m_tap();
    QDialog dialog(this);
    dialog.setWindowTitle(ui::tr("Add a tap"));
    auto *name = new QLineEdit;
    name->setPlaceholderText(QStringLiteral("user/repo"));
    auto *trust = new QCheckBox(ui::tr("Trust its formulae and casks (brew trust --tap)"));
    auto *hint = new QLabel(ui::tr("From GitHub: user/homebrew-repo.") + "\n" +
                            ui::tr("Homebrew only loads packages from trusted third-party taps."));
    hint->setStyleSheet(QStringLiteral("color: gray"));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *form = new QFormLayout(&dialog);
    form->addRow(ui::tr("Tap (user/repo):"), name);
    form->addRow(trust);
    form->addRow(hint);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return std::nullopt;
    return TapRequest{name->text().trimmed(), trust->isChecked()};
}

void MainWindow::setTabTitles()
{
    m_tabs->setTabText(NewsTab, ui::tr("News") + (m_newsUnseen ? QStringLiteral(" *") : QString()));
}

void MainWindow::loadNews()
{
    // Na primeira visita: as do cache na hora, e as novas em segundo plano.
    m_newsStarted = true;
    m_news = m_newsFeed.cached();
    refreshNews();
}

void MainWindow::refreshNews()
{
    m_newsStarted = true;
    if (m_newsBusy)
        return;
    m_newsBusy = true;
    log(ui::trf("Searching for %1 latest news...", {QString::fromLatin1(octo::kNewsSource)}));
    renderNews();
    runAsync<octo::NewsResult>([this] { return m_newsFeed.refresh(); },
                               [this](const octo::NewsResult &r) { applyNews(r); });
}

void MainWindow::applyNews(const octo::NewsResult &result)
{
    m_newsBusy = false;
    m_news = result;
    if (!result.error.empty())
        log(ui::trf("WARNING: %1", {qs(result.error)}));
    else
        log(ui::trf("%1 news: %2 item(s).", {QString::fromLatin1(octo::kNewsSource), QString::number(result.items.size())}));
    if (result.updated && m_tabs->currentIndex() != NewsTab)
        m_newsUnseen = true;
    setTabTitles();
    renderNews();
}

void MainWindow::renderNews()
{
    m_newsView->setHtml(pages::newsHtml(m_news.items, m_newsBusy, qs(m_news.error), m_news.fromCache));
}

void MainWindow::loadServices()
{
    if (m_servicesBusy)
        return;
    m_servicesBusy = true;
    runAsync<Result<std::vector<octo::Service>>>(
        [this] {
            Result<std::vector<octo::Service>> r;
            try {
                r.value = m_mgr.listServices();
            } catch (const std::exception &e) {
                r.error = QString::fromUtf8(e.what());
            }
            return r;
        },
        [this](const Result<std::vector<octo::Service>> &r) {
            m_servicesBusy = false;
            m_servicesLoaded = true;
            m_servicesError = r.error;
            if (r.error.isEmpty())
                m_services = r.value;
            else
                log(ui::trf("WARNING: %1", {r.error}));
            renderServices();
            showDetails();   // o serviço da fórmula na aba Informação
        });
}

void MainWindow::renderServices()
{
    m_servicesView->setRowCount(0);
    if (!m_servicesError.isEmpty()) {
        m_servicesView->setRowCount(1);
        m_servicesView->setItem(0, 0, new QTableWidgetItem(ui::trf("ERROR: %1", {m_servicesError})));
        return;
    }
    m_servicesView->setRowCount(static_cast<int>(m_services.size()));
    for (int i = 0; i < static_cast<int>(m_services.size()); ++i) {
        const auto &sv = m_services[static_cast<size_t>(i)];
        const QString status = pages::serviceStatusText(sv.status);
        const QStringList cells{qs(sv.name), status, qs(sv.user), sv.exitCode >= 0 ? QString::number(sv.exitCode) : QString(),
                                qs(sv.file)};
        for (int c = 0; c < cells.size(); ++c) {
            auto *item = new QTableWidgetItem(cells[c]);
            if (c == 1 && (sv.status == "started" || sv.status == "scheduled"))
                item->setForeground(QColor(0x2e, 0x8b, 0x2e));
            else if (c == 1 && sv.status == "error")
                item->setForeground(QColor(0xc0, 0x30, 0x30));
            m_servicesView->setItem(i, c, item);
        }
    }
}

void MainWindow::service(const QString &verb)
{
    // Na aba Serviços, o da linha selecionada; fora dela, o da fórmula selecionada.
    QString name;
    const int row = m_servicesView->currentRow();
    if (m_tabs->currentIndex() == ServicesTab && row >= 0 && row < static_cast<int>(m_services.size()))
        name = qs(m_services[static_cast<size_t>(row)].name);
    else {
        const octo::Package *p = current();
        if (!p || !p->installed || p->kind != octo::Kind::Formula) {
            setStatus(ui::tr("Select an installed formula."));
            return;
        }
        if (!p->hasService) {
            setStatus(ui::trf("%1 has no service.", {qs(p->name)}));
            return;
        }
        name = qs(p->name);
    }
    const QString status = verb == "start"  ? ui::trf("Starting the service %1…", {name})
                           : verb == "stop" ? ui::trf("Stopping the service %1…", {name})
                                            : ui::trf("Restarting the service %1…", {name});
    runOperation(status, qs(m_mgr.commandLine({"services", verb.toStdString(), name.toStdString()})),
                 [this, verb, name](const octo::OutputFn &out) { m_mgr.service(verb.toStdString(), name.toStdString(), out); },
                 [this] {
                     m_servicesLoaded = false;
                     loadServices();
                 });
}

void MainWindow::addTap()
{
    if (refuseWhileBusy())
        return;
    const auto request = askTap();
    if (!request)
        return;
    const QString tap = request->name;
    if (!octo::validTapName(tap.toStdString())) {
        notice(ui::tr("Error"), ui::trf("Invalid tap name: %1 (use user/repo)", {tap}), true);
        return;
    }
    const bool trust = request->trust;
    runOperation(ui::trf("Adding the tap %1…", {tap}),
                 qs(m_mgr.commandLine({"tap", tap.toStdString()}))
                     + (trust ? "; " + qs(m_mgr.commandLine({"trust", "--tap", tap.toStdString()})) : QString()),
                 [this, tap, trust](const octo::OutputFn &out) { m_mgr.tap(tap.toStdString(), trust, out); },
                 // Os pacotes do tap entram na visão Repositório na próxima leitura.
                 [this, tap] {
                     m_hasRemote = m_remoteView;
                     notice(ui::tr("Command finished OK!"),
                            ui::trf("Tap added: %1", {tap}) + "\n" + ui::tr("Its packages are in the repository view (i)."));
                 });
}

void MainWindow::removeTap()
{
    if (refuseWhileBusy())
        return;
    m_busy = true;
    setStatus(ui::tr("Listing the taps…"));
    runAsync<Result<std::vector<octo::Tap>>>(
        [this] {
            Result<std::vector<octo::Tap>> r;
            try {
                for (auto &t : m_mgr.listTaps())
                    if (!t.official)
                        r.value.push_back(std::move(t));
            } catch (const std::exception &e) {
                r.error = QString::fromUtf8(e.what());
            }
            return r;
        },
        [this](const Result<std::vector<octo::Tap>> &r) {
            m_busy = false;
            setStatus(r.error.isEmpty() ? ui::tr("Ready.") : ui::trf("ERROR: %1", {r.error}), !r.error.isEmpty());
            if (!r.error.isEmpty()) {
                notice(ui::tr("Error"), r.error, true);
                return;
            }
            if (r.value.empty()) {
                notice(ui::tr("Remove a tap"), ui::tr("There are no third-party taps."));
                return;
            }
            QStringList items;
            for (const auto &t : r.value)
                items << qs(t.name) + QStringLiteral("  (%1)").arg(t.formulae.size() + t.casks.size())
                             + (t.trusted ? QString() : "  " + ui::tr("untrusted"));
            const auto index = choose(ui::tr("Remove a tap"), ui::tr("Tap:"), items);
            if (!index || *index < 0)
                return;
            const QString tap = qs(r.value[static_cast<size_t>(*index)].name);
            // O brew recusa o untap com pacotes do tap instalados: melhor dizer quais.
            QStringList used;
            for (const auto &p : m_installed)
                if (qs(p.tap) == tap)
                    used << qs(p.fullName);
            if (!used.isEmpty()) {
                notice(ui::tr("Remove a tap"),
                       ui::trf("These packages from %1 are installed; remove them first:", {tap}) + "\n• " + used.join(QStringLiteral("\n• ")));
                return;
            }
            ConfirmRequest req;
            req.title = ui::tr("Remove a tap");
            req.text = ui::trf("Remove the tap %1?", {tap});
            req.danger = true;
            if (!confirm(req).accepted)
                return;
            runOperation(ui::trf("Removing the tap %1…", {tap}), qs(m_mgr.commandLine({"untap", tap.toStdString()})),
                         [this, tap](const octo::OutputFn &out) { m_mgr.untap(tap.toStdString(), out); },
                         [this] { m_hasRemote = m_remoteView; });
        });
}

void MainWindow::doctor()
{
    if (refuseWhileBusy())
        return;
    m_busy = true;
    setStatus(ui::tr("Checking the system (brew doctor)…"));
    runAsync<Result<octo::DoctorReport>>(
        [this] {
            Result<octo::DoctorReport> r;
            try {
                r.value = m_mgr.doctor();
            } catch (const std::exception &e) {
                r.error = QString::fromUtf8(e.what());
            }
            return r;
        },
        [this](const Result<octo::DoctorReport> &r) {
            m_busy = false;
            setStatus(r.error.isEmpty() ? ui::tr("Ready.") : ui::trf("ERROR: %1", {r.error}), !r.error.isEmpty());
            if (!r.error.isEmpty()) {
                notice(ui::tr("Error"), r.error, true);
                return;
            }
            log(QStringLiteral("brew doctor"));
            appendOutput(qs(r.value.output) + "\n");
            if (r.value.ready) {
                notice(ui::tr("Check the system"), ui::tr("Your system is ready to brew."));
                return;
            }
            if (r.value.warnings.empty()) {
                notice(ui::tr("Check the system"), ui::tr("brew doctor reported problems.") + "\n" + ui::tr("Details in the Output tab."));
                return;
            }
            notice(ui::tr("Check the system"), ui::trf("brew doctor found %1 warning(s):", {QString::number(r.value.warnings.size())})
                                                   + "\n• " + toQ(r.value.warnings).join(QStringLiteral("\n• ")) + "\n\n"
                                                   + ui::tr("Details in the Output tab."));
        });
}

void MainWindow::exportBrewfile()
{
    if (refuseWhileBusy())
        return;
    const QString file = askFile(true, ui::tr("Export a Brewfile"), QDir::homePath() + QStringLiteral("/Brewfile"));
    if (file.isEmpty())
        return;
    runOperation(ui::tr("Exporting the Brewfile…"), qs(m_mgr.commandLine({"bundle", "dump", "--file=" + file.toStdString(), "--force"})),
                 [this, file](const octo::OutputFn &out) { m_mgr.snapshot(file.toStdString(), out); },
                 [this, file] { notice(ui::tr("Command finished OK!"), ui::trf("Brewfile saved: %1", {file})); });
}

void MainWindow::importBrewfile()
{
    if (refuseWhileBusy())
        return;
    const QString file = askFile(false, ui::tr("Install from a Brewfile"), QDir::homePath() + QStringLiteral("/Brewfile"));
    if (file.isEmpty())
        return;
    // A prévia: o que falta, pelo brew bundle check.
    m_busy = true;
    setStatus(ui::tr("Collecting transaction data…"));
    runAsync<Result<std::vector<std::string>>>(
        [this, file] {
            Result<std::vector<std::string>> r;
            try {
                r.value = m_mgr.bundleCheck(file.toStdString());
            } catch (const std::exception &e) {
                r.error = QString::fromUtf8(e.what());
            }
            return r;
        },
        [this, file](const Result<std::vector<std::string>> &r) {
            m_busy = false;
            setStatus(r.error.isEmpty() ? ui::tr("Ready.") : ui::trf("ERROR: %1", {r.error}), !r.error.isEmpty());
            if (!r.error.isEmpty()) {
                notice(ui::tr("Error"), r.error, true);
                return;
            }
            if (r.value.empty()) {
                notice(ui::tr("Install from a Brewfile"), ui::trf("Everything in %1 is already installed.", {file}));
                return;
            }
            const QStringList missing = toQ(r.value);
            log(ui::tr("Transaction:"));
            for (const auto &m : missing)
                log("  + " + m);
            ConfirmRequest req;
            req.title = ui::tr("Install from a Brewfile");
            req.text = ui::tr("Confirm?");
            req.details = section(ui::trf("The following %1 packages need to be retrieved", {QString::number(missing.size())}), missing)
                          + QStringLiteral("<p>%1</p>").arg(ui::tr("Installs what is missing from it; nothing is removed or upgraded.").toHtmlEscaped());
            if (!confirm(req).accepted)
                return;
            const bool interactive = askpassAvailable();
            runOperation(ui::tr("Installing from the Brewfile…"),
                         qs(m_mgr.commandLine({"bundle", "install", "--file=" + file.toStdString(), "--no-upgrade"})),
                         [this, file, interactive](const octo::OutputFn &out) { m_mgr.restore(file.toStdString(), out, interactive); },
                         [this, file] {
                             notice(ui::tr("Command finished OK!"), ui::trf("Installed from: %1", {file}) + "\n" + ui::tr("Details in the Output tab."));
                         });
        });
}
