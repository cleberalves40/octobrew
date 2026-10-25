# Changelog

Todas as mudanças relevantes do octobrew-tui são registradas aqui.
O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto usa [versionamento semântico](https://semver.org/lang/pt-BR/),
com tags no formato `vX.Y.Z`.

## [1.0.1] - 2026-10-25

Os atalhos na notação do teclado da Apple.

### Alterado
- No macOS, a Ajuda, os menus, a barra de teclas e as dicas da tela mostram os atalhos
  como os menus do macOS: <kbd>⌃</kbd> (Control), <kbd>⌥</kbd> (Option), <kbd>⇧</kbd>
  (Shift), <kbd>↩</kbd>, <kbd>⇥</kbd>, <kbd>⌦</kbd> (o delete para a frente, fn ⌫) e
  fn↑ fn↓ fn← fn→ para Page Up, Page Down, Home e End. Por exemplo, ⌥M aplica, ⌃K
  atualiza o Homebrew e ⌥H abre o menu Homebrew. O Insert, que o teclado da Apple não
  tem, sai dos textos. A barra de teclas mostra ⌥M no lugar do M-m.
- A nota da Ajuda sobre o teclado do Mac diz como ativar o ⌥ (Option) como Meta no
  terminal, e só aparece no macOS.
- No Linux, nada muda: os nomes do teclado de PC (Alt+M, Ctrl+L, M-m).
- As teclas em si não mudaram, e o ⌘ continua sem uso: o terminal não o repassa ao
  programa.
- O README e o `docs/KEYMAP.md` usam a mesma notação, com uma tabela de equivalência
  para o Linux.

### Corrigido
- Num terminal de 80 colunas em português (ou com a contagem de muitos arquivos), a
  barra de abas ainda passava da largura sem os ícones, e a aba Serviços era cortada.
  Agora as abas perdem o nome uma a uma, da última para a primeira, até caber, ficando
  o número e a contagem; a aba atual sempre tem o nome.
- Na aba Ajuda, uma descrição longa (a das folhas, em português) encolhia a coluna das
  teclas e desalinhava a linha; agora a descrição é cortada.

## [1.0.0] - 2026-10-25

Distribuição (Fase 5 do [roadmap](docs/ROADMAP.md)): o octobrew-tui se instala com o
próprio Homebrew, e a interface e as teclas ficam estáveis.

### Adicionado
- Fórmula do Homebrew em `homebrew/octobrew-tui.rb`, para o tap
  [cleberalves40/homebrew-tap](https://github.com/cleberalves40/homebrew-tap):
  `brew install cleberalves40/tap/octobrew-tui`. Compila com o CMake e o Ninja a partir
  do código da tag e depende do `ftxui` do Homebrew; o `brew test` confere o
  `--version`, o `--help` e a saída de uma opção desconhecida.
- `ci/formula-sha.sh`: aponta a fórmula para uma versão publicada (o `url` e o `sha256`
  do tarball da tag) e, com `--check`, confere se ela bate. A fórmula acompanha a última
  tag publicada, como o port do octopkg-tui.
- CI: o job **Fórmula** (`ci/formula-test.sh`), que põe a fórmula num tap local e roda
  o `brew style`, o `brew audit --strict --online`, a instalação a partir do código, o
  `brew test`, o `brew linkage --test` e o programa instalado num pseudo-terminal; o
  `brew audit --new`, as regras de fórmula nova do homebrew-core, só informa (a
  notabilidade do repositório só é conferida dentro do core). O job do macOS
  confere se a fórmula não está à frente do `CMakeLists.txt` e se o `sha256` bate.
- `docs/RELEASING.md`: o processo de versão (um PR por versão contra o `main`, a tag, a
  fórmula, o tap) e o envio ao homebrew-core.
- Tags e releases v0.1.0 a v0.4.0, nos commits de merge do `main`.

## [0.4.0] - 2026-10-25

O que só o Homebrew tem (Fase 4 do [roadmap](docs/ROADMAP.md)): serviços, taps, folhas,
`brew doctor` e Brewfiles, no novo menu **Homebrew** (**Alt+H**).

### Adicionado
- Aba **Serviços** (**7** ou **Alt+7**): os serviços das fórmulas instaladas
  (`brew services list --json`), com o estado (iniciado, parado, agendado, erro, não
  está rodando), quem o iniciou, o código de saída e o plist. Ela é lida em segundo
  plano na primeira visita, com F5 e depois de cada ação, e o Ctrl+F busca nela.
  Homebrew → Iniciar, Parar e Reiniciar o serviço da fórmula selecionada
  (`brew services start|stop|restart`). A aba Informação mostra o serviço da fórmula.
- **Folhas** (**l**, ou o menu Busca): só as fórmulas instaladas de que nenhum outro
  instalado depende, como o `brew leaves`; o que dá para remover sem levar nada junto.
  O cabeçalho mostra "· leaves", e combina com o filtro e com o tap.
- **Taps:** Homebrew → Adicionar um tap… (`brew tap user/repo`, com a opção de
  `brew trust --tap`, sem a qual o brew 6+ não carrega os pacotes de um tap de
  terceiros) e Remover um tap… (`brew untap`), que antes diz quais pacotes do tap estão
  instalados, porque o brew recusa o untap com eles.
- A visão Repositório inclui os pacotes dos **taps de terceiros**, que o cache da API não
  tem (`brew tap-info --json --installed`: o nome e o tap; a versão e a descrição vêm
  dos detalhes). Se o tap-info falhar, ficam os da API, com o aviso na aba Saída.
- Homebrew → **Verificar o sistema** (`brew doctor`): "pronto para o brew" ou a lista
  dos avisos, com o texto completo na aba Saída. O doctor sai com 1 quando há avisos, e
  isso não é tratado como falha.
- Homebrew → **Exportar um Brewfile…** (`brew bundle dump`, pergunta antes de trocar um
  arquivo que existe) e **Instalar de um Brewfile…**: a prévia é o
  `brew bundle check --verbose` (o que falta), e a operação, o
  `brew bundle install --no-upgrade`; com casks e o sudo sem senha em cache, a tela é
  suspensa.
- Num terminal estreito (as sete abas passam de 80 colunas), as abas perdem os ícones e
  ficam os números e os nomes.
- Testes: os serviços, os taps (e os pacotes deles no repositório), o doctor, o bundle
  check, cada ação contra o brew falso, as folhas no modelo e na tela, as abas em 80
  colunas e, na integração, os serviços, os taps, o doctor e o bundle check do brew real
  (só leitura).

## [0.3.0] - 2026-10-25

A aba Notícias (Fase 3 do [roadmap](docs/ROADMAP.md)): agora todas as abas do OctoPkg
estão no octobrew-tui.

### Adicionado
- Aba **Notícias** (**5** ou **Alt+5**), como no OctoPkg e no octopkg-tui: as 10
  últimas notícias do blog do Homebrew (`https://brew.sh/atom.xml`), com título, data,
  link e o resumo. **Ctrl+G** ou Arquivo → Buscar as últimas notícias baixa de novo.
- Leitor de **Atom** (o feed do brew.sh, do Jekyll), além do RSS 2.0 do octopkg-tui:
  o link `alternate`, a data de `published` (`2026-09-13`), o `summary` (ou o começo do
  `content`) como descrição, cortado em 600 caracteres, e "Homebrew 7.0.0" para os
  posts de versão, cujo título é só o número.
- O feed é baixado com o `/usr/bin/curl` do macOS, só por https e com o certificado
  conferido (o OctoPkg usa `curl -k`, que não confere), e fica em
  `~/.config/octobrew-tui/news.xml`: na primeira visita à aba, as notícias salvas
  aparecem na hora e as novas chegam em segundo plano. Sem rede, ou com um feed que não
  dá para ler, ficam as salvas, com o aviso. Notícias novas marcam a aba com `*`.
- **Ctrl+F** busca também na aba Notícias.
- Testes: o Atom do brew.sh e o RSS, o corte das descrições, o download com um curl
  falso (argumentos, cache, mudança, falha e feed inválido), a aba dirigida por teclas
  e, na integração, o download do feed de verdade.

## [0.2.0] - 2026-10-25

Transações: marcar, instalar, remover, atualizar, limpar e fixar, como no OctoPkg
(Fase 2 do [roadmap](docs/ROADMAP.md)).

### Adicionado
- Marcas do OctoPkg: **+** (ou Insert) para instalar e **-** (ou Delete) para remover;
  de novo, desmarca. Um instalado só é marcado para instalar se está desatualizado (o
  `brew install` o atualiza); pacotes fixados não são marcados; o `ftxui`, de que o
  próprio programa depende, e o `octobrew-tui` não podem ser removidos.
- Aba **Ações** (3), com os pacotes a instalar e a remover; **Ctrl+E** desfaz as marcas;
  a barra de status mostra quantos estão marcados.
- **Alt+M** aplica. Antes, a prévia: `brew install --dry-run` (fórmulas e casks em
  chamadas separadas, que o brew não aceita juntos) e, para a remoção, os instalados
  que dependem dos marcados, direta ou indiretamente. O brew não remove o que outros
  usam, então eles vão juntos, como no `pkg remove -R` do OctoPkg, e o diálogo vira
  **Aviso**, em vermelho, com o foco em Cancelar. Um dependente fixado ou protegido
  impede a remoção. Depois, `brew uninstall` (os casks primeiro) e `brew install`.
- **Ctrl+U** instala as atualizações (`brew upgrade`), com a prévia do
  `brew upgrade --dry-run`; os fixados ficam de fora, e os avisos do brew (casks
  desativados) vão para a aba Saída. **Ctrl+K** atualiza o Homebrew e a lista
  (`brew update`).
- Arquivo → Limpar o cache local (`brew cleanup --prune=all`), com a prévia do que sai
  e do espaço liberado; Arquivo → Remover dependências desnecessárias
  (`brew autoremove`), com a lista; Ações → Fixar e Liberar (`brew pin`/`unpin`, para
  fórmulas e casks).
- **Snapshot em Brewfile** antes da transação, no lugar do Boot Environment do
  OctoPkg: `brew bundle dump` em `~/.config/octobrew-tui/snapshots/AAMMDD-hhmmss.Brewfile`,
  marcado ao instalar e atualizar e desmarcado ao remover. Sem shell: se o snapshot
  falhar, nada muda. Arquivo → Restaurar um snapshot… (`brew bundle install
  --no-upgrade`) instala o que falta dele.
- Casks que pedem o **sudo** (instaladores `.pkg`, passos de sistema na desinstalação)
  são avisados no diálogo. Se o sudo não tiver a senha em cache, a tela é suspensa
  enquanto ele a pede, como o doas no octopkg-tui; no resto, a operação roda em
  segundo plano, com a saída ao vivo na aba Saída.
- Menu **Ações** (**Alt+C**); a barra de teclas mostra **+/-** e **M-m**.
- Testes: a leitura das prévias do brew 7 (install, upgrade, autoremove, cleanup), os
  dependentes, os casks que pedem o sudo, os snapshots, cada operação contra o brew
  falso (argumentos e ambiente), as marcas, os diálogos e os menus dirigidos por
  teclas, e as prévias contra o brew real. Com `--transactions`
  (`tests/macos-e2e.sh`, no CI), a integração instala, fixa e remove o figlet.

### Alterado
- As operações rodam sem auto-update (fazem o que a prévia mostrou), sem a pergunta
  do brew 7 (`--no-ask`, `HOMEBREW_NO_ASK`) e sem o autoremove automático do
  `uninstall` e do `cleanup` (`HOMEBREW_NO_AUTOREMOVE`), que levaria pacotes que não
  estavam na prévia. Sem o terminal, um sudo que precise de senha falha na hora
  (`SUDO_ASKPASS=/usr/bin/false`) em vez de disputar a tela.
- Sair (q, F10, Ctrl+Q) é recusado durante uma operação.

## [0.1.0] - 2026-10-25

Primeira versão: somente leitura (Fases 0 e 1 do [roadmap](docs/ROADMAP.md)).

### Adicionado
- Repositório a partir do octopkg-tui v0.7.2. Sem mudança: a execução de processos
  sem shell, o i18n (en_US, pt_BR, es_ES), as preferências, os 15 temas, os ícones,
  os diálogos, os menus, a busca nas abas (Ctrl+F), a busca por descrição (F7) e as
  disposições do F11.
- Núcleo do Homebrew (`src/core/brew.*`), no lugar do `pkg.*`:
  - instalados por `brew info --json=v2 --installed`: fórmulas e casks numa chamada,
    com os desatualizados, os fixados (`brew pin`, que fazem as vezes do bloqueio do
    pkg), as dependências (`installed_on_request`) e quantos instalados dependem de
    cada um;
  - repositório lido direto do cache da API do Homebrew, sem rede e sem Ruby: o
    `packages.<tag>.jws.json` do brew 5 ou mais novo e o `formula.jws.json` e o
    `cask.jws.json` do brew 4. São cerca de 16 mil pacotes em 0,3 s;
  - detalhes por `brew info --json=v2 --formula|--cask`, e arquivos por `brew list`;
  - todas as consultas com `HOMEBREW_NO_AUTO_UPDATE=1`, sem cores nem emojis: uma
    consulta nunca vira um `brew update`.
- Leitor de JSON próprio (`src/core/json.*`), em C++17 puro, como o resto do núcleo.
- Coluna **Tipo** (fórmula ou cask). Fórmula e cask de mesmo nome são linhas
  diferentes, e a seleção os distingue.
- Aba Informação com o tap, a página, a licença, o keg-only, a atualização automática
  dos casks, os avisos de obsoleto ou desativado, as dependências de execução e de
  compilação, o "Requerido por", os conflitos, os artifacts dos casks e os
  **caveats**, quebrados na largura da aba.
- Tamanho em disco calculado em segundo plano depois da lista. Para as fórmulas, todas
  as versões no Cellar; para os casks, o Caskroom e o aplicativo em `/Applications`.
  Uma nova listagem ou a saída interrompem o cálculo.
- **F8** filtra por tap (homebrew/core, homebrew/cask, taps de terceiros), no lugar da
  categoria do port.
- Alternativas às teclas F, que no Mac pedem o fn: `?` (F1), `s` (F6), `Alt+D` (F7) e
  `t` (F8).
- Testes: `test_core` (JSON, brew 7, cache da API, brew falso, tamanhos),
  `test_model`, `test_view` (inclusive o caminho completo com o brew falso),
  `test_integration` (Homebrew real, só leitura) e `tests/tui-smoke.sh`.
- CI no macOS (Homebrew do runner) e no Ubuntu (FTXUI 6.1.9, ASan e UBSan).

### Ainda não
- Abas Ações (3) e Notícias (5); marcar, instalar, remover, atualizar e fixar: Fases 2
  e 3.
