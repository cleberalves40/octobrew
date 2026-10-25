// SPDX-License-Identifier: GPL-2.0-or-later
//
// octobrew-askpass: o sudo o executa (SUDO_ASKPASS, "sudo -A") com o prompt
// como argumento e lê a senha da saída padrão. Cancelar sai com 1, e o sudo
// desiste da operação.

#include "askpass.h"
#include "i18n.h"
#include "packagemodel.h"

#include <QApplication>
#include <QStringList>
#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("OctoBrew"));
    PackageModel::initResources();
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/images/octopi_green-big.png")));
    i18n::initFromEnvironment();
    const QStringList args = QApplication::arguments();
    const auto password = askpass::askPassword(args.size() > 1 ? args.mid(1).join(' ') : QString());
    if (!password)
        return 1;
    const QByteArray bytes = password->toUtf8() + '\n';
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout);
    std::fflush(stdout);
    return 0;
}
