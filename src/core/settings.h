#ifndef OCTOBREW_TUI_SETTINGS_H
#define OCTOBREW_TUI_SETTINGS_H

#include <map>
#include <string>

namespace octo {

/**
 * Preferências em texto "chave=valor", em $XDG_CONFIG_HOME/octobrew-tui/config
 * (ou ~/.config/octobrew-tui/config). Linhas vazias e iniciadas por "#" são
 * ignoradas.
 */
class Settings
{
public:
    /** path vazio: o caminho padrão acima. */
    explicit Settings(std::string path = std::string());

    std::string value(const std::string &key, const std::string &fallback = std::string()) const;
    int intValue(const std::string &key, int fallback) const;
    void setValue(const std::string &key, const std::string &value);
    void setValue(const std::string &key, int value) { setValue(key, std::to_string(value)); }

    /** Grava o arquivo (cria o diretório se preciso); false em caso de erro. */
    bool save() const;
    const std::string &path() const { return m_path; }

    static std::string defaultPath();

private:
    std::string m_path;
    std::map<std::string, std::string> m_values;
};

} // namespace octo

#endif
