// SPDX-License-Identifier: GPL-2.0-or-later
//
// O que só o AppKit faz, em Objective-C++ (macos.mm); fora do macOS, nada.

#ifndef OCTOBREW_MACOS_H
#define OCTOBREW_MACOS_H

namespace macos {

/** Tira o processo do Dock e do ⌘⇥ (o notificador só vive na barra de menus). */
void hideFromDock();

} // namespace macos

#endif
