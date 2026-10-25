// SPDX-License-Identifier: GPL-2.0-or-later

#include "texts.h"
#include "i18n.h"

#include <cstring>

namespace ui {

namespace {

// { inglês (a chave), pt_BR, es_ES }: só o que o catálogo do núcleo não tem.
struct Entry {
    const char *en;
    const char *pt;
    const char *es;
};

const Entry kEntries[] = {
    {"Search by description", "Buscar por descrição", "Buscar por descripción"},
    {"About Qt", "Sobre o Qt", "Acerca de Qt"},
    {"View", "Exibir", "Ver"},
    {"Main toolbar", "Barra de ferramentas", "Barra de herramientas"},
    {"A graphical front end for Homebrew.", "Uma interface gráfica para o Homebrew.", "Una interfaz gráfica para Homebrew."},
    {"and on octobrew-tui", "e no octobrew-tui", "y en octobrew-tui"},
    {"Leaves", "Folhas", "Hojas"},
};

} // namespace

QString tr(const char *key)
{
    const std::string locale = i18n::locale();
    for (const auto &e : kEntries) {
        if (std::strcmp(e.en, key) != 0)
            continue;
        if (locale == "pt_BR")
            return QString::fromUtf8(e.pt);
        if (locale == "es_ES")
            return QString::fromUtf8(e.es);
        return QString::fromUtf8(e.en);
    }
    return QString::fromStdString(i18n::tr(key));
}

QString trf(const char *key, std::initializer_list<QString> args)
{
    // A substituição numa passada, como o i18n::trf: um argumento com "%2" fica como está.
    const QString pattern = tr(key);
    const QList<QString> values(args);
    QString out;
    out.reserve(pattern.size());
    for (int i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '%' && i + 1 < pattern.size() && pattern[i + 1] >= '1' && pattern[i + 1] <= '9') {
            const int n = pattern[i + 1].unicode() - '1';
            if (n < values.size()) {
                out += values[n];
                ++i;
                continue;
            }
        }
        out += pattern[i];
    }
    return out;
}

QStringList catalogKeys()
{
    QStringList keys;
    for (const auto &e : kEntries)
        keys << QString::fromUtf8(e.en);
    return keys;
}

} // namespace ui
