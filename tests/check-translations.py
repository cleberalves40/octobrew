#!/usr/bin/env python3
# Todo texto da GUI (ui::tr e ui::trf com uma chave literal, e os rótulos da
# barra da aba Serviços) tem de estar no catálogo da GUI (src/gui/texts.cpp)
# ou no do núcleo (tui/src/core/i18n.cpp), com o português e o espanhol.
import glob, re, sys

root = sys.argv[1] if len(sys.argv) > 1 else "."
src = "".join(open(f, encoding="utf-8").read() for f in glob.glob(root + "/src/**/*.cpp", recursive=True))
used = set(re.findall(r'\bui::trf?\(\s*"((?:[^"\\]|\\.)*)"', src))
used |= set(re.findall(r'std::make_pair\("[a-z]+", "((?:[^"\\]|\\.)*)"\)', src))
catalogs = ""
for f in (root + "/src/gui/texts.cpp", root + "/tui/src/core/i18n.cpp"):
    catalogs += open(f, encoding="utf-8").read()
entries = {m.group(1): (m.group(2), m.group(3))
           for m in re.finditer(r'^\s*\{"((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)"\},', catalogs, re.M)}
missing = sorted(k for k in used if k not in entries)
empty = sorted(k for k in used if k in entries and (not entries[k][0] or not entries[k][1]))
for k in missing:
    print("sem tradução:", k)
for k in empty:
    print("tradução vazia:", k)
print(f"{len(used)} textos, {len(missing) + len(empty)} com problema")
sys.exit(1 if missing or empty else 0)
