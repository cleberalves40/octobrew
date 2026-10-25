// SPDX-License-Identifier: GPL-2.0-or-later
//
// A janela principal, com o desenho do OctoPkg: a lista de pacotes, à esquerda
// os taps (no lugar dos grupos), a busca, e as abas embaixo. O trabalho com o
// brew roda fora da interface (QtConcurrent), com o núcleo do octobrew-tui.

#ifndef OCTOBREW_MAINWINDOW_H
#define OCTOBREW_MAINWINDOW_H

#include "brew.h"
#include "settings.h"

#include <QFutureWatcher>
#include <QMainWindow>
#include <QMap>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>
#include <atomic>
#include <functional>
#include <map>

class PackageModel;
class QAction;
class QActionGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QSplitter;
class QTabWidget;
class QTableView;
class QTextBrowser;
class QTimer;

struct WindowOptions {
    QString brewPath;          // vazio: o brew do Homebrew instalado
    QString settingsPath;      // vazio: ~/.config/octobrew/config
    bool remote = false;       // começa na visão Repositório
    bool synchronous = false;  // os testes: o trabalho roda na hora, sem threads
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /** As abas, com as teclas do OctoPkg (⌥1, ⌥2, ⌥4 e ⌥6; ⌥3 e ⌥5 nas próximas fases). */
    enum Tab { InfoTab = 0, FilesTab, OutputTab, HelpTab };

    explicit MainWindow(const WindowOptions &options, QWidget *parent = nullptr);
    ~MainWindow() override;

    /** Lista de novo (F5, ⌘R): os instalados e, na visão Repositório, o cache da API. */
    void refresh();
    void toggleView();

    // Para os testes.
    PackageModel *packageModel() const { return m_model; }
    QTableView *table() const { return m_table; }
    QListWidget *tapList() const { return m_taps; }
    QLineEdit *searchField() const { return m_search; }
    QTabWidget *tabs() const { return m_tabs; }
    QTextBrowser *infoView() const { return m_info; }
    QPlainTextEdit *filesView() const { return m_files; }
    QPlainTextEdit *outputView() const { return m_output; }
    QTextBrowser *helpView() const { return m_help; }
    QString statusText() const;
    QString countText() const;
    bool remoteView() const { return m_remoteView; }
    bool busy() const { return m_busy; }
    /** Seleciona o pacote de key (Package::key()); false se não estiver na lista. */
    bool selectKey(const QString &key);
    QString currentKey() const;
    QAction *action(const QString &name) const;
    /** Pede os detalhes do selecionado já (a tela espera um instante entre as setas). */
    void loadDetails();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    struct Listing {
        std::vector<octo::Package> installed;
        std::vector<octo::Package> remote;
        bool hasRemote = false;
        QString error;
        QString warning;
    };
    struct DetailsResult {
        octo::Details details;
        QString error;
    };

    void buildUi();
    void buildActions();
    void restoreState();
    void saveState();

    void applyListing(const Listing &l, int generation);
    void applySizes(const std::map<std::string, std::int64_t> &sizes, int generation);
    void rebuildView();
    void rebuildTaps();
    void applyFilter();
    void setSearchMode(bool description);
    void selectionChanged();
    void showDetails();
    void setTab(int tab);
    void about();
    void log(const QString &message);
    void setStatus(const QString &message, bool error = false);
    void updateCounts();
    QString detailsKey(const octo::Package &p) const;
    const octo::Package *current() const;

    /**
     * work fora da interface (QtConcurrent) e done nela, com o resultado; nos
     * testes (synchronous), os dois na hora.
     */
    template <typename R>
    void runAsync(std::function<R()> work, std::function<void(const R &)> done);

    WindowOptions m_options;
    octo::Settings m_settings;
    octo::Manager m_mgr;

    std::vector<octo::Package> m_installed;
    std::vector<octo::Package> m_remote;
    std::map<std::string, std::int64_t> m_sizes;
    bool m_hasRemote = false;
    bool m_remoteView = false;
    bool m_busy = false;
    bool m_sizing = false;
    int m_generation = 0;
    std::atomic<bool> m_cancelSizes{false};
    QString m_status;
    bool m_statusError = false;

    QMap<QString, octo::Details> m_details;    // por detailsKey()
    QMap<QString, QString> m_detailErrors;
    QString m_detailsPending;                  // detailsKey() pedido e ainda não chegado
    QTimer *m_detailsTimer = nullptr;

    PackageModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QListWidget *m_taps = nullptr;
    QLineEdit *m_search = nullptr;
    QComboBox *m_viewCombo = nullptr;
    QTabWidget *m_tabs = nullptr;
    QTextBrowser *m_info = nullptr;
    QPlainTextEdit *m_files = nullptr;
    QPlainTextEdit *m_output = nullptr;
    QTextBrowser *m_help = nullptr;
    QSplitter *m_vertical = nullptr;
    QSplitter *m_horizontal = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_outdatedLabel = nullptr;
    QLabel *m_sizingLabel = nullptr;
    QMap<QString, QAction *> m_actions;
    QActionGroup *m_searchGroup = nullptr;
    QList<QFutureWatcherBase *> m_pending;     // o trabalho em andamento, esperado ao fechar
};

#endif
