#!/bin/sh
#
# Valida a fórmula (homebrew/octobrew.rb) como o tap a usaria: copia-a para um
# tap local, roda o brew style e o brew audit, instala a partir do código da
# tag (--build-from-source), roda o brew test e o brew linkage, e abre o
# OctoBrew instalado por alguns segundos. Muda o Homebrew da máquina: é para o
# CI (um runner descartável), não para o Mac de alguém.
#
# uso: ci/formula-test.sh

set -eu

tap=octobrew-ci/local
name=octobrew
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

echo "== instalação a partir do código da tag"
brew install --build-from-source --verbose "$tap/$name"
brew test "$tap/$name"
brew linkage --test "$tap/$name"

echo "== o programa instalado"
prefix=$(brew --prefix "$tap/$name")
"$(brew --prefix)/bin/octobrew" --version
app="$prefix/OctoBrew.app/Contents/MacOS"
for bin in OctoBrew octobrew-askpass octobrew-notifier; do
	[ -x "$app/$bin" ] || { echo "FAIL: falta $bin no bundle instalado" >&2; exit 1; }
done
# Aberto de verdade (o runner tem uma sessão gráfica), contra o Homebrew real.
work=$(mktemp -d "${TMPDIR:-/tmp}/formula-app.XXXXXX")
"$app/OctoBrew" --config="$work/config" &
pid=$!
sleep 8
kill -0 "$pid" 2>/dev/null || { echo "FAIL: o OctoBrew instalado saiu sozinho" >&2; exit 1; }
kill "$pid"
wait "$pid" 2>/dev/null || true
rm -rf "$work"

brew uninstall "$tap/$name"
brew untap "$tap"
echo "== fórmula ok"
