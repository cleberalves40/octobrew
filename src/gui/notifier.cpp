// SPDX-License-Identifier: GPL-2.0-or-later

#include "notifier.h"
#include "packagemodel.h"
#include "preferences.h"
#include "texts.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMenu>
#include <QProcess>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

namespace notifier {

Summary summarize(const std::vector<octo::Package> &installed)
{
    Summary s;
    for (const auto &p : installed)
        if (!p.newVersion.empty() && !p.pinned)
            s.outdated << QString::fromStdString(p.name + " " + p.installedVersion + " → " + p.newVersion);
    s.state = s.outdated.isEmpty() ? State::UpToDate : State::Outdated;
    return s;
}

QString title(const Summary &s)
{
    switch (s.state) {
    case State::Checking:
        return ui::tr("Checking for updates…");
    case State::Error:
        return ui::trf("ERROR: %1", {s.error});
    case State::Outdated:
        return s.outdated.size() == 1 ? ui::tr("1 update available")
                                      : ui::trf("%1 updates available", {QString::number(s.outdated.size())});
    default:
        return ui::tr("Homebrew is up to date");
    }
}

QIcon icon(const Summary &s)
{
    PackageModel::initResources();
    switch (s.state) {
    case State::Outdated: return QIcon(QStringLiteral(":/images/octopi_yellow.png"));
    case State::Error:    return QIcon(QStringLiteral(":/images/octopi_red.png"));
    default:              return QIcon(QStringLiteral(":/images/octopi_green.png"));
    }
}

QString agentLabel()
{
    return QStringLiteral("io.github.cleberalves40.octobrew.notifier");
}

QString agentPath(const QString &home)
{
    return home + "/Library/LaunchAgents/" + agentLabel() + ".plist";
}

QString agentPlist(const QString &program)
{
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
               "<plist version=\"1.0\">\n<dict>\n"
               "    <key>Label</key>\n    <string>%1</string>\n"
               "    <key>ProgramArguments</key>\n    <array>\n        <string>%2</string>\n    </array>\n"
               "    <key>RunAtLoad</key>\n    <true/>\n"
               "    <key>ProcessType</key>\n    <string>Interactive</string>\n"
               "</dict>\n</plist>\n")
        .arg(agentLabel(), program.toHtmlEscaped());
}

bool setStartAtLogin(bool enable, const QString &program, const QString &home, QString *error)
{
    const QString path = agentPath(home);
    if (!enable) {
        if (QFile::exists(path) && !QFile::remove(path)) {
            if (error)
                *error = ui::trf("Could not remove %1", {path});
            return false;
        }
        return true;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = ui::trf("Could not save %1", {path});
        return false;
    }
    file.write(agentPlist(program).toUtf8());
    return true;
}

bool startsAtLogin(const QString &home)
{
    return QFile::exists(agentPath(home));
}

QString notifierPath()
{
    const QFileInfo info(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("octobrew-notifier")));
    return info.isExecutable() ? info.absoluteFilePath() : QString();
}

QString appPath()
{
    QDir dir(QCoreApplication::applicationDirPath());
#ifdef Q_OS_MACOS
    // Contents/MacOS dentro do OctoBrew.app: o "open" abre o bundle.
    if (dir.dirName() == "MacOS" && dir.cdUp() && dir.dirName() == "Contents" && dir.cdUp())
        return dir.absolutePath();
    return QString();
#else
    const QFileInfo info(dir.filePath(QStringLiteral("octobrew")));
    return info.isExecutable() ? info.absoluteFilePath() : QString();
#endif
}

} // namespace notifier

