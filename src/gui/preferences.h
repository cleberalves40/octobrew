// SPDX-License-Identifier: GPL-2.0-or-later
//
// As preferências (⌘, no macOS): o idioma, a aparência (a do sistema, clara ou
// escura) e o notificador da barra de menus. Ficam no mesmo arquivo da janela,
// ~/.config/octobrew/config.

#ifndef OCTOBREW_PREFERENCES_H
#define OCTOBREW_PREFERENCES_H

#include "settings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QSpinBox;

namespace prefs {

/** $XDG_CONFIG_HOME/octobrew/config, ou ~/.config/octobrew/config. */
QString defaultSettingsPath();

/** "system", "en_US", "pt_BR" ou "es_ES" (a chave "language"). */
QString language(const octo::Settings &s);
/** O idioma das preferências no i18n (o "system" segue o ambiente). */
void applyLanguage(const octo::Settings &s);
/** "system", "light" ou "dark" (a chave "appearance"); no Qt anterior ao 6.8, só "system". */
QString appearance(const octo::Settings &s);
void applyAppearance(const QString &appearance);

/** De quantas em quantas horas o notificador verifica (1 a 24; 6 por padrão). */
int notifierHours(const octo::Settings &s);
/** O notificador roda o brew update antes de verificar (sim por padrão, como o octopi-notifier). */
bool notifierUpdates(const octo::Settings &s);

} // namespace prefs

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    /** notifierProgram: o octobrew-notifier, para o LaunchAgent; vazio desliga a opção. */
    PreferencesDialog(octo::Settings &settings, const QString &notifierProgram, QWidget *parent = nullptr);

    // Os campos, para os testes.
    QComboBox *languageBox() const { return m_language; }
    QComboBox *appearanceBox() const { return m_appearance; }
    QCheckBox *loginBox() const { return m_login; }
    QSpinBox *hoursBox() const { return m_hours; }
    QCheckBox *updateBox() const { return m_update; }

    /** Grava as preferências, aplica a aparência e instala ou tira o LaunchAgent. */
    bool save(QString *error = nullptr);

private:
    octo::Settings &m_settings;
    QString m_notifier;
    QComboBox *m_language = nullptr;
    QComboBox *m_appearance = nullptr;
    QCheckBox *m_login = nullptr;
    QSpinBox *m_hours = nullptr;
    QCheckBox *m_update = nullptr;
};

#endif
