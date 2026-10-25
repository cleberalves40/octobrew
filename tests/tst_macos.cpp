// SPDX-License-Identifier: GPL-2.0-or-later
//
// A Fase 4: o notificador (o resumo, o menu, a verificação com o brew falso e
// o LaunchAgent), as Preferências, a contagem do Dock e o --upgrade. O HOME
// dos testes é um diretório temporário: nada vai para o ~/Library de verdade.

#include "fixtures.h"
#include "i18n.h"
#include "mainwindow.h"
#include "notifier.h"
#include "preferences.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QMenu>
#include <QSpinBox>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

class TstMacos : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_home;
    QByteArray m_realHome;

    static QStringList menuTexts(QMenu *menu)
    {
        QStringList out;
        for (QAction *a : menu->actions())
            if (!a->isSeparator())
                out << a->text();
        return out;
    }

private slots:
    void initTestCase()
    {
        m_realHome = qgetenv("HOME");
        qputenv("HOME", m_home.path().toLocal8Bit());
        QCOMPARE(QDir::homePath(), m_home.path());
    }
    void cleanupTestCase() { qputenv("HOME", m_realHome); }
    void init() { i18n::setLocale("en_US"); }

    void summaryTitleAndIcon()
    {
        const auto installed = octo::parseInstalled(kInstalledJson);
        const notifier::Summary s = notifier::summarize(installed);
        // curl e o cask docker; o wget está fixado e não conta.
        QCOMPARE(s.state, notifier::State::Outdated);
        QCOMPARE(s.outdated, (QStringList{QStringLiteral("curl 8.9.1 → 8.10.0"), QStringLiteral("docker 4.38.0 → 4.40.0")}));
        QCOMPARE(notifier::title(s), QStringLiteral("2 updates available"));
        notifier::Summary one = s;
        one.outdated.removeLast();
        QCOMPARE(notifier::title(one), QStringLiteral("1 update available"));
        QCOMPARE(notifier::title(notifier::summarize({})), QStringLiteral("Homebrew is up to date"));
        notifier::Summary error;
        error.state = notifier::State::Error;
        error.error = QStringLiteral("boom");
        QCOMPARE(notifier::title(error), QStringLiteral("ERROR: boom"));
        // O polvo na cor do estado.
        QVERIFY(!notifier::icon(s).isNull());
        QVERIFY(notifier::icon(s).cacheKey() != notifier::icon(notifier::summarize({})).cacheKey());
        QVERIFY(notifier::icon(error).cacheKey() != notifier::icon(s).cacheKey());
    }

    void launchAgent()
    {
        const QString home = m_home.path();
        const QString plist = notifier::agentPlist(QStringLiteral("/Applications/OctoBrew.app/Contents/MacOS/octobrew-notifier"));
        QVERIFY(plist.contains(QStringLiteral("<string>io.github.cleberalves40.octobrew.notifier</string>")));
        QVERIFY(plist.contains(QStringLiteral("<string>/Applications/OctoBrew.app/Contents/MacOS/octobrew-notifier</string>")));
        QVERIFY(plist.contains(QStringLiteral("<key>RunAtLoad</key>\n    <true/>")));
        QVERIFY(!notifier::startsAtLogin(home));
        QVERIFY(notifier::setStartAtLogin(true, QStringLiteral("/x/octobrew-notifier"), home));
        QVERIFY(notifier::startsAtLogin(home));
        QCOMPARE(notifier::agentPath(home), home + "/Library/LaunchAgents/io.github.cleberalves40.octobrew.notifier.plist");
        QVERIFY(notifier::setStartAtLogin(false, QString(), home));
        QVERIFY(!notifier::startsAtLogin(home));
        QVERIFY(notifier::setStartAtLogin(false, QString(), home));   // já fora: nada a fazer
    }

    void notifierChecksWithTheBrew()
    {
        FakeBrew brew;
        Notifier::Options o;
        o.brewPath = QString::fromStdString(brew.path);
        o.settingsPath = QString::fromStdString(brew.dir.file("config"));
        o.synchronous = true;
        o.showIcon = false;
        Notifier n(o);
        QStringList launched;
        n.setLauncher([&launched](const QStringList &args) { launched = args.isEmpty() ? QStringList{"(abrir)"} : args; });
        QCOMPARE(n.summary().state, notifier::State::Checking);
        n.check();
        QCOMPARE(n.summary().state, notifier::State::Outdated);
        // Como o octopi-notifier: o brew update antes de contar.
        QCOMPARE(brew.operations(), std::vector<std::string>{"update"});
        const QStringList texts = menuTexts(n.menu());
        QCOMPARE(texts.first(), QStringLiteral("2 updates available"));
        QVERIFY(texts.contains(QStringLiteral("    curl 8.9.1 → 8.10.0")));
        QAction *upgrade = nullptr;
        for (QAction *a : n.menu()->actions())
            if (a->text() == QStringLiteral("Install updates"))
                upgrade = a;
        QVERIFY(upgrade && upgrade->isEnabled());
        upgrade->trigger();
        QCOMPARE(launched, QStringList{QStringLiteral("--upgrade")});
        // De novo, sem o brew update (a preferência é relida a cada verificação),
        // e os itens não se repetem no menu.
        octo::Settings s(brew.dir.file("config"));
        s.setValue("notifier.update", "no");
        s.save();
        n.check();
        QCOMPARE(brew.operations().size(), size_t(1));
        QCOMPARE(menuTexts(n.menu()).count(QStringLiteral("    curl 8.9.1 → 8.10.0")), 1);
        // O brew falha: o polvo vermelho e o erro.
        writeFile(brew.dir.file("installed.json"), "não é JSON");
        n.check();
        QCOMPARE(n.summary().state, notifier::State::Error);
        QVERIFY(menuTexts(n.menu()).first().startsWith(QStringLiteral("ERROR: ")));
        QVERIFY(!upgrade->isEnabled());
    }

    void preferencesDialog()
    {
        QTemporaryDir dir;
        octo::Settings settings(dir.filePath(QStringLiteral("config")).toStdString());
        QCOMPARE(prefs::language(settings), QStringLiteral("system"));
        QCOMPARE(prefs::notifierHours(settings), 6);
        QVERIFY(prefs::notifierUpdates(settings));
        PreferencesDialog d(settings, QStringLiteral("/x/octobrew-notifier"));
        d.languageBox()->setCurrentIndex(2);    // Português
        d.appearanceBox()->setCurrentIndex(2);  // escura
        d.hoursBox()->setValue(12);
        d.updateBox()->setChecked(false);
        d.loginBox()->setChecked(true);
        QVERIFY(d.save());
        octo::Settings reread(dir.filePath(QStringLiteral("config")).toStdString());
        QCOMPARE(prefs::language(reread), QStringLiteral("pt_BR"));
        QCOMPARE(prefs::notifierHours(reread), 12);
        QVERIFY(!prefs::notifierUpdates(reread));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QCOMPARE(prefs::appearance(reread), QStringLiteral("dark"));
        // O plugin offscreen dos testes não troca o esquema de cores; o cocoa, sim.
        if (QGuiApplication::platformName() != QStringLiteral("offscreen"))
            QCOMPARE(QGuiApplication::styleHints()->colorScheme(), Qt::ColorScheme::Dark);
        prefs::applyAppearance(QStringLiteral("system"));
#endif
        // O LaunchAgent, no HOME dos testes.
        QVERIFY(notifier::startsAtLogin(m_home.path()));
        PreferencesDialog again(reread, QStringLiteral("/x/octobrew-notifier"));
        QVERIFY(again.loginBox()->isChecked());
        again.loginBox()->setChecked(false);
        QVERIFY(again.save());
        QVERIFY(!notifier::startsAtLogin(m_home.path()));
        // Sem o octobrew-notifier ao lado, a opção fica desligada.
        PreferencesDialog none(reread, QString());
        QVERIFY(!none.loginBox()->isEnabled());
    }

    void upgradeOnStartAndDockBadge()
    {
        FakeBrew brew;
        brew.preview("upgrade", kUpgradeDryRun);
        WindowOptions o;
        o.brewPath = QString::fromStdString(brew.path);
        o.settingsPath = QString::fromStdString(brew.dir.file("config"));
        o.synchronous = true;
        o.upgradeOnStart = true;
        MainWindow w(o);
        QList<ConfirmRequest> requests;
        w.setConfirmHandler([&requests](const ConfirmRequest &r) {
            requests << r;
            return ConfirmAnswer{false, false};
        });
        w.setNoticeHandler([](const QString &, const QString &, bool) {});
        w.refresh();
        // Logo depois da lista, o diálogo do Instalar as atualizações (uma vez só).
        QCOMPARE(requests.size(), 1);
        QCOMPARE(requests.first().title, QStringLiteral("System upgrade"));
        w.refresh();
        QCOMPARE(requests.size(), 1);
        QCOMPARE(w.badgeCount(), 2);   // curl e docker; o wget fixado não conta
        QVERIFY(w.action(QStringLiteral("preferences")));
        QCOMPARE(w.action(QStringLiteral("preferences"))->menuRole(), QAction::PreferencesRole);
#ifdef Q_OS_MACOS
        QVERIFY(w.action(QStringLiteral("minimize")));
        QCOMPARE(w.action(QStringLiteral("minimize"))->shortcut().toString(QKeySequence::NativeText),
                 QKeySequence(QStringLiteral("Ctrl+M")).toString(QKeySequence::NativeText));
#endif
    }
};

QTEST_MAIN(TstMacos)
#include "tst_macos.moc"
