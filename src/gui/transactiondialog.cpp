// SPDX-License-Identifier: GPL-2.0-or-later

#include "transactiondialog.h"
#include "texts.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTextBrowser>
#include <QVBoxLayout>

TransactionDialog::TransactionDialog(const ConfirmRequest &r, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(r.title);
    setWindowModality(Qt::WindowModal);   // no macOS, uma "sheet" presa à janela
    auto *icon = new QLabel;
    const QStyle::StandardPixmap pixmap = r.danger ? QStyle::SP_MessageBoxWarning : QStyle::SP_MessageBoxQuestion;
    icon->setPixmap(style()->standardIcon(pixmap).pixmap(48, 48));
    icon->setAlignment(Qt::AlignTop);
    auto *text = new QLabel(QStringLiteral("<b>%1</b>").arg(r.text.toHtmlEscaped()));
    text->setWordWrap(true);
    auto *head = new QHBoxLayout;
    head->addWidget(icon);
    head->addWidget(text, 1);

    auto *details = new QTextBrowser;
    details->setHtml(r.details);
    details->setMinimumSize(520, 220);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(head);
    layout->addWidget(details, 1);
    if (r.snapshot) {
        m_snapshot = new QCheckBox(ui::tr("Save a snapshot (Brewfile) before the transaction"));
        m_snapshot->setChecked(r.snapshotChecked);
        m_snapshot->setToolTip(ui::trf("File: %1", {r.snapshotFile}));
        auto *file = new QLabel(ui::trf("File: %1", {r.snapshotFile}));
        file->setStyleSheet(QStringLiteral("color: gray"));
        file->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(m_snapshot);
        layout->addWidget(file);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No);
    buttons->button(QDialogButtonBox::Yes)->setText(ui::tr("Yes"));
    buttons->button(QDialogButtonBox::No)->setText(ui::tr("No"));
    // Como no OctoPkg: no Aviso, o padrão é não fazer nada. O Qt dá o padrão ao
    // botão com o foco (o primeiro), então o outro perde o autoDefault e o
    // escolhido recebe o foco.
    QPushButton *chosen = buttons->button(r.danger ? QDialogButtonBox::No : QDialogButtonBox::Yes);
    QPushButton *other = buttons->button(r.danger ? QDialogButtonBox::Yes : QDialogButtonBox::No);
    other->setAutoDefault(false);
    other->setDefault(false);
    chosen->setDefault(true);
    m_default = chosen;
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void TransactionDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_default->setFocus();
}

ConfirmAnswer TransactionDialog::answer() const
{
    return {result() == QDialog::Accepted, m_snapshot && m_snapshot->isChecked()};
}
