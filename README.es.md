<div align="center">

[English](README.md) | [简体中文](README.zh-CN.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | **Español** | [日本語](README.ja.md)

</div>

# Ctron

**El lenguaje de programación de sistemas nativo para IA** — pensado para «la IA escribe, los humanos revisan»: errores detectados en tiempo de compilación, código que significa exactamente lo que dice, sin magia.

```bash
ctron run main.ct      # interpreta y ejecuta — cero dependencias externas
ctron check main.ct    # comprobaciones estáticas — --format=json para diagnósticos estructurados
ctron build main.ct    # emite C equivalente → cc local → ejecutable nativo
```

- Repositorio: <https://github.com/ZturnLibs/Ctron>
- Sitio de documentación: <https://zturnlibs.github.io/Ctron/> (inglés / 简体中文)

---

> **Estado del proyecto:** Ctron está en desarrollo activo y **aún no se recomienda para producción** — el lenguaje y la toolchain siguen evolucionando. Los desarrolladores interesados son bienvenidos a construirlo con nosotros: véase [Contribuir](#contribuir).

## Por qué Ctron

El código generado por IA tiene un patrón de fallo estable: **estructura correcta, detalles equivocados** — identificadores mal escritos, tipos que no coinciden, casos límite omitidos. Los lenguajes dinámicos los destapan en tiempo de ejecución; en C/C++ pueden convertirse en comportamiento indefinido. La respuesta de Ctron no es «una IA mejor», sino un diseño de lenguaje más estricto:

- **Cada diagnóstico lleva un código de error estable** (E2010, E2030, E3040…). Con el código en la mano, una IA puede corregir con precisión — sin tener que adivinar la intención a partir de prosa.
- **Diagnósticos estructurados** (`ctron check --format=json`) que los editores y las tuberías de CI consumen directamente.
- **Gramática sin ambigüedades y reglas de inferencia de tipos explícitas** — el mismo código recibe el mismo veredicto en todas partes.

**Sin magia.** Lo que una IA teme de un lenguaje no es una sintaxis compleja, sino lo que el lenguaje hace a sus espaldas:

| Lo que Ctron no tiene | Lo que se usa en su lugar |
|---|---|
| Sin null | `Option[T]` |
| Sin conversiones numéricas implícitas | `as[T]()` explícito |
| Sin sobrecarga de operadores | `+` significa suma |
| Sin macros | sin sustitución textual — el código fuente es la verdad |
| Sin comportamiento indefinido | la especificación enumera todos los comportamientos legales; lo que no declara, no existe |

**Todo es código fuente.** La biblioteca estándar se distribuye como fuente de Ctron, no como binarios compilados — `use std.*` lee la fuente en tiempo de compilación y la fusiona en un único AST con el programa. Una IA puede leer la fuente de la stdlib para entender el comportamiento de la API — sin necesidad de documentación.

## Inicio rápido

```bash
# Instalación en una línea para macOS / Linux (con verificación SHA256)
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
export PATH="$HOME/.ctron/bin:$PATH"
```

```bash
$ ctron new hello && cd hello     # genera el esqueleto del proyecto: Ctron.toml + Ctron.ctcl + src/main.ct

$ ctron run src/main.ct           # analizar, comprobar y evaluar — una sola tubería
hello, ctron

$ ctron check src/main.ct --format=json
{"diagnostics":[]}

$ ctron build src/main.ct         # emite C legible y autocontenido, y después invoca el cc local
$ ./src/main
hello, ctron
```

`run` / `check` / `new` no necesitan ninguna cadena de herramientas de C; solo `build` requiere un cc local (la variable de entorno `CC` permite usar otro). En Windows, `run` / `check` funcionan sin ningún paso previo; `build` necesita mingw-w64. Códigos de salida: `0` éxito / `1` diagnósticos del programa fallidos / `2` error de entorno o de uso.

## El lenguaje de un vistazo

Sin null, propagación de `Result`, cadenas de errores, tests dentro del propio lenguaje:

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
    let x = safe_div(a, b)?                   // `?` propaga y adjunta contexto
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
            assert(e.cause.is_some())          // la cadena de causas conserva la raíz
        }
        Ok(_) => assert(false)
    }
}
```

Una herramienta real y completa (`examples/ctwc`, una utilidad al estilo de `wc`):

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

Yendo más allá:

- **Concurrencia estructurada** — las tareas viven bajo `scope` / `spawn` / `join`, con su ciclo de vida ligado al ámbito del bloque; la cancelación se propaga a lo largo del ámbito, y una tarea que entra en pánico desencadena una difusión de cancelación estructurada. Los tres puntos de control `Send` se ejecutan por completo en tiempo de compilación.
- **Dos niveles de memoria** — sobre el GC se sitúan los bloques `own` (un subconjunto de memoria sin GC dentro de un ámbito) y `Arena`; el perfil `bare` rechaza por completo las asignaciones del GC y tiene como destino los microcontroladores bare-metal.
- **Ejecución en tiempo de compilación** — `const` / `comptime fn` se evalúan en tiempo de compilación; `@derive(Show, Eq)` sintetiza las implementaciones más comunes; los bloques de código de los comentarios de documentación `///` se compilan y ejecutan (doc-test).
- **Capacidades explícitas** — los permisos de E/S son objetos `Cap` explícitos declarados en el manifiesto del paquete; pasarse de los límites falla en tiempo de compilación.
- **FFI de primera clase** — declaraciones `extern "c"` junto con `c_src/*.c` enlazadas automáticamente.

