// SPDX-License-Identifier: GPL-2.0-or-later

#include "packagemodel.h"
#include "pages.h"
#include "strings.h"

#include <QBrush>
#include <QGuiApplication>
#include <QPalette>

namespace {

QString qs(const std::string &s)
{
    return QString::fromStdString(s);
}

} // namespace

// Fora de qualquer namespace, como o Q_INIT_RESOURCE exige.
static void registerResources()
{
    Q_INIT_RESOURCE(resources);
}

void PackageModel::initResources()
{
    static const bool once = (registerResources(), true);
    (void)once;
}

PackageModel::PackageModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    initResources();
}

int PackageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int PackageModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : model::ColumnCount;
}

const octo::Package *PackageModel::packageAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
        return nullptr;
    return &m_all[static_cast<size_t>(m_rows[static_cast<size_t>(row)])];
}

int PackageModel::rowOf(const QString &key) const
{
    const std::string k = key.toStdString();
    for (size_t i = 0; i < m_rows.size(); ++i)
        if (m_all[static_cast<size_t>(m_rows[i])].key() == k)
            return static_cast<int>(i);
    return -1;
}

int PackageModel::outdatedCount() const
{
    int n = 0;
    for (const auto &p : m_all)
        n += p.installed && !p.newVersion.empty() ? 1 : 0;
    return n;
}

QIcon PackageModel::iconFor(const octo::Package &p)
{
    initResources();
    // Os ícones e a lógica do OctoPkg: o instalado de que nenhum outro depende
    // tem o ícone "unrequired"; o fixado (brew pin), o cadeado do bloqueio.
    static const QIcon installed(QStringLiteral(":/images/installed.png"));
    static const QIcon unrequired(QStringLiteral(":/images/unrequired.png"));
    static const QIcon notInstalled(QStringLiteral(":/images/noninstalled.png"));
    static const QIcon outdated(QStringLiteral(":/images/outdated.png"));
    static const QIcon newer(QStringLiteral(":/images/newer.png"));
    static const QIcon pinned(QStringLiteral(":/images/locker-key.png"));
    switch (p.status) {
    case octo::Status::Pinned:       return pinned;
    case octo::Status::Outdated:     return outdated;
    case octo::Status::Newer:        return newer;
    case octo::Status::NotInstalled: return notInstalled;
    default:                         return p.requiredBy > 0 ? installed : unrequired;
    }
}

QVariant PackageModel::data(const QModelIndex &index, int role) const
{
    const octo::Package *p = packageAt(index.row());
    if (!p)
        return QVariant();
    const int column = index.column();
    if (role == Qt::DecorationRole && column == model::StateIcon)
        return iconFor(*p);
    if (role == Qt::ToolTipRole && column == model::StateIcon)
        return pages::statusText(*p);
    if (role == Qt::TextAlignmentRole && column == model::Size)
        return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::ForegroundRole && !p->installed)
        return QGuiApplication::palette().brush(QPalette::Disabled, QPalette::Text);
    if (role != Qt::DisplayRole)
        return QVariant();
    switch (column) {
    case model::Name:
        return qs(p->name);
    case model::Type:
        return p->kind == octo::Kind::Cask ? ui::tr("Cask") : ui::tr("Formula");
    case model::Version:
        return qs(p->version);
    case model::Size:
        if (p->size > 0)
            return qs(octo::formatSize(p->size));
        return p->installed && m_sizing ? QStringLiteral("…") : QString();
    case model::InstalledOn:
        return qs(octo::formatTime(p->installedOn));
    default:
        return QVariant();
    }
}

QVariant PackageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    switch (section) {
    case model::StateIcon:   return QString();   // só o ícone, como no OctoPkg
    case model::Name:        return ui::tr("Name");
    case model::Type:        return ui::tr("Type");
    case model::Version:     return ui::tr("Version");
    case model::Size:        return ui::tr("Size");
    case model::InstalledOn: return ui::tr("Installed on");
    default:                 return QVariant();
    }
}

void PackageModel::sort(int column, Qt::SortOrder order)
{
    m_sort = {column, order == Qt::AscendingOrder};
    rebuild();
}

void PackageModel::setPackages(std::vector<octo::Package> packages)
{
    m_all = std::move(packages);
    rebuild();
}

void PackageModel::setFilter(const QString &text, model::SearchMode mode, const QString &tap, bool leavesOnly)
{
    if (text == m_filter && mode == m_mode && tap == m_tap && leavesOnly == m_leaves)
        return;
    m_filter = text;
    m_mode = mode;
    m_tap = tap;
    m_leaves = leavesOnly;
    rebuild();
}

void PackageModel::setSizing(bool sizing)
{
    if (sizing == m_sizing)
        return;
    m_sizing = sizing;
    if (!m_rows.empty())
        emit dataChanged(index(0, model::Size), index(rowCount() - 1, model::Size));
}

void PackageModel::rebuild()
{
    beginResetModel();
    m_rows = model::visibleRows(m_all, m_filter.toStdString(), m_sort, m_mode, m_tap.toStdString(), m_leaves);
    endResetModel();
}