Notifier::Notifier(const Options &options, QObject *parent)
    : QObject(parent)
    , m_options(options)
    , m_settings((options.settingsPath.isEmpty() ? prefs::defaultSettingsPath() : options.settingsPath).toStdString())
    , m_mgr(options.brewPath.toStdString())
{
    m_menu = new QMenu;
    m_title = m_menu->addAction(notifier::title(m_summary));
    m_title->setEnabled(false);
    m_menu->addSeparator();
    m_upgrade = m_menu->addAction(QIcon(QStringLiteral(":/images/octopi_yellow.png")), ui::tr("Install updates"),
                                  this, [this] { launch({QStringLiteral("--upgrade")}); });
    m_menu->addAction(ui::tr("Open OctoBrew"), this, [this] { launch({}); });
    m_menu->addAction(ui::tr("Check for updates now"), this, [this] { check(); });
    m_menu->addSeparator();
    m_menu->addAction(ui::tr("Quit the notifier"), qApp, &QCoreApplication::quit);
    if (options.showIcon && QSystemTrayIcon::isSystemTrayAvailable()) {
        m_tray = new QSystemTrayIcon(notifier::icon(m_summary), this);
        m_tray->setContextMenu(m_menu);
        m_tray->show();
    }
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Notifier::check);
    m_timer->start(prefs::notifierHours(m_settings) * 3600 * 1000);
    apply(m_summary);
}

Notifier::~Notifier()
{
    delete m_menu;
}

void Notifier::check()
{
    if (m_checking)
        return;
    m_checking = true;
    notifier::Summary checking = m_summary;
    checking.state = notifier::State::Checking;
    apply(checking);
    // A preferência é relida a cada verificação: o diálogo do OctoBrew pode tê-la mudado.
    m_settings = octo::Settings(m_settings.path());
    m_timer->start(prefs::notifierHours(m_settings) * 3600 * 1000);
    const bool update = prefs::notifierUpdates(m_settings);
    const auto work = [this, update] {
        notifier::Summary s;
        try {
            if (update)
                m_mgr.update();   // como o octopi-notifier: o catálogo em dia antes de contar
            s = notifier::summarize(m_mgr.listInstalled());
        } catch (const std::exception &e) {
            s.state = notifier::State::Error;
            s.error = QString::fromUtf8(e.what());
        }
        return s;
    };
    const auto done = [this](const notifier::Summary &s) {
        m_checking = false;
        apply(s);
    };
    if (m_options.synchronous) {
        done(work());
        return;
    }
    auto *watcher = new QFutureWatcher<notifier::Summary>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [watcher, done] {
        done(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(work));
}

void Notifier::apply(const notifier::Summary &summary)
{
    if (summary.state != notifier::State::Checking)
        m_summary = summary;
    const QString text = notifier::title(summary);
    m_title->setText(text);
    m_upgrade->setEnabled(summary.state == notifier::State::Outdated);
    // Os nomes dos desatualizados, logo abaixo do título (até 10).
    for (QAction *a : m_menu->actions())
        if (a->property("outdatedItem").toBool())
            m_menu->removeAction(a);
    QAction *before = m_menu->actions().value(1);
    for (int i = 0; i < summary.outdated.size() && i < 10; ++i) {
        auto *item = new QAction(QStringLiteral("    ") + summary.outdated[i], m_menu);
        item->setEnabled(false);
        item->setProperty("outdatedItem", true);
        m_menu->insertAction(before, item);
    }
    if (m_tray) {
        m_tray->setIcon(notifier::icon(summary));
        m_tray->setToolTip(QStringLiteral("OctoBrew: ") + text);
    }
}

void Notifier::launch(const QStringList &args)
{
    m_lastLaunch = args;
    if (m_launcher) {
        m_launcher(args);
        return;
    }
    const QString app = notifier::appPath();
    if (app.isEmpty())
        return;
#ifdef Q_OS_MACOS
    QStringList open{QStringLiteral("-a"), app};
    if (!args.isEmpty())
        open << QStringLiteral("--args") << args;
    QProcess::startDetached(QStringLiteral("/usr/bin/open"), open);
#else
    QProcess::startDetached(app, args);
#endif
}
