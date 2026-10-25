// SPDX-License-Identifier: GPL-2.0-or-later

#include "preferences.h"
#include "i18n.h"
#include "notifier.h"
#include "texts.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QStyleHints>
#include <QVBoxLayout>

namespace prefs {

namespace {

const QStringList kLanguages{QStringLiteral("system"), QStringLiteral("en_US"), QStringLiteral("pt_BR"), QStringLiteral("es_ES")};
const QStringList kAppearances{QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")};

} // namespace

QString defaultSettingsPath()
{
    const QByteArray xdg = qgetenv("XDG_CONFIG_HOME");
    if (!xdg.isEmpty())
        return QString::fromLocal8Bit(xdg) + "/octobrew/config";
    return QDir::homePath() + "/.config/octobrew/config";
}

QString language(const octo::Settings &s)
{
    const QString v = QString::fromStdString(s.value("language", "system"));
    return kLanguages.contains(v) ? v : QStringLiteral("system");
}

void applyLanguage(const octo::Settings &s)
{
    const QString lang = language(s);
    if (lang == "system")
        i18n::initFromEnvironment();
    else
        i18n::setLocale(lang.toStdString());
}

QString appearance(const octo::Settings &s)
{
    const QString v = QString::fromStdString(s.value("appearance", "system"));
    return kAppearances.contains(v) ? v : QStringLiteral("system");
}

void applyAppearance(const QString &appearance)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Claro ou escuro, ou o do sistema (o modo escuro do macOS).
    QStyleHints *hints = QGuiApplication::styleHints();
    if (appearance == "light")
        hints->setColorScheme(Qt::ColorScheme::Light);
    else if (appearance == "dark")
        hints->setColorScheme(Qt::ColorScheme::Dark);
    else
        hints->unsetColorScheme();
#else
    Q_UNUSED(appearance)
#endif
}

int notifierHours(const octo::Settings &s)
{
    return qBound(1, s.intValue("notifier.hours", 6), 24);
}

bool notifierUpdates(const octo::Settings &s)
{
    return s.value("notifier.update", "yes") != "no";
}

} // namespace prefs

PreferencesDialog::PreferencesDialog(octo::Settings &settings, const QString &notifierProgram, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_notifier(notifierProgram)
{
    setWindowTitle(ui::tr("Preferences"));
    setWindowModality(Qt::WindowModal);

    m_language = new QComboBox;
    m_language->addItems({ui::tr("System default"), QStringLiteral("English"), QStringLiteral("Português (Brasil)"),
                          QStringLiteral("Español")});
    m_language->setCurrentIndex(static_cast<int>(prefs::kLanguages.indexOf(prefs::language(settings))));
    m_appearance = new QComboBox;
    m_appearance->addItems({ui::tr("System"), ui::tr("Light"), ui::tr("Dark")});
    m_appearance->setCurrentIndex(static_cast<int>(prefs::kAppearances.indexOf(prefs::appearance(settings))));
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    m_appearance->setEnabled(false);   // o Qt anterior ao 6.8 não troca o esquema de cores
#endif
    auto *general = new QFormLayout;
    general->addRow(ui::tr("Language:"), m_language);
    general->addRow(ui::tr("Appearance:"), m_appearance);
    auto *note = new QLabel(ui::tr("The language changes when OctoBrew starts again."));
    note->setStyleSheet(QStringLiteral("color: gray"));
    general->addRow(note);

    m_login = new QCheckBox(ui::tr("Start the notifier when you log in"));
    m_login->setChecked(notifier::startsAtLogin(QDir::homePath()));
    m_login->setEnabled(!notifierProgram.isEmpty());
    m_hours = new QSpinBox;
    m_hours->setRange(1, 24);
    m_hours->setSuffix(ui::tr(" h"));
    m_hours->setValue(prefs::notifierHours(settings));
    m_update = new QCheckBox(ui::tr("Update Homebrew before checking (brew update)"));
    m_update->setChecked(prefs::notifierUpdates(settings));
    auto *group = new QGroupBox(ui::tr("Notifier in the menu bar"));
    auto *notifierForm = new QFormLayout(group);
    notifierForm->addRow(m_login);
    notifierForm->addRow(ui::tr("Check every:"), m_hours);
    notifierForm->addRow(m_update);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(general);
    layout->addWidget(group);
    layout->addWidget(buttons);
}

bool PreferencesDialog::save(QString *error)
{
    m_settings.setValue("language", prefs::kLanguages.value(m_language->currentIndex(), QStringLiteral("system")).toStdString());
    const QString look = prefs::kAppearances.value(m_appearance->currentIndex(), QStringLiteral("system"));
    m_settings.setValue("appearance", look.toStdString());
    m_settings.setValue("notifier.hours", m_hours->value());
    m_settings.setValue("notifier.update", m_update->isChecked() ? "yes" : "no");
    prefs::applyAppearance(look);
    bool ok = m_settings.save();
    if (!ok && error)
        *error = ui::trf("Could not save %1", {QString::fromStdString(m_settings.path())});
    if (!m_notifier.isEmpty() && m_login->isChecked() != notifier::startsAtLogin(QDir::homePath())) {
        QString why;
        if (!notifier::setStartAtLogin(m_login->isChecked(), m_notifier, QDir::homePath(), &why)) {
            ok = false;
            if (error)
                *error = why;
        }
    }
    return ok;
}
