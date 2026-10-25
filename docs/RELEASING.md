# Versões e o núcleo

## Versionamento

O projeto segue o [versionamento semântico](https://semver.org/lang/pt-BR/), com tags
`vX.Y.Z`, e o mesmo processo do octobrew-tui: num branch `release-X.Y.Z`, a versão no
`CMakeLists.txt` e a entrada no `CHANGELOG.md`; um pull request por versão contra o
`main`, com o CI verde (macOS e Ubuntu); o merge com um *merge commit*; e a tag anotada
no commit de merge, com a release do GitHub.

## A fórmula e o tap

A fórmula fica em `homebrew/octobrew.rb` e acompanha a **última tag publicada**: o
`sha256` não pode vir no próprio release, porque o tarball da tag contém a fórmula. É o
mesmo arranjo do octobrew-tui. O CI reprova se a fórmula estiver à frente do
`CMakeLists.txt` ou se o `sha256` não bater com a tag (`ci/formula-sha.sh --check`), e o
job **Fórmula** (`ci/formula-test.sh`) a instala num tap local, a partir do código, e roda
o `brew style`, o `brew audit --strict --online`, o `brew test`, o `brew linkage` e o
OctoBrew instalado.

Depois da tag de uma versão:

1. Num branch `formula-X.Y.Z`, aponte a fórmula para a tag e abra o pull request
   ("formula: vX.Y.Z"):

   ```sh
   sh ci/formula-sha.sh X.Y.Z
   sh ci/formula-sha.sh X.Y.Z --check
   ```

2. Depois do merge, copie a fórmula para o tap
   [cleberalves40/homebrew-tap](https://github.com/cleberalves40/homebrew-tap), em
   `Formula/octobrew.rb`, num pull request de lá (o CI do tap instala e testa no macOS e no
   Linux).

Os usuários instalam com `brew install cleberalves40/tap/octobrew`; o `OctoBrew.app` fica
em `$(brew --prefix)/opt/octobrew/OctoBrew.app` (os *caveats* da fórmula mostram o `ln -s`
para o `/Applications`).

### Por que não o homebrew-core, nem um cask (por enquanto)

- O homebrew-core **não aceita aplicativos gráficos**: eles vão para o homebrew-cask.
- Um cask distribui o `.app` pronto (o bundle autocontido do `ci/bundle.sh`) e precisa
  passar no Gatekeeper: assinado com um Apple Developer ID e notarizado. Sem isso, o brew 7
  desativa o cask ("does not pass the macOS Gatekeeper check"). Fica para quando houver o
  Developer ID; até lá, a fórmula do tap, que compila na instalação.

## O núcleo (git subtree)

O diretório `tui/` é o [octobrew-tui](https://github.com/cleberalves40/octobrew-tui),
trazido com `git subtree` (com `--squash`, um commit por atualização). O OctoBrew usa o
núcleo (`tui/src/core/`, `tui/src/model/`) e os testes de lá (`tui/tests/fixtures.h`,
`support.h`); a interface de terminal (`tui/src/tui/`) não entra no build.

Regras:

1. **Mudanças no núcleo nascem no octobrew-tui**, com os testes de lá, e chegam aqui numa
   versão publicada. Não edite `tui/` direto: a próxima atualização traria um conflito.
2. Para trazer uma versão nova do octobrew-tui:

   ```sh
   git subtree pull --prefix=tui https://github.com/cleberalves40/octobrew-tui.git vX.Y.Z --squash \
     -m "tui: o octobrew-tui vX.Y.Z"
   ```

   O commit entra num pull request, com o CI, e o `CHANGELOG.md` registra a versão do
   núcleo.
3. Cada programa faz a própria versão: uma correção no núcleo pede uma versão do
   octobrew-tui e, se ela muda algo aqui, uma versão do OctoBrew.

A versão do núcleo em uso é a da última linha `tui: o octobrew-tui vX.Y.Z` do
`git log -- tui`.
