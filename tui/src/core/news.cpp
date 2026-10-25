// SPDX-License-Identifier: GPL-2.0-or-later
//
// O feed do blog do Homebrew: porte do utils::retrieveDistroNews e do
// parseDistroNews do OctoPkg (via octopkg-tui), com o Atom do brew.sh.
// OctoPkg: Copyright (C) 2015 Alexandre Albuquerque Arnt.

#include "news.h"
#include "i18n.h"
#include "process.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace octo {

const char kNewsUrl[] = "https://brew.sh/atom.xml";
const char kNewsSource[] = "Homebrew";

namespace {

constexpr int kFetchTimeoutMs = 60000;

// Um post do blog do Homebrew pode ter milhares de caracteres: a aba mostra o começo.
constexpr size_t kMaxDescription = 600;

std::string readFile(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// mkdir -p do diretório de path.
void makeParentDirs(const std::string &path)
{
    const size_t slash = path.rfind('/');
    if (slash == std::string::npos || slash == 0)
        return;
    const std::string dir = path.substr(0, slash);
    size_t pos = 0;
    while ((pos = dir.find('/', pos + 1)) != std::string::npos)
        ::mkdir(dir.substr(0, pos).c_str(), 0755);
    ::mkdir(dir.c_str(), 0755);
}

void appendUtf8(std::string &out, unsigned long cp)
{
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x110000) {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// &amp; &lt; &gt; &quot; &apos; &nbsp; &#NNN; &#xHH;
std::string decodeEntities(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') {
            out += s[i];
            continue;
        }
        const size_t semi = s.find(';', i);
        if (semi == std::string::npos || semi - i > 10) {
            out += s[i];
            continue;
        }
        const std::string name = s.substr(i + 1, semi - i - 1);
        if (name == "amp")
            out += '&';
        else if (name == "lt")
            out += '<';
        else if (name == "gt")
            out += '>';
        else if (name == "quot")
            out += '"';
        else if (name == "apos")
            out += '\'';
        else if (name == "nbsp")
            out += ' ';
        else if (name.size() > 1 && name[0] == '#') {
            const bool hex = name[1] == 'x' || name[1] == 'X';
            char *end = nullptr;
            const unsigned long cp = std::strtoul(name.c_str() + (hex ? 2 : 1), &end, hex ? 16 : 10);
            if (!end || *end != '\0') {
                out += s.substr(i, semi - i + 1);
            } else {
                appendUtf8(out, cp);
            }
        } else {
            out += s.substr(i, semi - i + 1);   // desconhecida: fica como está
        }
        i = semi;
    }
    return out;
}

// Texto puro: a descrição pode vir como HTML escapado (&lt;p&gt;) ou dentro de
// CDATA; decodifica, tira as tags, decodifica as entidades que estavam no
// HTML e junta os espaços.
std::string plainText(const std::string &value)
{
    const std::string html = decodeEntities(value);
    std::string noTags;
    bool inTag = false;
    for (size_t i = 0; i < html.size(); ++i) {
        const char c = html[i];
        // Uma tag começa com letra, "/" ou "!" logo depois do "<"; "3 < 4" fica.
        const char next = i + 1 < html.size() ? html[i + 1] : '\0';
        if (c == '<' && !inTag && (std::isalpha(static_cast<unsigned char>(next)) || next == '/' || next == '!'))
            inTag = true;
        else if (c == '>' && inTag) {
            inTag = false;
            noTags += ' ';
        } else if (!inTag)
            noTags += c;
    }
    const std::string decoded = decodeEntities(noTags);
    std::string out;
    bool space = false;
    for (char c : decoded) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            space = !out.empty();
            continue;
        }
        if (space)
            out += ' ';
        space = false;
        out += c;
    }
    return out;
}

