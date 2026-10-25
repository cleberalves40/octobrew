#ifndef OCTOBREW_TUI_ICONS_H
#define OCTOBREW_TUI_ICONS_H

#include <string>

namespace tui {

/**
 * Ícones da interface. "nerd" usa glifos Nerd Fonts (Font Awesome, U+Fxxx),
 * que exigem uma fonte Nerd no terminal, como a JetBrainsMono Nerd Font Mono
 * (brew install --cask font-jetbrains-mono-nerd-font); "unicode" e "ascii" servem para
 * terminais sem ela. Os estados são os do OctoPkg.
 */
struct IconSet {
    const char *installed;      // instalado explicitamente
    const char *dependency;     // instalado como dependência
    const char *outdated;
    const char *newer;
    const char *pinned;          // fixado (brew pin)
    const char *notInstalled;
    const char *toInstall;      // marcado para instalar (+)
    const char *toRemove;       // marcado para remover (-)
    const char *installedView;
    const char *remoteView;
    const char *info;
    const char *files;
    const char *actions;
    const char *output;
    const char *news;
    const char *help;
    const char *services;
    const char *filter;
    const char *sortAscending;
    const char *sortDescending;
    const char *busy;
    const char *error;
};

enum class IconMode { Nerd, Unicode, Ascii };

const IconSet &icons(IconMode mode);
IconMode iconModeFromString(const std::string &s);   // "nerd" (padrão), "unicode", "ascii"
const char *iconModeName(IconMode mode);

} // namespace tui

#endif
