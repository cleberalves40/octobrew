# Roadmap

O OctoBrew é a interface gráfica (Qt6) do Homebrew, com o desenho do OctoPkg e o núcleo
do octobrew-tui. Ele se diferencia do app nativo do Homebrew 7 pelos fluxos do
OctoPkg/Octopi (lista, abas, marcas, diálogo de transação), pela prévia antes de agir
(incluindo os dependentes e o autoremove escondido do `brew uninstall`), pelos snapshots
em Brewfile e por rodar também no Linux.

## Decisões de arquitetura

| Tema | Decisão |
|:---|:---|
| Núcleo | O octobrew-tui num **git subtree** em `tui/`: uma correção no núcleo serve aos dois. Uma biblioteca comum (`octobrew-core`) só se a sincronia ficar cara |
| Interface | **Qt6 Widgets**, como o OctoPkg e o bectl-gui, com CMake; o Qt 6.4 é o mínimo (o do Ubuntu 24.04) |
| Lista | `QAbstractTableModel` sobre o `std::vector<Package>`, com o `model::visibleRows` do núcleo; não o `QStandardItemModel` do OctoPkg |
| Concorrência | O núcleo síncrono em `QtConcurrent::run` e `QFutureWatcher` |
| Privilégios | Sem o qt-sudo: o brew roda como o usuário. Para os casks com `.pkg`, um `octobrew-askpass` (diálogo de senha Qt) pelo `SUDO_ASKPASS`, que o brew já aceita |
| Atalhos | Os do OctoPkg; numa GUI o ⌘ funciona (o Qt mostra o Ctrl como ⌘ no macOS) |
| Textos | O catálogo da GUI, que cai no do núcleo: nada é traduzido duas vezes. (O roadmap previa `.ts` do Qt; o catálogo do núcleo, sem o lupdate, ficou mais simples.) |
| Ícones e temas | Os do OctoPkg/Octopi; o modo escuro do macOS (o estilo nativo do Qt); os 15 temas do bectl-gui numa fase posterior |

## Fases

### Fase 0 — Fundação ✅ (v0.1.0)
- Repositório com o octobrew-tui v1.0.1 em `tui/` (git subtree) e a atribuição
  GPL-2.0-or-later.
- CMake + Qt6, o `OctoBrew.app` com o ícone do polvo e o `ci/bundle.sh` (macdeployqt,
  assinatura *ad hoc*, o aplicativo aberto de verdade).
- CI no macOS (o Qt do Homebrew) e no Ubuntu (Qt 6.4, ASan/UBSan), com o brew falso e as
  amostras do octobrew-tui.

### Fase 1 — Leitura ✅ (v0.1.0)
- A janela do OctoPkg: a lista Instalados/Repositório (Estado, Nome, Tipo, Versão,
  Tamanho, Data), os taps à esquerda e a busca por nome ou descrição.
- Abas Informação (com os *caveats*), Arquivos, Saída e Ajuda; tamanhos em segundo plano;
  as folhas como filtro.

### Fase 2 — Transações ✅ (v0.2.0)
- Marcas, a aba Ações e o `transactiondialog.ui` do OctoPkg, com a prévia, os
  dependentes (Aviso) e o snapshot.
- Atualizar, upgrade, cleanup, autoremove, pin e a restauração de snapshot.
- O `octobrew-askpass` para os casks com `.pkg`; a saída ao vivo na aba Saída.

### Fase 3 — Notícias, Serviços e Homebrew ✅ (v0.3.0)
- As abas Notícias e Serviços; taps (com a confiança), `brew doctor` e Brewfiles, com
  os diálogos de arquivo nativos.

### Fase 4 — macOS de verdade (v0.4.0)
- Barra de menus global, Preferências em ⌘, e diálogos como *sheets*.
- Notificador, como o octopi-notifier: um ícone na barra de menus com a contagem de
  atualizações, um LaunchAgent que roda o `brew outdated` e a contagem no ícone do Dock.

### Fase 5 — Distribuição (v1.0.0)
- Fórmula no tap `cleberalves40/tap`, compilando a partir do código, com o **`qtbase`**
  (o Homebrew já divide o Qt: não é preciso o Qt inteiro) e o `.app` no prefixo.
- Depois, um cask assinado e notarizado, que exige o Apple Developer ID; sem a
  notarização, o brew 7 desativa o cask ("does not pass the macOS Gatekeeper check").
  E um pacote para o Linux.

## Riscos e decisões em aberto
1. **Assinatura e notarização:** sem o Developer ID, só a fórmula (que compila na
   instalação) e o bundle com a assinatura *ad hoc*, que o Gatekeeper recusa num
   download. Decisão do mantenedor, só para a Fase 5.
2. **Sincronia do núcleo:** com o subtree, uma correção no núcleo vai para os dois, e
   cada um faz a própria versão (veja `docs/RELEASING.md`).
3. **Reaproveitar a interface do OctoPkg:** os `.ui`, os ícones e o desenho, não o código
   da janela, preso ao `QStandardItemModel` e ao `UnixCommand` do `pkg`.
