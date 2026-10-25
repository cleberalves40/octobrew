// SPDX-License-Identifier: GPL-2.0-or-later

#include "pages.h"
#include "texts.h"

namespace pages {

namespace {

QString esc(const std::string &s)
{
    return QString::fromStdString(s).toHtmlEscaped();
}

QString row(const QString &label, const QString &valueHtml)
{
    return QStringLiteral("<tr><td style='padding-right:16px; white-space:nowrap'><b>%1:</b></td><td>%2</td></tr>")
        .arg(label.toHtmlEscaped(), valueHtml);
}

QString list(const std::vector<std::string> &items)
{
    QStringList out;
    for (const auto &i : items)
        out << esc(i);
    return out.join(QStringLiteral(", "));
}

// A cor do estado, a mesma em todos os temas claros e escuros do macOS.
QString statusColor(const octo::Package &p)
{
    switch (p.status) {
    case octo::Status::Pinned:       return QStringLiteral("#a04ca0");
    case octo::Status::Outdated:     return QStringLiteral("#c08000");
    case octo::Status::Newer:        return QStringLiteral("#2080a0");
    case octo::Status::NotInstalled: return QString();
    default:                         return QStringLiteral("#2e8b2e");
    }
}

} // namespace

QString statusText(const octo::Package &p)
{
    const QString available = ui::trf("(version %1 is available)", {QString::fromStdString(p.newVersion)});
    switch (p.status) {
    case octo::Status::Pinned:
        return p.newVersion.empty() ? ui::tr("Pinned") : ui::tr("Pinned") + " " + available;
    case octo::Status::Outdated:
        return ui::tr("Outdated") + " " + available;
    case octo::Status::Newer:
        return ui::tr("Newer than the repository")
               + ui::trf(" (newer installed version is %1)", {QString::fromStdString(p.installedVersion)});
    case octo::Status::NotInstalled:
        return ui::tr("Not installed");
    default:
        return p.automatic ? ui::tr("Installed as a dependency") : ui::tr("Installed");
    }
}

QString kindText(octo::Kind kind)
{
    return kind == octo::Kind::Cask ? ui::tr("Cask") : ui::tr("Formula");
}

QString emptyInfoHtml()
{
    return QStringLiteral("<p><i>%1</i></p>").arg(ui::tr("Select a package to see details.").toHtmlEscaped());
}

QString infoHtml(const octo::Package &p, const octo::Details *d, const QString &error)
{
    QString html = QStringLiteral("<h3 style='margin-bottom:2px'>%1</h3><p style='margin-top:0'>%2</p><table>")
                       .arg(esc(p.name), esc(p.comment));
    const QString color = statusColor(p);
    const QString status = statusText(p).toHtmlEscaped();
    html += row(ui::tr("Status"), color.isEmpty() ? status : QStringLiteral("<span style='color:%1'>%2</span>").arg(color, status));
    html += row(ui::tr("Type"), kindText(p.kind).toHtmlEscaped());
    if (!error.isEmpty())
        return html + QStringLiteral("</table><p style='color:#c03030'>%1</p>")
                          .arg(ui::trf("ERROR: %1", {error}).toHtmlEscaped());
    if (!d)
        return html + QStringLiteral("</table><p><i>%1</i></p>").arg(ui::tr("Loading…").toHtmlEscaped());

    if (p.installed && !p.installedVersion.empty() && p.installedVersion != d->version)
        html += row(ui::tr("Installed version"), esc(p.installedVersion));
    html += row(ui::tr("Version"), esc(d->version));
    if (!d->names.empty())
        html += row(ui::tr("Application"), esc(d->names.front()));
    html += row(ui::tr("Tap"), esc(d->tap));
    if (!d->homepage.empty())
        html += row(ui::tr("Homepage"), QStringLiteral("<a href='%1'>%1</a>").arg(esc(d->homepage)));
    if (!d->license.empty())
        html += row(ui::tr("License"), esc(d->license));
    if (!d->kegOnly.empty())
        html += row(ui::tr("Keg-only"), esc(d->kegOnly));
    if (d->autoUpdates)
        html += row(ui::tr("Auto-updates"), ui::tr("yes (brew upgrade skips it)").toHtmlEscaped());
    if (!d->deprecated.empty())
        html += row(ui::tr("Deprecated"), QStringLiteral("<span style='color:#c08000'>%1</span>").arg(esc(d->deprecated)));
    if (!d->disabled.empty())
        html += row(ui::tr("Disabled"), QStringLiteral("<span style='color:#c03030'>%1</span>").arg(esc(d->disabled)));
    if (p.size > 0)
        html += row(ui::tr("Installed size"), esc(octo::formatSize(p.size)));
    if (d->installedOn > 0)
        html += row(ui::tr("Installed on"), esc(octo::formatTime(d->installedOn)));
    if (!d->dependencies.empty())
        html += row(ui::tr("Depends on"), list(d->dependencies));
    if (!d->buildDependencies.empty())
        html += row(ui::tr("Build dependencies"), list(d->buildDependencies));
    if (!d->requiredBy.empty())
        html += row(ui::tr("Required by"), list(d->requiredBy));
    if (!d->conflicts.empty())
        html += row(ui::tr("Conflicts with"), list(d->conflicts));
    if (!d->artifacts.empty())
        html += row(ui::tr("Artifacts"), list(d->artifacts));
    html += QStringLiteral("</table>");
    // Os caveats do brew (o que fazer depois de instalar), como o pkg-message.
    if (!d->caveats.empty())
        html += QStringLiteral("<h4 style='color:#c08000'>%1</h4><pre>%2</pre>")
                    .arg(ui::tr("Caveats").toHtmlEscaped(), esc(d->caveats));
    return html;
}

QString helpHtml(const QList<QPair<QString, QString>> &shortcuts)
{
    QString html = QStringLiteral("<h3>%1</h3><table>").arg(ui::tr("Keyboard shortcuts").toHtmlEscaped());
    for (const auto &s : shortcuts)
        html += QStringLiteral("<tr><td style='padding-right:24px; white-space:nowrap'><b>%1</b></td><td>%2</td></tr>")
                    .arg(s.first.toHtmlEscaped(), s.second.toHtmlEscaped());
    html += QStringLiteral("</table><p><i>%1</i></p>").arg(ui::tr("Click a column header to sort").toHtmlEscaped());
    return html;
}

} // namespace pages
