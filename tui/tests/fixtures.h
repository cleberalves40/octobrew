// Amostras da saída do brew 7.0.10 (macOS 27, arm64), reduzidas aos campos
// que o octobrew-tui lê, mais os casos que o brew real nem sempre mostra:
// pin, desatualizado, tap de terceiros e fórmula e cask de mesmo nome.
#ifndef OCTOBREW_TUI_TEST_FIXTURES_H
#define OCTOBREW_TUI_TEST_FIXTURES_H

#include "support.h"

#include <string>
#include <vector>

// "brew info --json=v2 --installed"
const char kInstalledJson[] = R"json({
  "formulae": [
    {
      "name": "curl", "full_name": "curl", "tap": "homebrew/core",
      "desc": "Get a file from an HTTP, HTTPS or FTP server",
      "versions": {"stable": "8.10.0", "head": "HEAD", "bottle": true},
      "revision": 0, "pinned": false, "outdated": true, "linked_keg": null,
      "installed": [
        {"version": "8.9.1", "time": 1760000000, "installed_on_request": true,
         "runtime_dependencies": [
           {"full_name": "brotli", "version": "1.2.0", "declared_directly": true},
           {"full_name": "zstd", "version": "1.5.7", "declared_directly": false}
         ]}
      ]
    },
    {
      "name": "brotli", "full_name": "brotli", "tap": "homebrew/core",
      "desc": "Generic-purpose lossless compression algorithm",
      "versions": {"stable": "1.2.0"}, "revision": 0, "pinned": false, "outdated": false,
      "linked_keg": "1.2.0",
      "installed": [
        {"version": "1.1.0", "time": 1750000000, "installed_on_request": false, "runtime_dependencies": []},
        {"version": "1.2.0", "time": 1760000100, "installed_on_request": false, "runtime_dependencies": []}
      ]
    },
    {
      "name": "wget", "full_name": "wget", "tap": "homebrew/core",
      "desc": "Internet file retriever",
      "versions": {"stable": "1.25.0"}, "revision": 1, "pinned": true, "outdated": true, "linked_keg": "1.24.5",
      "installed": [
        {"version": "1.24.5", "time": 1740000000, "installed_on_request": true,
         "runtime_dependencies": [{"full_name": "brotli", "version": "1.2.0", "declared_directly": true}]}
      ]
    },
    {
      "name": "foo", "full_name": "someone/tap/foo", "tap": "someone/tap",
      "desc": "A formula from a third-party tap",
      "service": {"run": ["/opt/homebrew/opt/foo/bin/food"], "keep_alive": {"always": true}},
      "versions": {"stable": "0.3"}, "revision": 0, "pinned": false, "outdated": false, "linked_keg": "0.3",
      "installed": [{"version": "0.3", "time": 1745000000, "installed_on_request": true, "runtime_dependencies": []}]
    }
  ],
  "casks": [
    {
      "token": "alacritty", "full_token": "alacritty", "tap": "homebrew/cask",
      "name": ["Alacritty"], "desc": "GPU-accelerated terminal emulator",
      "version": "0.17.0", "installed": "0.17.0", "installed_time": 1776215584,
      "outdated": false, "pinned": false, "auto_updates": null,
      "depends_on": {"macos": {}},
      "artifacts": [
        {"app": ["Alacritty.app"], "target": "APPDIR/Alacritty.app"},
        {"binary": ["/Applications/Alacritty.app/Contents/MacOS/alacritty"], "target": "/opt/homebrew/bin/alacritty"},
        {"zap": [{"trash": ["~/Library/Saved Application State/org.alacritty.savedState"]}]}
      ]
    },
    {
      "token": "docker", "full_token": "docker", "tap": "homebrew/cask",
      "name": ["Docker Desktop"], "desc": null,
      "version": "4.40.0", "installed": "4.38.0", "installed_time": 1770000000,
      "outdated": true, "pinned": false, "auto_updates": true,
      "depends_on": {"formula": ["brotli"]}, "artifacts": []
    }
  ]
})json";

