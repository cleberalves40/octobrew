// SPDX-License-Identifier: GPL-2.0-or-later
//
// O diálogo de transação do OctoPkg (transactiondialog.ui): o título, o
// resumo, a opção do snapshot em Brewfile (no lugar do Boot Environment) e a
// lista do que muda. Destrutivo (a remoção leva dependentes): o ícone de aviso
// e o "Não" como padrão.

#ifndef OCTOBREW_TRANSACTIONDIALOG_H
#define OCTOBREW_TRANSACTIONDIALOG_H

#include <QDialog>

class QCheckBox;
class QLabel;
class QPushButton;
class QTextBrowser;

/** O que o diálogo mostra: a MainWindow monta e os testes conferem. */
struct ConfirmRequest {
    QString title;            // "Confirmation", "Warning", "System upgrade"…
    QString text;             // a pergunta
    QString details;          // a lista do que muda, em HTML
    bool danger = false;      // Aviso: o "Não" como padrão
    bool snapshot = false;    // oferece o snapshot antes
    bool snapshotChecked = false;
    QString snapshotFile;     // onde ele vai parar
};

/** A resposta: aceito, e com ou sem o snapshot. */
struct ConfirmAnswer {
    bool accepted = false;
    bool snapshot = false;
};

class TransactionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TransactionDialog(const ConfirmRequest &request, QWidget *parent = nullptr);
    ConfirmAnswer answer() const;

protected:
    /** O foco no botão padrão ao abrir (no construtor, o Qt o daria ao primeiro campo). */
    void showEvent(QShowEvent *event) override;

private:
    QCheckBox *m_snapshot = nullptr;
    QPushButton *m_default = nullptr;
};

#endif
