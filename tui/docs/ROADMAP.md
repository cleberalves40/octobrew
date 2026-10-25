# Roadmap

O octobrew-tui leva o octopkg-tui (a interface de terminal do OctoPkg para o `pkg(8)`)
ao Homebrew, com as mesmas funcionalidades. As camadas genéricas (processos, i18n,
preferências, temas, ícones, diálogos, menus, modelo da lista) vêm do octopkg-tui. O
núcleo que fala com o gerenciador de pacotes foi reescrito: `core/brew` no lugar de
`core/pkg`.

## Correspondência pkg → brew

| octopkg-tui (pkg) | octobrew-tui (brew) | Fase |
|:---|:---|:---|
| `pkg query -a` | `brew info --json=v2 --installed` | 1 ✅ |
| `pkg rquery -a` | cache da API do Homebrew, lido direto | 1 ✅ |
| `pkg upgrade -n` (desatualizados) | campo `outdated` do `brew info` (o mesmo do `brew outdated`) | 1 ✅ |
| `automatic` (dependência) | `installed_on_request` | 1 ✅ |
| `pkg lock` (bloqueado) | `brew pin` (fixado): estado na Fase 1, operação na Fase 2 | 1 ✅ / 2 ✅ |
| aba Arquivos | `brew list --formula --verbose` / `brew list --cask` | 1 ✅ |
| `pkg install -n` / `upgrade -n` | `brew install --dry-run` / `brew upgrade --dry-run` | 2 ✅ |
| `pkg remove -n -R` | não há dry-run: os dependentes vêm dos recibos já lidos (`runtime_dependencies`), sem chamar o brew | 2 ✅ |
| install / remove / upgrade | `brew install --no-ask [--cask]` / `brew uninstall` / `brew upgrade --no-ask` | 2 ✅ |
| `pkg update -f` (Ctrl+K) | `brew update` | 2 ✅ |
| `pkg clean -a` | `brew cleanup --prune=all`, com a prévia do `--dry-run`; e `brew autoremove` | 2 ✅ |
| doas/sudo | sai: o brew roda como usuário; casks com `.pkg` pedem o `sudo` com a tela suspensa | 2 ✅ |
| Boot Environment (`bectl`) | snapshot em Brewfile (`brew bundle dump`) antes da transação | 2 ✅ |
| aba Notícias (feed da variante BSD) | feed Atom do blog do Homebrew (`brew.sh/atom.xml`), com o `/usr/bin/curl` | 3 ✅ |

## Fases

### Fase 0 — Fundação ✅ (v0.1.0)
- Repositório a partir do octopkg-tui v0.7.2, com a atribuição GPL-2.0-or-later.
- CMake + FTXUI do brew (7.x), com o fallback para o FTXUI 6.1.9.
- JSON: um leitor próprio em C++17 (`core/json`), em vez do nlohmann-json, para o núcleo
  continuar sem dependências, como no octopkg-tui.
- CI: macOS com o Homebrew do runner; Ubuntu com ASan e UBSan e um brew falso.

### Fase 1 — Somente leitura ✅ (v0.1.0)
- Listas de Instalados e de Repositório; coluna Tipo (fórmula ou cask).
- Estados: instalado, dependência, desatualizado, fixado, mais novo que o repositório,
  não instalado.
- Aba Informação com os caveats; aba Arquivos.
- Tamanho em disco medido em segundo plano.
- O que veio junto, por ser só interface: temas, Preferências, Ctrl+F, F7 (descrição),
  F8 (tap, no lugar da categoria), F11 e alternativas às teclas F do Mac.

### Fase 2 — Transações ✅ (v0.2.0)
- Marcas +/−, aba Ações (3), prévia e Alt+M; Ctrl+U (upgrade), Ctrl+K (update),
  cleanup, pin/unpin e `brew autoremove`.
- Casks que instalam `.pkg` pedem o `sudo`: a tela é suspensa, como o doas no
  octopkg-tui.
- Snapshot em Brewfile antes da transação, com a opção de restaurar
  (`brew bundle install --no-upgrade`).
- Descobertas no brew 7: o `uninstall` e o `cleanup` fazem um autoremove automático,
  que levaria pacotes fora da prévia (`HOMEBREW_NO_AUTOREMOVE=1` nas operações; o
  autoremove virou uma ação própria). O brew 7 pergunta antes de agir (`ask`); sem um
  terminal, a pergunta é pulada, e o octobrew-tui passa `--no-ask` de qualquer forma.

### Fase 3 — Notícias ✅ (v0.3.0)
- Aba Notícias (5) com o feed do blog do Homebrew, cache em disco e Ctrl+G.
- O feed é Atom (não RSS, como o dos BSDs): o octobrew-tui lê os dois.

### Fase 4 — Funções que só o Homebrew tem ✅ (v0.4.0)
- Aba Serviços (7): `brew services list --json`, com iniciar, parar e reiniciar no
  menu Homebrew (Alt+H).
- Taps: `brew tap` (com `brew trust --tap`, que o brew 6+ exige dos taps de terceiros)
  e `brew untap`; os pacotes dos taps de terceiros na visão Repositório.
- Folhas (`l`), como o `brew leaves`, e o `brew doctor`, com os avisos na aba Saída.
- Exportar um Brewfile (`brew bundle dump`) e instalar de um (`brew bundle check` como
  prévia, `brew bundle install --no-upgrade`).

### Fase 5 — Distribuição ✅ (v1.0.0)
- Tap próprio, [cleberalves40/homebrew-tap](https://github.com/cleberalves40/homebrew-tap):
  `brew install cleberalves40/tap/octobrew-tui`. A fórmula fica também neste
  repositório (`homebrew/`), validada no CI com `brew audit --strict --online`, a
  instalação a partir do código e o `brew test` (veja `docs/RELEASING.md`).
- Tags e releases a partir da v0.1.0.
- Envio ao homebrew-core quando o projeto atender aos critérios de notabilidade (o
  `brew audit --new` já passa; a notabilidade só o core confere).
- Homebrew no Linux: o CI do tap instala a fórmula a partir do código e roda o
  `brew test` também no Ubuntu, com o Homebrew em `/home/linuxbrew/.linuxbrew`. Falta
  rodar a integração e a interface contra o Homebrew do Linux.
- Bottles (binários pré-compilados no tap, com `brew test-bot`), para não compilar na
  instalação.

## Riscos e decisões em aberto

1. **Teclado do Mac:** as teclas F pedem o fn. Resolvido na v0.1.0 com `?`, `s`, `t` e
   `Alt+D`. Para o Alt, é preciso ativar "Usar Option como tecla Meta" no terminal.
2. **Lentidão do brew:** cada chamada custa o arranque do Ruby. Por isso há uma chamada
   só para os instalados, o cache da API é lido direto e tudo roda em segundo plano.
3. **Formato do cache da API:** é interno ao Homebrew (`api/internal/`) e pode mudar. Os
   formatos do brew 5+ e do brew 4 são lidos, e os testes usam amostras reais. Se o
   formato mudar, a visão Repositório avisa, e a visão Instalados não é afetada.
4. **Fórmulas removidas do homebrew-core** (exa, neofetch, youtube-dl…) aparecem só
   como instaladas e podem ser removidas (−). Falta destacá-las na lista.
6. **Reinstalar** um instalado em dia (o `pkg install -f` do OctoPkg) não foi portado:
   `brew reinstall` não tem `--dry-run` para a prévia.
5. **Runner Intel no CI:** só `macos-latest` (arm64) por enquanto.
