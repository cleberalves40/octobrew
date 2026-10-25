# OctoBrew

[![Build & Test](https://github.com/cleberalves40/octobrew/actions/workflows/build.yml/badge.svg)](https://github.com/cleberalves40/octobrew/actions/workflows/build.yml)
[![License: GPL v2+](https://img.shields.io/badge/License-GPL%20v2%2B-blue.svg)](LICENSE)

Interface gráfica (Qt6) para o [Homebrew](https://brew.sh), com o desenho do
[OctoPkg](https://github.com/aarnt/octopkg), a interface do `pkg(8)` nos BSDs, e o núcleo
do [octobrew-tui](https://github.com/cleberalves40/octobrew-tui), a versão de terminal.

![A janela do OctoBrew](docs/images/octobrew-installed.png)

## Estado

**v0.1.0, somente leitura** (as Fases 0 e 1 do [roadmap](docs/ROADMAP.md)). O programa:

- lista as fórmulas e os casks instalados e os do repositório (o cache da API do
  Homebrew, mais os taps de terceiros), com os ícones de estado do OctoPkg;
- filtra pelo tap (à esquerda, no lugar dos grupos do OctoPkg), pelo nome ou pela
  descrição, e mostra só as folhas (`brew leaves`);
- mostra os detalhes do pacote selecionado (com os *caveats*), os arquivos e o espaço em
  disco, medido em segundo plano;
- ordena por qualquer coluna, lembra a ordem, o modo de busca e o tamanho da janela.

Instalar, remover e atualizar é a Fase 2. Veja o [CHANGELOG](CHANGELOG.md).

## Compilação

```sh
brew install qtbase cmake ninja
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build
open build/OctoBrew.app
```

Opções: `--remote` (começa na visão Repositório), `--lang=en_US|pt_BR|es_ES`,
`--config=ARQUIVO`, `--brew=EXECUTÁVEL`, `--version`. As preferências ficam em
`~/.config/octobrew/config`, separadas das do octobrew-tui.

Para um `OctoBrew.app` autocontido, com o Qt dentro e uma assinatura *ad hoc*:
`sh ci/bundle.sh build` (o aplicativo fica em `build/deploy/`).

No Linux, com o Qt 6.4 ou mais novo (`apt install qt6-base-dev`), o mesmo CMake gera o
executável `octobrew`.

## Teclado

Os atalhos do OctoPkg. Numa GUI, o ⌘ chega ao programa: o Qt mostra o Ctrl como ⌘ no
macOS.

| Tecla (macOS) | Linux | Ação |
|:---|:---|:---|
| <kbd>F5</kbd> / <kbd>⌘R</kbd> | F5 / Ctrl+R | Atualizar a lista |
| <kbd>⌘I</kbd> | Ctrl+I | Alternar entre os instalados e o repositório |
| <kbd>⌘L</kbd> | Ctrl+L | Buscar um pacote na lista |
| <kbd>F7</kbd> | F7 | Buscar por descrição |
| <kbd>⇧⌘L</kbd> | Shift+Ctrl+L | Só as folhas |
| <kbd>⌥1</kbd> <kbd>⌥2</kbd> <kbd>⌥4</kbd> <kbd>⌥6</kbd> | Alt+1… | Abas Informação, Arquivos, Saída e Ajuda, com os números do OctoPkg |
| <kbd>F1</kbd> | F1 | Ajuda |
| <kbd>⌘Q</kbd> | Ctrl+Q | Sair |

## Arquitetura

| Camada | Local | Responsabilidade |
|:---|:---|:---|
| Núcleo | `tui/src/core/`, `tui/src/model/` | O octobrew-tui, num [git subtree](docs/RELEASING.md): o brew sem shell, o JSON, o cache da API, os estados, os tamanhos, i18n e o filtro e a ordem da lista. C++17 puro, sem Qt |
| Interface | `src/gui/` | Qt6 Widgets: a janela do OctoPkg (`mainwindow`), a tabela sobre o vetor de pacotes do núcleo (`packagemodel`, sem um item por célula: são ~17 mil pacotes), as páginas das abas em HTML (`pages`) e o catálogo de textos da GUI, que cai no do núcleo (`strings`) |
| Recursos | `resources/` | Os ícones do OctoPkg (Octopi), © Alexandre Albuquerque Arnt, e o `OctoBrew.icns` |

O trabalho com o brew roda fora da interface (`QtConcurrent`), com o núcleo síncrono do
octobrew-tui; os testes rodam o mesmo trabalho na hora (`synchronous`).

## Testes

```sh
cmake -B build -S . -DOCTOBREW_BUILD_TESTS=ON && cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build
OCTOBREW_TEST_BREW=$(command -v brew) OCTOBREW_SCREENSHOT=/tmp/octobrew.png ./build/tst_integration
```

- `tst_model`: a tabela (filtro, ordem, tap, folhas, ícones, células), as páginas das abas
  e o catálogo.
- `tst_mainwindow`: a janela sem tela, com o brew falso e as amostras do brew 7 dos testes
  do octobrew-tui: listagem, repositório, taps, busca, folhas, detalhes, arquivos, erros,
  Ajuda e preferências.
- `tst_integration`: a janela contra o Homebrew real, só lendo, com uma captura da tela.

O CI roda no macOS (o Qt do Homebrew, a integração e o bundle aberto de verdade) e no
Ubuntu (o Qt 6.4, também com ASan e UBSan).

## Licença

[GPL-2.0-or-later](LICENSE). O desenho e os ícones vêm do OctoPkg, © Alexandre
Albuquerque Arnt; o núcleo, do octobrew-tui.
