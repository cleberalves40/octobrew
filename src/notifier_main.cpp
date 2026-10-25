// SPDX-License-Identifier: GPL-2.0-or-later
//
// octobrew-notifier: o polvo na barra de menus, como o octopi-notifier. Fica
// fora do Dock, verifica ao abrir e de tempos em tempos, e abre o OctoBrew.

#include "i18n.h"
#include "macos.h"
#include "notifier.h"
#include "packagemodel.h"
#include "preferences.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <unistd.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("OctoBrew"));
    QApplication::setApplicationVersion(QStringLiteral(OCTOBREW_VERSION));
    QApplication::setQuitOnLastWindowClosed(false);
    PackageModel::initResources();

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption brew(QStringLiteral("brew"), QStringLiteral("O executável do brew."), QStringLiteral("executável"));
    const QCommandLineOption config(QStringLiteral("config"), QStringLiteral("Arquivo de preferências."), QStringLiteral("arquivo"));
    parser.addOptions({brew, config});
    parser.process(app);   // o --version e o --help saem aqui, sem tocar no AppKit
    if (::geteuid() == 0)
        return 1;   // o Homebrew não roda como root
    // Fora do Dock só com a tela de verdade: sem o servidor de janelas (o sandbox
    // do brew test, o plugin offscreen), o AppKit derrubaria o processo.
    if (QGuiApplication::platformName() == QStringLiteral("cocoa"))
        macos::hideFromDock();

    const octo::Settings settings((parser.isSet(config) ? parser.value(config) : prefs::defaultSettingsPath()).toStdString());
    prefs::applyLanguage(settings);
    Notifier::Options o;
    o.brewPath = parser.value(brew);
    o.settingsPath = parser.value(config);
    Notifier notifier(o);
    QTimer::singleShot(0, &notifier, [&notifier] { notifier.check(); });
    return app.exec();
}
