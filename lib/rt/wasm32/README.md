# lib/rt/wasm32 —— web 档 wasm32-unknown-unknown 运行时(T37 §9.2)

发射 C 与 native/bare 同文(零新代码生成器);靶差全在本目录:

- `include/` — stub 头族(承编译面;与 lib/rt/bare 同款,平台 ifdef 由靶宏自然裁剪:
  wasm32 无 `__APPLE__`/`__linux__`/`_WIN32`,宿主面 include 全部落空)
- `src/wasm_libc.c` — minilibc 本体:
  - 输出/退出走宿主 import:`env.ct_print(ptr, len)` / `env.ct_exit(code)`
  - mem/str 七件 + vsnprintf 核(与 bare_libc 同文)+ 数值转换面(atol/strtol/strtod…)
  - 堆 = 16MiB 线性内存静态池 bump(GC arena 首块 4MB 自此取材;无 free;
    web 档无 bare 100KB 口径)
  - 线程/fs/dirent/dlfcn 族无定义——未触达的发射运行时块被 `--gc-sections` 整体裁除
  - 单线程口径(§7.8):通道/Mutex 列 T38 后

## 构建

```
compiler/ctc.sh build app.ct --target wasm32-unknown-unknown -o app.wasm
```

工具链三通道(env → 宿主 clang[wasm32 后端] → docker ctron-bare-tools):

1. `CTRON_WASM_CC=/path/to/clang`(须带 wasm32 后端,链接走 clang→wasm-ld 内呼)
2. 宿主 clang(`brew install llvm`;Apple clang 无 wasm32 后端=常态)
3. docker 镜像 `ctron-bare-tools`(Ubuntu clang 18.1.3,与 bare 档同镜像)

链接形态:`-nostdlib -Wl,--no-entry -Wl,--export=main -Wl,--export-memory
-Wl,--gc-sections -Wl,-s -Wl,-z,stack-size=1048576`

## 运行(node 胶水)

```
node tests/wasm/ct_wasm_run.js app.wasm
```

胶水契约(`tests/wasm/ct_wasm_run.js`):

| 面 | 约定 |
|---|---|
| import `env.ct_print(ptr: i32, len: i32)` | stdout 字节直写(UTF-8 原样) |
| import `env.ct_exit(code: i32)` | 进程退出码(panic=1) |
| export `main(argc=0, argv=0) -> i32` | 入口;main 内 argv 探测段被 argc=0 短路 |
| export `memory` | 线性内存(`--export-memory`;memory.grow 后胶水每次现取 buffer) |

MVP 范围(T37):no_alloc/core 子集先行;FFI 面(`#[link]`)在 wasm target 禁用
(诊断非静默,rc=2)。WasmGC/JSPI/类型化 JS 桥列 T38;stdweb 真实化列 T39。

## 验收门

`tests/wasm/run.sh`(ci.sh [6/9] 前挂载;门序:注册表/hello/arith/strfmt 真跑
+ ffi 负锚 + 体积报告)。node 三通道(PATH → 打包内置运行时 → docker node 镜像),
全缺 = 环境登记 SKIP。
