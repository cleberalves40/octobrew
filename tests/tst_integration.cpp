// SPDX-License-Identifier: GPL-2.0-or-later
//
// A janela contra o Homebrew real, só lendo: OCTOBREW_TEST_BREW aponta o brew
// (sem a variável, o teste é pulado). Com OCTOBREW_SCREENSHOT, salva a janela
// num PNG, para conferir o desenho.

#include "i18n.h"
#include "mainwindow.h"
#include "packagemodel.h"

#include <QTemporaryDir>
#include <QTextBrowser>
#include <QtTest>

class TstIntegration : public QObject
{
    Q_OBJECT

private slots:
    void againstRealHomebrew()
    {
        const QString brew = qEnvironmentVariable("OCTOBREW_TEST_BREW");
        if (brew.isEmpty())
            QSKIP("OCTOBREW_TEST_BREW não definido");
        i18n::initFromEnvironment();
        QTemporaryDir dir;
        WindowOptions o;
        o.brewPath = brew;
        o.settingsPath = dir.filePath(QStringLiteral("config"));
        o.synchronous = true;
        MainWindow w(o);
        w.resize(1200, 800);
        w.show();
        w.refresh();
        QVERIFY2(w.packageModel()->rowCount() > 0, qPrintable(w.statusText()));
        QVERIFY(w.selectKey(QStringLiteral("f:ftxui")) || w.selectKey(QStringLiteral("f:qtbase")));
        QVERIFY(w.infoView()->toPlainText().contains(QStringLiteral("homebrew/core")));
        const QString shot = qEnvironmentVariable("OCTOBREW_SCREENSHOT");
        QCoreApplication::processEvents();   // o layout da barra de status, antes da imagem
        if (!shot.isEmpty())
            QVERIFY(w.grab().save(shot));
        w.toggleView();
        QVERIFY(w.packageModel()->rowCount() > 5000);
        QCoreApplication::processEvents();
        if (!shot.isEmpty())
            QVERIFY(w.grab().save(QString(shot).replace(QStringLiteral(".png"), QStringLiteral("-repo.png"))));
    }
};

QTEST_MAIN(TstIntegration)
#include "tst_integration.moc"
