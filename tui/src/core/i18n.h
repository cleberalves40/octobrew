#ifndef OCTOBREW_TUI_I18N_H
#define OCTOBREW_TUI_I18N_H

#include <initializer_list>
#include <string>
#include <vector>

/**
 * Tradução (en_US, pt_BR, es_ES). As chaves são o texto em inglês, como no
 * OctoPkg (Qt tr()), traduzidos para pt_BR e es_ES.
 */
namespace i18n {

/** Idioma a partir de LC_ALL, LC_MESSAGES ou LANG; en_US se não houver catálogo. */
void initFromEnvironment();
/** "en_US", "pt_BR" ou "es_ES"; outro valor vira en_US. */
void setLocale(const std::string &locale);
std::string locale();
std::vector<std::string> availableLocales();

std::string tr(const char *key);
/** tr() com %1, %2… substituídos pelos argumentos, em uma passada. */
std::string trf(const char *key, std::initializer_list<std::string> args);

} // namespace i18n

#endif
