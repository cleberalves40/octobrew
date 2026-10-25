// SPDX-License-Identifier: GPL-2.0-or-later
//
// A Fase 3, sem tela e sem threads, com o brew falso do octobrew-tui: as
// abas Notícias e Serviços, os serviços, os taps, o brew doctor e os
// Brewfiles, com os diálogos respondidos pelo teste.

#include "fixtures.h"
#include "i18n.h"
#include "mainwindow.h"
#include "packagemodel.h"

#include <QAction>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QtTest>
#include <memory>

namespace {

struct Fixture {
    FakeBrew brew;
    std::unique_ptr<MainWindow> window;
    QList<ConfirmRequest> requests;
    ConfirmAnswer answer{true, false};
    QStringList notices;
    std::optional<int> choice = 0;
    QStringList chooseItems;
    QString file;                        // a resposta do diálogo de arquivo
    std::optional<MainWindow::TapRequest> tap;

    explicit Fixture(bool withCurl = false)
    {
        brew.writeApiCache();
        writeFile(brew.dir.file("feed.xml"), kAtomFeed);
        if (withCurl)
            writeExecutable(brew.dir.file("curl.sh"), "#!/bin/sh\necho \"$*\" >> '" + brew.dir.file("curl.log") + "'\n"
                                                     "while [ $# -gt 1 ]; do [ \"$1\" = -o ] && out=$2; shift; done\n"
                                                     "cp '" + brew.dir.file("feed.xml") + "' \"$out\"\n");
        WindowOptions o;
        o.brewPath = QString::fromStdString(brew.path);
        o.settingsPath = QString::fromStdString(brew.dir.file("config"));
        o.curlPath = QString::fromStdString(brew.dir.file("curl.sh"));   // sem rede nos testes
        o.synchronous = true;
        window = std::make_unique<MainWindow>(o);
        window->setConfirmHandler([this](const ConfirmRequest &r) {
            requests << r;
            return answer;
        });
        window->setNoticeHandler([this](const QString &t, const QString &x, bool) { notices << t + ": " + x; });
        window->setChooseHandler([this](const QString &, const QString &, const QStringList &items) {
            chooseItems = items;
            return choice;
        });
        window->setFileHandler([this](bool, const QString &, const QString &) { return file; });
        window->setTapHandler([this] { return tap; });
        window->refresh();
    }
    QStringList operations() const
    {
        QStringList out;
        for (const auto &o : brew.operations())
            out << QString::fromStdString(o);
        return out;
    }
    int calls(const std::string &call) const
    {
        int n = 0;
        for (const auto &c : brew.calls())
            n += c == call;
        return n;
    }
};

} // namespace

class TstHomebrew : public QObject
{
    Q_OBJECT

private slots:
    void init() { i18n::setLocale("en_US"); }

    void newsTab()
    {
        Fixture f(true);
        QVERIFY(f.window->news().empty());   // ainda não visitada: nada baixado
        f.window->action(QStringLiteral("tabNews"))->trigger();   // ⌥5
        QCOMPARE(f.window->tabs()->currentIndex(), int(MainWindow::NewsTab));
        QCOMPARE(f.window->news().size(), size_t(2));
        const QString html = f.window->newsView()->toHtml();
        QVERIFY(html.contains(QStringLiteral("Homebrew 7.0.0")));
        QVERIFY(html.contains(QStringLiteral("https://brew.sh/2026/09/13/homebrew-7.0.0/")));
        QVERIFY(f.window->newsView()->toPlainText().contains(QStringLiteral("2026-09-13")));
        QCOMPARE(readLines(f.brew.dir.file("curl.log")).size(), size_t(1));
        // Voltar à aba não baixa de novo; ⌘G, sim.
        f.window->action(QStringLiteral("tabInfo"))->trigger();
        f.window->action(QStringLiteral("tabNews"))->trigger();
        QCOMPARE(readLines(f.brew.dir.file("curl.log")).size(), size_t(1));
        f.window->action(QStringLiteral("refreshNews"))->trigger();
        QCOMPARE(readLines(f.brew.dir.file("curl.log")).size(), size_t(2));
        // O cache fica ao lado das preferências.
        QVERIFY(QFile::exists(QString::fromStdString(f.brew.dir.file("news.xml"))));
    }

