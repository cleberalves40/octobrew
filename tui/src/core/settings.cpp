#include "settings.h"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>

namespace octo {

namespace {

// mkdir -p
bool makeDirs(const std::string &dir)
{
    if (dir.empty())
        return true;
    size_t pos = 0;
    while ((pos = dir.find('/', pos + 1)) != std::string::npos) {
        const std::string part = dir.substr(0, pos);
        if (::mkdir(part.c_str(), 0755) != 0 && errno != EEXIST)
            return false;
    }
    return ::mkdir(dir.c_str(), 0755) == 0 || errno == EEXIST;
}

} // namespace

std::string Settings::defaultPath()
{
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        return std::string(xdg) + "/octobrew-tui/config";
    const char *home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/.config/octobrew-tui/config";
}

Settings::Settings(std::string path)
    : m_path(path.empty() ? defaultPath() : std::move(path))
{
    std::ifstream in(m_path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        m_values[line.substr(0, eq)] = line.substr(eq + 1);
    }
}

std::string Settings::value(const std::string &key, const std::string &fallback) const
{
    const auto it = m_values.find(key);
    return it == m_values.end() ? fallback : it->second;
}

int Settings::intValue(const std::string &key, int fallback) const
{
    const auto it = m_values.find(key);
    if (it == m_values.end())
        return fallback;
    char *end = nullptr;
    const long v = std::strtol(it->second.c_str(), &end, 10);
    return (end && *end == '\0' && !it->second.empty()) ? static_cast<int>(v) : fallback;
}

void Settings::setValue(const std::string &key, const std::string &value)
{
    m_values[key] = value;
}

bool Settings::save() const
{
    const size_t slash = m_path.rfind('/');
    if (slash != std::string::npos && !makeDirs(m_path.substr(0, slash)))
        return false;
    std::ofstream out(m_path, std::ios::trunc);
    if (!out)
        return false;
    out << "# octobrew-tui\n";
    for (const auto &kv : m_values)
        out << kv.first << '=' << kv.second << '\n';
    return static_cast<bool>(out);
}

} // namespace octo
