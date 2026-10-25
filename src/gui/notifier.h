// SPDX-License-Identifier: GPL-2.0-or-later
//
// O notificador da barra de menus, como o octopi-notifier: o polvo verde (em
// dia), amarelo (há atualizações) ou vermelho (o brew falhou), a contagem e um
// menu para abrir o OctoBrew ou instalar as atualizações. Verifica de tempos em
// tempos (o brew update, opcional, e os desatualizados) e pode iniciar com o
// sistema por um LaunchAgent.

#ifndef OCTOBREW_NOTIFIER_H
#define OCTOBREW_NOTIFIER_H

#include "brew.h"
#include "settings.h"

#include <QIcon>
#include <QObject>
#include <QStringList>
#include <atomic>

class QAction;
class QMenu;
class QSystemTrayIcon;
class QTimer;

namespace notifier {

enum class State { Checking, UpToDate, Outdated, Error };

struct Summary {
    State state = State::Checking;
    QStringList outdated;   // os nomes, com as versões: "curl 8.9.1 → 8.10.0"
    QString error;
};

/** O resumo dos instalados (o brew info --installed); os fixados não contam. */
Summary summarize(const std::vector<octo::Package> &installed);
/** O texto do ícone e da primeira linha do menu ("3 atualizações disponíveis"). */
QString title(const Summary &s);
/** O polvo do Octopi na cor do estado. */
QIcon icon(const Summary &s);

/** O rótulo do LaunchAgent: io.github.cleberalves40.octobrew.notifier. */
QString agentLabel();
/** ~/Library/LaunchAgents/<rótulo>.plist (home: o diretório pessoal). */
QString agentPath(const QString &home);
/** O plist que inicia program ao entrar no sistema (RunAtLoad). */
QString agentPlist(const QString &program);
/** Instala (enable) ou tira o LaunchAgent em home; o efeito é no próximo login. */
bool setStartAtLogin(bool enable, const QString &program, const QString &home, QString *error = nullptr);
bool startsAtLogin(const QString &home);

/** O octobrew-notifier ao lado do executável (no bundle, Contents/MacOS); vazio se não houver. */
QString notifierPath();
/** O OctoBrew (o bundle no macOS), para o "Abrir"; vazio se não houver. */
QString appPath();

} // namespace notifier

class Notifier : public QObject
{
    Q_OBJECT

public:
    struct Options {
        QString brewPath;
        QString settingsPath;
        bool synchronous = false;   // os testes: a verificação roda na hora
        bool showIcon = true;       // os testes: sem o ícone na barra de menus
    };
    explicit Notifier(const Options &options, QObject *parent = nullptr);
    ~Notifier() override;

    /** Verifica agora: o brew update (se a preferência mandar) e os desatualizados. */
    void check();
    const notifier::Summary &summary() const { return m_summary; }
    QMenu *menu() const { return m_menu; }
    /** O que o último "Abrir" ou "Instalar as atualizações" pediu ao OctoBrew (os testes). */
    QStringList lastLaunch() const { return m_lastLaunch; }
    /** No lugar do abrir o OctoBrew de verdade (os testes). */
    void setLauncher(std::function<void(const QStringList &args)> launcher) { m_launcher = std::move(launcher); }

private:
    void apply(const notifier::Summary &summary);
    void launch(const QStringList &args);

    Options m_options;
    octo::Settings m_settings;
    octo::Manager m_mgr;
    notifier::Summary m_summary;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_title = nullptr;
    QAction *m_upgrade = nullptr;
    QTimer *m_timer = nullptr;
    bool m_checking = false;
    QStringList m_lastLaunch;
    std::function<void(const QStringList &)> m_launcher;
};

#endif
