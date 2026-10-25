#ifndef OCTOBREW_TUI_THEME_H
#define OCTOBREW_TUI_THEME_H

#include <cstdint>
#include <ftxui/screen/color.hpp>
#include <string>
#include <vector>

namespace tui {

/**
 * Temas do bectl-gui 1.2.0 (os mesmos nomes e paletas), para o terminal.
 * "system" usa as cores do próprio terminal, como antes da v0.3.0; os outros
 * pintam fundo, texto, seleção e estados em RGB, que o FTXUI reduz para 256
 * ou 16 cores se o terminal não tiver mais.
 */
struct ThemeInfo {
    const char *id;
    const char *label;
    bool system;               // cores do terminal
    // Paleta em 0xRRGGBB, na ordem dos campos de Theme (de background a magenta).
    std::uint32_t palette[12];
};

/** As cores de um tema, montadas na hora (veja theme()). */
struct Theme {
    const char *id;
    const char *label;
    bool system;
    ftxui::Color background;
    ftxui::Color foreground;
    ftxui::Color panel;        // diálogos e menus
    ftxui::Color accent;       // seleção e destaque
    ftxui::Color accentText;   // texto sobre o destaque
    ftxui::Color border;
    ftxui::Color dim;
    ftxui::Color green;        // instalado, marcado para instalar
    ftxui::Color yellow;       // desatualizado, resultados da busca
    ftxui::Color red;          // erro, marcado para remover
    ftxui::Color cyan;         // mais novo que o repositório, visão
    ftxui::Color magenta;      // bloqueado
};

const std::vector<ThemeInfo> &themes();
/** O tema seguinte ao de id, em ciclo (Alt+T). */
const ThemeInfo &nextTheme(const std::string &id);
/**
 * As cores do tema id (o "system" se não houver). Montadas a cada chamada: o
 * FTXUI reduz uma cor RGB à paleta do terminal quando ela é criada, e o
 * suporte de cor só é conhecido com o terminal em uso.
 */
Theme theme(const std::string &id);

} // namespace tui

#endif
