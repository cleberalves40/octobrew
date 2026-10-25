# Changelog

Todas as mudanças relevantes do OctoBrew são registradas aqui.
O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto usa [versionamento semântico](https://semver.org/lang/pt-BR/),
com tags no formato `vX.Y.Z`.

## [1.0.0] - 2026-10-25

Distribuição pela fórmula (Fase 5 do [roadmap](docs/ROADMAP.md)): o OctoBrew se instala
com o próprio Homebrew, e a interface fica estável.

### Adicionado
- Fórmula `homebrew/octobrew.rb`, para o tap
  [cleberalves40/homebrew-tap](https://github.com/cleberalves40/homebrew-tap):
  `brew install cleberalves40/tap/octobrew`. Compila com o CMake e o Ninja a partir do
  código da tag e depende só do **`qtbase`** (o Homebrew divide o Qt). No macOS, o
  `OctoBrew.app` (com o octobrew-askpass e o octobrew-notifier dentro) fica no prefixo, o
  comando `octobrew` o abre, e os *caveats* mostram o `ln -s` para o `/Applications`. O
  `brew test` confere o `--version`, o `--help`, o octobrew-askpass e o octobrew-notifier,
  sem tela.
- `ci/formula-sha.sh`: aponta a fórmula para uma tag publicada (o `url` e o `sha256`) e,
  com `--check`, confere. A fórmula acompanha a última tag, como no octobrew-tui.
- CI: o job **Fórmula** (`ci/formula-test.sh`) põe a fórmula num tap local e roda o
  `brew style`, o `brew audit --strict --online`, a instalação a partir do código, o
  `brew test`, o `brew linkage --test` e o OctoBrew instalado, aberto de verdade. O job do
  macOS confere se a fórmula não está à frente do `CMakeLists.txt` e se o `sha256` bate.
- `docs/RELEASING.md`: a fórmula, o tap e por que não o homebrew-core (que não aceita
  aplicativos gráficos) nem um cask (que precisa da notarização da Apple), por enquanto.

### Corrigido
- O `octobrew-notifier --version` (e o `--help`) caía sem o servidor de janelas (o sandbox
  do `brew test`, ou por SSH): ele chamava o AppKit, para sair do Dock, antes de ler as
  opções. Agora as opções vêm antes, e o AppKit só com a tela de verdade (o plugin cocoa).

## [0.4.0] - 2026-10-25

O macOS de verdade (Fase 4 do [roadmap](docs/ROADMAP.md)).

### Adicionado
- **octobrew-notifier**, o notificador da barra de menus, como o octopi-notifier:
  - o polvo do Octopi em verde (em dia), amarelo (há atualizações) ou vermelho (o brew
    falhou), e um menu com a contagem ("3 atualizações disponíveis"), os pacotes e as
    versões, **Instalar as atualizações** (abre o OctoBrew direto no diálogo do
    upgrade), **Abrir o OctoBrew**, **Procurar atualizações agora** e Sair;
  - verifica ao abrir e de tempos em tempos (6 h por padrão), com o `brew update` antes,
    como o octopi-notifier, ou sem ele, pela preferência; os fixados não contam;
  - fica fora do Dock e do ⌘⇥ (só na barra de menus) e pode iniciar com o sistema, por
    um LaunchAgent em `~/Library/LaunchAgents/io.github.cleberalves40.octobrew.notifier.plist`;
  - mora dentro do bundle, ao lado do OctoBrew e do octobrew-askpass.
- **Preferências** (⌘,, no menu OctoBrew do macOS): o idioma (vale ao abrir de novo), a
  **aparência** (a do sistema, clara ou escura; Qt 6.8+) e o notificador (iniciar com o
  sistema, de quantas em quantas horas e o `brew update` antes).
- A **contagem no ícone do Dock**: os pacotes que dá para atualizar (Qt 6.5+).
- O menu **Janela** do macOS, com **Minimizar** (⌘M, livre porque o Aplicar ficou no ⌥M) e
  Zoom.
- `--upgrade`: abre o OctoBrew já no Instalar as atualizações (o que o notificador usa).
- As mensagens (resultados e erros) ficam presas à janela, como os diálogos de transação:
  no macOS, *sheets*.
- Testes: `tst_macos` (o resumo e o menu do notificador, a verificação com o brew falso,
  o LaunchAgent, as Preferências, a contagem do Dock e o `--upgrade`), com o `HOME` num
  diretório temporário, para nada ir ao `~/Library` de verdade.

## [0.3.0] - 2026-10-25

Notícias, Serviços e o menu Homebrew (Fase 3 do [roadmap](docs/ROADMAP.md)).

### Adicionado
- Aba **Notícias** (⌥5): o blog do Homebrew (`brew.sh/atom.xml`), com o título como link,
  a data e o resumo. Baixa na primeira visita, com as salvas na hora; ⌘G ou Arquivo →
  Buscar as últimas notícias baixa de novo. O cache fica em `~/.config/octobrew/news.xml`;
  sem rede, ficam as salvas com o aviso, e as novas marcam a aba com `*`.
- Aba **Serviços** (⌥7): a tabela do `brew services list`, com o estado (em verde o
  iniciado, em vermelho o erro), o usuário, o código de saída e o plist, e os botões
  Iniciar, Parar e Reiniciar. Na aba, valem para a linha selecionada; fora dela, para a
  fórmula selecionada na lista. A aba Informação mostra o serviço da fórmula.
- Menu **Homebrew**: Serviços; Iniciar, Parar e Reiniciar o serviço; Adicionar um tap…
  (com a opção do `brew trust --tap`, que o brew 6+ exige dos taps de terceiros) e
  Remover um tap… (que antes diz quais pacotes do tap estão instalados); Exportar um
  Brewfile… e Instalar de um Brewfile… (com o diálogo de arquivo nativo; a prévia é o
  `brew bundle check`); e Verificar o sistema (`brew doctor`, com os avisos num diálogo e o
  texto completo na aba Saída).
- `tests/check-translations.py`, no `ctest`: todo texto da GUI tem o português e o espanhol.
- Testes: `tst_homebrew` (as duas abas, os serviços, os taps, o doctor e os Brewfiles,
  com o brew e o curl falsos) e, na integração, o feed de verdade e os serviços do
  Homebrew real, com as capturas das abas.

### Corrigido
- O diálogo de senha do sudo (`octobrew-askpass`) aparecia em inglês mesmo em português:
  dois textos dele não estavam no catálogo. A nova verificação das traduções pegou.

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