// O cache da API do brew 5 ou mais novo (api/internal/packages.<tag>.jws.json):
// o JSON vai como string no "payload" do envelope JWS.
const char kApiPayload[] = R"json({"metadata":{"homebrew_version":"7.0.10","bottle_tag":"arm64_golden_gate"},)json"
                           R"json("formulae":{)json"
                           R"json("brotli":{"desc":"Generic-purpose lossless compression algorithm","stable_version":"1.2.0"},)json"
                           R"json("curl":{"desc":"Get a file from an HTTP, HTTPS or FTP server","stable_version":"8.10.0","stable_dependencies":["brotli",{"pkgconf":":build"}]},)json"
                           R"json("docker":{"desc":"Pack, ship and run any application as a lightweight container","stable_version":"28.0.4"},)json"
                           R"json("wget":{"desc":"Internet file retriever","stable_version":"1.25.0","revision":1},)json"
                           R"json("vim":{"desc":"Vi 'workalike' with many additional features","stable_version":"9.1.1000"})json"
                           R"json(},"casks":{)json"
                           R"json("alacritty":{"desc":"GPU-accelerated terminal emulator","names":["Alacritty"],"tap_string":"homebrew/cask","version":"0.16.0"},)json"
                           R"json("docker":{"names":["Docker Desktop"],"tap_string":"homebrew/cask","version":"4.40.0"},)json"
                           R"json("kitty":{"desc":"GPU-based terminal emulator","names":["kitty"],"tap_string":"homebrew/cask","version":"0.49.2"})json"
                           R"json(}})json";

// formula.jws.json do brew 4: um array no formato do "brew info --json=v2".
const char kLegacyFormulae[] = R"json([
  {"name": "curl", "full_name": "curl", "tap": "homebrew/core", "desc": "Get a file",
   "versions": {"stable": "8.10.0"}, "revision": 0, "installed": []},
  {"name": "wget", "full_name": "wget", "tap": "homebrew/core", "desc": "Internet file retriever",
   "versions": {"stable": "1.25.0"}, "revision": 1, "installed": []}
])json";

// cask.jws.json do brew 4.
const char kLegacyCasks[] = R"json([
  {"token": "kitty", "full_token": "kitty", "tap": "homebrew/cask", "name": ["kitty"],
   "desc": "GPU-based terminal emulator", "version": "0.49.2", "installed": null}
])json";

// "brew info --json=v2 --formula curl"
const char kCurlInfo[] = R"json({
  "formulae": [{
    "name": "curl", "full_name": "curl", "tap": "homebrew/core",
    "desc": "Get a file from an HTTP, HTTPS or FTP server",
    "homepage": "https://curl.se", "license": "curl",
    "versions": {"stable": "8.10.0"}, "revision": 0,
    "caveats": "curl is keg-only, which means it was not symlinked into /opt/homebrew,\nbecause macOS already provides this software.\n\nIf you need to have curl first in your PATH, run:\n  echo 'export PATH=\"/opt/homebrew/opt/curl/bin:$PATH\"' >> ~/.zshrc\n",
    "keg_only": true, "keg_only_reason": {"reason": ":provided_by_macos", "explanation": ""},
    "deprecated": false, "deprecation_reason": null, "disabled": false, "disable_reason": null,
    "dependencies": ["brotli", "libnghttp2"], "build_dependencies": ["pkgconf"], "conflicts_with": [],
    "installed": [{"version": "8.9.1", "time": 1760000000, "installed_on_request": true}],
    "linked_keg": null
  }],
  "casks": []
})json";

// "brew info --json=v2 --cask kitty" (não instalado)
const char kKittyInfo[] = R"json({
  "formulae": [],
  "casks": [{
    "token": "kitty", "full_token": "kitty", "tap": "homebrew/cask", "name": ["kitty"],
    "desc": "GPU-based terminal emulator", "homepage": "https://github.com/kovidgoyal/kitty",
    "version": "0.49.2", "installed": null, "installed_time": null, "auto_updates": null, "caveats": null,
    "depends_on": {"macos": {">=": ["12"]}}, "conflicts_with": {"cask": ["kitty@nightly"]},
    "deprecated": true, "deprecation_reason": ":unmaintained",
    "artifacts": [
      {"app": ["kitty.app"], "target": "/Applications/kitty.app"},
      {"command_wrapper": ["kitty", {"executable": "/Applications/kitty.app/Contents/MacOS/kitty"}], "target": "/opt/homebrew/bin/kitty"},
      {"uninstall": [{"quit": "net.kovidgoyal.kitty"}]},
      {"zap": [{"trash": ["~/.config/kitty"]}]}
    ]
  }]
})json";

