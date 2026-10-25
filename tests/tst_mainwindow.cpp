// SPDX-License-Identifier: GPL-2.0-or-later
//
// A janela, sem tela (offscreen) e sem threads (synchronous), com o brew falso
// dos testes do octobrew-tui: listagem, repositório, taps, busca, folhas,
// detalhes, arquivos, Saída, Ajuda e preferências.

#include "fixtures.h"
#include "i18n.h"
#include "mainwindow.h"
#include "packagemodel.h"

#include <QAction>
#include <QHeaderView>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTableView>
#include <QTextBrowser>
#include <QtTest>
#include <memory>

namespace {

struct Fixture {
    FakeBrew brew;
    std::unique_ptr<MainWindow> window;
    explicit Fixture(bool apiCache = true)
    {
        if (apiCache)
            brew.writeApiCache();
        (void)std::system(("mkdir -p '" + brew.dir.file("Cellar/curl/8.9.1") + "'").c_str());
        writeFile(brew.dir.file("Cellar/curl/8.9.1/curl"), std::string(2048, 'x'));
        window = std::make_unique<MainWindow>(options());
        window->refresh();
    }
    WindowOptions options() const
    {
        WindowOptions o;
        o.brewPath = QString::fromStdString(brew.path);
        o.settingsPath = QString::fromStdString(brew.dir.file("config"));
        o.synchronous = true;
        return o;
    }
    QString info() const { return window->infoView()->toPlainText(); }
};

} // namespace

class TstMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void init() { i18n::setLocale("en_US"); }

    void listsInstalledWithSizes()
    {
        Fixture f;
        PackageModel *m = f.window->packageModel();
        QCOMPARE(m->rowCount(), 6);
        QCOMPARE(f.window->statusText(), QStringLiteral("Ready."));
        QCOMPARE(f.window->countText(), QStringLiteral("6 package(s)"));
        QVERIFY(!f.window->busy());
        QCOMPARE(m->data(m->index(m->rowOf(QStringLiteral("f:curl")), model::Size)).toString(), QStringLiteral("2.00 KiB"));
        QVERIFY(f.window->outputView()->toPlainText().contains(QStringLiteral("6 package(s) installed: 4 formula(e) and 2 cask(s).")));
        // A primeira linha fica selecionada, com os detalhes pedidos.
        QCOMPARE(f.window->currentKey(), QStringLiteral("c:alacritty"));
    }

    void tapsSearchAndLeaves()
    {
        Fixture f;
        QListWidget *taps = f.window->tapList();
        QCOMPARE(taps->count(), 4);   // (todos), homebrew/cask, homebrew/core, someone/tap
        QVERIFY(taps->item(0)->text().startsWith(QStringLiteral("(all taps)")));
        taps->setCurrentRow(3);
        QCOMPARE(f.window->packageModel()->rowCount(), 1);
        QCOMPARE(f.window->currentKey(), QStringLiteral("f:someone/tap/foo"));
        taps->setCurrentRow(0);
        f.window->searchField()->setText(QStringLiteral("terminal"));
        QCOMPARE(f.window->packageModel()->rowCount(), 0);
        f.window->action(QStringLiteral("searchDescription"))->trigger();   // F7
        QCOMPARE(f.window->packageModel()->rowCount(), 1);
        QCOMPARE(f.window->searchField()->placeholderText(), QStringLiteral("Filter by description…"));
        f.window->searchField()->clear();
        f.window->action(QStringLiteral("leaves"))->trigger();
        QCOMPARE(f.window->packageModel()->rowCount(), 3);
    }

    void repositoryView()
    {
        Fixture f;
        f.window->action(QStringLiteral("toggleView"))->trigger();
        QVERIFY(f.window->remoteView());
        QCOMPARE(f.window->packageModel()->rowCount(), 9);   // 8 da API e o foo do tap de terceiros
        QVERIFY(f.window->selectKey(QStringLiteral("c:docker")));
        QVERIFY(f.window->selectKey(QStringLiteral("f:docker")));   // fórmula e cask de mesmo nome
        QVERIFY(f.window->selectKey(QStringLiteral("f:vim")));
        QVERIFY(f.info().contains(QStringLiteral("Not installed")));
        // De volta, sem chamar o brew de novo.
        const auto calls = f.brew.calls().size();
        f.window->toggleView();
        QVERIFY(!f.window->remoteView());
        QCOMPARE(f.brew.calls().size(), calls);
    }

    void repositoryWithoutCacheFallsBack()
    {
        Fixture f(false);
        f.window->toggleView();
        QVERIFY(!f.window->remoteView());
        QVERIFY(f.window->statusText().contains(QStringLiteral("brew update")));
        QCOMPARE(f.window->packageModel()->rowCount(), 6);
    }

    void detailsAndFiles()
    {
        Fixture f;
        QVERIFY(f.window->selectKey(QStringLiteral("f:curl")));
        QString info = f.info();
        for (const char *t : {"curl", "Outdated (version 8.10.0 is available)", "https://curl.se", "provided by macos",
                              "Caveats", "curl is keg-only"})
            QVERIFY2(info.contains(QString::fromUtf8(t)), t);
        QCOMPARE(f.window->tabs()->tabText(MainWindow::FilesTab), QStringLiteral("Files (2)"));
        QVERIFY(f.window->filesView()->toPlainText().contains(QStringLiteral("/opt/homebrew/Cellar/curl/8.9.1/bin/curl")));
        // O brew falso não tem o info do brotli: o erro aparece na aba.
        QVERIFY(f.window->selectKey(QStringLiteral("f:brotli")));
        QVERIFY(f.info().contains(QStringLiteral("ERROR: No available formula with the name \"brotli\"")));
        // Os detalhes ficam em cache: voltar ao curl não chama o brew.
        const auto calls = f.brew.calls().size();
        QVERIFY(f.window->selectKey(QStringLiteral("f:curl")));
        QCOMPARE(f.brew.calls().size(), calls);
    }

    void tabsAndHelpWithNativeShortcuts()
    {
        Fixture f;
        f.window->action(QStringLiteral("tabOutput"))->trigger();
        QCOMPARE(f.window->tabs()->currentIndex(), int(MainWindow::OutputTab));
        f.window->action(QStringLiteral("tabHelp"))->trigger();
        QCOMPARE(f.window->tabs()->currentIndex(), int(MainWindow::HelpTab));
        // Na notação do sistema: ⌘R no macOS, Ctrl+R no Linux.
        const QString help = f.window->helpView()->toPlainText();
        QVERIFY2(help.contains(QKeySequence(QStringLiteral("Ctrl+R")).toString(QKeySequence::NativeText)), qPrintable(help));
        QVERIFY(help.contains(QKeySequence(QStringLiteral("Alt+1")).toString(QKeySequence::NativeText)));
        QVERIFY(help.contains(QStringLiteral("Refresh the package list")));
    }

    void sortAndSearchModeAreSaved()
    {
        Fixture f;
        f.window->table()->sortByColumn(model::Version, Qt::DescendingOrder);
        f.window->action(QStringLiteral("searchDescription"))->trigger();
        Fixture again;   // um brew falso novo; as preferências do primeiro
        auto o = again.options();
        o.settingsPath = QString::fromStdString(f.brew.dir.file("config"));
        MainWindow w(o);
        QCOMPARE(w.packageModel()->sortSpec().column, int(model::Version));
        QVERIFY(!w.packageModel()->sortSpec().ascending);
        QVERIFY(w.action(QStringLiteral("searchDescription"))->isChecked());
    }

    void errorKeepsLastList()
    {
        Fixture f;
        writeFile(f.brew.dir.file("installed.json"), "isto não é JSON");
        f.window->refresh();
        QVERIFY(f.window->statusText().startsWith(QStringLiteral("ERROR: ")));
        QCOMPARE(f.window->packageModel()->rowCount(), 6);
    }

    void portuguese()
    {
        i18n::setLocale("pt_BR");
        Fixture f;
        QCOMPARE(f.window->statusText(), QStringLiteral("Pronto."));
        QCOMPARE(f.window->tabs()->tabText(MainWindow::InfoTab), QStringLiteral("Informação"));
        QCOMPARE(f.window->packageModel()->headerData(model::Version, Qt::Horizontal).toString(), QStringLiteral("Versão"));
        i18n::setLocale("en_US");
    }
};

QTEST_MAIN(TstMainWindow)
#include "tst_mainwindow.moc"
