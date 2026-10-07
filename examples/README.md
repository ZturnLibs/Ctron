# Examples

Complete, runnable Ctron programs. Run any of them with the toolchain, e.g.:

```bash
ctron run examples/ctwc/src/main.ct
```

## CLI tools

| Example | What it shows |
|---|---|
| [ctwc](ctwc/) | wc-style line/word/byte counter — the smallest end-to-end CLI |
| [ctgrep](ctgrep/) | grep-style substring search |
| [ctwf](ctwf/) | word-frequency counter with deterministic output |

## Network & web services

| Example | What it shows |
|---|---|
| [ctecho](ctecho/) | thread-per-connection TCP echo server (`scope` / `spawn` concurrency) |
| [todo_api](todo_api/) | REST CRUD + JWT auth + CORS + rate limiting + OpenAPI + graceful shutdown on the `http` framework |
| [web_todo](web_todo/) | server-rendered todo app — the web framework's first live example |
| [ctslink](ctslink/) | self-hosted URL shortener built on the `web.*` packages |

## GUI applications

| Example | What it shows |
|---|---|
| [gui_counter](gui_counter/) | the smallest GUI app: the view in `app.ctml`, the logic in Ctron |
| [gui_calc](gui_calc/) | calculator with decimal-string arithmetic and a state machine |
| [gui_cjk](gui_cjk/) | CJK text rendering (FreeType textures) with a live click counter |
| [gui_zitie](gui_zitie/) | Chinese copybook: the Thousand Character Classic, vertical layout, 50 pages |
| [gui_files](gui_files/) | file browser — the first example with real I/O |
| [gui_dash](gui_dash/) | metrics dashboard: cross-file components + content projection |
| [gui_snippets](gui_snippets/) | snippet manager: list props, model capture, Select / WList / Dialog |
| [gui_themes](gui_themes/) | theme showroom: eight switchable themes |
| [gui_widgets](gui_widgets/) | widget showroom: every interactive widget |
| [todo](todo/) | todo app on the `gui` package facade (spec §10) |
| [todo_app](todo_app/) | the full todo application — the largest GUI example |
| [todo_v10](todo_v10/) | todo per spec §10.3, final-anchor form |
