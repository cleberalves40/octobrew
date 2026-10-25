#!/bin/sh
#
# Aponta a fórmula (homebrew/octobrew-tui.rb) para uma versão publicada:
# ajusta o url e o sha256 a partir do tarball que o GitHub serve para a tag,
# o mesmo que o brew baixa. Roda em qualquer sistema com sh, curl e
# sha256sum (ou shasum).
#
# O sha256 não pode vir no próprio release: o tarball da tag contém a
# fórmula, e o checksum mudaria ao incluí-lo. Por isso a fórmula acompanha a
# última tag publicada, num PR depois de cada release (veja docs/RELEASING.md),
# como o port do octopkg-tui.
#
# uso: ci/formula-sha.sh X.Y.Z [--check]
#   --check: não altera nada; confere se a fórmula bate com a tag.

set -eu

version=$1
mode=${2:-write}
account=cleberalves40
name=octobrew-tui
formula=homebrew/$name.rb
url="https://github.com/$account/$name/archive/refs/tags/v$version.tar.gz"

fail() { echo "FAIL: $*" >&2; exit 1; }
sha256() {
	if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
	else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

tmp=$(mktemp -d "${TMPDIR:-/tmp}/formula-sha.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
code=$(curl -sSL -o "$tmp/$name.tar.gz" -w '%{http_code}' "$url") || fail "não foi possível baixar $url"
[ "$code" = 200 ] || fail "a tag v$version não está publicada (HTTP $code)"
top=$(tar -tzf "$tmp/$name.tar.gz" | head -1)
[ "$top" = "$name-$version/" ] || fail "o tarball começa por $top, não por $name-$version/"
sum=$(sha256 "$tmp/$name.tar.gz")

if [ "$mode" = --check ]; then
	grep -qx "  url \"$url\"" "$formula" || fail "$formula não aponta para $url"
	grep -qx "  sha256 \"$sum\"" "$formula" || fail "$formula não tem o sha256 da tag v$version ($sum)"
	echo "$formula confere com a tag v$version: sha256 $sum"
	exit 0
fi

sed -i.bak -e "s#^  url \".*\"#  url \"$url\"#" -e "s#^  sha256 \".*\"#  sha256 \"$sum\"#" "$formula" && rm -f "$formula.bak"
echo "fórmula em $version:"
grep -E '^  (url|sha256) ' "$formula"
