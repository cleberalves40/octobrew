// SPDX-License-Identifier: GPL-2.0-or-later
//
// Notícias do Homebrew (aba Notícias): o feed Atom do blog (brew.sh), no
// lugar do feed da variante BSD do octopkg-tui, que é um porte do
// utils::retrieveDistroNews e do parseDistroNews do OctoPkg.
// OctoPkg: Copyright (C) 2015 Alexandre Albuquerque Arnt.

#ifndef OCTOBREW_TUI_NEWS_H
#define OCTOBREW_TUI_NEWS_H

#include <string>
#include <vector>

namespace octo {

/** O feed do blog do Homebrew. */
extern const char kNewsUrl[];
/** Nome da fonte, para a tela. */
extern const char kNewsSource[];

struct NewsItem {
    std::string title;
    std::string link;
    std::string description;   // texto puro, sem as tags HTML
    std::string date;          // "2026-09-13" (Atom) ou o pubDate sem o fuso (RSS)
};

/**
 * Os primeiros max itens de um feed Atom (as <entry> do brew.sh) ou RSS 2.0
 * (os <item>), com CDATA e entidades HTML decodificados. No Atom, a descrição
 * é o <summary> (ou o começo do <content>), e um título que é só a versão
 * ("7.0.0") vira "Homebrew 7.0.0".
 */
std::vector<NewsItem> parseFeed(const std::string &xml, size_t max = 10);

struct NewsResult {
    std::vector<NewsItem> items;
    bool updated = false;      // o feed baixado é diferente do que estava no cache
    bool fromCache = false;    // não foi possível baixar: estes são os do cache
    std::string error;         // por que não foi possível baixar
};

/**
 * O feed e o cache em disco, como o ~/.config/octopkg/distro_rss.xml do
 * OctoPkg. O download usa o /usr/bin/curl do macOS, que confere o
 * certificado TLS (o OctoPkg usa "curl -k", que não confere).
 */
class NewsFeed
{
public:
    /** curlPath vazio: /usr/bin/curl (ou o do PATH, no Linux). url vazia: kNewsUrl. */
    explicit NewsFeed(std::string cachePath, std::string curlPath = std::string(), std::string url = std::string());

    /** As notícias do cache, sem rede (vazio se ainda não há cache). */
    NewsResult cached() const;
    /** Baixa o feed; se falhar, devolve o cache com o erro. */
    NewsResult refresh() const;

    const std::string &cachePath() const { return m_cachePath; }

private:
    std::string m_cachePath;
    std::string m_curlPath;
    std::string m_url;
};

} // namespace octo

#endif
