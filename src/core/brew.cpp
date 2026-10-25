// SPDX-License-Identifier: GPL-2.0-or-later
//
// Leitura do Homebrew: o JSON do brew e o cache da API, no lugar das
// consultas ao pkg(8) do octopkg-tui.
// OctoPkg: Copyright (C) 2015 Alexandre Albuquerque Arnt.

#include "brew.h"
#include "i18n.h"
#include "json.h"
#include "process.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace octo {

namespace {

// O brew leva de meio segundo a alguns segundos para começar (é Ruby).
constexpr int kTimeoutMs = 120000;

const char kCoreTap[] = "homebrew/core";
const char kCaskTap[] = "homebrew/cask";

[[noreturn]] void fail(const std::string &msg)
{
    throw std::runtime_error(msg);
}

std::string trim(const std::string &s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return std::string();
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<std::string> lines(const std::string &s)
{
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string l;
    while (std::getline(in, l)) {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (!l.empty())
            out.push_back(l);
    }
    return out;
}

std::string join(const std::vector<std::string> &v, const std::string &sep)
{
    std::string out;
    for (size_t i = 0; i < v.size(); ++i)
        out += (i ? sep : std::string()) + v[i];
    return out;
}

std::string readFile(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::string();
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// A saída do brew em inglês, sem cores nem emojis, e sem atualizar nada por
// conta própria: uma consulta não pode virar um "brew update" nem baixar a
// API (o brew faz isso quando o cache tem mais de alguns minutos).
Environment queryEnv()
{
#ifdef __APPLE__
    const std::string utf8 = "en_US.UTF-8";
#else
    const std::string utf8 = "C.UTF-8";
#endif
    return {{"LANG", utf8},
            {"LC_ALL", utf8},
            {"HOMEBREW_NO_AUTO_UPDATE", "1"},
            {"HOMEBREW_NO_ENV_HINTS", "1"},
            {"HOMEBREW_NO_COLOR", "1"},
            {"HOMEBREW_NO_EMOJI", "1"}};
}

// As operações: o mesmo ambiente das consultas (sem auto-update, para fazer o
// que a prévia mostrou), sem a pergunta do brew 7 (a prévia é a da
// interface) e sem o autoremove automático do uninstall e do cleanup, que
// levaria pacotes que não estavam na prévia. Sem o terminal, um sudo que
// precise de senha falha na hora, em vez de disputar a tela.
Environment operationEnv(bool interactive)
{
    Environment env = queryEnv();
    env.push_back({"HOMEBREW_NO_ASK", "1"});
    env.push_back({"HOMEBREW_NO_AUTOREMOVE", "1"});
    if (!interactive)
        env.push_back({"SUDO_ASKPASS", "/usr/bin/false"});
    return env;
}

void requirePackages(const std::vector<Package> &packages)
{
    if (packages.empty())
        fail(i18n::tr("No packages selected."));
    for (const auto &p : packages)
        if (!validPackageName(p.fullName))
            fail(i18n::trf("Invalid package name: %1", {p.fullName}));
}

// O comando com --formula para as fórmulas e --cask para os casks: o brew não
// aceita os dois numa chamada.
std::vector<std::vector<std::string>> byKind(const std::string &verb, const std::vector<std::string> &extra,
                                             const std::vector<Package> &packages)
{
    std::vector<std::vector<std::string>> out;
    for (const Kind kind : {Kind::Formula, Kind::Cask}) {
        std::vector<std::string> args{verb};
        args.insert(args.end(), extra.begin(), extra.end());
        args.push_back(kind == Kind::Cask ? "--cask" : "--formula");
        const size_t fixed = args.size();
        for (const auto &p : packages)
            if (p.kind == kind)
                args.push_back(p.fullName);
        if (args.size() > fixed)
            out.push_back(args);
    }
    return out;
}

std::string defaultBrew()
{
    // Os prefixos do Homebrew: Apple Silicon, Intel e Linux.
    for (const char *p : {"/opt/homebrew/bin/brew", "/usr/local/bin/brew", "/home/linuxbrew/.linuxbrew/bin/brew"})
        if (::access(p, X_OK) == 0)
            return p;
    const std::string found = findExecutable("brew");
    return found.empty() ? std::string("brew") : found;
}

// Uma fórmula de um tap oficial é chamada pelo nome; as outras, com o tap.
std::string fullNameOf(const json::Value &v, const std::string &name, const std::string &tap, Kind kind)
{
    const std::string given = v[kind == Kind::Cask ? "full_token" : "full_name"].toString();
    if (!given.empty())
        return given;
    if (tap.empty() || tap == kCoreTap || tap == kCaskTap)
        return name;
    return tap + "/" + name;
}

// O registro instalado de uma fórmula: o da versão ligada (linked_keg), ou o
// último, que é o mais novo.
const json::Value &currentInstall(const json::Value &f)
{
    const auto &installed = f["installed"];
    if (installed.items().empty())
        return installed[0];
    const std::string linked = f["linked_keg"].toString();
    for (const auto &i : installed.items())
        if (!linked.empty() && i["version"].toString() == linked)
            return i;
    return installed.items().back();
}

Package formulaFromInfo(const json::Value &f)
{
    Package p;
    p.kind = Kind::Formula;
    p.name = f["name"].toString();
    p.tap = f["tap"].toString(kCoreTap);
    p.fullName = fullNameOf(f, p.name, p.tap, Kind::Formula);
    p.comment = f["desc"].toString();
    const std::string available = formulaVersion(f["versions"]["stable"].toString(), static_cast<int>(f["revision"].toInt()));
    const auto &inst = currentInstall(f);
    p.installed = !f["installed"].items().empty();
    if (p.installed) {
        p.installedVersion = inst["version"].toString();
        p.version = p.installedVersion;
        p.installedOn = inst["time"].toInt();
        // brew 7 só tem installed_on_request; o 4 também tinha installed_as_dependency.
        if (inst.contains("installed_on_request"))
            p.automatic = !inst["installed_on_request"].toBool(true);
        else
            p.automatic = inst["installed_as_dependency"].toBool(false);
        for (const auto &d : inst["runtime_dependencies"].items())
            if (d["declared_directly"].toBool(true))
                p.dependencies.push_back(d["full_name"].toString());
    } else {
        p.version = available;
    }
    p.pinned = f["pinned"].toBool();
    p.hasService = f["service"].isObject();
    if (f["outdated"].toBool())
        p.newVersion = available;
    return p;
}

Package caskFromInfo(const json::Value &c)
{
    Package p;
    p.kind = Kind::Cask;
    p.name = c["token"].toString();
    p.tap = c["tap"].toString(kCaskTap);
    p.fullName = fullNameOf(c, p.name, p.tap, Kind::Cask);
    p.comment = c["desc"].toString();
    if (p.comment.empty())   // muitos casks não têm desc: o nome do aplicativo serve
        p.comment = join(c["name"].strings(), ", ");
    const std::string available = c["version"].toString();
    p.installedVersion = c["installed"].toString();
    p.installed = !p.installedVersion.empty();
    p.version = p.installed ? p.installedVersion : available;
    p.installedOn = c["installed_time"].toInt();
    p.pinned = c["pinned"].toBool();
    if (c["outdated"].toBool())
        p.newVersion = available;
    for (const auto &f : c["depends_on"]["formula"].strings())
        p.dependencies.push_back(f);
    for (const auto &a : c["artifacts"].items()) {
        const std::string target = a["target"].toString();
        if (!target.empty())
            p.paths.push_back(target);
    }
    return p;
}

void setInstalledStatus(Package &p)
{
    // Como no OctoPkg, o bloqueio (o pin) prevalece sobre a atualização.
    if (p.pinned)
        p.status = Status::Pinned;
    else
        p.status = p.newVersion.empty() ? Status::Installed : Status::Outdated;
}

// Ignora o envelope JWS ({"payload": "<json>", "signatures": [...]}).
json::Value unwrap(const std::string &text)
{
    json::Value v = json::parse(text);
    if (v.isObject() && v["payload"].isString())
        return json::parse(v["payload"].str());
    return v;
}

Package formulaFromApi(const std::string &name, const json::Value &f)
{
    Package p;
    p.kind = Kind::Formula;
    p.name = name;
    p.tap = kCoreTap;
    p.fullName = name;
    p.comment = f["desc"].toString();
    p.version = formulaVersion(f["stable_version"].toString(), static_cast<int>(f["revision"].toInt()));
    return p;
}

Package caskFromApi(const std::string &token, const json::Value &c)
{
    Package p;
    p.kind = Kind::Cask;
    p.name = token;
    p.tap = c["tap_string"].toString(kCaskTap);
    p.fullName = token;
    p.comment = c["desc"].toString();
    if (p.comment.empty())
        p.comment = join(c["names"].strings(), ", ");
    p.version = c["version"].toString();
    return p;
}

// ":provided_by_macos" → "provided by macos"
std::string symbolText(const std::string &s)
{
    std::string out = s;
    if (!out.empty() && out[0] == ':')
        out.erase(0, 1);
    for (auto &c : out)
        if (c == '_')
            c = ' ';
    return out;
}

// "command_wrapper" → "Command wrapper"
std::string artifactLabel(const std::string &key)
{
    std::string out = symbolText(key);
    if (!out.empty())
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

// O que um cask instala; os passos de desinstalação ficam de fora.
std::vector<std::string> artifactLines(const json::Value &artifacts)
{
    static const std::set<std::string> skip{"uninstall", "zap", "preflight", "postflight",
                                            "uninstall_preflight", "uninstall_postflight"};
    std::vector<std::string> out;
    for (const auto &a : artifacts.items()) {
        for (const auto &m : a.members()) {
            if (m.first == "target" || skip.count(m.first))
                continue;
            std::vector<std::string> what = m.second.strings();
            if (what.empty() && m.second.isString())
                what.push_back(m.second.str());
            if (!what.empty())
                out.push_back(artifactLabel(m.first) + ": " + what.front());
            break;
        }
    }
    return out;
}

} // namespace

std::string Package::key() const
{
    return (kind == Kind::Cask ? "c:" : "f:") + (fullName.empty() ? name : fullName);
}

std::string formulaVersion(const std::string &stable, int revision)
{
    if (stable.empty() || revision <= 0)
        return stable;
    return stable + "_" + std::to_string(revision);
}

int compareVersions(const std::string &a, const std::string &b)
{
    size_t i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        while (i < a.size() && !std::isalnum(static_cast<unsigned char>(a[i])))
            ++i;
        while (j < b.size() && !std::isalnum(static_cast<unsigned char>(b[j])))
            ++j;
        if (i >= a.size() || j >= b.size())
            break;
        const bool numeric = std::isdigit(static_cast<unsigned char>(a[i]));
        const auto isPart = [numeric](char c) {
            const auto u = static_cast<unsigned char>(c);
            return numeric ? std::isdigit(u) != 0 : std::isalpha(u) != 0;
        };
        size_t ie = i, je = j;
        while (ie < a.size() && isPart(a[ie]))
            ++ie;
        while (je < b.size() && isPart(b[je]))
            ++je;
        if (je == j)   // tipos diferentes: o número vence
            return numeric ? 1 : -1;
        std::string pa = a.substr(i, ie - i), pb = b.substr(j, je - j);
        if (numeric) {
            pa.erase(0, std::min(pa.find_first_not_of('0'), pa.size()));
            pb.erase(0, std::min(pb.find_first_not_of('0'), pb.size()));
            if (pa.size() != pb.size())
                return pa.size() < pb.size() ? -1 : 1;
        }
        if (const int c = pa.compare(pb))
            return c < 0 ? -1 : 1;
        i = ie;
        j = je;
    }
    const bool restA = i < a.size(), restB = j < b.size();
    return restA == restB ? 0 : (restA ? 1 : -1);
}

std::vector<Package> parseInstalled(const std::string &text)
{
    const json::Value doc = json::parse(text);
    std::vector<Package> result;
    for (const auto &f : doc["formulae"].items()) {
        Package p = formulaFromInfo(f);
        if (p.name.empty() || !p.installed)
            continue;
        setInstalledStatus(p);
        result.push_back(std::move(p));
    }
    for (const auto &c : doc["casks"].items()) {
        Package p = caskFromInfo(c);
        if (p.name.empty() || !p.installed)
            continue;
        setInstalledStatus(p);
        result.push_back(std::move(p));
    }
    // Quantos instalados dependem de cada um, como o %#r do pkg.
    std::map<std::string, int> count;
    for (const auto &p : result)
        for (const auto &d : p.dependencies)
            ++count[d];
    for (auto &p : result) {
        const auto it = count.find(p.fullName);
        p.requiredBy = it == count.end() ? 0 : it->second;
    }
    return result;
}

std::vector<Package> parseApi(const std::string &text)
{
    const json::Value doc = unwrap(text);
    std::vector<Package> result;
    if (doc.isObject()) {
        const auto &formulae = doc["formulae"];
        const auto &casks = doc["casks"];
        // brew 5+: {"formulae": {"nome": {...}}, "casks": {"token": {...}}}
        for (const auto &m : formulae.members())
            result.push_back(formulaFromApi(m.first, m.second));
        for (const auto &m : casks.members())
            result.push_back(caskFromApi(m.first, m.second));
        // O formato do "brew info --json=v2": arrays de registros completos.
        for (const auto &f : formulae.items()) {
            Package p = formulaFromInfo(f);
            p.installed = false;
            p.version = formulaVersion(f["versions"]["stable"].toString(), static_cast<int>(f["revision"].toInt()));
            p.installedVersion.clear();
            p.newVersion.clear();
            p.dependencies.clear();
            p.pinned = false;
            p.automatic = false;
            p.installedOn = 0;
            result.push_back(std::move(p));
        }
        for (const auto &c : casks.items()) {
            Package p = caskFromInfo(c);
            p.installed = false;
            p.version = c["version"].toString();
            p.installedVersion.clear();
            p.newVersion.clear();
            p.dependencies.clear();
            p.paths.clear();
            p.pinned = false;
            p.installedOn = 0;
            result.push_back(std::move(p));
        }
    } else if (doc.isArray()) {
        // brew 4: formula.jws.json e cask.jws.json, um array cada.
        for (const auto &item : doc.items()) {
            Package p = item.contains("token") ? caskFromInfo(item) : formulaFromInfo(item);
            if (p.kind == Kind::Formula)
                p.version = formulaVersion(item["versions"]["stable"].toString(),
                                           static_cast<int>(item["revision"].toInt()));
            else
                p.version = item["version"].toString();
            p.installed = false;
            p.installedVersion.clear();
            p.newVersion.clear();
            p.dependencies.clear();
            p.paths.clear();
            p.pinned = false;
            p.automatic = false;
            p.installedOn = 0;
            result.push_back(std::move(p));
        }
    }
    std::vector<Package> kept;
    kept.reserve(result.size());
    for (auto &p : result) {
        if (p.name.empty())
            continue;
        p.status = Status::NotInstalled;
        kept.push_back(std::move(p));
    }
    return kept;
}

Details parseDetails(const std::string &text)
{
    const json::Value doc = json::parse(text);
    Details d;
    if (!doc["formulae"].items().empty()) {
        const auto &f = doc["formulae"][0];
        d.kind = Kind::Formula;
        d.name = f["name"].toString();
        d.tap = f["tap"].toString(kCoreTap);
        d.fullName = fullNameOf(f, d.name, d.tap, Kind::Formula);
        d.version = formulaVersion(f["versions"]["stable"].toString(), static_cast<int>(f["revision"].toInt()));
        d.homepage = f["homepage"].toString();
        d.license = f["license"].toString();
        d.description = f["desc"].toString();
        d.caveats = trim(f["caveats"].toString());
        if (f["keg_only"].toBool()) {
            const auto &why = f["keg_only_reason"];
            const std::string explanation = trim(why["explanation"].toString());
            d.kegOnly = explanation.empty() ? symbolText(why["reason"].toString()) : explanation;
            if (d.kegOnly.empty())
                d.kegOnly = "yes";
        }
        if (f["deprecated"].toBool())
            d.deprecated = symbolText(f["deprecation_reason"].toString("deprecated"));
        if (f["disabled"].toBool())
            d.disabled = symbolText(f["disable_reason"].toString("disabled"));
        d.dependencies = f["dependencies"].strings();
        d.buildDependencies = f["build_dependencies"].strings();
        d.conflicts = f["conflicts_with"].strings();
        d.installed = !f["installed"].items().empty();
        if (d.installed)
            d.installedOn = currentInstall(f)["time"].toInt();
        return d;
    }
    if (!doc["casks"].items().empty()) {
        const auto &c = doc["casks"][0];
        d.kind = Kind::Cask;
        d.name = c["token"].toString();
        d.tap = c["tap"].toString(kCaskTap);
        d.fullName = fullNameOf(c, d.name, d.tap, Kind::Cask);
        d.version = c["version"].toString();
        d.homepage = c["homepage"].toString();
        d.description = c["desc"].toString();
        d.names = c["name"].strings();
        d.caveats = trim(c["caveats"].toString());
        d.autoUpdates = c["auto_updates"].toBool();
        if (c["deprecated"].toBool())
            d.deprecated = symbolText(c["deprecation_reason"].toString("deprecated"));
        if (c["disabled"].toBool())
            d.disabled = symbolText(c["disable_reason"].toString("disabled"));
        for (const auto &f : c["depends_on"]["formula"].strings())
            d.dependencies.push_back(f);
        for (const auto &k : c["depends_on"]["cask"].strings())
            d.dependencies.push_back(k + " (cask)");
        d.conflicts = c["conflicts_with"]["cask"].strings();
        d.artifacts = artifactLines(c["artifacts"]);
        d.installed = !c["installed"].toString().empty();
        d.installedOn = c["installed_time"].toInt();
    }
    return d;
}

std::vector<Package> mergeRemote(const std::vector<Package> &remote, const std::vector<Package> &installed)
{
    std::map<std::string, const Package *> byKey;
    for (const auto &p : installed)
        byKey[p.key()] = &p;

    std::vector<Package> result;
    result.reserve(remote.size() + installed.size());
    std::set<std::string> seen;
    for (const auto &r : remote) {
        const std::string key = r.key();
        if (!seen.insert(key).second)
            continue;
        Package p = r;
        const auto it = byKey.find(key);
        if (it != byKey.end()) {
            const Package &i = *it->second;
            p.installed = true;
            p.installedVersion = i.installedVersion;
            p.installedOn = i.installedOn;
            p.pinned = i.pinned;
            p.automatic = i.automatic;
            p.requiredBy = i.requiredBy;
            p.newVersion = i.newVersion;
            p.size = i.size;
            p.dependencies = i.dependencies;
            if (i.pinned)
                p.status = Status::Pinned;
            else if (!i.newVersion.empty())
                p.status = Status::Outdated;
            else if (compareVersions(i.installedVersion, r.version) > 0)
                p.status = Status::Newer;   // o brew não o lista como desatualizado: a API é mais antiga
            else
                p.status = Status::Installed;
        }
        result.push_back(std::move(p));
    }
    for (const auto &i : installed)
        if (!seen.count(i.key()))
            result.push_back(i);
    return result;
}

std::vector<std::string> dependents(const std::vector<Package> &installed, const std::string &fullName)
{
    std::vector<std::string> out;
    for (const auto &p : installed)
        for (const auto &d : p.dependencies)
            if (d == fullName) {
                out.push_back(p.kind == Kind::Cask ? p.fullName + " (cask)" : p.fullName);
                break;
            }
    return out;
}

std::string formatSize(std::int64_t bytes)
{
    const double kb = static_cast<double>(bytes) / 1024.0;
    char buf[32];
    if (kb >= 1073741824.0)
        std::snprintf(buf, sizeof buf, "%.2f TiB", kb / 1073741824.0);
    else if (kb >= 1048576.0)
        std::snprintf(buf, sizeof buf, "%.2f GiB", kb / 1048576.0);
    else if (kb >= 1024.0)
        std::snprintf(buf, sizeof buf, "%.2f MiB", kb / 1024.0);
    else if (kb >= 1.0)
        std::snprintf(buf, sizeof buf, "%.2f KiB", kb);
    else
        std::snprintf(buf, sizeof buf, "%.2f Bytes", static_cast<double>(bytes));
    return buf;
}

std::string formatTime(std::int64_t unixTime)
{
    if (unixTime <= 0)
        return std::string();
    const std::time_t t = static_cast<std::time_t>(unixTime);
    std::tm tm {};
    ::localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    return buf;
}

bool validPackageName(const std::string &name)
{
    if (name.empty() || name[0] == '-' || name.size() > 255)
        return false;
    for (unsigned char c : name)
        if (!std::isalnum(c) && c != '@' && c != '+' && c != '-' && c != '.' && c != '_' && c != '/')
            return false;
    return true;
}

std::int64_t treeSize(const std::string &path, const std::atomic<bool> *cancel)
{
    struct stat st {};
    if (::lstat(path.c_str(), &st) != 0)
        return 0;
    if (!S_ISDIR(st.st_mode))
        return S_ISREG(st.st_mode) ? static_cast<std::int64_t>(st.st_size) : 0;
    std::int64_t total = 0;
    std::vector<std::string> pending{path};
    while (!pending.empty()) {
        if (cancel && *cancel)
            return total;
        const std::string dir = pending.back();
        pending.pop_back();
        DIR *d = ::opendir(dir.c_str());
        if (!d)
            continue;
        while (const dirent *e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n == "." || n == "..")
                continue;
            const std::string child = dir + "/" + n;
            if (::lstat(child.c_str(), &st) != 0)
                continue;
            if (S_ISDIR(st.st_mode))
                pending.push_back(child);
            else if (S_ISREG(st.st_mode))
                total += static_cast<std::int64_t>(st.st_size);
        }
        ::closedir(d);
    }
    return total;
}

Manager::Manager(std::string brewPath)
    : m_brewPath(brewPath.empty() ? defaultBrew() : std::move(brewPath))
{
}

std::string Manager::query(const std::vector<std::string> &args) const
{
    std::vector<std::string> cmd{m_brewPath};
    cmd.insert(cmd.end(), args.begin(), args.end());
    const std::string shown = join(cmd, " ");
    const ProcessResult r = runProcess(cmd, kTimeoutMs, queryEnv());
    if (!r.started)
        fail(i18n::trf("Failed to start '%1': %2", {shown, r.startError}));
    if (r.timedOut)
        fail(i18n::trf("Command timed out (%1 ms): %2", {std::to_string(kTimeoutMs), shown}));
    if (r.crashed)
        fail(i18n::trf("Process crashed: %1", {shown}));
    if (r.status != 0) {
        // O brew escreve "Error: ..." no stderr; avisos ("Warning: ...") também
        // vão para lá, então só contam quando o comando falha.
        std::string msg = trim(r.err).empty() ? trim(r.out) : trim(r.err);
        if (msg.rfind("Error: ", 0) == 0)
            msg.erase(0, 7);
        if (msg.empty())
            msg = i18n::trf("Command failed with exit code %1", {std::to_string(r.status)});
        fail(msg);
    }
    return r.out;
}

Manager::Paths Manager::paths() const
{
    std::lock_guard<std::mutex> lock(m_pathsMutex);
    if (!m_hasPaths) {
        // Os três são respondidos pelo script em shell do brew, sem o Ruby.
        Paths p;
        p.cellar = trim(query({"--cellar"}));
        p.caskroom = trim(query({"--caskroom"}));
        p.cache = trim(query({"--cache"}));
        m_paths = p;
        m_hasPaths = true;
    }
    return m_paths;
}

std::vector<Package> Manager::listInstalled() const
{
    try {
        return parseInstalled(query({"info", "--json=v2", "--installed"}));
    } catch (const std::runtime_error &e) {
        const std::string what = e.what();
        if (what.rfind("JSON: ", 0) == 0)
            fail(i18n::trf("Could not read the output of brew info: %1", {what}));
        throw;
    }
}

std::vector<Package> Manager::listRemote(std::string *warning) const
{
    const std::string api = paths().cache + "/api";
    // brew 5+: api/internal/packages.<tag>.jws.json, um por plataforma; o mais recente vale.
    std::string newest;
    long long newestTime = -1;
    const std::string internal = api + "/internal";
    if (DIR *d = ::opendir(internal.c_str())) {
        while (const dirent *e = ::readdir(d)) {
            const std::string n = e->d_name;
            const std::string suffix = ".jws.json";
            if (n.rfind("packages.", 0) != 0 || n.size() <= suffix.size()
                || n.compare(n.size() - suffix.size(), suffix.size(), suffix) != 0)
                continue;
            struct stat st {};
            const std::string full = internal + "/" + n;
            if (::stat(full.c_str(), &st) == 0 && static_cast<long long>(st.st_mtime) > newestTime) {
                newestTime = static_cast<long long>(st.st_mtime);
                newest = full;
            }
        }
        ::closedir(d);
    }
    std::vector<std::string> files;
    if (!newest.empty())
        files.push_back(newest);
    else
        for (const char *legacy : {"/formula.jws.json", "/cask.jws.json"})   // brew 4
            if (::access((api + legacy).c_str(), R_OK) == 0)
                files.push_back(api + legacy);
    if (files.empty())
        fail(i18n::trf("The Homebrew API cache was not found in %1. Run 'brew update' to download it.", {api}));

    std::vector<Package> all;
    for (const auto &f : files) {
        const std::string text = readFile(f);
        if (text.empty())
            fail(i18n::trf("Could not read %1", {f}));
        try {
            std::vector<Package> part = parseApi(text);
            all.insert(all.end(), std::make_move_iterator(part.begin()), std::make_move_iterator(part.end()));
        } catch (const std::exception &e) {
            fail(i18n::trf("Could not read %1: %2", {f, e.what()}));
        }
    }
    // Os taps de terceiros não estão na API: os nomes vêm do brew tap-info.
    try {
        const std::vector<Package> extra = tapPackages(listTaps());
        all.insert(all.end(), extra.begin(), extra.end());
    } catch (const std::exception &e) {
        if (warning)
            *warning = e.what();
    }
    return all;
}

Details Manager::details(const Package &p) const
{
    if (!validPackageName(p.fullName))
        fail(i18n::trf("Invalid package name: %1", {p.fullName}));
    const bool cask = p.kind == Kind::Cask;
    Details d = parseDetails(query({"info", "--json=v2", cask ? "--cask" : "--formula", p.fullName}));
    if (d.name.empty())
        fail(i18n::trf("Package not found: %1", {p.fullName}));
    if (d.installed) {
        // brew list: os arquivos do keg, ou o que o cask deixou no Caskroom.
        const std::string files = cask ? query({"list", "--cask", p.fullName})
                                       : query({"list", "--formula", "--verbose", p.fullName});
        if (files.size() > kMaxFilesOutput)
            d.filesTruncated = true;   // como no OctoPkg: pacote grande demais para a aba
        else
            d.files = lines(files);
    }
    return d;
}

std::map<std::string, std::int64_t> Manager::sizes(const std::vector<Package> &installed,
                                                   const std::atomic<bool> *cancel) const
{
    const Paths p = paths();
    std::map<std::string, std::int64_t> out;
    for (const auto &pkg : installed) {
        if (cancel && *cancel)
            break;
        if (!pkg.installed || !validPackageName(pkg.name))
            continue;
        std::int64_t bytes = 0;
        if (pkg.kind == Kind::Formula) {
            // Todas as versões no Cellar (o "rack"), como o brew info --sizes.
            bytes = treeSize(p.cellar + "/" + pkg.name, cancel);
        } else {
            bytes = treeSize(p.caskroom + "/" + pkg.name, cancel);
            // O aplicativo fica em /Applications: o tamanho dele é o do cask.
            for (const auto &target : pkg.paths)
                if (target.rfind(p.caskroom, 0) != 0)
                    bytes += treeSize(target, cancel);
        }
        if (bytes > 0)
            out[pkg.key()] = bytes;
    }
    return out;
}

std::string Manager::version() const
{
    const auto l = lines(query({"--version"}));
    return l.empty() ? std::string() : l.front();
}

// ---------------------------------------------------------------------------
// Transações
// ---------------------------------------------------------------------------

int TransactionPreview::count() const
{
    int n = 0;
    for (const auto &s : sections)
        n += static_cast<int>(s.items.size());
    return n;
}

std::vector<std::string> TransactionPreview::items(const std::string &verb) const
{
    std::vector<std::string> out;
    for (const auto &s : sections)
        if (verb.empty() || s.verb == verb)
            out.insert(out.end(), s.items.begin(), s.items.end());
    return out;
}

TransactionPreview parseDryRun(const std::string &output, const std::string &errors)
{
    // Seções "==> Would <verbo> ...:" seguidas de um item por linha, com as
    // colunas alinhadas por espaços ("figlet  2.2.5", "curl 8.9.1 -> 8.10.0");
    // outra linha "==>" (Downloading...) encerra a seção. O cleanup imprime
    // "Would remove: caminho (tamanho)" e, no fim, o espaço que liberaria.
    TransactionPreview p;
    TransactionPreview::Section *current = nullptr;
    const auto collapse = [](const std::string &s) {
        std::string out;
        for (char c : trim(s)) {
            if (c == ' ' && !out.empty() && out.back() == ' ')
                continue;
            out += c;
        }
        return out;
    };
    const std::string freeMark = "would free approximately ";
    for (const auto &line : lines(output)) {
        if (line.rfind("==> ", 0) == 0) {
            const std::string text = trim(line.substr(4));
            current = nullptr;
            if (text.rfind("Would ", 0) == 0 && text.back() == ':') {
                const std::string title = text.substr(0, text.size() - 1);
                const size_t space = title.find(' ', 6);
                p.sections.push_back({title, title.substr(6, space == std::string::npos ? std::string::npos : space - 6), {}});
                current = &p.sections.back();
            } else if (text.find(freeMark) != std::string::npos) {
                const size_t start = text.find(freeMark) + freeMark.size();
                p.freed = text.substr(start, text.find(' ', start) - start);
            }
            continue;
        }
        if (line.rfind("Would remove: ", 0) == 0) {
            if (p.sections.empty() || p.sections.back().title != "Would remove")
                p.sections.push_back({"Would remove", "remove", {}});
            p.sections.back().items.push_back(trim(line.substr(14)));
            current = nullptr;
            continue;
        }
        if (line.rfind("Warning: ", 0) == 0) {
            p.warnings.push_back(trim(line.substr(9)));
            current = nullptr;
            continue;
        }
        if (current && !trim(line).empty())
            current->items.push_back(collapse(line));
    }
    for (const auto &line : lines(errors))
        if (line.rfind("Warning: ", 0) == 0)
            p.warnings.push_back(trim(line.substr(9)));
    std::vector<TransactionPreview::Section> kept;
    for (auto &s : p.sections)
        if (!s.items.empty())
            kept.push_back(std::move(s));
    p.sections = std::move(kept);
    return p;
}

std::string itemName(const std::string &item)
{
    const std::string t = trim(item);
    return t.substr(0, t.find(' '));
}

std::vector<std::string> dependentsClosure(const std::vector<Package> &installed, const std::vector<std::string> &keys)
{
    std::set<std::string> marked(keys.begin(), keys.end());
    std::set<std::string> gone;   // fórmulas que saem, pelo nome completo (é o que as dependências citam)
    for (const auto &p : installed)
        if (marked.count(p.key()) && p.kind == Kind::Formula)
            gone.insert(p.fullName);
    std::vector<std::string> out;
    for (bool changed = true; changed;) {
        changed = false;
        for (const auto &p : installed) {
            if (marked.count(p.key()))
                continue;
            for (const auto &d : p.dependencies) {
                if (!gone.count(d))
                    continue;
                marked.insert(p.key());
                out.push_back(p.key());
                if (p.kind == Kind::Formula)
                    gone.insert(p.fullName);
                changed = true;
                break;
            }
        }
    }
    return out;
}

bool isForbidden(const Package &p)
{
    return p.kind == Kind::Formula && (p.name == "ftxui" || p.name == "octobrew-tui");
}

bool caskNeedsSudo(const std::string &infoJson, const std::string &token)
{
    static const std::set<std::string> systemSteps{"pkgutil", "launchctl", "kext", "script", "early_script"};
    const json::Value doc = json::parse(infoJson);
    for (const auto &c : doc["casks"].items()) {
        if (c["token"].toString() != token && c["full_token"].toString() != token)
            continue;
        for (const auto &a : c["artifacts"].items())
            for (const auto &m : a.members()) {
                if (m.first == "pkg" || m.first == "installer")
                    return true;
                if (m.first != "uninstall")
                    continue;
                for (const auto &step : m.second.items())
                    for (const auto &s : step.members())
                        if (systemSteps.count(s.first))
                            return true;
            }
    }
    return false;
}

std::string snapshotName(std::time_t when)
{
    std::tm tm {};
    ::localtime_r(&when, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%y%m%d-%H%M%S", &tm);
    return std::string(buf) + ".Brewfile";
}

std::vector<std::string> listSnapshots(const std::string &dir)
{
    std::vector<std::string> out;
    const std::string suffix = ".Brewfile";
    if (DIR *d = ::opendir(dir.c_str())) {
        while (const dirent *e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n.size() > suffix.size() && n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0)
                out.push_back(n);
        }
        ::closedir(d);
    }
    // AAMMDD-hhmmss: a ordem do nome é a do tempo.
    std::sort(out.rbegin(), out.rend());
    return out;
}

TransactionPreview Manager::dryRun(const std::vector<std::string> &args) const
{
    std::vector<std::string> cmd{m_brewPath};
    cmd.insert(cmd.end(), args.begin(), args.end());
    const std::string shown = join(cmd, " ");
    const ProcessResult r = runProcess(cmd, kTimeoutMs, operationEnv(false));
    if (!r.started)
        fail(i18n::trf("Failed to start '%1': %2", {shown, r.startError}));
    if (r.timedOut)
        fail(i18n::trf("Command timed out (%1 ms): %2", {std::to_string(kTimeoutMs), shown}));
    if (r.crashed)
        fail(i18n::trf("Process crashed: %1", {shown}));
    if (r.status != 0) {
        // Só as linhas de erro: os avisos não explicam a falha.
        std::string msg;
        for (const auto &l : lines(r.err))
            if (l.rfind("Warning: ", 0) != 0)
                msg += (msg.empty() ? "" : "\n") + l;
        if (msg.rfind("Error: ", 0) == 0)
            msg.erase(0, 7);
        if (msg.empty())
            msg = i18n::trf("Command failed with exit code %1", {std::to_string(r.status)});
        fail(msg);
    }
    return parseDryRun(r.out, r.err);
}

TransactionPreview Manager::previewInstall(const std::vector<Package> &packages) const
{
    requirePackages(packages);
    TransactionPreview all;
    for (const auto &args : byKind("install", {"--dry-run"}, packages)) {
        TransactionPreview p = dryRun(args);
        all.sections.insert(all.sections.end(), p.sections.begin(), p.sections.end());
        all.warnings.insert(all.warnings.end(), p.warnings.begin(), p.warnings.end());
    }
    return all;
}

TransactionPreview Manager::previewUpgrade() const
{
    return dryRun({"upgrade", "--dry-run"});
}

TransactionPreview Manager::previewAutoremove() const
{
    return dryRun({"autoremove", "--dry-run"});
}

TransactionPreview Manager::previewCleanup() const
{
    return dryRun({"cleanup", "--dry-run", "--prune=all"});
}

std::vector<std::string> Manager::casksNeedingSudo(const std::vector<std::string> &tokens) const
{
    std::vector<std::string> out;
    if (tokens.empty())
        return out;
    for (const auto &t : tokens)
        if (!validPackageName(t))
            fail(i18n::trf("Invalid package name: %1", {t}));
    std::vector<std::string> args{"info", "--json=v2", "--cask"};
    args.insert(args.end(), tokens.begin(), tokens.end());
    const std::string json = query(args);
    for (const auto &t : tokens)
        if (caskNeedsSudo(json, t))
            out.push_back(t);
    return out;
}

std::string Manager::commandLine(const std::vector<std::string> &args) const
{
    std::vector<std::string> cmd{"brew"};
    cmd.insert(cmd.end(), args.begin(), args.end());
    return join(cmd, " ");
}

std::vector<std::vector<std::string>> Manager::installArgs(const std::vector<Package> &packages)
{
    return byKind("install", {}, packages);
}

std::vector<std::vector<std::string>> Manager::uninstallArgs(const std::vector<Package> &packages)
{
    return byKind("uninstall", {}, packages);
}

std::vector<std::string> Manager::pinArgs(const Package &p, bool pin)
{
    return {pin ? "pin" : "unpin", p.kind == Kind::Cask ? "--cask" : "--formula", p.fullName};
}

void Manager::run(const std::vector<std::string> &args, const OutputFn &out, bool interactive) const
{
    std::vector<std::string> cmd{m_brewPath};
    cmd.insert(cmd.end(), args.begin(), args.end());
    const std::string shown = commandLine(args);
    // Sem limite de tempo: um cask pode baixar gigabytes, e a espera inclui a
    // senha do sudo.
    const ProcessResult r = runProcess(cmd, -1, operationEnv(interactive), out);
    if (!r.started)
        fail(i18n::trf("Failed to start '%1': %2", {shown, r.startError}));
    if (r.crashed)
        fail(i18n::trf("Process crashed: %1", {shown}));
    if (r.status != 0) {
        std::string msg;
        for (const auto &l : lines(r.err))
            if (l.rfind("Error: ", 0) == 0 || (!msg.empty() && l.rfind("Warning: ", 0) != 0))
                msg += (msg.empty() ? "" : "\n") + (l.rfind("Error: ", 0) == 0 ? l.substr(7) : l);
        if (msg.empty())
            msg = trim(r.err);
        if (msg.find("no password was provided") != std::string::npos
            || msg.find("a password is required") != std::string::npos)
            msg += "\n" + i18n::tr("This cask needs sudo: run 'sudo -v' in the terminal and try again.");
        if (msg.empty())
            msg = i18n::trf("Command failed with exit code %1", {std::to_string(r.status)});
        fail(msg);
    }
}

void Manager::install(const std::vector<Package> &packages, const OutputFn &out, bool interactive) const
{
    requirePackages(packages);
    for (const auto &args : installArgs(packages)) {
        std::vector<std::string> a = args;
        a.insert(a.begin() + 1, "--no-ask");
        run(a, out, interactive);
    }
}

void Manager::uninstall(const std::vector<Package> &packages, const OutputFn &out, bool interactive) const
{
    requirePackages(packages);
    for (const auto &p : packages)
        if (isForbidden(p))
            fail(i18n::trf("%1 cannot be removed.", {p.name}));
    // Os casks primeiro: um cask pode depender de uma fórmula que também sai.
    auto groups = uninstallArgs(packages);
    std::stable_sort(groups.begin(), groups.end(), [](const std::vector<std::string> &a, const std::vector<std::string> &b) {
        return a[1] == "--cask" && b[1] != "--cask";
    });
    for (const auto &args : groups)
        run(args, out, interactive);
}

void Manager::upgrade(const OutputFn &out, bool interactive) const
{
    run({"upgrade", "--no-ask"}, out, interactive);
}

void Manager::update(const OutputFn &out) const
{
    run({"update"}, out, false);
}

void Manager::cleanup(const OutputFn &out) const
{
    run({"cleanup", "--prune=all"}, out, false);
}

void Manager::autoremove(const OutputFn &out) const
{
    run({"autoremove"}, out, false);
}

void Manager::pin(const Package &p, bool pin, const OutputFn &out) const
{
    requirePackages({p});
    run(pinArgs(p, pin), out, false);
}

void Manager::snapshot(const std::string &file, const OutputFn &out) const
{
    const size_t slash = file.rfind('/');
    if (slash != std::string::npos)
        (void)runProcess({"/bin/mkdir", "-p", file.substr(0, slash)}, 10000);
    run({"bundle", "dump", "--file=" + file, "--force"}, out, false);
}

void Manager::restore(const std::string &file, const OutputFn &out, bool interactive) const
{
    if (::access(file.c_str(), R_OK) != 0)
        fail(i18n::trf("Could not read %1", {file}));
    run({"bundle", "install", "--file=" + file, "--no-upgrade"}, out, interactive);
}

// ---------------------------------------------------------------------------
// Fase 4: serviços, taps, doctor e Brewfiles
// ---------------------------------------------------------------------------

std::vector<Service> parseServices(const std::string &text)
{
    std::vector<Service> out;
    // O documento numa variável: o laço sobre o items() de um temporário ficaria pendurado.
    const json::Value doc = json::parse(text);
    for (const auto &v : doc.items()) {
        Service sv;
        sv.name = v["name"].toString();
        sv.status = v["status"].toString("unknown");
        sv.user = v["user"].toString();
        sv.file = v["file"].toString();
        sv.exitCode = v["exit_code"].isNumber() ? static_cast<int>(v["exit_code"].toInt()) : -1;
        if (!sv.name.empty())
            out.push_back(std::move(sv));
    }
    return out;
}

std::vector<Tap> parseTaps(const std::string &text)
{
    std::vector<Tap> out;
    const json::Value doc = json::parse(text);
    for (const auto &v : doc.items()) {
        Tap t;
        t.name = v["name"].toString();
        t.official = v["official"].toBool();
        // Os oficiais são sempre confiáveis; o campo só existe no brew 6+.
        t.trusted = t.official || v["trusted"].toBool(!v.contains("trusted"));
        t.formulae = v["formula_names"].strings();
        t.casks = v["cask_tokens"].strings();
        if (!t.name.empty())
            out.push_back(std::move(t));
    }
    return out;
}

std::vector<Package> tapPackages(const std::vector<Tap> &taps)
{
    std::vector<Package> out;
    for (const auto &t : taps) {
        if (t.official)
            continue;   // os oficiais já vêm da API, com versão e descrição
        for (const Kind kind : {Kind::Formula, Kind::Cask})
            for (const auto &full : kind == Kind::Cask ? t.casks : t.formulae) {
                Package p;
                p.kind = kind;
                p.fullName = full.find('/') == std::string::npos ? t.name + "/" + full : full;
                p.name = p.fullName.substr(p.fullName.rfind('/') + 1);
                p.tap = t.name;
                p.status = Status::NotInstalled;
                out.push_back(std::move(p));
            }
    }
    return out;
}

DoctorReport parseDoctor(const std::string &output, int exitStatus)
{
    // O código de saída decide: o texto dos avisos muda de uma versão para outra.
    DoctorReport r;
    r.output = output;
    r.ready = exitStatus == 0;
    for (const auto &l : lines(output)) {
        if (l.rfind("Warning: ", 0) == 0)
            r.warnings.push_back(trim(l.substr(9)));
        else if (l.rfind("Error: ", 0) == 0)
            r.warnings.push_back(trim(l.substr(7)));
    }
    return r;
}

std::vector<std::string> parseBundleCheck(const std::string &output)
{
    std::vector<std::string> out;
    const std::string arrow = "\xE2\x86\x92 ";   // "→ "
    const std::string tail = " needs to be installed or updated.";
    for (const auto &l : lines(output)) {
        if (l.rfind(arrow, 0) != 0)
            continue;
        std::string item = trim(l.substr(arrow.size()));
        const size_t t = item.find(tail);
        if (t != std::string::npos)
            item = item.substr(0, t);
        out.push_back(item);
    }
    return out;
}

bool validTapName(const std::string &name)
{
    const size_t slash = name.find('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 >= name.size() || name.find('/', slash + 1) != std::string::npos
        || name[0] == '-')
        return false;
    for (unsigned char c : name)
        if (!std::isalnum(c) && c != '-' && c != '_' && c != '.' && c != '/')
            return false;
    return true;
}

std::vector<Service> Manager::listServices() const
{
    return parseServices(query({"services", "list", "--json"}));
}

void Manager::service(const std::string &verb, const std::string &name, const OutputFn &out) const
{
    if (verb != "start" && verb != "stop" && verb != "restart")
        fail(i18n::trf("Invalid service action: %1", {verb}));
    if (!validPackageName(name))
        fail(i18n::trf("Invalid package name: %1", {name}));
    run({"services", verb, name}, out, false);
}

std::vector<Tap> Manager::listTaps() const
{
    return parseTaps(query({"tap-info", "--json", "--installed"}));
}

void Manager::tap(const std::string &name, bool trust, const OutputFn &out) const
{
    if (!validTapName(name))
        fail(i18n::trf("Invalid tap name: %1 (use user/repo)", {name}));
    run({"tap", name}, out, false);
    if (trust)
        run({"trust", "--tap", name}, out, false);
}

void Manager::untap(const std::string &name, const OutputFn &out) const
{
    if (!validTapName(name))
        fail(i18n::trf("Invalid tap name: %1 (use user/repo)", {name}));
    run({"untap", name}, out, false);
}

DoctorReport Manager::doctor() const
{
    const std::vector<std::string> cmd{m_brewPath, "doctor"};
    const ProcessResult r = runProcess(cmd, kTimeoutMs, queryEnv());
    if (!r.started)
        fail(i18n::trf("Failed to start '%1': %2", {m_brewPath, r.startError}));
    if (r.timedOut)
        fail(i18n::trf("Command timed out (%1 ms): %2", {std::to_string(kTimeoutMs), "brew doctor"}));
    // 1: há avisos; mais que isso, falhou.
    if (r.crashed || (r.status != 0 && r.status != 1))
        fail(trim(r.err).empty() ? i18n::trf("Command failed with exit code %1", {std::to_string(r.status)}) : trim(r.err));
    return parseDoctor(r.out + (r.out.empty() || r.err.empty() ? "" : "\n") + r.err, r.status);
}

std::vector<std::string> Manager::bundleCheck(const std::string &file) const
{
    if (::access(file.c_str(), R_OK) != 0)
        fail(i18n::trf("Could not read %1", {file}));
    const std::vector<std::string> cmd{m_brewPath, "bundle", "check", "--file=" + file, "--verbose"};
    const ProcessResult r = runProcess(cmd, kTimeoutMs, queryEnv());
    if (!r.started)
        fail(i18n::trf("Failed to start '%1': %2", {m_brewPath, r.startError}));
    if (r.timedOut || r.crashed)
        fail(i18n::trf("Process crashed: %1", {"brew bundle check"}));
    // Sai com 1 quando falta algo; o que falta vem nas linhas "→".
    std::vector<std::string> missing = parseBundleCheck(r.out + "\n" + r.err);
    if (r.status != 0 && missing.empty()) {
        std::string msg;
        for (const auto &l : lines(r.err))
            if (l.rfind("Error: ", 0) == 0)
                msg = l.substr(7);
        fail(msg.empty() ? i18n::trf("Command failed with exit code %1", {std::to_string(r.status)}) : msg);
    }
    return missing;
}

} // namespace octo
