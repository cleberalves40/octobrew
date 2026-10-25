#include "theme.h"

using ftxui::Color;

namespace tui {

namespace {

Color rgb(std::uint32_t v)
{
    return Color::RGB(static_cast<std::uint8_t>(v >> 16), static_cast<std::uint8_t>(v >> 8),
                      static_cast<std::uint8_t>(v));
}

} // namespace

const std::vector<ThemeInfo> &themes()
{
    // Paletas do bectl-gui 1.2.0 (src/gui/themes.cpp): fundo, texto, painel,
    // destaque, texto sobre o destaque, bordas e texto apagado das folhas de
    // estilo, e o verde das linhas; amarelo, vermelho, ciano e magenta das
    // paletas oficiais de cada tema.
    static const std::vector<ThemeInfo> list = {
        {"system", "System", true, {}},
        {"octopkg", "OctoPKG (claro)", false,
         {0xf5f6f8, 0x1a1d23, 0xeef0f3, 0x3d7eac, 0xffffff, 0xd0d4da,
          0x5c6370, 0x2e7d32, 0xe65100, 0xc62828, 0x00838f, 0x8e24aa}},
        {"frappe", "Catppuccin Frappé", false,
         {0x303446, 0xc6d0f5, 0x292c3c, 0x8caaee, 0x303446, 0x51576d,
          0xa5adce, 0xa6d189, 0xe5c890, 0xe78284, 0x81c8be, 0xca9ee6}},
        {"latte", "Catppuccin Latte", false,
         {0xeff1f5, 0x4c4f69, 0xe6e9ef, 0x1e66f5, 0xeff1f5, 0xbcc0cc,
          0x6c6f85, 0x40a02b, 0xdf8e1d, 0xd20f39, 0x179299, 0x8839ef}},
        {"mocha", "Catppuccin Mocha", false,
         {0x1e1e2e, 0xcdd6f4, 0x181825, 0x89b4fa, 0x1e1e2e, 0x45475a,
          0xa6adc8, 0xa6e3a1, 0xf9e2af, 0xf38ba8, 0x94e2d5, 0xcba6f7}},
        {"macchiato", "Catppuccin Macchiato", false,
         {0x24273a, 0xcad3f5, 0x1e2030, 0x8aadf4, 0x24273a, 0x494d64,
          0xa5adcb, 0xa6da95, 0xeed49f, 0xed8796, 0x8bd5ca, 0xc6a0f6}},
        {"solarized_dark", "Solarized Dark", false,
         {0x002b36, 0x839496, 0x073642, 0x268bd2, 0x002b36, 0x586e75,
          0x657b83, 0x859900, 0xb58900, 0xdc322f, 0x2aa198, 0xd33682}},
        {"solarized_light", "Solarized Light", false,
         {0xfdf6e3, 0x657b83, 0xeee8d5, 0x268bd2, 0xfdf6e3, 0x93a1a1,
          0x93a1a1, 0x859900, 0xb58900, 0xdc322f, 0x2aa198, 0xd33682}},
        {"selenized_dark", "Selenized Dark", false,
         {0x103c48, 0xadbcbc, 0x184956, 0x4695f7, 0x103c48, 0x72898f,
          0x72898f, 0x75b938, 0xdbb32d, 0xfa5750, 0x41c7b9, 0xf275be}},
        {"selenized_light", "Selenized Light", false,
         {0xfbf3db, 0x53676d, 0xece3cc, 0x0072d4, 0xfbf3db, 0x909995,
          0x909995, 0x489100, 0xad8900, 0xd2212d, 0x009c8f, 0xca4898}},
        {"dracula", "Dracula", false,
         {0x282a36, 0xf8f8f2, 0x21222c, 0xbd93f9, 0x282a36, 0x6272a4,
          0x6272a4, 0x50fa7b, 0xf1fa8c, 0xff5555, 0x8be9fd, 0xff79c6}},
        {"tokyo_night", "Tokyo Night", false,
         {0x1a1b26, 0xc0caf5, 0x16161e, 0x7aa2f7, 0x1a1b26, 0x414868,
          0x565f89, 0x9ece6a, 0xe0af68, 0xf7768e, 0x7dcfff, 0xbb9af7}},
        {"nord", "Nord", false,
         {0x2e3440, 0xd8dee9, 0x3b4252, 0x81a1c1, 0x2e3440, 0x4c566a,
          0x4c566a, 0xa3be8c, 0xebcb8b, 0xbf616a, 0x88c0d0, 0xb48ead}},
        {"gruvbox", "Gruvbox Dark", false,
         {0x282828, 0xebdbb2, 0x1d2021, 0x83a598, 0x282828, 0x504945,
          0xa89984, 0xb8bb26, 0xfabd2f, 0xfb4934, 0x8ec07c, 0xd3869b}},
        {"one_dark", "One Dark", false,
         {0x282c34, 0xabb2bf, 0x21252b, 0x61afef, 0x282c34, 0x4b5263,
          0x5c6370, 0x98c379, 0xe5c07b, 0xe06c75, 0x56b6c2, 0xc678dd}},
    };
    return list;
}

const ThemeInfo &nextTheme(const std::string &id)
{
    const auto &list = themes();
    for (size_t i = 0; i < list.size(); ++i)
        if (id == list[i].id)
            return list[(i + 1) % list.size()];
    return list.front();
}

Theme theme(const std::string &id)
{
    const ThemeInfo *info = &themes().front();
    for (const auto &t : themes())
        if (id == t.id)
            info = &t;
    if (info->system)
        return {info->id, info->label, true, Color::Default, Color::Default, Color::Default, Color::Default,
                Color::Default, Color::Default, Color::GrayDark, Color::Green, Color::Yellow, Color::Red,
                Color::Cyan, Color::Magenta};
    const std::uint32_t *p = info->palette;
    return {info->id, info->label, false, rgb(p[0]), rgb(p[1]), rgb(p[2]), rgb(p[3]), rgb(p[4]), rgb(p[5]),
            rgb(p[6]), rgb(p[7]), rgb(p[8]), rgb(p[9]), rgb(p[10]), rgb(p[11])};
}

} // namespace tui
