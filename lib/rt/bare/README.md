# lib/rt/bare —— bare 档运行时(T40 §9.3)

零 OS 依赖的最小 C 运行时:`ctc build --target <bare-triple>` 的「运行时文件集」。
设计契约:发射 C 与 native 完全同文(后端不分支),裸靶适配全部由本目录承接——

1. **stub 头**(`include/`):承接发射 C 无条件 `#include` 的宿主头
   (stdio/stdlib/string/errno/time/setjmp/dirent/pthread/dlfcn)。
   stdint/stddef/stdarg 走 clang freestanding 内建头(勿在此覆写)。
2. **minilibc**(`src/bare_libc.c`):
   - 半主机输出(ARM `bkpt 0xAB` / RISC-V `slli-ebreak-srai` 三联):printf 族 + exit;
   - mem/str 常规七件(freestanding 代码生成会自发调用,必须提供定义);
   - printf 面 = 有界 vsnprintf 核(`%d/%i/%u/%x/%X/%s/%c/%p/%lld/%llu/%lx/%llx/%g/%.Ng`,
     无 libm 整数法抽位);`%f/%e` ≙ 同 g(有效位口径,文档化志向);
   - **堆/线程/fs/dirent/dlfcn 族无定义**——发射运行时引用它们的块均为 `static`,
     bare 程序不触达时被 `-ffunction-sections + --gc-sections` 整体裁除。
3. **startup**(`src/start_arm.c`/`start_riscv.c`):向量表/`_start` + 数据搬运 + bss 清零。
4. **链接脚本**(`link/`):静态栈 8K + guard 页 1K + 链接期 ASSERT(RAM 溢出/guard 重叠
   红为响亮诊断;运行期硬隔离 = MPU 升级路径,志向)。
5. **setjmp 面**:bare 档单线程无任务面——`setjmp` 恒 0(首次返回)、`longjmp` = 响亮
   exit(70)。panic 口径 = `exit(1)`(任务 longjmp 全貌列 full 档)。

## 工具链解析(ctc.sh 同表)

优先级:`CTRON_BARE_CC`/`CTRON_BARE_LD` env → 宿主 clang(+`ld.lld`,含
`/opt/homebrew/opt/llvm/bin`)→ docker 镜像 `ctron-bare-tools:latest`(clang+lld+qemu)。
全缺 = 清晰诊断 exit 2(fail-closed)。RISC-V 后端 Apple clang 不含,docker 通道必走。

## qemu 冒烟(tests/bare/run.sh)

- ARM:`qemu-system-arm -M netduinoplus2 -semihosting-config enable=on,target=native`
  (flash @ 0x08000000,cortex-m4);
- RISC-V:`qemu-system-riscv32 -M virt -bios none -semihosting-config enable=on,target=native`;
- 断言看输出文本(stdout 走半主机 SYS_WRITE0),rc 宽容(qemu 对 SYS_EXIT 的
  退出码传播依版本)。
