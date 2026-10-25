// SPDX-License-Identifier: GPL-2.0-or-later
//
// As transações da janela, sem tela e sem threads, com o brew falso do
// octobrew-tui: as marcas, a aba Ações, o diálogo (a ConfirmRequest), as
// operações na ordem certa e com o ambiente certo, o snapshot, as falhas e o
// diálogo de senha do octobrew-askpass.

#include "askpass.h"
#include "fixtures.h"
#include "i18n.h"
#include "mainwindow.h"
#include "packagemodel.h"

#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QPushButton>
#include "transactiondialog.h"
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>
#include <memory>

namespace {

struct Fixture {
    FakeBrew brew;
    std::unique_ptr<MainWindow> window;
    QList<ConfirmRequest> requests;      // o que cada diálogo mostrou
    ConfirmAnswer answer{true, true};     // a resposta dos diálogos
    QStringList notices;                  // "título: texto"
    bool lastNoticeError = false;

    // unpinned: o wget sem o pin, para que o brotli possa sair com os dependentes.
    explicit Fixture(bool unpinned = false)
    {
        if (unpinned) {
            std::string json = kInstalledJson;
            const std::string pinned = "\"pinned\": true";
            json.replace(json.find(pinned), pinned.size(), "\"pinned\": false");
            writeFile(brew.dir.file("installed.json"), json);
        }
        brew.writeApiCache();
        WindowOptions o;
        o.brewPath = QString::fromStdString(brew.path);
        o.settingsPath = QString::fromStdString(brew.dir.file("config"));
        o.synchronous = true;
        window = std::make_unique<MainWindow>(o);
        window->setConfirmHandler([this](const ConfirmRequest &r) {
            requests << r;
            return answer;
        });
        window->setNoticeHandler([this](const QString &title, const QString &text, bool error) {
            notices << title + ": " + text;
            lastNoticeError = error;
        });
        window->refresh();
    }
    void select(const char *key)
    {
        const QString why = QStringLiteral("%1 não está na lista (%2 linhas; status: %3)")
                                .arg(QString::fromUtf8(key))
                                .arg(window->packageModel()->rowCount())
                                .arg(window->statusText());
        QVERIFY2(window->selectKey(QString::fromUtf8(key)), qPrintable(why));
    }
    QStringList operations() const
    {
        QStringList out;
        for (const auto &o : brew.operations())
            out << QString::fromStdString(o);
        return out;
    }
    QString output() const { return window->outputView()->toPlainText(); }
};

} // namespace