    void newsWithoutNetwork()
    {
        Fixture f;   // curl.sh não existe
        f.window->action(QStringLiteral("tabNews"))->trigger();
        const QString text = f.window->newsView()->toPlainText();
        QVERIFY2(text.contains(QStringLiteral("WARNING: Failed to start")), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("No news yet.")));
    }

    void servicesTabAndActions()
    {
        Fixture f;
        writeFile(f.brew.dir.file("services.json"), kServicesJson);
        QCOMPARE(f.calls("services|list|--json"), 0);   // só na primeira visita
        f.window->action(QStringLiteral("tabServices"))->trigger();   // ⌥7
        QTableWidget *t = f.window->servicesView();
        QCOMPARE(t->rowCount(), 3);
        QCOMPARE(t->item(0, 0)->text(), QStringLiteral("foo"));
        QCOMPARE(t->item(0, 1)->text(), QStringLiteral("Started"));
        QCOMPARE(t->item(1, 1)->text(), QStringLiteral("Not running"));
        QCOMPARE(t->item(2, 3)->text(), QStringLiteral("78"));
        // Na aba, a ação vale para a linha selecionada.
        t->selectRow(2);
        f.window->action(QStringLiteral("serviceRestart"))->trigger();
        QCOMPARE(f.operations().last(), QStringLiteral("services|restart|postgresql@17"));
        QCOMPARE(f.calls("services|list|--json"), 2);   // relida depois da ação
        // Fora dela, a da fórmula selecionada; sem serviço, só o aviso.
        f.window->action(QStringLiteral("tabInfo"))->trigger();
        QVERIFY(f.window->selectKey(QStringLiteral("f:curl")));
        f.window->service(QStringLiteral("start"));
        QCOMPARE(f.window->statusText(), QStringLiteral("curl has no service."));
        QVERIFY(f.window->selectKey(QStringLiteral("f:someone/tap/foo")));
        QVERIFY(f.window->infoView()->toPlainText().contains(QStringLiteral("Started (cleber)")));
        f.window->service(QStringLiteral("stop"));
        QCOMPARE(f.operations().last(), QStringLiteral("services|stop|foo"));
    }

    void addAndRemoveTaps()
    {
        Fixture f;
        f.tap = MainWindow::TapRequest{QStringLiteral("not a tap"), false};
        f.window->addTap();
        QVERIFY(f.notices.last().contains(QStringLiteral("Invalid tap name: not a tap")));
        QVERIFY(f.operations().isEmpty());
        f.tap = MainWindow::TapRequest{QStringLiteral("someone/tap"), true};
        f.window->addTap();
        QCOMPARE(f.operations(), (QStringList{QStringLiteral("tap|someone/tap"), QStringLiteral("trust|--tap|someone/tap")}));
        f.tap.reset();   // cancelado
        f.window->addTap();
        QCOMPARE(f.operations().size(), 2);

        f.window->removeTap();
        QCOMPARE(f.notices.last(), QStringLiteral("Remove a tap: There are no third-party taps."));
        std::string taps = kTapsJson;
        taps.insert(taps.rfind(']'), R"(, {"name": "other/tap", "official": false, "trusted": true, "formula_names": ["other/tap/x"], "cask_tokens": []})");
        writeFile(f.brew.dir.file("taps.json"), taps);
        f.choice = 0;   // someone/tap: o foo dele está instalado
        f.window->removeTap();
        QCOMPARE(f.chooseItems, (QStringList{QStringLiteral("someone/tap  (3)  untrusted"), QStringLiteral("other/tap  (1)")}));
        QVERIFY(f.notices.last().contains(QStringLiteral("remove them first:\n• someone/tap/foo")));
        f.choice = 1;
        f.window->removeTap();
        QVERIFY(f.requests.last().danger);
        QCOMPARE(f.operations().last(), QStringLiteral("untap|other/tap"));
    }

    void doctorReports()
    {
        Fixture f;
        f.window->doctor();
        QCOMPARE(f.notices.last(), QStringLiteral("Check the system: Your system is ready to brew."));
        writeFile(f.brew.dir.file("doctor.txt"), kDoctorOutput);
        f.window->doctor();
        QVERIFY(f.notices.last().contains(QStringLiteral("brew doctor found 2 warning(s):\n• Some installed casks are deprecated or disabled.")));
        QVERIFY(f.window->outputView()->toPlainText().contains(QStringLiteral("You should find replacements for the following casks:")));
        QVERIFY(f.operations().isEmpty());   // o doctor só lê
    }

    void brewfiles()
    {
        Fixture f;
        const QString file = QString::fromStdString(f.brew.dir.file("meu.Brewfile"));
        f.file.clear();   // cancelado no diálogo
        f.window->exportBrewfile();
        QVERIFY(f.operations().isEmpty());
        f.file = file;
        f.window->exportBrewfile();
        QCOMPARE(f.operations(), QStringList{QStringLiteral("bundle|dump|--file=") + file + QStringLiteral("|--force")});
        QCOMPARE(f.notices.last(), QStringLiteral("Command finished OK!: Brewfile saved: ") + file);
        f.window->importBrewfile();
        QCOMPARE(f.notices.last(), QStringLiteral("Install from a Brewfile: Everything in ") + file + QStringLiteral(" is already installed."));
        writeFile(f.brew.dir.file("bundle-check.txt"), kBundleCheckOutput);
        f.window->importBrewfile();
        QVERIFY(f.requests.last().details.contains(QStringLiteral("<li>Cask iterm2</li>")));
        QVERIFY(f.requests.last().details.contains(QStringLiteral("<li>Formula figlet</li>")));
        QCOMPARE(f.operations().last(), QStringLiteral("bundle|install|--file=") + file + QStringLiteral("|--no-upgrade"));
    }

    void homebrewMenuAndPortuguese()
    {
        i18n::setLocale("pt_BR");
        Fixture f;
        QCOMPARE(f.window->tabs()->tabText(MainWindow::NewsTab), QStringLiteral("Notícias"));
        QCOMPARE(f.window->tabs()->tabText(MainWindow::ServicesTab), QStringLiteral("Serviços"));
        QCOMPARE(f.window->action(QStringLiteral("doctor"))->text(), QStringLiteral("Verificar o sistema (brew doctor)"));
        QCOMPARE(f.window->servicesView()->horizontalHeaderItem(3)->text(), QStringLiteral("Código de saída"));
        i18n::setLocale("en_US");
    }
};

QTEST_MAIN(TstHomebrew)
#include "tst_homebrew.moc"
