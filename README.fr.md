<div align="center">

[English](README.md) | [简体中文](README.zh-CN.md) | [Deutsch](README.de.md) | **Français** | [Español](README.es.md) | [日本語](README.ja.md)

</div>

# Ctron

**Le langage de programmation système natif IA** — conçu pour « l'IA écrit, l'humain relit » : des erreurs interceptées à la compilation, du code qui dit exactement ce qu'il veut dire, aucune magie.

```bash
ctron run main.ct      # interprète et exécute — zéro dépendance externe
ctron check main.ct    # contrôles statiques — --format=json pour des diagnostics structurés
ctron build main.ct    # émet du C équivalent → cc local → exécutable natif
```

- Dépôt : <https://github.com/ZturnLibs/Ctron>
- Site de documentation : <https://zturnlibs.github.io/Ctron/> (English / 简体中文)

---

> **État du projet :** Ctron est en cours de développement actif et **n'est pas encore recommandé en production** — le langage et la toolchain évoluent encore. Les développeurs intéressés sont les bienvenus pour construire Ctron avec nous : voir [Contribuer](#contribuer).

## Pourquoi Ctron

Le code généré par IA présente un schéma d'échec stable : **structure correcte, détails erronés** — identifiants mal orthographiés, incompatibilités de types, cas limites oubliés. Les langages dynamiques révèlent ces défauts à l'exécution ; en C/C++, ils peuvent dégénérer en comportements indéfinis. La réponse de Ctron n'est pas « une meilleure IA », mais une conception de langage plus stricte :

- **Chaque diagnostic porte un code d'erreur stable** (E2010, E2030, E3040…). Forte d'un code, une IA peut corriger avec précision — sans avoir à deviner l'intention derrière la prose.
- **Diagnostics structurés** (`ctron check --format=json`), consommés directement par les éditeurs et les pipelines CI.
- **Grammaire sans ambiguïté et règles d'inférence de types explicites** — le même code reçoit le même verdict partout.

**Aucune magie.** Ce qu'une IA redoute dans un langage, ce n'est pas une syntaxe complexe, mais ce que le langage fait dans son dos :

| Ce que Ctron n'a pas | Ce que vous utilisez à la place |
|---|---|
| Pas de null | `Option[T]` |
| Pas de conversions numériques implicites | `as[T]()` explicite |
| Pas de surcharge d'opérateurs | `+` signifie addition |
| Pas de macros | pas de substitution textuelle — la source est la vérité |
| Pas de comportement indéfini | la spécification énumère chaque comportement légal ; ce qu'elle ne stipule pas n'existe pas |

**Tout est source.** La bibliothèque standard est livrée sous forme de source Ctron, et non de binaires compilés — `use std.*` lit la source à la compilation et la fusionne en un unique AST avec votre programme. Une IA peut lire la source de la stdlib pour comprendre le comportement des API — aucune documentation nécessaire.

## Démarrage rapide

```bash
# Installation macOS / Linux en une ligne (vérification SHA256)
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
export PATH="$HOME/.ctron/bin:$PATH"
```

```bash
$ ctron new hello && cd hello     # génère l'échafaudage Ctron.toml + Ctron.ctcl + src/main.ct

$ ctron run src/main.ct           # analyse, vérifie, évalue — un seul pipeline
hello, ctron

$ ctron check src/main.ct --format=json
{"diagnostics":[]}

$ ctron build src/main.ct         # émet un C lisible et autonome, puis appelle le cc local
$ ./src/main
hello, ctron
```

`run` / `check` / `new` n'ont pas besoin de toolchain C ; seul `build` requiert un cc local (la variable d'environnement `CC` est prioritaire). Sous Windows, `run` / `check` fonctionnent dès l'installation ; `build` requiert mingw-w64. Codes de sortie : `0` succès / `1` échec des diagnostics du programme / `2` erreur d'environnement ou d'utilisation.

## Le langage en un coup d'œil

Pas de null, propagation de `Result`, chaînes d'erreurs, tests dans le langage :

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
    let x = safe_div(a, b)?                   // `?` propage, en attachant le contexte
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
            assert(e.cause.is_some())          // la chaîne de causes conserve la racine
        }
        Ok(_) => assert(false)
    }
}
```

Un véritable outil complet (`examples/ctwc`, un utilitaire à la wc) :

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

Pour aller plus loin :

- **Concurrence structurée** — les tâches vivent sous `scope` / `spawn` / `join`, leur durée de vie liée à la portée du bloc ; l'annulation se propage le long de la portée, et une tâche qui panique déclenche une diffusion d'annulation structurée. Les trois points de contrôle `Send` s'exécutent à la compilation.
- **Deux niveaux de mémoire** — par-dessus le GC se trouvent les blocs `own` (un sous-ensemble de mémoire sans GC au sein d'une portée) et `Arena` ; le profil `bare` rejette toute allocation GC et cible les microcontrôleurs bare-metal.
- **Exécution à la compilation** — `const` / `comptime fn` s'évaluent à la compilation ; `@derive(Show, Eq)` synthétise les impls courants ; les blocs de code dans les commentaires de documentation `///` sont compilés et exécutés (doc-test).
- **Capacités explicites** — les permissions d'E/S sont des objets `Cap` explicites déclarés dans le manifeste de paquet ; tout dépassement échoue à la compilation.
- **FFI de première classe** — des déclarations `extern "c"` plus les `c_src/*.c` liés automatiquement.