class TstTransactions : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        i18n::setLocale("en_US");
        qunsetenv("SUDO_ASKPASS");
    }

    void marksAndActionsTab()
    {
        Fixture f;
        // + num instalado em dia não faz nada; no desatualizado (curl), marca.
        f.select("f:brotli");
        f.window->markInstall();
        QVERIFY(f.window->toInstall().isEmpty());
        QCOMPARE(f.window->statusText(), QStringLiteral("brotli is already installed and up to date."));
        f.select("f:curl");
        f.window->markInstall();
        QCOMPARE(f.window->toInstall(), QSet<QString>{QStringLiteral("f:curl")});
        // Fixado: nem + nem -.
        f.select("f:wget");
        f.window->markRemove();
        QCOMPARE(f.window->statusText(), QStringLiteral("wget is pinned."));
        // - no cask; de novo, desmarca; + e - se excluem.
        f.select("c:alacritty");
        f.window->markRemove();
        f.window->markRemove();
        QVERIFY(f.window->toRemove().isEmpty());
        f.window->markRemove();
        f.select("f:curl");
        f.window->markRemove();
        QCOMPARE(f.window->toRemove(), (QSet<QString>{QStringLiteral("c:alacritty"), QStringLiteral("f:curl")}));
        QVERIFY(f.window->toInstall().isEmpty());
        // A aba Ações e o ícone da marca na lista.
        QCOMPARE(f.window->tabs()->tabText(MainWindow::ActionsTab), QStringLiteral("Actions (2)"));
        QTreeWidget *actions = f.window->actionsView();
        QCOMPARE(actions->topLevelItem(0)->text(0), QStringLiteral("To be removed (2)"));
        QCOMPARE(actions->topLevelItem(0)->childCount(), 2);
        PackageModel *m = f.window->packageModel();
        const QModelIndex curl = m->index(m->rowOf(QStringLiteral("f:curl")), model::StateIcon);
        QVERIFY(m->data(curl, Qt::DecorationRole).value<QIcon>().cacheKey()
                != PackageModel::iconFor(*m->packageAt(curl.row())).cacheKey());
        QVERIFY(f.window->action(QStringLiteral("apply"))->isEnabled());
        f.window->clearMarks();
        QVERIFY(!f.window->action(QStringLiteral("apply"))->isEnabled());
        QCOMPARE(f.window->tabs()->tabText(MainWindow::ActionsTab), QStringLiteral("Actions"));
        // O ftxui, de que o próprio programa depende, não pode sair.
        QVERIFY(f.operations().isEmpty());
    }

    void applyInstallsAndRemovesWithSnapshot()
    {
        Fixture f;
        f.brew.preview("install", kInstallCaskDryRun);
        f.window->toggleView();
        f.select("c:kitty");
        f.window->markInstall();
        f.select("c:alacritty");
        f.window->markRemove();
        f.window->apply();
        QCOMPARE(f.requests.size(), 1);
        const ConfirmRequest &r = f.requests.first();
        QCOMPARE(r.title, QStringLiteral("Confirmation"));
        QVERIFY(!r.danger);
        QVERIFY(r.details.contains(QStringLiteral("The following package needs to be retrieved")));
        QVERIFY(r.details.contains(QStringLiteral("<li>kitty</li>")));
        QVERIFY(r.details.contains(QStringLiteral("alacritty (cask)")));
        QVERIFY(r.snapshot);
        QVERIFY(r.snapshotChecked);   // instala algo: o snapshot vem marcado
        QVERIFY(r.snapshotFile.startsWith(f.window->snapshotDir()));
        const QStringList ops = f.operations();
        QCOMPARE(ops.size(), 3);
        QVERIFY(ops[0].startsWith(QStringLiteral("bundle|dump|--file=") + f.window->snapshotDir()));
        QCOMPARE(ops[1], QStringLiteral("uninstall|--cask|alacritty"));
        QCOMPARE(ops[2], QStringLiteral("install|--no-ask|--cask|kitty"));
        QVERIFY(QFile::exists(r.snapshotFile));
        QVERIFY(f.notices.last().startsWith(QStringLiteral("Command finished OK!: ")));
        QVERIFY(f.notices.last().contains(QStringLiteral("Installed: kitty (cask)")));
        QVERIFY(f.window->toInstall().isEmpty());
        QVERIFY(f.window->toRemove().isEmpty());
        QCOMPARE(f.window->tabs()->currentIndex(), int(MainWindow::OutputTab));
        QVERIFY(f.output().contains(QStringLiteral("brew uninstall --cask alacritty; brew install --cask kitty")));
        QVERIFY(f.output().contains(QStringLiteral("executando install")));
        QVERIFY(!f.window->inOperation());
        // Sem o octobrew-askpass, um sudo que precisasse de senha falharia na hora.
        bool env = false;
        for (const auto &e : readLines(f.brew.log + ".env"))
            env = env || QString::fromStdString(e).contains(QStringLiteral("|1|1|/usr/bin/false"));
        QVERIFY(env);
    }

    void refusingTheDialogChangesNothing()
    {
        Fixture f;
        f.answer = {false, false};
        f.select("f:curl");
        f.window->markInstall();
        f.window->apply();
        QCOMPARE(f.requests.size(), 1);
        QVERIFY(f.operations().isEmpty());
        QCOMPARE(f.window->toInstall().size(), 1);
        // Sem o snapshot, aceito.
        f.answer = {true, false};
        f.window->apply();
        QCOMPARE(f.operations(), QStringList{QStringLiteral("install|--no-ask|--formula|curl")});
    }

    void removalWithDependentsIsAWarning()
    {
        Fixture f(true);
        f.answer = {false, false};
        f.select("f:brotli");
        f.window->markRemove();
        f.window->apply();
        const ConfirmRequest r = f.requests.first();
        QCOMPARE(r.title, QStringLiteral("Warning"));
        QVERIFY(r.danger);
        QVERIFY(!r.snapshotChecked);   // só remove: o snapshot vem desmarcado
        QVERIFY(r.details.contains(QStringLiteral("The following 4 packages will be removed")));
        QVERIFY(r.details.contains(QStringLiteral("(3 of them depend on the marked packages)")));
        QVERIFY(f.operations().isEmpty());
        f.answer = {true, false};
        f.window->apply();
        QCOMPARE(f.operations(), (QStringList{QStringLiteral("uninstall|--cask|docker"),
                                              QStringLiteral("uninstall|--formula|brotli|curl|wget")}));
    }

    void pinnedDependentBlocksTheRemoval()
    {
        Fixture f;   // o wget, fixado, depende do brotli
        f.select("f:brotli");
        f.window->markRemove();
        f.window->apply();
        QVERIFY(f.requests.isEmpty());
        QVERIFY(f.lastNoticeError);
        QCOMPARE(f.notices.last(), QStringLiteral("Error: wget depends on a package marked for removal and cannot be removed."));
    }

    void failures()
    {
        Fixture f;
        f.window->apply();
        QCOMPARE(f.notices.last(), QStringLiteral("Actions: No packages marked. Use + to install and - to remove."));
        f.select("f:curl");
        f.window->markInstall();
        f.brew.failNext("install-dry");
        f.window->apply();
        QCOMPARE(f.notices.last(), QStringLiteral("Error: install-dry falhou"));
        QVERIFY(f.requests.isEmpty());
        QFile::remove(QString::fromStdString(f.brew.dir.file("install-dry.rc")));
        f.brew.failNext("install");
        f.answer = {true, false};
        f.window->apply();
        QCOMPARE(f.notices.last(), QStringLiteral("Error: install falhou"));
        QVERIFY(f.lastNoticeError);
        // A nova listagem não esconde o erro; as marcas ficam.
        QCOMPARE(f.window->statusText(), QStringLiteral("ERROR: install falhou"));
        QCOMPARE(f.window->toInstall().size(), 1);
        QVERIFY(f.window->action(QStringLiteral("refresh"))->isEnabled());
    }

    void sudoGoesThroughTheAskpassHelper()
    {
        Fixture f;
        std::string info = kPkgCaskInfo;
        info.replace(info.find("\"zoom\", \"full_token\": \"zoom\""), 28, "\"alacritty\", \"full_token\": \"alacritty\"");
        writeFile(f.brew.dir.file("alacritty.json"), info);
        qputenv("SUDO_ASKPASS", "/caminho/do/octobrew-askpass");
        f.answer = {true, false};   // sem o snapshot: só a remoção
        f.select("c:alacritty");
        f.window->markRemove();
        f.window->apply();
        QVERIFY(f.requests.first().details.contains(QStringLiteral("Needs sudo: alacritty")));
        QCOMPARE(f.operations(), QStringList{QStringLiteral("uninstall|--cask|alacritty")});
        // Com o octobrew-askpass, o brew herda o SUDO_ASKPASS: o sudo pede a senha por ele.
        bool helper = false;
        for (const auto &e : readLines(f.brew.log + ".env"))
            helper = helper || QString::fromStdString(e).endsWith(QStringLiteral("|1|1|/caminho/do/octobrew-askpass"));
        QVERIFY(helper);
        qunsetenv("SUDO_ASKPASS");
    }

    void upgradeAndUpdate()
    {
        Fixture f;
        f.window->upgrade();
        QCOMPARE(f.notices.last(), QStringLiteral("System upgrade: There are no new updates available!"));
        f.brew.preview("upgrade", kUpgradeDryRun);
        f.window->upgrade();
        const ConfirmRequest r = f.requests.last();
        QCOMPARE(r.title, QStringLiteral("System upgrade"));
        QVERIFY(r.details.contains(QStringLiteral("The following 2 packages need to be retrieved")));
        QVERIFY(r.details.contains(QStringLiteral("curl 8.9.1 -&gt; 8.10.0")));
        QVERIFY(r.details.contains(QStringLiteral("Pinned packages are not upgraded.")));
        QVERIFY(r.snapshotChecked);
        QStringList ops = f.operations();
        QCOMPARE(ops.size(), 2);
        QVERIFY(ops[0].startsWith(QStringLiteral("bundle|dump|")));
        QCOMPARE(ops[1], QStringLiteral("upgrade|--no-ask"));
        f.window->action(QStringLiteral("update"))->trigger();
        QCOMPARE(f.operations().last(), QStringLiteral("update"));
        QCOMPARE(f.window->statusText(), QStringLiteral("Ready."));
    }

    void cleanupAutoremovePinAndRestore()
    {
        Fixture f;
        f.window->cleanup();
        QCOMPARE(f.notices.last(), QStringLiteral("Clean the local cache: Nothing to clean."));
        f.brew.preview("cleanup", kCleanupDryRun);
        f.window->cleanup();
        QVERIFY(f.requests.last().details.contains(QStringLiteral("It would free approximately 6.2MB.")));
        QCOMPARE(f.operations().last(), QStringLiteral("cleanup|--prune=all"));

        f.window->autoremove();
        QCOMPARE(f.notices.last(), QStringLiteral("Remove unneeded dependencies: There are no unneeded dependencies."));
        f.brew.preview("autoremove", kAutoremoveDryRun);
        f.window->autoremove();
        QVERIFY(f.requests.last().details.contains(QStringLiteral("<li>libgpg-error</li>")));
        QCOMPARE(f.operations().last(), QStringLiteral("autoremove"));

        f.select("f:curl");
        f.window->pin(true);
        QCOMPARE(f.operations().last(), QStringLiteral("pin|--formula|curl"));
        f.select("f:wget");
        f.window->pin(true);
        QCOMPARE(f.window->statusText(), QStringLiteral("wget is pinned."));
        f.window->pin(false);
        QCOMPARE(f.operations().last(), QStringLiteral("unpin|--formula|wget"));

        const QString file = f.window->snapshotDir() + "/261020-080000.Brewfile";
        QDir().mkpath(f.window->snapshotDir());
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("brew \"curl\"\n");
        out.close();
        f.window->restoreSnapshot(file);
        QCOMPARE(f.operations().last(), QStringLiteral("bundle|install|--file=") + file + QStringLiteral("|--no-upgrade"));
        QVERIFY(f.notices.last().contains(QStringLiteral("Restored: ") + file));
    }

    void warningDefaultsToNo()
    {
        // No Aviso, o Enter não pode aplicar: o padrão e o foco são do "Não".
        for (const bool danger : {true, false}) {
            ConfirmRequest r;
            r.title = QStringLiteral("t");
            r.danger = danger;
            TransactionDialog d(r);
            d.show();
            QCoreApplication::processEvents();
            auto *box = d.findChild<QDialogButtonBox *>();
            QPushButton *yes = box->button(QDialogButtonBox::Yes);
            QPushButton *no = box->button(QDialogButtonBox::No);
            QCOMPARE(no->isDefault(), danger);
            QCOMPARE(yes->isDefault(), !danger);
            QCOMPARE(QApplication::focusWidget() == no, danger);
            d.close();
        }
    }

    void askpassDialog()
    {
        // O diálogo do octobrew-askpass: a senha digitada volta; cancelar, nada.
        QTimer::singleShot(0, [] {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QVERIFY(dialog->labelText().contains(QStringLiteral("Password for cleber:")));
            dialog->setTextValue(QStringLiteral("segredo"));
            dialog->accept();
        });
        QCOMPARE(askpass::askPassword(QStringLiteral("Password for cleber:")).value_or(QString()), QStringLiteral("segredo"));
        QTimer::singleShot(0, [] {
            if (auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget()))
                dialog->reject();
        });
        QVERIFY(!askpass::askPassword(QString()).has_value());
    }
};

QTEST_MAIN(TstTransactions)
#include "tst_transactions.moc"
