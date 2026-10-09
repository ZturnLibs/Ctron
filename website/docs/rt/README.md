# rt 域包 —— 自由运行时(bare / wasm32)

`lib/rt/` **没有 .ct 门面**(因此无生成式参考页):这里是 T35 档位轴的
bare / wasm 目标三档运行时支撑物——C 头文件垫片与链接脚本,不是 Ctron 源码
API。逐项文档见源内 README:

- **bare**:`lib/rt/bare/README.md`(Region/Pool/Static + zeros 守卫;
  freestanding 头垫片 dirent.h/dlfcn.h/errno.h/pthread.h/setjmp.h 等 + link 脚本)
- **wasm32**:T37–T39 泳道(MVP/桥 ABI/dom 三面),桥契约 = `(ptr,len) → i32 status`

## 与语言面的关系

- `--profile=bare` 将有效档钳到 core(use 点档位越界 = E3040,一次诊断);
  档位轴(core/alloc/std)见 [std/README.md](../std/README.md) 档位轴章。
- 裸档体检:`tier_real_pos`(core 五模块全量消费零诊断)。
- GC/分配语义差异与 ISR parity 以 `tests/bare`/`tests/ffi` 套件实测为准。

注:本页为指针页;rt 面的「API」即 C 垫片符号,以 `lib/rt/*/include` 与
`lib/rt/*/src` 源为单一真源。