// "brew install --dry-run --formula figlet wget2" (brew 7.0.10), com as linhas
// de download que o brew intercala.
const char kInstallDryRun[] = "==> Would install 2 formulae:\n"
                              "figlet  2.2.5\n"
                              "wget2   2.3.0\n"
                              "==> Downloading https://ghcr.io/v2/homebrew/core/figlet/manifests/2.2.5\n"
                              "==> Downloading https://ghcr.io/v2/homebrew/core/wget2/manifests/2.3.0\n"
                              "==> Would install 2 dependencies for wget2:\n"
                              "libgpg-error  1.61\n"
                              "lzlib         1.16\n";

// "brew install --dry-run --cask kitty"
const char kInstallCaskDryRun[] = "==> Would install 1 cask:\nkitty\n";

// "brew upgrade --dry-run": as fórmulas e os casks em resumos separados, e
// os casks desativados como aviso (no stderr).
const char kUpgradeDryRun[] = "==> Would upgrade 1 outdated package:\n"
                              "curl 8.9.1 -> 8.10.0\n"
                              "==> Would upgrade 1 outdated package:\n"
                              "docker 4.38.0 -> 4.40.0\n";
const char kUpgradeDryRunErr[] = "Warning: Not upgrading alacritty, it is disabled because it does not pass the "
                                 "macOS Gatekeeper check! It was disabled on 2026-09-01.\n";

// "brew autoremove --dry-run"
const char kAutoremoveDryRun[] = "==> Would autoremove 2 unneeded formulae:\nlibgpg-error\nlzlib\n";

// "brew cleanup --dry-run --prune=all"
const char kCleanupDryRun[] =
    "Would remove: /opt/homebrew/Cellar/curl/8.9.0 (558 files, 4.7MB)\n"
    "Would remove: /Users/x/Library/Caches/Homebrew/downloads/abc--curl-8.9.0.bottle.tar.gz (1.5MB)\n"
    "==> This operation would free approximately 6.2MB of disk space.\n";

// Um cask com instalador .pkg (pede o sudo), no formato do "brew info --json=v2 --cask".
const char kPkgCaskInfo[] = R"json({"formulae": [], "casks": [
  {"token": "zoom", "full_token": "zoom", "tap": "homebrew/cask", "version": "7.0.0",
   "artifacts": [{"pkg": ["zoomusInstallerFull.pkg"]},
                 {"uninstall": [{"launchctl": "us.zoom.ZoomDaemon", "pkgutil": "us.zoom.pkg.videomeeting"}]}]},
  {"token": "kitty", "full_token": "kitty", "tap": "homebrew/cask", "version": "0.49.2",
   "artifacts": [{"app": ["kitty.app"]}, {"zap": [{"launchctl": "x", "trash": ["~/.config/kitty"]}]}]}
]})json";

// Trecho do https://brew.sh/atom.xml (Jekyll): título "html", link alternate,
// summary em HTML escapado e content em CDATA.
const char kAtomFeed[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?><feed xmlns=\"http://www.w3.org/2005/Atom\" >"
    "<link href=\"https://brew.sh/atom.xml\" rel=\"self\" type=\"application/atom+xml\" />"
    "<title type=\"html\">Homebrew</title>"
    "<entry><title type=\"html\">7.0.0</title>"
    "<link href=\"https://brew.sh/2026/09/13/homebrew-7.0.0/\" rel=\"alternate\" type=\"text/html\" title=\"7.0.0\" />"
    "<published>2026-09-13T00:00:00+00:00</published><updated>2026-09-13T00:00:00+00:00</updated>"
    "<content type=\"html\"><![CDATA[<p>Long post &amp; more.</p>]]></content>"
    "<summary type=\"html\">&lt;p&gt;Today, I&#8217;m proud to announce Homebrew 7.0.0. Faster installations, "
    "stronger sandboxing and a native macOS app.&lt;/p&gt;</summary></entry>"
    "<entry><title type=\"html\">Homebrew&#8217;s new git signing key</title>"
    "<link href=\"https://brew.sh/2025/01/01/key/\" rel=\"alternate\" type=\"text/html\" />"
    "<published>2025-01-01T10:00:00+00:00</published>"
    "<content type=\"html\"><![CDATA[<p>We rotated the key.</p><p>Second paragraph.</p>]]></content></entry>"
    "</feed>";

// "brew services list --json" (brew 7).
const char kServicesJson[] = R"json([
  {"name": "foo", "status": "started", "user": "cleber", "file": "/Users/cleber/Library/LaunchAgents/homebrew.mxcl.foo.plist", "exit_code": 0},
  {"name": "php", "status": "none", "user": null, "file": "/opt/homebrew/opt/php/sh.brew.php.plist", "exit_code": null},
  {"name": "postgresql@17", "status": "error", "user": "cleber", "file": "/Users/cleber/Library/LaunchAgents/homebrew.mxcl.postgresql@17.plist", "exit_code": 78}
])json";

