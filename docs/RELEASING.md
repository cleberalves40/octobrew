# Versões, a fórmula e o tap

## Versionamento

O projeto segue o [versionamento semântico](https://semver.org/lang/pt-BR/), com tags
`vX.Y.Z`:

- **patch** (`v1.0.1`): correções e empacotamento, sem funcionalidades novas;
- **minor** (`v1.1.0`): funcionalidades novas, compatíveis;
- **major** (`v2.0.0`): mudanças incompatíveis (opções de linha de comando, o formato do
  arquivo de preferências, as teclas).

A versão do programa fica no `CMakeLists.txt`, e a da fórmula no `url` e no `sha256` de
`homebrew/octobrew-tui.rb`. A fórmula acompanha a **última tag publicada**: o `sha256`
não pode vir no próprio release, porque o tarball da tag contém a fórmula, e o checksum
mudaria ao incluí-lo. É o mesmo arranjo do port do octopkg-tui.

| Arquivo | Campo |
|:---|:---|
| `CMakeLists.txt` | `project(octobrew_tui VERSION X.Y.Z ...)`: a fonte da versão exibida no `--version` e no Sobre |
| `homebrew/octobrew-tui.rb` | `url` e `sha256`: a última tag publicada |

O CI reprova se o `CHANGELOG.md` não tiver a entrada da versão do `CMakeLists.txt`, se a
fórmula estiver à frente dela, ou se o `sha256` não bater com o tarball da tag
(`ci/formula-sha.sh --check`).

## Publicando uma versão

1. Num branch `release-X.Y.Z`, altere a versão no `CMakeLists.txt` e registre as
   mudanças no `CHANGELOG.md`, no formato
   [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/). A fórmula fica como está.
2. Abra o pull request **contra o `main`**. Um PR por versão: um PR encadeado (com a
   base num `release-*`) é integrado no branch de baixo, e não no `main`, se esse branch
   não for apagado antes. O CI precisa passar nos três jobs:
   - **macOS**: os testes, a integração com o Homebrew real (incluindo a transação com o
     figlet) e a interface num pseudo-terminal;
   - **Fórmula**: o `brew audit`, a instalação a partir do código da tag, o `brew test` e o
     programa instalado num pseudo-terminal (`ci/formula-test.sh`);
   - **Ubuntu**: os testes contra o FTXUI 6.1.9, também com ASan e UBSan.
3. Faça o merge com um *merge commit*.
4. Depois do CI verde no commit de merge, crie a tag anotada nele e publique a release
   com o texto do CHANGELOG e o link de comparação com a anterior:

   ```sh
   git fetch origin
   git tag -a vX.Y.Z -m "octobrew-tui vX.Y.Z" origin/main
   git push origin vX.Y.Z
   gh release create vX.Y.Z --verify-tag --title "octobrew-tui vX.Y.Z" --notes-file notas.md
   ```

5. Com a tag publicada, aponte a fórmula para ela num branch `formula-X.Y.Z` e abra o
   pull request ("formula: vX.Y.Z"); é só a fórmula, sem versão nova do programa:

   ```sh
   sh ci/formula-sha.sh X.Y.Z          # url e sha256 da tag
   sh ci/formula-sha.sh X.Y.Z --check  # o mesmo que o CI confere
   ```

6. Depois do merge, copie a fórmula para o tap (a seguir).

## O tap

O tap é o repositório [cleberalves40/homebrew-tap](https://github.com/cleberalves40/homebrew-tap),
com a fórmula em `Formula/octobrew-tui.rb`, a mesma de `homebrew/` deste repositório.
Para publicar uma versão nele:

```sh
git clone https://github.com/cleberalves40/homebrew-tap.git
cp octobrew-tui/homebrew/octobrew-tui.rb homebrew-tap/Formula/
cd homebrew-tap && git checkout -b octobrew-tui-X.Y.Z && git commit -am "octobrew-tui X.Y.Z"
gh pr create --fill   # o CI do tap roda o audit, instala e testa
```

O CI do tap (`.github/workflows/tests.yml`) roda o `brew style`, o `brew audit`, a
instalação a partir do código e o `brew test` no macOS e no Linux, a cada mudança e toda
segunda-feira.

Os usuários instalam e atualizam com:

```sh
brew install cleberalves40/tap/octobrew-tui   # o nome completo já confia no tap (brew 6+)
brew upgrade octobrew-tui
```

Sem bottles por enquanto: o brew compila a partir do código, com o cmake e o ninja como
dependências de compilação (cerca de um minuto).

## Enviando ao homebrew-core

O homebrew-core aceita fórmulas de projetos com uso comprovado. Pelas
[regras de aceitação](https://docs.brew.sh/Acceptable-Formulae), o repositório precisa
ser notável (estrelas, forks, observadores) e estável, com versões publicadas. O
`brew audit --new` do job da fórmula passa no tap local desde a v1.0.0, mas a
notabilidade só é conferida dentro do próprio homebrew-core: ela é o que falta.

Quando o repositório for notável:

1. Copie `homebrew/octobrew-tui.rb` para `Formula/o/octobrew-tui.rb` num fork do
   [Homebrew/homebrew-core](https://github.com/Homebrew/homebrew-core) (sem o
   `head`, que o core não aceita em fórmulas novas, se o audit pedir).
2. Rode `brew audit --new octobrew-tui`, `brew install --build-from-source octobrew-tui`
   e `brew test octobrew-tui`, e abra o pull request com o título
   "octobrew-tui X.Y.Z (new formula)".
3. Com a fórmula no core, o tap passa a só redirecionar (`tap_migrations.json`) e a
   fórmula de `homebrew/` sai deste repositório.
