// SPDX-License-Identifier: GPL-2.0-or-later
//
// A tabela de pacotes: um QAbstractTableModel sobre o std::vector<Package> do
// núcleo, com o filtro, a ordenação e as folhas do model::visibleRows do
// octobrew-tui. O QStandardItemModel do OctoPkg cria um item por célula e fica
// lento com os ~16 mil pacotes do Homebrew; aqui as linhas são só índices.

#ifndef OCTOBREW_PACKAGEMODEL_H
#define OCTOBREW_PACKAGEMODEL_H

#include "brew.h"
#include "listmodel.h"

#include <QAbstractTableModel>
#include <QIcon>
#include <vector>

class PackageModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    /** As colunas são as do model:: (Estado, Nome, Tipo, Versão, Tamanho, Instalado em). */
    explicit PackageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    /** O clique no cabeçalho (QTableView::setSortingEnabled). */
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    /** A lista toda (a visão atual); o filtro e a ordem continuam. */
    void setPackages(std::vector<octo::Package> packages);
    void setFilter(const QString &text, model::SearchMode mode, const QString &tap, bool leavesOnly);
    /** Enquanto os tamanhos são calculados, os instalados sem tamanho mostram "…". */
    void setSizing(bool sizing);

    const std::vector<octo::Package> &packages() const { return m_all; }
    /** O pacote da linha, ou nullptr. */
    const octo::Package *packageAt(int row) const;
    /** A linha do pacote de key (Package::key()), ou -1 se não estiver visível. */
    int rowOf(const QString &key) const;
    model::SortSpec sortSpec() const { return m_sort; }
    int outdatedCount() const;

    /**
     * Carrega os ícones (resources.qrc). Numa biblioteca estática, o Qt não os
     * registra sozinho: o aplicativo e os testes chamam isto antes de usá-los
     * (o construtor do modelo e o iconFor() já chamam).
     */
    static void initResources();
    /** O ícone do estado, como o PackageModel::getIconFor do OctoPkg. */
    static QIcon iconFor(const octo::Package &p);

private:
    void rebuild();

    std::vector<octo::Package> m_all;
    std::vector<int> m_rows;
    QString m_filter;
    model::SearchMode m_mode = model::SearchMode::Name;
    QString m_tap;
    bool m_leaves = false;
    bool m_sizing = false;
    model::SortSpec m_sort;
};

#endif
