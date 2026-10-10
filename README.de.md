<div align="center">

**Deutsch** | [English](README.md) | [简体中文](README.zh-CN.md) | [Français](README.fr.md) | [Español](README.es.md) | [日本語](README.ja.md)

</div>

# Ctron

**Die KI-native Systemsprache** — gebaut für „KI schreibt, der Mensch prüft“: Fehler werden zur Kompilierzeit abgefangen, der Code bedeutet exakt das, was er sagt, keine Magie.

```bash
ctron run main.ct      # interpretieren und ausführen — keine externen Abhängigkeiten
ctron check main.ct    # statische Prüfungen — --format=json für strukturierte Diagnosen
ctron build main.ct    # äquivalentes C ausgeben → lokaler cc → natives Programm
```

- Repository: <https://github.com/ZturnLibs/Ctron>
- Dokumentationsseite: <https://zturnlibs.github.io/Ctron/> (English / 简体中文)

---

> **Projektstatus:** Ctron befindet sich in aktiver Entwicklung und ist **noch nicht für den Produktiveinsatz empfohlen** — Sprache und Toolchain entwickeln sich noch. Interessierte Entwickler sind herzlich eingeladen, mitzubauen: siehe [Mitwirken](#mitwirken).

## Warum Ctron

KI-generierter Code zeigt ein stabiles Fehlermuster: **stimmige Struktur, fehlerhafte Details** — vertippte Bezeichner, Typkonflikte, übersehene Randfälle. Dynamische Sprachen decken solche Fehler erst zur Laufzeit auf; in C/C++ können sie zu undefiniertem Verhalten werden. Ctrons Antwort ist nicht „eine bessere KI“, sondern ein strengeres Sprachdesign:

- **Jede Diagnose trägt einen stabilen Fehlercode** (E2010, E2030, E3040 …). Mit einem Fehlercode kann eine KI gezielt beheben — kein Raten der Absicht aus Fließtext.
- **Strukturierte Diagnosen** (`ctron check --format=json`), die Editoren und CI-Pipelines direkt verarbeiten.
- **Eindeutige Grammatik und explizite Typinferenzregeln** — derselbe Code erhält überall dasselbe Urteil.

**Keine Magie.** Was eine KI an einer Sprache fürchtet, ist nicht komplexe Syntax, sondern das, was die Sprache hinter ihrem Rücken tut:

| Was Ctron nicht hat | Was stattdessen verwendet wird |
|---|---|
| Kein null | `Option[T]` |
| Keine impliziten numerischen Konvertierungen | explizit `as[T]()` |
| Kein Operator-Overloading | `+` bedeutet Addition |
| Keine Makros | keine textuelle Substitution — der Quelltext ist die Wahrheit |
| Kein undefiniertes Verhalten | die Spezifikation zählt jedes legale Verhalten auf; was sie nicht ausweist, existiert nicht |

**Alles ist Quelltext.** Die Standardbibliothek wird als Ctron-Quelltext ausgeliefert, nicht als kompilierte Binärdateien — `use std.*` liest den Quelltext zur Kompilierzeit und verschmilzt ihn mit dem eigenen Programm zu einem einzigen AST. Eine KI kann den Quelltext der Standardbibliothek lesen, um das Verhalten von APIs zu verstehen — Dokumentation nicht erforderlich.

## Schnellstart

```bash
# macOS-/Linux-Installation per Einzeiler (SHA256-verifiziert)
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
export PATH="$HOME/.ctron/bin:$PATH"
```

```bash
$ ctron new hello && cd hello     # erzeugt das Projektgerüst Ctron.toml + Ctron.ctcl + src/main.ct

$ ctron run src/main.ct           # parsen, prüfen, auswerten — eine Pipeline
hello, ctron

$ ctron check src/main.ct --format=json
{"diagnostics":[]}

$ ctron build src/main.ct         # gibt lesbaren, in sich geschlossenen C-Code aus und ruft dann den lokalen cc auf
$ ./src/main
hello, ctron
```

`run` / `check` / `new` benötigen keine C-Toolchain; nur `build` erfordert einen lokalen cc (die Umgebungsvariable `CC` hat Vorrang). Unter Windows funktionieren `run` / `check` ohne weitere Vorbereitung; `build` benötigt mingw-w64. Exit-Codes: `0` Erfolg / `1` Programm-Diagnosen fehlgeschlagen / `2` Fehler in Umgebung oder Aufruf.

## Die Sprache auf einen Blick

Kein null, `Result`-Propagation, Fehlerketten, Tests in der Sprache selbst:

```ctron
@derive(Error)
enum MathErr {
    DivByZero
}

fn safe_div(a: I32, b: I32) -> Result[I32, MathErr] {
    if b == 0 { return Err(DivByZero) }
    return Ok(a / b)
}

fn ratio(a: I32, b: I32, c: I32) -> Result[I32, MathErr] {
    let x = safe_div(a, b)?                   // `?` propagiert und hängt dabei Kontext an
    return safe_div(x, c)
}

test "result propagation with ?" {
    assert_eq(ratio(100, 5, 4).or(-1), 5)
    match ratio(1, 0, 2) {
        Err(DivByZero) => assert(true)
        Ok(_)          => assert(false)
    }
}

test "error context chain" {
    let res = ratio(1, 0, 1).context("computing ratio")
    match res {
        Err(e) => {
            assert_eq(e.message, "computing ratio")
            assert(e.cause.is_some())          // die Ursachenkette bewahrt die Wurzel
        }
        Ok(_) => assert(false)
    }
}
```

Ein echtes, vollständiges Werkzeug (`examples/ctwc`, ein Dienstprogramm im Stil von wc):

```ctron
use std.str.{words, count_ch}

fn main() -> I32 {
    var path = ctron_entry()
    if path == "" {
        println("ctwc: usage: ctwc run <file>")
        return 1
    }
    match read_file(path) {
        Some(s) => {
            var nw = words(s).len
            var nl = count_ch(s, 10)
            println(nl.to_string() + " " + nw.to_string() + " " + s.len.to_string() + " " + path)
        }
        None => {
            println("ctwc: cannot open " + path)
            return 1
        }
    }
    return 0
}
```

Und darüber hinaus:

- **Strukturierte Nebenläufigkeit** — Tasks laufen unter `scope` / `spawn` / `join`, ihre Lebensdauer ist an den Block-Scope gebunden; ein Abbruch pflanzt sich entlang des Scopes fort, und ein Task, der in Panik gerät, löst einen strukturierten Abbruch-Broadcast aus. `Send`-Prüfungen laufen vollständig zur Kompilierzeit.
- **Zwei Speicherebenen** — über dem GC sitzen `own`-Blöcke (eine GC-freie Speicherteilmenge innerhalb eines Scopes) und `Arena`; das `bare`-Profil weist GC-Allokation vollständig zurück und zielt auf Bare-Metal-Mikrocontroller.
- **Ausführung zur Kompilierzeit** — `const` / `comptime fn` werden zur Kompilierzeit ausgewertet; `@derive(Show, Eq)` synthetisiert gängige Impls; Codeblöcke in `///`-Doc-Kommentaren werden kompiliert und ausgeführt (Doc-Test).
- **Explizite Capabilities** — I/O-Berechtigungen sind explizite `Cap`-Objekte, deklariert im Paketmanifest; jede Überschreitung scheitert zur Kompilierzeit.
- **FFI erster Klasse** — `extern "c"`-Deklarationen, dazu automatisch gelinkte `c_src/*.c`.

## Zwei Ausführungswege

```
ctron run main.ct      interpretieren: keine externen Abhängigkeiten, schnellster Entwicklungsloop
ctron build main.ct    C ausgeben → lokaler cc: natives Programm, durchgehend dieselbe Sprache
```

Das erzeugte Artefakt ist **lesbares, in sich geschlossenes C** (Runtime vollständig inline, ausschließlich System-Header) und bleibt zur Nachprüfung neben dem Quelltext liegen. Eine einzige Quellcodebasis deckt Entwicklung und Auslieferung ab.

## Standardbibliothek und Domänenpakete

Die Standardbibliothek wird **als Quelltext** zusammen mit der Toolchain ausgeliefert; Domänenpakete hängen an Top-Level-Namespaces: `use std.*` / `use net.*` / `use gui.*`.

| Paket | Inhalt |
|---|---|
| `std` | 26 Module: json / csv / uuid / crypto (SHA-256, HMAC, PBKDF2) / fmap / heap / iter / sort / unicode / strconv / rand / path / time… |
| `net` | TCP-Transport-Fassade: listen / connect / accept / read / write |
| `http` | HTTP/1.1-Parser + Client + WebSocket + SSE + eine **Web-Framework-Middleware-Familie** (router / JWT auth / cors / csrf / openapi / metrics / timeout / static…) |
| `tls` | TLS-Fassade |
| `db` | Wire-Protokoll-Treiber in reinem Ctron: **PostgreSQL** / RESP2 (Redis) + Connection-Pool + Zeilenzuordnung |
| `ffi` | Fehlerumhüllung an der C-Grenze (errno in `Result` eingefaltet) |

## Die Toolchain auf einen Blick

`ctron` ist der einzige Einstiegspunkt für Nutzer:

| Befehl | Zweck |
|---|---|
| `run` / `check` / `build` | interpretieren / statische Prüfungen (JSON-Diagnosen) / ausgeben und bauen |
| `test` | in der Sprache geschriebene `test`-Blöcke ausführen |
| `fmt` | kanonischer Formatierer |
| `lint` | Diagnoseübersicht (`--strict` stuft Warnungen zu Fehlern hoch) |
| `doc` | Schnittstellenprojektion: öffentliche Symboltabelle + Vertragskommentare |
| `bench` | Benchmark-Familien: lang / gc / http / net / ffi |
| `new` | Projektgerüst |
| `add` / `publish` / `lock` | Pakete installieren / veröffentlichen / Lockdatei |

## Editor-Unterstützung

- **Language Server** (`lsp/`) — Diagnosen, documentSymbol, Hover, Vervollständigung, Definition/Referenzen, Umbenennen, Signaturhilfe, Inlay-Hints, Formatierung, Folding.
- **VS-Code-Erweiterung** (`editors/vscode-ctron`) — Syntaxhervorhebung, Snippets, semantische Diagnosen beim Speichern mit Quick Fixes, semantische Tokens, Inlay-Hints für Parameternamen; unterstützt ebenfalls CTML (GUI-Markup) und CTCL (Konfigurationssprache).

## Status und Grenzen

- Die erste Veröffentlichung **v0.0.1** ist erschienen; die Sprachspezifikation ist ein eingefrorener Entwurf (v0.8). Falls der Download-Weg nicht erreichbar ist, aus dem Quelltext bauen — siehe [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md).
- Gut geeignet für: CLI-Werkzeuge, Netzwerkdienste, Compiler, Parser.
- Nicht geeignet für: GPU-lastige Berechnungen; harte Echtzeitsysteme (GC-Pausen sind unkontrollierbar — das `bare`-Profil mildert sie, hebt sie aber nicht auf).
- `build` und die Windows-Unterstützung sind insgesamt in β.

## Mitwirken

Issues, Beispiele, Dokumentation und Code sind gleichermaßen willkommen — die meisten Beiträge erfordern kein Wissen um Compiler-Interna.

**Einen Fehler melden** — ein Issue mit einem minimalen `.ct`-Reproduktor und der Ausgabe von `ctron check <file> --format=json` öffnen. Diagnosen tragen stabile Fehlercodes, sodass sich Ursachen gezielt beheben lassen.

**Das Ökosystem ausbauen** — Module der Standardbibliothek, Beispiele, Editor-Werkzeuge, Dokumentation und die Website: PRs sind willkommen.

**Die Sprache weiterentwickeln** — die Spezifikation ist ein eingefrorener Entwurf: Am Anfang steht eine Design-Diskussion in einem Issue; angenommene Änderungen landen als Revisionseintrag in der Spezifikation plus Korpus-Pins in `tests/`. Das Sprachverhalten muss über alle drei Implementierungen hinweg übereinstimmen (C-Seed / Rust-Referenz / self-hosted Compiler) — mit der Konformitätssuite als Schiedsrichter. Neue Diagnosen brauchen einen stabilen Fehlercode, registriert im Fehlercode-Register der Spezifikation.

**Entwicklungsworkflow**

```bash
git clone https://github.com/ZturnLibs/Ctron && cd Ctron
bash ci.sh    # ein Befehl: bootstrapt die Toolchain und durchläuft das vollständige Abnahme-Gate (21 Schritte via ctron gate; siehe ctron.ctcl)
```

PRs sollten erst mit grünem `ci.sh` landen. `ctron` (sh) und `ctron.ps1` / `ctron.cmd` verhaltensidentisch halten. Commit-Messages folgen Conventional Commits (`feat:` / `fix:` / `docs:`).

## Dokumentationsübersicht

| Dokument | Inhalt |
|---|---|
| [docs/spec/](docs/spec/README.md) | Sprachspezifikation v0.8 (12 Kapitel) |
| [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md) | Toolchain aus dem Quelltext bauen |
| [examples/](examples/) | Vollständige Beispielanwendungen |
| [website/](https://zturnlibs.github.io/Ctron/) | Einstieg, Stdlib-Referenz, Beispiele, Downloads |

## Lizenz

MIT — siehe [LICENSE](LICENSE).
