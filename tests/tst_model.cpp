// SPDX-License-Identifier: GPL-2.0-or-later
//
// A tabela de pacotes, as páginas das abas e o catálogo da GUI, sem janela,
// com as amostras do brew 7 dos testes do octobrew-tui.

#include "fixtures.h"
#include "i18n.h"
#include "packagemodel.h"
#include "pages.h"
#include "strings.h"

#include <QtTest>

class TstModel : public QObject
{
    Q_OBJECT

private:
    static QString name(const PackageModel &m, int row) { return m.data(m.index(row, model::Name)).toString(); }

private slots:
    void init() { i18n::setLocale("en_US"); }

    void rowsSortAndFilter()
    {
        PackageModel m;
        m.sort(model::Name, Qt::AscendingOrder);
        m.setPackages(octo::parseInstalled(kInstalledJson));
        QCOMPARE(m.rowCount(), 6);
        QCOMPARE(m.columnCount(), int(model::ColumnCount));
        QCOMPARE(name(m, 0), QStringLiteral("alacritty"));
        m.sort(model::Name, Qt::DescendingOrder);
        QCOMPARE(name(m, 0), QStringLiteral("wget"));
        // Filtro por nome e por descrição, como no octobrew-tui.
        m.setFilter(QStringLiteral("cur"), model::SearchMode::Name, QString(), false);
        QCOMPARE(m.rowCount(), 1);
        m.setFilter(QStringLiteral("terminal"), model::SearchMode::Name, QString(), false);
        QCOMPARE(m.rowCount(), 0);
        m.setFilter(QStringLiteral("terminal"), model::SearchMode::Description, QString(), false);
        QCOMPARE(m.rowCount(), 1);
        // Tap e folhas.
        m.setFilter(QString(), model::SearchMode::Name, QStringLiteral("homebrew/cask"), false);
        QCOMPARE(m.rowCount(), 2);
        m.setFilter(QString(), model::SearchMode::Name, QString(), true);
        QCOMPARE(m.rowCount(), 3);   // curl, wget e foo
        QCOMPARE(m.rowOf(QStringLiteral("f:brotli")), -1);
        QVERIFY(m.rowOf(QStringLiteral("f:curl")) >= 0);
        QCOMPARE(m.outdatedCount(), 3);
    }

    void cells()
    {
        PackageModel m;
        m.sort(model::Name, Qt::AscendingOrder);
        auto list = octo::parseInstalled(kInstalledJson);
        list[0].size = 4 * 1024 * 1024;   // curl
        m.setPackages(list);
        const int curl = m.rowOf(QStringLiteral("f:curl"));
        QCOMPARE(m.data(m.index(curl, model::Type)).toString(), QStringLiteral("Formula"));
        QCOMPARE(m.data(m.index(curl, model::Version)).toString(), QStringLiteral("8.9.1"));
        QCOMPARE(m.data(m.index(curl, model::Size)).toString(), QStringLiteral("4.00 MiB"));
        QCOMPARE(m.data(m.index(curl, model::StateIcon), Qt::ToolTipRole).toString(),
                 QStringLiteral("Outdated (version 8.10.0 is available)"));
        QVERIFY(!m.data(m.index(curl, model::StateIcon), Qt::DecorationRole).value<QIcon>().isNull());
        // Sem o tamanho ainda: "…" enquanto calcula.
        const int brotli = m.rowOf(QStringLiteral("f:brotli"));
        QCOMPARE(m.data(m.index(brotli, model::Size)).toString(), QString());
        m.setSizing(true);
        QCOMPARE(m.data(m.index(brotli, model::Size)).toString(), QStringLiteral("…"));
        QCOMPARE(m.headerData(model::Name, Qt::Horizontal).toString(), QStringLiteral("Name"));
        QCOMPARE(m.data(m.index(m.rowOf(QStringLiteral("c:docker")), model::Type)).toString(), QStringLiteral("Cask"));
    }

    void iconsLikeOctoPkg()
    {
        // O instalado de que outro depende tem o ícone "installed"; o que
        // ninguém usa, o "unrequired"; o fixado, o cadeado.
        const auto list = octo::parseInstalled(kInstalledJson);
        const auto find = [&](const std::string &key) {
            for (const auto &p : list)
                if (p.key() == key)
                    return p;
            return octo::Package{};
        };
        const qint64 brotli = PackageModel::iconFor(find("f:brotli")).cacheKey();   // 3 dependem dele
        const qint64 foo = PackageModel::iconFor(find("f:someone/tap/foo")).cacheKey();
        const qint64 wget = PackageModel::iconFor(find("f:wget")).cacheKey();
        const qint64 curl = PackageModel::iconFor(find("f:curl")).cacheKey();
        QVERIFY(brotli != foo);
        QVERIFY(wget != curl);
        QCOMPARE(PackageModel::iconFor(find("c:alacritty")).cacheKey(), foo);
    }

    void infoPage()
    {
        const auto list = octo::parseInstalled(kInstalledJson);
        const octo::Package curl = list[0];
        QString html = pages::infoHtml(curl, nullptr, QString());
        QVERIFY(html.contains(QStringLiteral("Loading…")));
        QVERIFY(html.contains(QStringLiteral("Outdated (version 8.10.0 is available)")));
        octo::Details d = octo::parseDetails(kCurlInfo);
        d.requiredBy = {"wget"};
        html = pages::infoHtml(curl, &d, QString());
        for (const char *t : {"<a href='https://curl.se'>", "provided by macos", "brotli, libnghttp2", "pkgconf",
                              "wget", "Caveats", "curl is keg-only", "Installed version"})
            QVERIFY2(html.contains(QString::fromUtf8(t)), t);
        // O HTML do brew é escapado.
        octo::Package evil = curl;
        evil.comment = "<script>x</script>";
        QVERIFY(!pages::infoHtml(evil, &d, QString()).contains(QStringLiteral("<script>")));
        QVERIFY(pages::infoHtml(curl, nullptr, QStringLiteral("boom")).contains(QStringLiteral("ERROR: boom")));
        const octo::Details kitty = octo::parseDetails(kKittyInfo);
        html = pages::infoHtml(octo::Package{}, &kitty, QString());
        QVERIFY(html.contains(QStringLiteral("App: kitty.app")));
        QVERIFY(html.contains(QStringLiteral("unmaintained")));
    }

    void portugueseAndCatalog()
    {
        i18n::setLocale("pt_BR");
        // O catálogo da GUI e, para o resto, o do núcleo.
        QCOMPARE(ui::tr("View"), QStringLiteral("Exibir"));
        QCOMPARE(ui::tr("Outdated"), QStringLiteral("Desatualizado"));
        QCOMPARE(ui::trf("%1 package(s)", {QStringLiteral("%2")}), QStringLiteral("%2 pacote(s)"));
        for (const QString &k : ui::catalogKeys()) {
            const QByteArray key = k.toUtf8();
            i18n::setLocale("pt_BR");
            const QString pt = ui::tr(key.constData());
            i18n::setLocale("es_ES");
            const QString es = ui::tr(key.constData());
            QVERIFY2(!pt.isEmpty() && !es.isEmpty(), key.constData());
        }
        i18n::setLocale("en_US");
    }
};

QTEST_MAIN(TstModel)
#include "tst_model.moc"