// Conteúdo de <tag>…</tag> dentro de item, com CDATA desembrulhado.
std::string element(const std::string &item, const std::string &tag)
{
    const std::string open = "<" + tag;
    size_t start = 0;
    for (;;) {
        start = item.find(open, start);
        if (start == std::string::npos)
            return std::string();
        const char next = start + open.size() < item.size() ? item[start + open.size()] : '\0';
        if (next == '>' || next == ' ' || next == '\t' || next == '\n')
            break;   // não confunde <link> com <linkx>
        start += open.size();
    }
    const size_t gt = item.find('>', start);
    if (gt == std::string::npos)
        return std::string();
    if (item[gt - 1] == '/')
        return std::string();   // <tag/>
    const size_t end = item.find("</" + tag + ">", gt);
    if (end == std::string::npos)
        return std::string();
    std::string value = item.substr(gt + 1, end - gt - 1);
    std::string out;
    size_t pos = 0;
    while (pos < value.size()) {
        const size_t cdata = value.find("<![CDATA[", pos);
        if (cdata == std::string::npos) {
            out += value.substr(pos);
            break;
        }
        out += value.substr(pos, cdata - pos);
        const size_t close = value.find("]]>", cdata);
        if (close == std::string::npos) {
            out += value.substr(cdata + 9);
            break;
        }
        // O conteúdo do CDATA já é texto: as entidades nele são escapadas para
        // sobreviver ao decodeEntities() de plainText().
        std::string raw = value.substr(cdata + 9, close - cdata - 9);
        std::string escaped;
        for (char c : raw)
            escaped += c == '&' ? std::string("&amp;") : std::string(1, c);
        out += escaped;
        pos = close + 3;
    }
    return out;
}


// O atributo attr da primeira tag <tag ...> de item ("href" do <link>).
std::string attribute(const std::string &item, const std::string &tag, const std::string &attr)
{
    size_t start = 0;
    const std::string open = "<" + tag;
    while ((start = item.find(open, start)) != std::string::npos) {
        const size_t gt = item.find('>', start);
        if (gt == std::string::npos)
            return std::string();
        const std::string t = item.substr(start, gt - start);
        // No Atom, o link do post é o rel="alternate" (ou o sem rel).
        const bool alternate = t.find("rel=") == std::string::npos || t.find("rel=\"alternate\"") != std::string::npos;
        const size_t a = t.find(attr + "=\"");
        if (alternate && a != std::string::npos) {
            const size_t v = a + attr.size() + 2;
            return decodeEntities(t.substr(v, t.find('"', v) - v));
        }
        start = gt;
    }
    return std::string();
}

// Até max caracteres (UTF-8), cortando numa palavra, com "…".
std::string shorten(const std::string &s, size_t max)
{
    size_t chars = 0, cut = std::string::npos;
    for (size_t i = 0; i < s.size(); ++i) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) == 0x80)
            continue;
        if (chars == max) {
            cut = i;
            break;
        }
        ++chars;
    }
    if (cut == std::string::npos)
        return s;
    const size_t space = s.rfind(' ', cut);
    return s.substr(0, space == std::string::npos || space == 0 ? cut : space) + " …";
}

// Os blocos <tag>…</tag> (ou <tag ...>…</tag>) de xml, até max.
std::vector<std::string> blocks(const std::string &xml, const std::string &tag, size_t max)
{
    std::vector<std::string> out;
    const std::string open = "<" + tag, close = "</" + tag + ">";
    size_t pos = 0;
    while (out.size() < max) {
        size_t start = xml.find(open, pos);
        while (start != std::string::npos && start + open.size() < xml.size() && xml[start + open.size()] != '>'
               && xml[start + open.size()] != ' ')
            start = xml.find(open, start + open.size());   // <items>, <entryx>...
        if (start == std::string::npos)
            break;
        const size_t end = xml.find(close, start);
        if (end == std::string::npos)
            break;
        out.push_back(xml.substr(start, end - start));
        pos = end + close.size();
    }
    return out;
}

bool isVersion(const std::string &s)
{
    if (s.empty() || !std::isdigit(static_cast<unsigned char>(s[0])))
        return false;
    for (char c : s)
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.')
            return false;
    return true;
}

std::string defaultCurl()
{
    if (::access("/usr/bin/curl", X_OK) == 0)
        return "/usr/bin/curl";
    const std::string found = findExecutable("curl");
    return found.empty() ? std::string("curl") : found;
}

} // namespace

