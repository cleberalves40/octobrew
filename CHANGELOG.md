# Changelog

Todas as mudanças relevantes do OctoBrew são registradas aqui.
O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto usa [versionamento semântico](https://semver.org/lang/pt-BR/),
com tags no formato `vX.Y.Z`.

## [0.2.0] - 2026-10-25

Transações: marcar, instalar, remover, atualizar, limpar e fixar, como no OctoPkg
(Fase 2 do [roadmap](docs/ROADMAP.md)).

### Adicionado
- Marcas do OctoPkg na lista: **+** (ou Insert) para instalar e **-** (ou Delete) para
  remover, com os ícones de instalar e de remover no lugar do estado. Valem com o foco na
  lista, para não roubar o + e o - do campo de busca. As regras são as do octobrew-tui:
  um instalado só é marcado para instalar se está desatualizado; fixados não são
  marcados; o `ftxui` e o `octobrew-tui` não podem sair.
- Aba **Ações** (⌥3), com os pacotes a instalar e a remover e a contagem no título;
  ⌘E desfaz as marcas.
- **Aplicar** (⌥M no macOS, onde o ⌘M é minimizar; Ctrl+M no Linux, como no OctoPkg): a
  prévia do `brew install --dry-run`, os dependentes que saem junto (diálogo de **Aviso**,
  com o "Não" como padrão) e o diálogo de transação do OctoPkg, com a opção do
  **snapshot em Brewfile** no lugar do Boot Environment (marcada ao instalar, desmarcada
  ao remover). Depois, o snapshot, o `brew uninstall` e o `brew install`, sem shell.
- **Instalar as atualizações** (⌘U, `brew upgrade` com a prévia), **Atualizar o Homebrew**
  (⌘K), **Limpar o cache local** (com o espaço liberado), **Remover dependências
  desnecessárias** (`brew autoremove`), **Fixar** e **Liberar** (`brew pin`/`unpin`) e
  **Restaurar um snapshot…** (`brew bundle install --no-upgrade`).
- **octobrew-askpass**: o diálogo de senha que o sudo chama pelo `SUDO_ASKPASS` (o
  `sudo -A` do brew), no lugar do qt-sudo do OctoPkg, para os casks com instaladores
  `.pkg`. Fica dentro do bundle (`Contents/MacOS`); a senha vai para o sudo e não é
  guardada. Sem ele, um sudo que precise de senha falha na hora, com a dica do `sudo -v`.
- A saída do brew ao vivo na aba Saída, com a barra de progresso; durante uma operação,
  as outras ficam desativadas e a janela não fecha.
- Testes: `tst_transactions` (as marcas, a aba Ações, cada diálogo, as operações na
  ordem e com o ambiente certos, o snapshot, as falhas, o askpass e o diálogo de senha),
  com o brew falso do octobrew-tui.

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
