#include "icons.h"

namespace tui {

namespace {

// Nerd Fonts 3.x, faixa Font Awesome (nomes nf-fa-*).
const IconSet kNerd = {
    "",   // check_circle
    "",   // check_circle_o
    "",   // arrow_circle_up
    "",   // arrow_circle_down
    "",   // thumb_tack (pin)
    "",   // circle_o
    "",   // plus_circle
    "",   // minus_circle
    "",   // archive
    "",   // cloud
    "",   // info_circle
    "",   // folder_open
    "",   // tasks
    "",   // terminal
    "",   // newspaper_o
    "",   // question_circle
    "",   // cogs (serviços)
    "",   // filter
    "",   // sort_asc
    "",   // sort_desc
    "",   // spinner
    "",   // exclamation_triangle
};

const IconSet kUnicode = {"●", "◉", "↑", "↓", "▪", "○", "⊕", "⊖", "▣", "☁", "ℹ", "▤", "☰", "»", "✉", "?", "⚙", "⌕", "▲", "▼", "…", "!"};

const IconSet kAscii = {"I", "d", "U", "N", "P", "-", "+", "x", "#", "@", "i", "f", "a", ">", "n", "?", "s", "/", "^", "v", "*", "!"};

} // namespace

const IconSet &icons(IconMode mode)
{
    switch (mode) {
    case IconMode::Unicode: return kUnicode;
    case IconMode::Ascii:   return kAscii;
    default:                return kNerd;
    }
}

IconMode iconModeFromString(const std::string &s)
{
    if (s == "unicode")
        return IconMode::Unicode;
    if (s == "ascii")
        return IconMode::Ascii;
    return IconMode::Nerd;
}

const char *iconModeName(IconMode mode)
{
    switch (mode) {
    case IconMode::Unicode: return "unicode";
    case IconMode::Ascii:   return "ascii";
    default:                return "nerd";
    }
}

} // namespace tui
