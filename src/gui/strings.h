// SPDX-License-Identifier: GPL-2.0-or-later
//
// Textos da interface gráfica: o catálogo próprio da GUI (en_US, pt_BR, es_ES)
// e, para o resto, o do núcleo do octobrew-tui (tui/src/core/i18n), que já
// traduz as mensagens do brew, os estados e as colunas. Assim nada é
// traduzido duas vezes, e o subtree do tui não muda.

#ifndef OCTOBREW_STRINGS_H
#define OCTOBREW_STRINGS_H

#include <QString>
#include <QStringList>
#include <initializer_list>

namespace ui {

/** O texto de key (em inglês) no idioma do i18n do núcleo. */
QString tr(const char *key);
/** tr() com %1, %2… substituídos pelos argumentos, numa passada. */
QString trf(const char *key, std::initializer_list<QString> args);
/** As chaves do catálogo da GUI (para o teste que confere as traduções). */
QStringList catalogKeys();

} // namespace ui

#endif
