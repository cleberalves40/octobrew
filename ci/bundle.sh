#!/bin/sh
#
# Monta o OctoBrew.app autocontido (com o Qt dentro, pelo macdeployqt), assina
# com uma assinatura ad hoc (sem o Developer ID: o macOS não carrega, no Apple
# Silicon, as bibliotecas que o macdeployqt altera sem assinar de novo),
# confere a assinatura e roda o --version. Com --launch (o CI faz isso), abre
# o aplicativo de verdade por alguns segundos, contra um brew que não existe:
# o bundle só leva o plugin cocoa, então a janela aparece na tela.
#
# uso: ci/bundle.sh [--launch] [diretório-de-build]

set -eu

launch=no
if [ "${1:-}" = --launch ]; then
	launch=yes
	shift
fi
build=$(cd "${1:-build}" && pwd)
qtbin=$(dirname "$(command -v qmake6 || command -v qmake)")
out="$build/deploy"
rm -rf "$out" && mkdir -p "$out"
cp -R "$build/OctoBrew.app" "$out/"
app="$out/OctoBrew.app"

"$qtbin/macdeployqt" "$app" -always-overwrite 2>&1 | grep -v "^$" || true
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
echo "== $(du -sh "$app" | cut -f1) em $app"

version=$("$app/Contents/MacOS/OctoBrew" --version)
echo "$version"
expected=$(sed -n 's/^project(octobrew VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
[ "$version" = "OctoBrew $expected" ] || { echo "FAIL: --version disse \"$version\"" >&2; exit 1; }

# O Qt do bundle, não o do Homebrew: nenhuma biblioteca de /opt/homebrew.
if otool -L "$app/Contents/MacOS/OctoBrew" | grep -q "/opt/homebrew\|/usr/local/opt"; then
	otool -L "$app/Contents/MacOS/OctoBrew"
	echo "FAIL: o binário ainda usa bibliotecas do Homebrew" >&2
	exit 1
fi

[ "$launch" = yes ] || { echo "== bundle ok (sem --launch, o aplicativo não foi aberto)"; exit 0; }

# Aberto contra um brew que não existe: precisa continuar de pé.
work=$(mktemp -d "${TMPDIR:-/tmp}/octobrew-bundle.XXXXXX")
trap 'rm -rf "$work"' EXIT
"$app/Contents/MacOS/OctoBrew" --brew="$work/brew" --config="$work/config" &
pid=$!
sleep 5
kill -0 "$pid" 2>/dev/null || { echo "FAIL: o aplicativo saiu sozinho" >&2; exit 1; }
kill "$pid"
wait "$pid" 2>/dev/null || true
echo "== bundle ok"