## Deux voies d'exécution

```
ctron run main.ct      interprétation : zéro dépendance externe, boucle de dev la plus rapide
ctron build main.ct    émission de C → cc local : exécutable natif, le même langage de bout en bout
```

L'artefact émis est un **C lisible et autonome** (runtime entièrement inliné, seuls les en-têtes système), conservé à côté de la source pour audit. Une seule base de code source couvre le développement et le déploiement.

## Bibliothèque standard et paquets métier

La bibliothèque standard est livrée **sous forme de source** aux côtés de la toolchain ; les paquets métier se rattachent à des espaces de noms de premier niveau : `use std.*` / `use net.*` / `use gui.*`.

| Paquet | Contenu |
|---|---|
| `std` | 26 modules : json / csv / uuid / crypto (SHA-256, HMAC, PBKDF2) / fmap / heap / iter / sort / unicode / strconv / rand / path / time… |
| `net` | façade de transport TCP : listen / connect / accept / read / write |
| `http` | parseur HTTP/1.1 + client + WebSocket + SSE + une **famille de middlewares pour framework web** (router / JWT auth / cors / csrf / openapi / metrics / timeout / static…) |
| `tls` | façade TLS |
| `db` | pilotes de protocole filaire en pur Ctron : **PostgreSQL** / RESP2 (Redis) + pool de connexions + mappage des lignes |
| `ffi` | encapsulation des erreurs à la frontière C (errno replié dans `Result`) |

## La toolchain en un coup d'œil

`ctron` est le point d'entrée utilisateur unique :

| Commande | Rôle |
|---|---|
| `run` / `check` / `build` | interpréter / contrôles statiques (diagnostics JSON) / émettre et construire |
| `test` | exécuter les blocs `test` dans le langage |
| `fmt` | formateur canonique |
| `lint` | synthèse des diagnostics (`--strict` escalade les avertissements en erreurs) |
| `doc` | projection d'interface : table des symboles publics + commentaires de contrat |
| `bench` | familles de benchmarks : lang / gc / http / net / ffi |
| `new` | échafaudage de projet |
| `add` / `publish` / `lock` | installer des paquets / publier / lockfile |

## Prise en charge des éditeurs

- **Serveur de langage** (`lsp/`) — diagnostics, documentSymbol, hover, complétion, définition/références, rename, aide à la signature, inlay hints, formatage, pliage.
- **Extension VS Code** (`editors/vscode-ctron`) — coloration syntaxique, snippets, diagnostics sémantiques à la sauvegarde avec corrections rapides, jetons sémantiques, inlay hints de noms de paramètres ; prend également en charge CTML (balisage GUI) et CTCL (langage de configuration).

## État des lieux et limites

- La première version, **v0.0.1**, est disponible ; la spécification du langage est un brouillon figé (v0.8). Si le chemin de téléchargement est indisponible, compilez depuis les sources — voir [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md).
- Bien adapté : outils en ligne de commande, services réseau, compilateurs, parseurs.
- À éviter : calcul massif sur GPU ; systèmes temps réel stricts (les pauses du GC sont incontrôlables — le profil `bare` les atténue sans les éliminer).
- `build` et la prise en charge de Windows sont globalement en β.

## Contribuer

Issues, exemples, documentation et code : tout est bienvenu — la plupart des contributions ne requièrent aucune connaissance des rouages internes du compilateur.

**Signaler un bug** — ouvrez une issue avec un reproducteur `.ct` minimal et la sortie de `ctron check <file> --format=json`. Les diagnostics portent des codes d'erreur stables, ce qui permet des corrections précises.

**Faire grandir l'écosystème** — modules de la bibliothèque standard, exemples, outillage pour les éditeurs, documentation et site web : les PR sont les bienvenues.

**Faire évoluer le langage** — la spécification est un brouillon figé : commencez par une discussion de conception dans une issue ; les changements acceptés aboutissent sous forme d'une entrée de révision de la spécification, accompagnée d'ancrages de corpus dans `tests/`. Le comportement du langage doit rester aligné sur les trois implémentations (seed C / référence Rust / compilateur auto-hébergé), la suite de conformité servant d'arbitre. Tout nouveau diagnostic requiert un code d'erreur stable enregistré dans le registre des codes d'erreur de la spécification.

**Workflow de développement**

```bash
git clone https://github.com/ZturnLibs/Ctron && cd Ctron
bash ci.sh    # une seule commande : bootstrappe la toolchain, exécute la porte d'acceptation complète en 9 étapes
```

Les PR doivent arriver avec `ci.sh` au vert. Gardez `ctron` (sh) et `ctron.ps1` / `ctron.cmd` identiques en comportement. Les messages de commit suivent les conventional commits (`feat:` / `fix:` / `docs:`).

## Plan de la documentation

| Document | Contenu |
|---|---|
| [docs/spec/](docs/spec/README.md) | Spécification du langage v0.8 (12 chapitres) |
| [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md) | Construire la toolchain depuis les sources |
| [examples/](examples/) | Applications d'exemple complètes |
| [website/](https://zturnlibs.github.io/Ctron/) | Prise en main, référence de la stdlib, exemples, téléchargements |

## Licence

MIT — voir [LICENSE](LICENSE).
