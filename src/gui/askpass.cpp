// SPDX-License-Identifier: GPL-2.0-or-later

#include "askpass.h"
#include "texts.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>

namespace askpass {

std::optional<QString> askPassword(const QString &prompt, QWidget *parent)
{
    QInputDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("OctoBrew"));
    dialog.setLabelText(ui::tr("A cask needs administrator rights (sudo).") + "\n\n"
                        + (prompt.trimmed().isEmpty() ? ui::tr("Password:") : prompt.trimmed()));
    dialog.setTextEchoMode(QLineEdit::Password);
    dialog.setInputMode(QInputDialog::TextInput);
    dialog.setOkButtonText(ui::tr("OK"));
    dialog.setCancelButtonText(ui::tr("Cancel"));
    if (dialog.exec() != QDialog::Accepted)
        return std::nullopt;
    return dialog.textValue();
}

QString helperPath()
{
    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("octobrew-askpass"));
    const QFileInfo info(path);
    return info.isExecutable() ? info.absoluteFilePath() : QString();
}

} // namespace askpass
