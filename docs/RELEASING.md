# Versões e o núcleo

## Versionamento

O projeto segue o [versionamento semântico](https://semver.org/lang/pt-BR/), com tags
`vX.Y.Z`, e o mesmo processo do octobrew-tui: num branch `release-X.Y.Z`, a versão no
`CMakeLists.txt` e a entrada no `CHANGELOG.md`; um pull request por versão contra o
`main`, com o CI verde (macOS e Ubuntu); o merge com um *merge commit*; e a tag anotada
no commit de merge, com a release do GitHub.

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
