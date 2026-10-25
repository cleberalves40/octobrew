# octobrew-tui

[![Build & Test](https://github.com/cleberalves40/octobrew-tui/actions/workflows/build.yml/badge.svg)](https://github.com/cleberalves40/octobrew-tui/actions/workflows/build.yml)
[![License: GPL v2+](https://img.shields.io/badge/License-GPL%20v2%2B-blue.svg)](LICENSE)

Interface de terminal (TUI) para o [Homebrew](https://brew.sh), o gerenciador de pacotes
do macOS (e do Linux). É a irmã do [octopkg-tui](https://github.com/cleberalves40/octopkg-tui),
a interface de terminal do [OctoPkg](https://github.com/aarnt/octopkg) para o `pkg(8)` do
FreeBSD: as mesmas abas, os mesmos atalhos, os mesmos temas e ícones
[Nerd Fonts](https://www.nerdfonts.com/), agora com fórmulas e casks.

```
 OctoBrew — Homebrew [# Installed packages]                 / Filter by name…
────────────────────────────────────────────────────────────────────────────────────
   Name ^                       Type     Version                Size  Installed on
────────────────────────────────────────────────────────────────────────────────────
 I alacritty                    Cask     0.17.0            13.97 MiB  2026-04-14 22:13
 d brotli                       Formula  1.2.0              1.67 MiB  2026-10-19 09:12
 U curl                         Formula  8.9.1              4.72 MiB  2026-10-19 09:12
 P wget                         Formula  1.24.5             4.01 MiB  2026-09-02 15:40
────────────────────────────────────────────────────────────────────────────────────
 1 Info  2 Files  3 Actions  4 Output  5 News  6 Help  7 Services
curl  Get a file from an HTTP, HTTPS or FTP server
Status:             Outdated (version 8.10.0 is available)
Type:               Formula
Installed version:  8.9.1
Version:            8.10.0
Tap:                homebrew/core
────────────────────────────────────────────────────────────────────────────────────
Ready.                                   Theme: System  4 package(s)  2 outdated
F1Help  F5Refresh  F6Sort  F8Tap  F9Menu  F10Exit  +/-Mark  M-mApply
```

(com `--icons=ascii`: **I** instalado, **d** dependência, **U** desatualizado, **N** mais
novo que o repositório, **P** fixado com `brew pin`, **-** não instalado; **+** marcado
para instalar e **x** para remover.)

## Estado

**v1.0.0** (as Fases 1 a 5 do [roadmap](docs/ROADMAP.md)): todas as abas do OctoPkg, o
que só o Homebrew tem, e a instalação pelo próprio Homebrew. O programa:

- marca pacotes para instalar (**+**) e remover (**-**), mostra a prévia da transação e
  a executa (**⌥M**), com um snapshot em Brewfile antes;
- instala as atualizações (**⌃U**), atualiza o Homebrew (**⌃K**), limpa o cache,
  remove as dependências desnecessárias, fixa e libera pacotes e restaura um snapshot;
- lista as fórmulas e os casks instalados e os do repositório;
- mostra os detalhes do pacote selecionado, incluindo os *caveats*;
- mostra os arquivos de cada instalado e o espaço que ele ocupa em disco;
- busca por nome ou por descrição (**F7**) e filtra por tap (**F8**);
- ordena por qualquer coluna (**F6**) e maximiza a lista ou as abas (**F11**);
- tem os 15 temas e as Preferências do octopkg-tui;
- mostra as notícias do blog do Homebrew (aba **5**, **⌃G**);
- mostra os serviços (aba **7**) e os inicia, para e reinicia; adiciona e remove taps,
  com a confiança do brew 6+; filtra as folhas (**l**, como o `brew leaves`); roda o
  `brew doctor`; e exporta e instala de Brewfiles, tudo no menu **Homebrew** (**⌥H**).

Veja o [CHANGELOG](CHANGELOG.md).

## Instalação

```sh
brew install cleberalves40/tap/octobrew-tui
brew install --cask font-jetbrains-mono-nerd-font   # ícones; opcional
octobrew-tui
```

O nome completo (`cleberalves40/tap/…`) adiciona o tap e, no brew 6+, já confia nele. O
brew compila a partir do código, com o CMake e o Ninja, e o `brew upgrade` traz as
versões novas. O processo de versão está em [docs/RELEASING.md](docs/RELEASING.md).

## Requisitos (para compilar)

```sh
brew install ftxui cmake ninja
```

Para os ícones, configure o terminal com a fonte **JetBrainsMono Nerd Font Mono**. Sem
uma fonte Nerd, use `--icons=unicode` ou `--icons=ascii`.

Sem o Qt, sem X11 e sem Ruby próprio: só o FTXUI e o brew do sistema. Funciona também
por SSH.

## Compilação

```sh
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/octobrew-tui
```

Opções: `--remote` (começa na visão Repositório), `--icons=nerd|unicode|ascii`,
`--lang=en_US|pt_BR|es_ES`, `--theme=TEMA`, `--config=ARQUIVO`, `--brew=EXECUTÁVEL`,
`--version`. As preferências ficam em `~/.config/octobrew-tui/config`: a ordem da tabela,
o modo de busca, o tema, o idioma e os ícones (pelo diálogo de Preferências, **⌥,**).

## Como ele lê o Homebrew

| O quê | De onde | Custo |
|:---|:---|:---|
| Instalados (fórmulas e casks, com desatualizados, pins e dependências) | `brew info --json=v2 --installed`, uma chamada | ~2 s (o Ruby do brew) |
| Repositório (~16 mil fórmulas e casks) | o cache da API do Homebrew, lido direto (`$(brew --cache)/api/internal/packages.*.jws.json`; no brew 4, `formula.jws.json` e `cask.jws.json`), sem rede e sem Ruby | ~0,3 s |
| Notícias (aba 5) | `https://brew.sh/atom.xml`, com o `/usr/bin/curl` (só https, certificado conferido), em cache em `~/.config/octobrew-tui/news.xml` | na primeira visita e no ⌃G, em segundo plano |
| Pacotes dos taps de terceiros (visão Repositório) | `brew tap-info --json --installed`: o nome e o tap | ~1,5 s, junto da lista do repositório |
| Serviços (aba 7) | `brew services list --json` | ~2 s, na primeira visita, no F5 e depois de cada ação |
| Detalhes do selecionado | `brew info --json=v2 --formula\|--cask NOME` | ~0,5–1 s, em segundo plano e em cache |
| Arquivos (aba 2) | `brew list --formula --verbose` / `brew list --cask` | idem |
| Tamanho em disco | o Cellar (todas as versões) e o Caskroom, mais o aplicativo em `/Applications`, medidos pelo próprio programa | segundo plano, depois da lista |

Todas as consultas rodam com `HOMEBREW_NO_AUTO_UPDATE=1`, `HOMEBREW_NO_ENV_HINTS=1`,
`HOMEBREW_NO_COLOR=1` e `HOMEBREW_NO_EMOJI=1`, sem shell. Uma consulta nunca vira um
`brew update` nem baixa a API. Se o cache da API não existir (por exemplo, com
`HOMEBREW_NO_INSTALL_FROM_API`), a visão Repositório avisa e pede um `brew update`.

Fórmulas que saíram do homebrew-core continuam na lista de instalados, como pacotes
que só existem localmente. O mesmo vale para as fórmulas de taps de terceiros, que a API
não tem. Fórmula e cask de mesmo nome (`docker`) são duas linhas.

O Homebrew não roda como root, e o octobrew-tui também não.

## Transações

| Ação | Prévia (não muda nada) | Operação |
|:---|:---|:---|
| ⌥M: instalar os marcados com + | `brew install --dry-run --formula …` / `--cask …` | `brew install --no-ask --formula …` / `--cask …` |
| ⌥M: remover os marcados com − | os dependentes, dos recibos do brew já lidos | `brew uninstall --cask …`, depois `--formula …` |
| ⌃U: instalar as atualizações | `brew upgrade --dry-run` | `brew upgrade --no-ask` |
| ⌃K: atualizar o Homebrew | — | `brew update` |
| Limpar o cache local | `brew cleanup --dry-run --prune=all` | `brew cleanup --prune=all` |
| Remover dependências desnecessárias | `brew autoremove --dry-run` | `brew autoremove` |
| Fixar / Liberar | — | `brew pin` / `brew unpin` |
| Restaurar um snapshot | — | `brew bundle install --file=… --no-upgrade` |
| Instalar de um Brewfile | `brew bundle check --verbose` (o que falta) | `brew bundle install --file=… --no-upgrade` |
| Exportar um Brewfile | — | `brew bundle dump --file=… --force` (pergunta antes de trocar) |
| Iniciar / parar / reiniciar o serviço | — | `brew services start\|stop\|restart nome` |
| Adicionar / remover um tap | — | `brew tap user/repo` (e `brew trust --tap`) / `brew untap` |
| Verificar o sistema | `brew doctor` (só lê) | — |

- **Dependentes:** o brew não remove o que outros instalados usam. Os que dependem dos
  marcados, direta ou indiretamente, vão juntos, como no `pkg remove -R` do OctoPkg, e o
  diálogo vira **Aviso**, com o foco em Cancelar. Um dependente fixado impede a remoção.
- **Snapshot:** antes da transação, o diálogo oferece um `brew bundle dump` em
  `~/.config/octobrew-tui/snapshots/AAMMDD-hhmmss.Brewfile`, o Boot Environment do
  OctoPkg no Homebrew. Vem marcado ao instalar e atualizar e desmarcado ao remover. Se o
  snapshot falhar, nada muda.
- **Sem surpresas:** as operações rodam sem auto-update, sem a pergunta do brew 7 e sem o
  autoremove automático do `uninstall` (que levaria pacotes fora da prévia: ele é a ação
  própria do menu Arquivo).
- **sudo:** casks com instaladores `.pkg` ou passos de sistema pedem o sudo; o diálogo os
  avisa. Se o sudo não tiver a senha em cache, a tela é suspensa enquanto ele a pede no
  terminal; no resto, a operação roda em segundo plano, com a saída ao vivo na aba Saída.
- O `ftxui`, de que o octobrew-tui depende, e o próprio `octobrew-tui` não podem ser
  removidos, como o `pkg` no OctoPkg.

## Teclado e mouse

As teclas são as do octopkg-tui, na notação do teclado da Apple: <kbd>⌃</kbd> Control,
<kbd>⌥</kbd> Option, <kbd>⇧</kbd> Shift, <kbd>⌦</kbd> o delete para a frente (fn ⌫), e
fn↑ fn↓ fn← fn→ para Page Up, Page Down, Home e End. No teclado do Mac, as teclas F podem
pedir o **fn**, então cada uma tem uma alternativa. No Linux, ⌥ é o Alt e ⌃ é o Ctrl, e a
interface mostra os nomes do teclado de PC (Alt+M, Ctrl+L).

| Tecla | Ação |
|:---|:---|
| <kbd>F5</kbd> / <kbd>⌃R</kbd> | Atualizar a lista |
| <kbd>↑</kbd> <kbd>↓</kbd> <kbd>fn↑</kbd> <kbd>fn↓</kbd> <kbd>fn←</kbd> <kbd>fn→</kbd> (ou <kbd>j</kbd> <kbd>k</kbd>) | Mover a seleção |
| <kbd>⌃L</kbd> / <kbd>/</kbd> | Buscar pelo nome, ou pela descrição (<kbd>Esc</kbd>, <kbd>↩</kbd> ou <kbd>⌃P</kbd> volta à lista) |
| <kbd>F7</kbd> / <kbd>⌥D</kbd> | Buscar por nome ou por descrição |
| <kbd>F8</kbd> / <kbd>t</kbd> | Filtrar por tap (homebrew/core, homebrew/cask, taps de terceiros); uma letra salta na lista |
| <kbd>i</kbd> / <kbd>⌥I</kbd> | Alternar entre os instalados e o repositório |
| <kbd>+</kbd> | Marcar o pacote para instalar (de novo: desmarca) |
| <kbd>-</kbd> / <kbd>⌦</kbd> | Marcar o pacote para remover |
| <kbd>⌥M</kbd> | Aplicar: prévia, confirmação e a transação |
| <kbd>⌃E</kbd> | Desfazer as marcas |
| <kbd>⌃U</kbd> / <kbd>⌃K</kbd> | Instalar as atualizações / atualizar o Homebrew |
| <kbd>F6</kbd> / <kbd>s</kbd> | Escolher a coluna de ordenação (de novo na mesma coluna inverte) |
| Clique no cabeçalho | Ordenar: crescente, decrescente, ordem do brew |
| <kbd>1</kbd>…<kbd>6</kbd> / <kbd>⌥1</kbd>…<kbd>⌥6</kbd> | Abas Informação, Arquivos, Ações, Saída, Notícias e Ajuda, com os números do OctoPkg |
| <kbd>7</kbd> / <kbd>⌥7</kbd> | Aba Serviços (`brew services`) |
| <kbd>l</kbd> | Só as folhas: fórmulas instaladas de que nada mais depende (`brew leaves`) |
| <kbd>⌃G</kbd> | Buscar as últimas notícias do Homebrew (aba Notícias) |
| <kbd>F1</kbd> / <kbd>?</kbd> | Ajuda |
| <kbd>↩</kbd> | Aba Informação do pacote selecionado |
| <kbd>⇥</kbd> | Rolar a aba com as setas (de novo, ou <kbd>Esc</kbd>, volta à lista) |
| <kbd>⌃F</kbd> | Buscar texto nas abas Arquivos, Saída, Notícias e Ajuda (<kbd>F3</kbd> / <kbd>⇧F3</kbd>) |
| <kbd>F9</kbd> / <kbd>⌥A</kbd> <kbd>⌥B</kbd> <kbd>⌥C</kbd> <kbd>⌥H</kbd> <kbd>⌥J</kbd> | Menus Arquivo, Busca, Ações, Homebrew e Ajuda |
| <kbd>⌥,</kbd> / <kbd>⌥T</kbd> | Preferências (tema, idioma, ícones) / próximo tema |
| <kbd>F11</kbd> / <kbd>⌥Z</kbd> | Maximizar a lista, depois as abas, depois voltar às duas |
| <kbd>⌃Q</kbd> / <kbd>F10</kbd> / <kbd>q</kbd> | Sair (não durante uma operação) |

No Terminal do macOS, ative **"Usar Option como tecla Meta"** para os atalhos com ⌥
(Terminal → Ajustes → Perfis → Teclado; no iTerm2, *Left Option key: Esc+*). O ⌘ não
serve: o terminal não o repassa ao programa. O mapa completo está em [docs/KEYMAP.md](docs/KEYMAP.md).

## Arquitetura

| Camada | Local | Responsabilidade |
|:---|:---|:---|
| Núcleo | `src/core/` | `brew.*`: o brew sem shell, a leitura do JSON dele, do cache da API e das prévias (`--dry-run`), estados, dependentes, as operações e os snapshots, e os tamanhos em disco. `json.*`: um leitor de JSON próprio. `news.*`: o feed Atom/RSS e o cache das notícias. `process`, `i18n` e `settings` vêm do octopkg-tui sem mudança. C++17 puro, sem dependências. |
| Modelo | `src/model/` | Filtro, ordenação (versão, tipo, tamanho, data, estado), filtro por tap e a seleção que acompanha o pacote (pela chave `f:nome` / `c:token`) |
| Interface | `src/tui/` | FTXUI: tela, temas, teclado, mouse, diálogos, menus e busca nas abas, do octopkg-tui; listagem, detalhes e tamanhos em segundo plano |

## Testes

```sh
cmake -B build -S . -DOCTOBREW_TUI_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
sh tests/macos-e2e.sh build    # tudo, mais a integração com o Homebrew real (consultas e prévias)
sh tests/macos-e2e.sh --transactions build   # e instala, fixa e remove o figlet (o CI faz isso)
sh tests/tui-smoke.sh build    # a interface num pseudo-terminal, com um brew falso
```

- `test_core`: o leitor de JSON, a saída do brew 7 (instalados, detalhes de fórmula e de
  cask) e o cache da API (formatos do brew 5+ e do brew 4), com um brew falso que confere
  os argumentos e o ambiente de cada chamada; tamanhos em disco, processos e preferências.
- `test_model`: filtro, ordenação, tap, seleção e rolagem, com 40 mil pacotes.
- `test_view`: desenha a tela em texto e dirige a interface com eventos de teclado,
  inclusive o caminho completo (listagem, repositório, detalhes e tamanhos) com o brew
  falso.
- `test_integration`: contra o Homebrew real: as consultas, as prévias e o feed de
  notícias, sem mudar nada;
  com `OCTOBREW_TEST_TRANSACTIONS=1`, instala, fixa e remove o figlet (se ele não estiver
  instalado).
- `tests/tui-smoke.sh`: a interface num pseudo-terminal com um brew falso; roda também no
  Ubuntu.

O CI roda no macOS (com o Homebrew do runner) e no Ubuntu (FTXUI 6.1.9, com ASan e UBSan).

## Licença

[GPL-2.0-or-later](LICENSE). A interface e o modelo vêm do octopkg-tui, cujo núcleo é um
porte de código do OctoPkg, © Alexandre Albuquerque Arnt (GPL-2.0-or-later).
