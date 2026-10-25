#!/bin/sh
#
# No macOS (ou no Linux com o Homebrew): os testes de núcleo, modelo e tela,
# a integração com o Homebrew real (só leitura: lista, detalhes, o cache da
# API e os tamanhos) e a interface num pseudo-terminal. Espera os pacotes de
# compilação instalados pelo brew (cmake, ninja, ftxui, catch2).
#
# Com --transactions, a integração também instala, fixa e remove o figlet
# (o CI faz isso num runner descartável).
#
# uso: tests/macos-e2e.sh [--transactions] [diretório-de-build]

set -eu

if [ "${1:-}" = --transactions ]; then
	export OCTOBREW_TEST_TRANSACTIONS=1
	shift
fi
build=${1:-build}
brew=$(command -v brew || true)
[ -n "$brew" ] || { echo "FAIL: brew não encontrado no PATH" >&2; exit 1; }
prefix=$("$brew" --prefix)

cmake -B "$build" -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$prefix" \
	-DOCTOBREW_TUI_BUILD_TESTS=ON
cmake --build "$build"

echo "== testes"
ctest --test-dir "$build" --output-on-failure

echo "== integração com $("$brew" --version | head -n 1)"
OCTOBREW_TEST_BREW="$brew" "$build/test_integration"

echo "== interface"
sh tests/tui-smoke.sh "$build"
"$build/octobrew-tui" --version