// "brew tap-info --json --installed", reduzido.
const char kTapsJson[] = R"json([
  {"name": "homebrew/core", "official": true, "trusted": true, "installed": true,
   "formula_names": ["curl", "wget"], "cask_tokens": []},
  {"name": "someone/tap", "official": false, "trusted": false, "installed": true,
   "formula_names": ["someone/tap/foo", "someone/tap/bar"], "cask_tokens": ["someone/tap/baz-app"]}
])json";

// "brew doctor" com avisos (brew 7.0.10): sai com 1.
const char kDoctorOutput[] =
    "Please note that these warnings are just used to help the Homebrew maintainers\n"
    "with debugging if you file an issue.\n\n"
    "Warning: Some installed casks are deprecated or disabled.\n"
    "You should find replacements for the following casks:\n\n  alacritty\n\n"
    "Warning: Some installed kegs have no formulae!\n"
    "This means they were either deleted or installed manually.\n";

// "brew bundle check --verbose" com o que falta.
const char kBundleCheckOutput[] = "brew bundle can't satisfy your Brewfile's dependencies.\n"
                                  "\xE2\x86\x92 Cask iterm2 needs to be installed or updated.\n"
                                  "\xE2\x86\x92 Formula figlet needs to be installed or updated.\n"
                                  "Satisfy missing dependencies with `brew bundle install`.\n";

