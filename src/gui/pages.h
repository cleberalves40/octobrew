// SPDX-License-Identifier: GPL-2.0-or-later
//
// O conteúdo das abas em HTML (QTextBrowser): Informação e Ajuda. Funções
// puras, testadas sem janela, com os mesmos campos da aba Informação do
// octobrew-tui.

#ifndef OCTOBREW_PAGES_H
#define OCTOBREW_PAGES_H

#include "brew.h"
#include "news.h"

#include <QList>
#include <QPair>
#include <QString>

namespace pages {

/** O estado por extenso ("Outdated (version 8.10.0 is available)"). */
QString statusText(const octo::Package &p);
/** "Formula" ou "Cask", traduzido. */
QString kindText(octo::Kind kind);

/**
 * A aba Informação: o pacote, e os detalhes do brew info quando chegam
 * (details nulo: "Carregando…"; error: a mensagem do brew, em vermelho).
 */
QString infoHtml(const octo::Package &p, const octo::Details *details, const QString &error,
                 const QString &service = QString());
/** O estado de um serviço, traduzido ("started" → "Started"). */
QString serviceStatusText(const std::string &status);
/** A aba Notícias: as do feed, com o aviso se não deu para baixar. */
QString newsHtml(const std::vector<octo::NewsItem> &items, bool loading, const QString &error, bool fromCache);
/** A aba Informação sem pacote selecionado. */
QString emptyInfoHtml();

/** A aba Ajuda: os atalhos (na notação do sistema: ⌘R no macOS) e o que fazem. */
QString helpHtml(const QList<QPair<QString, QString>> &shortcuts);

} // namespace pages

#endif
