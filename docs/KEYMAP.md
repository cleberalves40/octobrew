# Atalhos: OctoPkg → octopkg-tui → octobrew-tui

O octobrew-tui mantém os atalhos do octopkg-tui, que por sua vez mantém os do
OctoPkg 0.4.2 sempre que o terminal os entrega ao programa. A interface os mostra na
**notação do teclado da Apple**, como os menus do macOS:

| Símbolo | Tecla | No Linux |
|:---|:---|:---|
| ⌃ | Control | Ctrl |
| ⌥ | Option | Alt |
| ⇧ | Shift | Shift |
| ↩ | return | Enter |
| ⇥ | tab | Tab |
| ⌦ | delete para a frente (fn ⌫) | Delete |
| fn↑ fn↓ fn← fn→ | Page Up, Page Down, Home, End | PgUp, PgDn, Home, End |

No Linux, a interface mostra os nomes do teclado de PC (Alt+M, Ctrl+L). No macOS, há
três detalhes:

- as teclas F1–F12 pedem o **fn** (a menos que "Usar as teclas F1, F2 etc. como teclas
  de função padrão" esteja ativado). Por isso cada uma tem uma alternativa sem fn;
- os atalhos com <kbd>⌥</kbd> só chegam ao programa com "Usar Option como tecla Meta"
  ativado (Terminal → Ajustes → Perfis → Teclado; no iTerm2, *Left Option key: Esc+*);
- o <kbd>⌘</kbd> não serve: o terminal não o repassa ao programa, e os atalhos do
  OctoPkg com Ctrl ficam com <kbd>⌃</kbd>.

Como no octopkg-tui: o terminal envia <kbd>⌃I</kbd> como <kbd>⇥</kbd> e <kbd>⌃M</kbd>
como <kbd>↩</kbd>, e <kbd>⌃⇧letra</kbd> chega igual a <kbd>⌃letra</kbd>. O Insert, que o
teclado da Apple não tem, continua valendo (como o <kbd>+</kbd>) num teclado de PC.

| Ação | OctoPkg | octobrew-tui | Sem fn (Mac) | Disponível |
|:---|:---|:---|:---|:---|
| Abas Informação / Arquivos / Saída / Ajuda | Alt+1 / Alt+2 / Alt+4 / Alt+6 | **1 2 4 6** ou **⌥1 ⌥2 ⌥4 ⌥6** | — | v0.1.0 |
| Ajuda | F1 | **F1** | **?** | v0.1.0 |
| Buscar um pacote na lista | Ctrl+L | **⌃L** ou **/** | — | v0.1.0 |
| Buscar por nome / por descrição | menu Busca | **F7**; menu Busca (**⌥B**) | **⌥D** | v0.1.0 |
| Filtrar por tap | — | **F8**; menu Busca | **t** | v0.1.0 |
| Maximizar a lista / as abas | F10 / F11 | **F11**, em ciclo | **⌥Z** | v0.1.0 |
| Voltar à lista de pacotes | Ctrl+P | **⌃P** ou **Esc** | — | v0.1.0 |
| Instalados / repositório | Ctrl+I | **i** ou **⌥I** | — | v0.1.0 |
| Aba Informação do selecionado | Enter | **↩** | — | v0.1.0 |
| Ordenar | clique no cabeçalho | clique no cabeçalho ou **F6** | **s** | v0.1.0 |
| Atualizar a lista | — | **F5** | **⌃R** | v0.1.0 |
| Rolar a aba atual | — (mouse) | **⇥**, depois as setas, fn↑, fn↓ | — | v0.1.0 |
| Menus | barra de menus | **F9** ou **⌥A** (Arquivo), **⌥B** (Busca), **⌥J** (Ajuda) | **⌥A** | v0.1.0 |
| Buscar texto nas abas Arquivos, Saída, Notícias e Ajuda | Ctrl+F (F3 / Shift+F3) | **⌃F**; **↩**/**F3** e **⇧F3** (ou ↓ ↑) | ↓ ↑ | v0.1.0 |
| Preferências / próximo tema | — | **⌥,** / **⌥T** | — | v0.1.0 |
| Sair | Ctrl+Q | **⌃Q**, **F10** ou **q** | **q** | v0.1.0 |
| Marcar para instalar / remover | + / − | **+** / **−**, ou **⌦** | — | v0.2.0 |
| Aba Ações | Alt+3 | **3** ou **⌥3** | — | v0.2.0 |
| Aplicar | Ctrl+M | **⌥M** (⌃M é Enter) | — | v0.2.0 |
| Cancelar (desfazer as marcas) | Ctrl+E | **⌃E** | — | v0.2.0 |
| Procurar atualizações | Ctrl+K | **⌃K** (`brew update`) | — | v0.2.0 |
| Instalar as atualizações | Ctrl+U | **⌃U** (`brew upgrade`) | — | v0.2.0 |
| Limpar o cache | Ctrl+Shift+C | menu Arquivo (`brew cleanup`), como no octopkg-tui | — | v0.2.0 |
| Remover dependências desnecessárias | — | menu Arquivo (`brew autoremove`) | — | v0.2.0 |
| Bloquear / desbloquear | menu de contexto | menu Ações: Fixar / Liberar (`brew pin` / `unpin`) | — | v0.2.0 |
| Restaurar um snapshot | — (Boot Environments) | menu Arquivo | — | v0.2.0 |
| Menu Ações | barra de menus | **⌥C** | — | v0.2.0 |
| Aba Notícias / buscar as últimas notícias | Alt+5 / Ctrl+G | **5** ou **⌥5** / **⌃G** | — | v0.3.0 |
| Aba Serviços (brew services) | — | **7** ou **⌥7** | — | v0.4.0 |
| Só as folhas (brew leaves) | — | **l**; menu Busca | — | v0.4.0 |
| Menu Homebrew: serviços, taps, Brewfiles, brew doctor | — | **⌥H**, ou F9 e as setas | — | v0.4.0 |

Nos diálogos: <kbd>⇥</kbd> ou as setas mudam o campo, <kbd>Espaço</kbd> marca a
opção do snapshot, <kbd>↩</kbd> confirma e <kbd>Esc</kbd> cancela; nas confirmações,
<kbd>y</kbd>/<kbd>s</kbd> e <kbd>n</kbd> também valem.

No seletor de coluna (F6/s), no de taps (F8/t) e nos diálogos, as teclas são as do
octopkg-tui: setas, <kbd>↩</kbd> escolhe, <kbd>Esc</kbd> fecha; nos taps, uma letra
salta para o próximo que começa com ela.
