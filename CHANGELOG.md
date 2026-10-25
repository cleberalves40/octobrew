# Changelog

Todas as mudanças relevantes do OctoBrew são registradas aqui.
O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto usa [versionamento semântico](https://semver.org/lang/pt-BR/),
com tags no formato `vX.Y.Z`.

## [0.1.0] - 2026-10-25

Primeira versão: somente leitura (Fases 0 e 1 do [roadmap](docs/ROADMAP.md)).

### Adicionado
- O núcleo do [octobrew-tui](https://github.com/cleberalves40/octobrew-tui) v1.0.1 num
  git subtree em `tui/`: o brew, o JSON, o cache da API, os estados, os tamanhos, i18n
  e o filtro e a ordem da lista, sem cópia e sem mudança.
- A janela, com o desenho do OctoPkg:
  - a lista Instalados/Repositório (Estado, Nome, Tipo, Versão, Tamanho, Instalado em),
    ordenada pelo clique no cabeçalho, sobre um `QAbstractTableModel` que não cria um
    item por célula (são ~17 mil pacotes com os taps de terceiros);
  - os ícones de estado do OctoPkg: o instalado de que outro depende, o de que ninguém
    depende (*unrequired*), o desatualizado, o mais novo que o repositório, o fixado
    (`brew pin`) e o não instalado, esmaecido;
  - à esquerda, os taps com as contagens, no lugar dos grupos;
  - a busca por nome ou por descrição (F7) e só as folhas (⇧⌘L);
  - as abas Informação (com os *caveats*, a página como link e o "Requerido por"),
    Arquivos (com a contagem no título), Saída e Ajuda, com os atalhos do OctoPkg (⌥1,
    ⌥2, ⌥4, ⌥6) e a Ajuda na notação do sistema (⌘R no macOS);
  - o espaço em disco medido em segundo plano, com "…" até chegar;
  - a barra de status do OctoPkg, com a mensagem, os pacotes e os desatualizados.
- O brew roda fora da interface (`QtConcurrent`); os detalhes são pedidos um instante
  depois da seleção, para não chamar o brew a cada seta, e ficam em cache.
- A ordem, o modo de busca, a geometria da janela e os divisores ficam em
  `~/.config/octobrew/config`.
- Textos em inglês, português e espanhol: o catálogo próprio da GUI, que cai no do
  núcleo para o resto.
- `OctoBrew.app` com o ícone do polvo do Octopi; `ci/bundle.sh` monta o bundle
  autocontido (macdeployqt), assina com uma assinatura *ad hoc* e o abre.
- Testes com QtTest, sem tela: a tabela, as páginas, a janela com o brew falso do
  octobrew-tui e a janela contra o Homebrew real, com uma captura da tela.
- CI no macOS (o Qt do Homebrew, a integração e o bundle aberto) e no Ubuntu (Qt 6.4,
  também com ASan e UBSan).
