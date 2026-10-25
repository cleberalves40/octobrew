// SPDX-License-Identifier: GPL-2.0-or-later
//
// A janela principal, com o desenho do OctoPkg: a lista de pacotes, à esquerda
// os taps (no lugar dos grupos), a busca, e as abas embaixo. O trabalho com o
// brew roda fora da interface (QtConcurrent), com o núcleo do octobrew-tui.

#ifndef OCTOBREW_MAINWINDOW_H
#define OCTOBREW_MAINWINDOW_H

#include "brew.h"
#include "news.h"
#include "settings.h"
#include "transactiondialog.h"

#include <QFutureWatcher>
#include <QMainWindow>
#include <QMap>
#include <QPointer>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>
#include <atomic>
#include <functional>
#include <map>
#include <optional>

class PackageModel;
class QAction;
class QActionGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QTreeWidget;
class QTableWidget;
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
    QString curlPath;          // vazio: /usr/bin/curl, para o feed de notícias
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /** As abas, com as teclas do OctoPkg (⌥1 a ⌥6) e a dos serviços do Homebrew (⌥7). */
    enum Tab { InfoTab = 0, FilesTab, ActionsTab, OutputTab, NewsTab, HelpTab, ServicesTab };

    /** Confirma uma transação (o diálogo; os testes respondem sem abrir janela). */
    using ConfirmHandler = std::function<ConfirmAnswer(const ConfirmRequest &)>;
    /** Avisa o resultado ou um erro (uma caixa de mensagem; os testes guardam o texto). */
    using NoticeHandler = std::function<void(const QString &title, const QString &text, bool error)>;
    /** Escolhe um item de uma lista (QInputDialog::getItem); nullopt se cancelou. */
    using ChooseHandler = std::function<std::optional<int>(const QString &title, const QString &label, const QStringList &items)>;
    /** Pede um arquivo (o diálogo nativo); save: para gravar. Vazio se cancelou. */
    using FileHandler = std::function<QString(bool save, const QString &title, const QString &suggested)>;
    /** O tap a adicionar e se confia nele; nullopt se cancelou. */
    struct TapRequest {
        QString name;
        bool trust = false;
    };
    using TapHandler = std::function<std::optional<TapRequest>()>;

    explicit MainWindow(const WindowOptions &options, QWidget *parent = nullptr);
    ~MainWindow() override;

    /** Lista de novo (F5, ⌘R): os instalados e, na visão Repositório, o cache da API. */
    void refresh();
    void toggleView();

    // As ações do OctoPkg, com o brew.
    void markInstall();
    void markRemove();
    void clearMarks();
    void apply();
    void upgrade();
    void update();
    void cleanup();
    void autoremove();
    void pin(bool pin);
    /** Restaura um snapshot (brew bundle install --no-upgrade); file vazio: escolher na lista. */
    void restoreSnapshot(const QString &file = QString());

    // Fase 3: as notícias e o que só o Homebrew tem.
    void loadNews();
    void refreshNews();
    void loadServices();
    /** brew services start|stop|restart: o serviço selecionado na aba, ou o da fórmula selecionada. */
    void service(const QString &verb);
    void addTap();
    void removeTap();
    void doctor();
    void exportBrewfile();
    void importBrewfile();

    void setChooseHandler(ChooseHandler handler) { m_choose = std::move(handler); }
    void setFileHandler(FileHandler handler) { m_file = std::move(handler); }
    void setTapHandler(TapHandler handler) { m_tap = std::move(handler); }
    QTextBrowser *newsView() const { return m_newsView; }
    QTableWidget *servicesView() const { return m_servicesView; }
    const std::vector<octo::NewsItem> &news() const { return m_news.items; }

    void setConfirmHandler(ConfirmHandler handler) { m_confirm = std::move(handler); }
    void setNoticeHandler(NoticeHandler handler) { m_notice = std::move(handler); }
    QSet<QString> toInstall() const { return m_toInstall; }
    QSet<QString> toRemove() const { return m_toRemove; }
    bool inOperation() const { return m_inOperation; }
    QTreeWidget *actionsView() const { return m_actionsView; }
    /** Onde ficam os snapshots: ~/.config/octobrew/snapshots. */
    QString snapshotDir() const;

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
    void setTab(int tab);

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

    /** Uma operação do brew: recebe para onde mandar a saída. */
    using Work = std::function<void(const octo::OutputFn &)>;
    /** O resultado de um trabalho fora da interface, ou o erro dele. */
    template <typename R>
    struct Result {
        R value{};
        QString error;
    };

    void runOperation(const QString &status, const QString &command, Work work, std::function<void()> onSuccess);
    void finishOperation(const QString &error, const std::function<void()> &onSuccess);
    void appendOutput(const QString &chunk);
    ConfirmAnswer confirm(const ConfirmRequest &request);
    void notice(const QString &title, const QString &text, bool error = false);
    bool refuseWhileBusy();
    void updateMarks();
    std::vector<octo::Package> packagesFor(const QSet<QString> &keys) const;
    QString newSnapshotFile() const;
    /** O sudo pode pedir a senha pelo octobrew-askpass (o SUDO_ASKPASS está definido)? */
    bool askpassAvailable() const;

    std::optional<int> choose(const QString &title, const QString &label, const QStringList &items);
    QString askFile(bool save, const QString &title, const QString &suggested);
    std::optional<TapRequest> askTap();
    void applyNews(const octo::NewsResult &result);
    void renderNews();
    void renderServices();
    void setTabTitles();

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
    QTreeWidget *m_actionsView = nullptr;
    QTextBrowser *m_newsView = nullptr;
    QTableWidget *m_servicesView = nullptr;
    QProgressBar *m_progress = nullptr;
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

    QSet<QString> m_toInstall;                 // Package::key() dos marcados com +
    QSet<QString> m_toRemove;                  // e com -
    bool m_inOperation = false;                // uma operação do brew: a janela não fecha
    QString m_partial;                         // a linha da Saída ainda sem "\n"
    QString m_operationError;                  // fica no status até a próxima listagem limpa
    ConfirmHandler m_confirm;
    NoticeHandler m_notice;
    ChooseHandler m_choose;
    FileHandler m_file;
    TapHandler m_tap;

    octo::NewsFeed m_newsFeed;
    octo::NewsResult m_news;
    bool m_newsStarted = false;                // a aba já foi aberta
    bool m_newsBusy = false;
    bool m_newsUnseen = false;                 // notícias novas ainda não vistas: "*" na aba
    std::vector<octo::Service> m_services;
    bool m_servicesLoaded = false;
    bool m_servicesBusy = false;
    QString m_servicesError;
};

#endif
