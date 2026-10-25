#!/bin/sh
#
# Valida a fórmula (homebrew/octobrew-tui.rb) como o tap a usaria: copia-a
# para um tap local, roda o brew audit e o brew style, instala a partir do
# código da tag (--build-from-source, como um usuário sem bottle), roda o brew
# test e abre o programa instalado num pseudo-terminal. Muda o Homebrew da
# máquina: é para o CI (um runner descartável), não para o Mac de alguém.
#
# uso: ci/formula-test.sh

set -eu

tap=octobrew-ci/local
name=octobrew-tui
export HOMEBREW_NO_AUTO_UPDATE=1

brew tap-new --no-git "$tap"
cp "homebrew/$name.rb" "$(brew --repository "$tap")/Formula/$name.rb"
# brew 6+: só os taps confiáveis carregam fórmulas.
if brew commands | grep -qx trust; then
	brew trust --tap "$tap"
fi

echo "== brew style e brew audit"
brew style "$tap/$name"
brew audit --strict --online "$tap/$name"
# As regras de fórmula nova do homebrew-core; a notabilidade do repositório só é
# conferida dentro do próprio core.
echo "== brew audit --new (só informa)"
brew audit --new "$tap/$name" || echo "(o brew audit --new recusaria: veja acima)"

echo "== instalação a partir do código da tag"
brew install --build-from-source --verbose "$tap/$name"
brew test "$tap/$name"
brew linkage --test "$tap/$name"

echo "== o programa instalado"
"$(brew --prefix)/bin/$name" --version
build=$(mktemp -d "${TMPDIR:-/tmp}/formula-smoke.XXXXXX")
ln -s "$(brew --prefix)/bin/$name" "$build/$name"
sh tests/tui-smoke.sh "$build"

brew uninstall "$tap/$name"
brew untap "$tap"
