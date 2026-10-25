// SPDX-License-Identifier: GPL-2.0-or-later
//
// OctoBrew: interface gráfica (Qt6) para o Homebrew, com o desenho do OctoPkg
// e o núcleo do octobrew-tui.

#include "i18n.h"
#include "mainwindow.h"
#include "packagemodel.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QTimer>
#include <unistd.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("OctoBrew"));
    QApplication::setApplicationVersion(QStringLiteral(OCTOBREW_VERSION));
    QApplication::setOrganizationName(QStringLiteral("octobrew"));
    PackageModel::initResources();
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/images/octopi_green-big.png")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Interface gráfica para o Homebrew, derivada do OctoPkg."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption remote(QStringLiteral("remote"), QStringLiteral("Começa na visão Repositório."));
    const QCommandLineOption lang(QStringLiteral("lang"), QStringLiteral("Idioma: en_US, pt_BR ou es_ES."),
                                  QStringLiteral("idioma"));
    const QCommandLineOption config(QStringLiteral("config"), QStringLiteral("Arquivo de preferências."),
                                    QStringLiteral("arquivo"));
    const QCommandLineOption brew(QStringLiteral("brew"), QStringLiteral("O executável do brew."),
                                  QStringLiteral("executável"));
    parser.addOptions({remote, lang, config, brew});
    parser.process(app);

    if (parser.isSet(lang))
        i18n::setLocale(parser.value(lang).toStdString());
    else
        i18n::initFromEnvironment();

    // O Homebrew não roda como root; melhor dizer logo por quê.
    if (::geteuid() == 0) {
        QMessageBox::critical(nullptr, QStringLiteral("OctoBrew"),
                              QStringLiteral("O Homebrew não roda como root; abra o OctoBrew com o seu usuário."));
        return 1;
    }

    WindowOptions options;
    options.remote = parser.isSet(remote);
    options.settingsPath = parser.value(config);
    options.brewPath = parser.value(brew);
    MainWindow window(options);
    window.show();
    QTimer::singleShot(0, &window, [&window] { window.refresh(); });
    return app.exec();
}