std::vector<NewsItem> parseFeed(const std::string &xml, size_t max)
{
    std::vector<NewsItem> items;
    // Atom, como o do brew.sh (Jekyll).
    for (const auto &entry : blocks(xml, "entry", max)) {
        NewsItem n;
        n.title = plainText(element(entry, "title"));
        if (isVersion(n.title))
            n.title = "Homebrew " + n.title;   // os posts de versão têm só o número
        n.link = attribute(entry, "link", "href");
        std::string summary = plainText(element(entry, "summary"));
        if (summary.empty())
            summary = plainText(element(entry, "content"));
        n.description = shorten(summary, kMaxDescription);
        std::string date = plainText(element(entry, "published"));
        if (date.empty())
            date = plainText(element(entry, "updated"));
        n.date = date.substr(0, date.find('T'));   // "2026-09-13T00:00:00+00:00" → "2026-09-13"
        if (!n.title.empty() || !n.description.empty())
            items.push_back(std::move(n));
    }
    if (!items.empty())
        return items;
    // RSS 2.0, como no octopkg-tui.
    for (const auto &item : blocks(xml, "item", max)) {
        NewsItem n;
        n.title = plainText(element(item, "title"));
        n.link = plainText(element(item, "link"));
        n.description = shorten(plainText(element(item, "description")), kMaxDescription);
        // Como o OctoPkg: a data sem o fuso ("Mon, 20 Oct 2026 10:00:00 +0000").
        n.date = plainText(element(item, "pubDate"));
        const size_t plus = n.date.find_last_of("+-");
        if (plus != std::string::npos && plus > 0 && n.date[plus - 1] == ' ')
            n.date = n.date.substr(0, plus - 1);
        if (!n.title.empty() || !n.description.empty())
            items.push_back(std::move(n));
    }
    return items;
}

NewsFeed::NewsFeed(std::string cachePath, std::string curlPath, std::string url)
    : m_cachePath(std::move(cachePath))
    , m_curlPath(curlPath.empty() ? defaultCurl() : std::move(curlPath))
    , m_url(url.empty() ? std::string(kNewsUrl) : std::move(url))
{
}

NewsResult NewsFeed::cached() const
{
    NewsResult r;
    r.items = parseFeed(readFile(m_cachePath));
    r.fromCache = true;
    return r;
}

NewsResult NewsFeed::refresh() const
{
    makeParentDirs(m_cachePath);
    const std::string tmp = m_cachePath + ".tmp";
    // curl: -f falha nos erros HTTP, -sS sem progresso mas com o erro, -L segue
    // redirecionamentos, só por https; o certificado é sempre conferido.
    const ProcessResult p = runProcess({m_curlPath, "-fsSL", "--proto", "=https", "--max-time", "30", "-o", tmp, m_url},
                                       kFetchTimeoutMs);
    std::string error;
    if (!p.started)
        error = i18n::trf("Failed to start '%1': %2", {m_curlPath, p.startError});
    else if (p.timedOut)
        error = i18n::trf("Command timed out (%1 ms): %2", {std::to_string(kFetchTimeoutMs), m_curlPath});
    else if (p.status != 0) {
        std::string msg = p.err;
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == ' '))
            msg.pop_back();
        error = msg.empty() ? i18n::trf("Command failed with exit code %1", {std::to_string(p.status)}) : msg;
    }
    const std::string fresh = error.empty() ? readFile(tmp) : std::string();
    // Só substitui o cache por um feed que dê para ler.
    if (error.empty() && parseFeed(fresh, 1).empty())
        error = i18n::tr("The news feed could not be read.");
    if (!error.empty()) {
        std::remove(tmp.c_str());
        NewsResult r = cached();
        r.error = error;
        return r;
    }
    NewsResult r;
    r.updated = fresh != readFile(m_cachePath);
    if (std::rename(tmp.c_str(), m_cachePath.c_str()) != 0) {
        r.error = i18n::trf("Could not save %1", {m_cachePath});
        std::remove(tmp.c_str());
    }
    r.items = parseFeed(fresh);
    return r;
}

} // namespace octo
