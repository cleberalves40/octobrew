// SPDX-License-Identifier: GPL-2.0-or-later
//
// O octobrew-askpass: o diálogo de senha que o sudo chama pelo SUDO_ASKPASS
// (o "sudo -A" do brew), no lugar do qt-sudo do OctoPkg. O brew roda como o
// usuário; só os casks com instaladores .pkg e passos de sistema pedem o sudo.
// A senha vai para a saída padrão, que o sudo lê, e não é guardada.

#ifndef OCTOBREW_ASKPASS_H
#define OCTOBREW_ASKPASS_H

#include <QString>
#include <optional>

class QWidget;

namespace askpass {

/** O diálogo; nullopt se o usuário cancelou. prompt: o texto do sudo ("Password:"). */
std::optional<QString> askPassword(const QString &prompt, QWidget *parent = nullptr);

/**
 * Onde está o octobrew-askpass: ao lado do executável (no bundle,
 * Contents/MacOS); vazio se não estiver lá.
 */
QString helperPath();

} // namespace askpass

#endif