// Uma string JSON com o texto (para montar o envelope JWS).
inline std::string quoted(const std::string &s)
{
    std::string out = "\"";
    for (char c : s) {
        if (c == '\n') {
            out += "\\n";
            continue;
        }
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    return out + "\"";
}

/** O envelope JWS do cache da API: {"payload": "<json>", "signatures": [...]}. */
inline std::string jws(const std::string &payload)
{
    return "{\"payload\":" + quoted(payload) + ",\"signatures\":[{\"signature\":\"x\"}]}";
}

/**
 * brew falso que responde como o brew 7.0.10: os diretórios (--cellar,
 * --caskroom, --cache, todos dentro de dir), --version, o info dos
 * instalados (kInstalledJson), o info de curl e kitty e o list. As prévias
 * (--dry-run) devolvem o arquivo <verbo>-dry.txt, se houver; as operações
 * escrevem "executando <verbo>" e falham se houver <verbo>.rc; o bundle dump
 * grava um Brewfile no --file. extra vem antes, para mudar uma resposta.
 */
struct FakeBrew {
    TempDir dir;
    std::string log = dir.file("brew.log");
    std::string path;

    explicit FakeBrew(const std::string &extra = std::string())
    {
        writeFile(dir.file("installed.json"), kInstalledJson);
        writeFile(dir.file("curl.json"), kCurlInfo);
        writeFile(dir.file("kitty.json"), kKittyInfo);
        const std::string d = dir.path;
        path = fakeBrew(dir, log,
                        extra + "\n"
                        "D='" + d + "'\n"
                        "fail() { [ -f \"$D/$1.rc\" ] && { echo 'Warning: aviso' >&2; echo \"Error: $1 falhou\" >&2; exit 1; }; return 0; }\n"
                        "case \"$1\" in\n"
                        "--cellar) echo \"$D/Cellar\" ;;\n"
                        "--caskroom) echo \"$D/Caskroom\" ;;\n"
                        "--cache) echo \"$D/cache\" ;;\n"
                        "--version) echo 'Homebrew 7.0.10'; echo 'Homebrew/homebrew-core (git revision x)' ;;\n"
                        "info)\n"
                        "  case \"$3\" in\n"
                        "  --installed) cat \"$D/installed.json\" ;;\n"
                        "  --cask) f=\"$D/$4.json\"; [ -f \"$f\" ] && cat \"$f\" || echo '{\"formulae\": [], \"casks\": []}' ;;\n"
                        "  *) f=\"$D/$4.json\"\n"
                        "     [ -f \"$f\" ] && cat \"$f\" && exit 0\n"
                        "     echo \"Error: No available formula with the name \\\"$4\\\".\" >&2; exit 1 ;;\n"
                        "  esac ;;\n"
                        "list) echo /opt/homebrew/Cellar/curl/8.9.1/bin/curl; echo /opt/homebrew/Cellar/curl/8.9.1/lib/libcurl.4.dylib ;;\n"
                        "install|upgrade|autoremove|cleanup)\n"
                        "  if [ \"$2\" = --dry-run ]; then\n"
                        "    fail \"$1-dry\"\n"
                        "    [ -f \"$D/$1-dry.txt\" ] && cat \"$D/$1-dry.txt\"\n"
                        "    exit 0\n"
                        "  fi\n"
                        "  echo \"executando $1\"; fail \"$1\" ;;\n"
                        "uninstall|update|pin|unpin|tap|untap|trust) echo \"executando $1\"; fail \"$1\" ;;\n"
                        "services)\n"
                        "  if [ \"$2\" = list ]; then [ -f \"$D/services.json\" ] && cat \"$D/services.json\" || echo '[]'; exit 0; fi\n"
                        "  echo \"executando services $2\"; fail services ;;\n"
                        "tap-info) [ -f \"$D/taps.json\" ] && cat \"$D/taps.json\" || echo '[]' ;;\n"
                        "doctor) [ -f \"$D/doctor.txt\" ] && { cat \"$D/doctor.txt\"; exit 1; }; echo 'Your system is ready to brew.' ;;\n"
                        "bundle)\n"
                        "  for a in \"$@\"; do case \"$a\" in --file=*) f=${a#--file=} ;; esac; done\n"
                        "  if [ \"$2\" = check ]; then\n"
                        "    [ -f \"$D/bundle-check.txt\" ] && { cat \"$D/bundle-check.txt\"; exit 1; }\n"
                        "    echo \"The Brewfile's dependencies are satisfied.\"; exit 0\n"
                        "  fi\n"
                        "  echo \"executando bundle $2\"; fail bundle\n"
                        "  [ \"$2\" = dump ] && echo 'brew \"curl\"' > \"$f\" ;;\n"
                        "esac\n"
                        "exit 0\n");
    }

    /** O que uma prévia (verbo --dry-run) imprime. */
    void preview(const std::string &verb, const std::string &output) const
    {
        writeFile(dir.file(verb + "-dry.txt"), output);
    }
    /** A próxima operação verbo falha (sai com 1, com "Error: verbo falhou"). */
    void failNext(const std::string &verb) const { writeFile(dir.file(verb + ".rc"), "1"); }
    /** As chamadas que mudam algo: sem as consultas e sem as prévias. */
    std::vector<std::string> operations() const
    {
        std::vector<std::string> out;
        for (const auto &l : readLines(log)) {
            const std::string verb = l.substr(0, l.find('|'));
            if (verb.rfind("--", 0) == 0 || verb == "info" || verb == "list" || verb == "tap-info" || verb == "doctor"
                || l.find("|--dry-run") != std::string::npos || l == "services|list|--json"
                || l.rfind("bundle|check|", 0) == 0)
                continue;
            out.push_back(l);
        }
        return out;
    }

    /** O cache da API do brew 7, com kApiPayload. */
    void writeApiCache() const
    {
        const std::string internal = dir.file("cache/api/internal");
        (void)std::system(("mkdir -p '" + internal + "'").c_str());
        writeFile(internal + "/packages.arm64_golden_gate.jws.json", jws(kApiPayload));
    }

    std::vector<std::string> calls() const { return readLines(log); }
};

#endif