## Dos rutas de ejecución

```
ctron run main.ct      interpretar: cero dependencias externas, el ciclo de desarrollo más rápido
ctron build main.ct    emitir C → cc local: ejecutable nativo, el mismo lenguaje de principio a fin
```

El artefacto emitido es **C legible y autocontenido** (el runtime está completamente inline, solo cabeceras del sistema) y se conserva junto al código fuente para poder auditarlo. Una única base de código sirve para desarrollo y despliegue.

## Biblioteca estándar y paquetes de dominio

La biblioteca estándar se distribuye **como fuente** junto con la cadena de herramientas; los paquetes de dominio se acoplan a espacios de nombres de primer nivel: `use std.*` / `use net.*` / `use gui.*`.

| Paquete | Contenido |
|---|---|
| `std` | 26 módulos: json / csv / uuid / crypto (SHA-256, HMAC, PBKDF2) / fmap / heap / iter / sort / unicode / strconv / rand / path / time… |
| `net` | fachada de transporte TCP: listen / connect / accept / read / write |
| `http` | parser de HTTP/1.1 + cliente + WebSocket + SSE + una **familia de middleware de framework web** (router / JWT auth / cors / csrf / openapi / metrics / timeout / static…) |
| `tls` | fachada TLS |
| `db` | drivers de protocolo de red escritos en Ctron puro: **PostgreSQL** / RESP2 (Redis) + pool de conexiones + mapeo de filas |
| `ffi` | envoltura de errores en el límite con C (errno plegado dentro de `Result`) |

## La cadena de herramientas de un vistazo

`ctron` es el único punto de entrada para el usuario:

| Comando | Propósito |
|---|---|
| `run` / `check` / `build` | interpretar / comprobaciones estáticas (diagnósticos JSON) / emitir y compilar |
| `test` | ejecutar los bloques `test` del propio lenguaje |
| `fmt` | formateador canónico |
| `lint` | resumen de diagnósticos (`--strict` convierte las advertencias en errores) |
| `doc` | proyección de la interfaz: tabla de símbolos públicos + comentarios de contrato |
| `bench` | familias de benchmarks: lang / gc / http / net / ffi |
| `new` | andamiaje de proyectos |
| `add` / `publish` / `lock` | instalar paquetes / publicar / lockfile |

## Soporte de editores

- **Servidor de lenguaje** (`lsp/`) — diagnósticos, documentSymbol, hover, autocompletado, definición/referencias, rename, ayuda de firmas, inlay hints, formateo, plegado.
- **Extensión de VS Code** (`editors/vscode-ctron`) — resaltado de sintaxis, snippets, diagnósticos semánticos al guardar con correcciones rápidas, tokens semánticos, inlay hints con nombres de parámetros; también admite CTML (marcado de GUI) y CTCL (lenguaje de configuración).

## Estado y límites

- La primera release, **v0.0.1**, ya está publicada; la especificación del lenguaje es un borrador congelado (v0.8). Si la ruta de descarga no está disponible, compilar desde el código fuente — véase [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md).
- Encaja bien con: herramientas de CLI, servicios de red, compiladores, parsers.
- No encaja para: cómputo intensivo en GPU; sistemas de tiempo real estricto (las pausas del GC son incontrolables — el perfil `bare` las mitiga, pero no las elimina).
- `build` y el soporte de Windows están, en conjunto, en β.

## Contribuir

Se aceptan issues, ejemplos, documentación y código — la mayoría de las contribuciones no requieren conocer las interioridades del compilador.

**Reportar un bug** — abrir un issue con un reproductor mínimo en `.ct` y la salida de `ctron check <file> --format=json`. Los diagnósticos llevan códigos de error estables, de modo que las correcciones pueden ser precisas.

**Hacer crecer el ecosistema** — módulos de la biblioteca estándar, ejemplos, herramientas para editores, documentación y el sitio web: los PR son bienvenidos.

**Evolucionar el lenguaje** — la especificación es un borrador congelado: empezar por una discusión de diseño en un issue; los cambios aceptados aterrizan como una entrada de revisión de la especificación más anclas de corpus en `tests/`. El comportamiento del lenguaje debe mantenerse alineado entre las tres implementaciones (semilla en C / referencia en Rust / compilador autohospedado), con la suite de conformidad como árbitro. Los nuevos diagnósticos necesitan un código de error estable registrado en el registro de códigos de error de la especificación.

**Flujo de trabajo de desarrollo**

```bash
git clone https://github.com/ZturnLibs/Ctron && cd Ctron
bash ci.sh    # un solo comando: genera por bootstrap la cadena de herramientas y ejecuta la puerta de aceptación completa de 9 etapas
```

Los PR deben aterrizar con `ci.sh` en verde. Mantener `ctron` (sh) y `ctron.ps1` / `ctron.cmd` con comportamiento idéntico. Los mensajes de commit siguen conventional commits (`feat:` / `fix:` / `docs:`).

## Mapa de la documentación

| Documento | Contenido |
|---|---|
| [docs/spec/](docs/spec/README.md) | Especificación del lenguaje v0.8 (12 capítulos) |
| [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md) | Construcción de la cadena de herramientas desde el código fuente |
| [examples/](examples/) | Aplicaciones de ejemplo completas |
| [website/](https://zturnlibs.github.io/Ctron/) | Primeros pasos, referencia de la stdlib, ejemplos, descargas |

## Licencia

MIT — véase [LICENSE](LICENSE).
