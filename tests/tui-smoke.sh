#!/bin/sh
#
# Teste de fumaça da interface num pseudo-terminal, com um brew falso: ela
# precisa listar os pacotes, carregar os detalhes do selecionado, calcular os
# tamanhos, marcar um pacote (+), abrir o menu (F9), passar à visão Repositório (i), abrir as
# Preferências (Alt+,) e sair com "q". Funciona com o script(1) do BSD/macOS
# e com o do util-linux. Não precisa do Homebrew.
#
# uso: tests/tui-smoke.sh [diretório-de-build]

set -eu

build=$(cd "${1:-build}" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/octobrew-tui-smoke.XXXXXX")
trap 'rm -rf "$work"' EXIT

mkdir -p "$work/Cellar/curl/8.9.1" "$work/Caskroom" "$work/cache/api/internal"
head -c 3072 /dev/zero >"$work/Cellar/curl/8.9.1/curl"
cat >"$work/installed.json" <<'EOF'
{"formulae": [
  {"name": "curl", "full_name": "curl", "tap": "homebrew/core", "desc": "Get a file from an HTTP server",
   "versions": {"stable": "8.10.0"}, "revision": 0, "pinned": false, "outdated": true,
   "installed": [{"version": "8.9.1", "time": 1760000000, "installed_on_request": true, "runtime_dependencies": []}]},
  {"name": "tela-ok", "full_name": "tela-ok", "tap": "homebrew/core", "desc": "Smoke test",
   "versions": {"stable": "1.0"}, "revision": 0, "pinned": false, "outdated": false,
   "installed": [{"version": "1.0", "time": 1760000000, "installed_on_request": false, "runtime_dependencies": []}]}
 ],
 "casks": []}
EOF
cat >"$work/curl.json" <<'EOF'
{"formulae": [{"name": "curl", "full_name": "curl", "tap": "homebrew/core", "desc": "Get a file",
  "homepage": "https://curl.se", "license": "curl", "versions": {"stable": "8.10.0"}, "revision": 0,
  "caveats": "curl is keg-only.", "keg_only": true, "keg_only_reason": {"reason": ":provided_by_macos"},
  "dependencies": [], "build_dependencies": [], "conflicts_with": [],
  "installed": [{"version": "8.9.1", "time": 1760000000}]}], "casks": []}
EOF
printf '%s' '{"payload":"{\"formulae\":{\"curl\":{\"desc\":\"Get a file\",\"stable_version\":\"8.10.0\"},\"vim-remoto\":{\"desc\":\"Vi\",\"stable_version\":\"9.1\"}},\"casks\":{}}"}' \
	>"$work/cache/api/internal/packages.test.jws.json"

cat >"$work/brew" <<EOF
#!/bin/sh
case "\$1" in
--cellar) echo "$work/Cellar" ;;
--caskroom) echo "$work/Caskroom" ;;
--cache) echo "$work/cache" ;;
--version) echo "Homebrew 7.0.10" ;;
info)
	case "\$3" in
	--installed) cat "$work/installed.json" ;;
	--formula) [ -f "$work/\$4.json" ] && cat "$work/\$4.json" || { echo "Error: No available formula" >&2; exit 1; } ;;
	esac ;;
list) echo "$work/Cellar/curl/8.9.1/curl" ;;
esac
EOF
chmod +x "$work/brew"

esc=$(printf '\033')
plain() {
	LC_ALL=C sed -e "s#${esc}\[[0-9;?]*[ -/]*[@-~]##g" -e "s#${esc}[()][0-9A-Za-z]##g" -e "s#${esc}[=>]##g" "$1"
}

cmd="$build/octobrew-tui --brew=$work/brew --icons=ascii --lang=en_US --config=$work/config"
# Espera o texto aparecer na tela (até 60 s).
wait_for() {
	for i in $(seq 1 60); do
		plain "$work/screen.raw" 2>/dev/null | grep -aq "$1" && return 0
		sleep 1
	done
}
keys() {
	wait_for "tela-ok"
	wait_for "Installed version:"   # curl, o selecionado: os detalhes carregaram
	wait_for "3.00 KiB"
	printf '+'          # curl, desatualizado: marcado para instalar
	wait_for "Actions (1)"
	printf '\033[20~'   # F9
	wait_for "Refresh the package list"
	printf '\033'
	sleep 1
	printf 'i'          # visão Repositório
	wait_for "vim-remoto"
	printf '\033,'
	wait_for "System (terminal colors)"
	printf '\033'
	sleep 1
	printf q
}
set +e
if script --version 2>/dev/null | grep -q util-linux; then
	keys | TERM=xterm-256color timeout 120 script -q -f -e -c "$cmd" "$work/screen.raw" >/dev/null 2>&1
else
	# shellcheck disable=SC2086
	keys | TERM=xterm-256color script -F -q "$work/screen.raw" $cmd >/dev/null 2>&1
fi
rc=$?
set -e

plain "$work/screen.raw" >"$work/screen.txt"
[ "$rc" -eq 0 ] || { tail -c 4000 "$work/screen.txt"; echo "FAIL: octobrew-tui terminou com status $rc" >&2; exit 1; }
for text in "OctoBrew" "Installed packages" "tela-ok" "2 package(s)" "1 outdated" "Installed version:" "8.9.1" \
	"3.00 KiB" "Actions (1)" "Refresh the package list" "Update Homebrew" "Repository" "vim-remoto" "Preferences" "System (terminal colors)"; do
	grep -aq "$text" "$work/screen.txt" || {
		tail -c 4000 "$work/screen.txt"
		echo "FAIL: a tela não mostrou: $text" >&2
		exit 1
	}
done
echo "== interface ok"
