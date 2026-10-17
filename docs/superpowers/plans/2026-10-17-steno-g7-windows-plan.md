# G7 Windows 链路收口 实施计划(dlopen 垫片 + ci windows 冒烟 + 验证矩阵)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** P1-5 收口(J9 已裁:mingw-w64 only + x64 先)——①发射面 dlopen 桥 Windows 垫片(今天 `#[dlsym]`/dlopen 族直出 C 调用,Windows 无 dlfcn 编译必败);②ci.yml 增 windows-mingw 日常冒烟 job(今天 Windows 只活在 release.yml,日常门零覆盖);③验证矩阵(并发族/__sync 钉/CJK 路径三件/dlopen DLL 探针)。

**Architecture:** 垫片走发射前言既有的 `#if defined(_WIN32)` include 块(driver_emit.ct:84-90,先于一切使用点:156 dlerror_str/1027,1029 thunk/trans_expr 1514-1527 直出调用)——POSIX 臂零语义变化(纯 include 块扩展);trans_expr 与 thunk 发射文本零改动(dlopen/dlsym/dlclose/dlerror 四宏在 Windows 臂内捕获)。CI 配方 = release.yml windows stage 同款(msys2 MINGW64 + prebuilt 预发射 C + gcc --stack,16777216),加 driver 件让真 `ctron build` 入冒烟。

**Tech Stack:** LoadLibraryW(CP_UTF8→宽字符,CJK 路径)/GetProcAddress(h==0→GetModuleHandleW(NULL)=RTLD_DEFAULT 等效)/FreeLibrary/GetLastError→FormatMessageW→UTF8(dlerror 读即清行为对齐);brew mingw-w64 本地交叉编译验证;gh run watch CI 迭代。

## Global Constraints

- POSIX 发射产物只增 `#if/#else/#include` 文本,语义零变化;smoke --full 全绿随批验。
- 发射模板字符串 `{`/`}` 全 `\{`/`\}` 转义(机刷坑 ①③ 家法);编译器 src 改后 make compiler-c + build.sh + native.sh 三连重建。
- CI 迭代必须 push;push 前本地 POSIX 腿( smoke + tests/win/smoke.sh)全绿。

---

### Wave 1: dlopen 桥 Windows 垫片(编译器面)

- [x] driver_emit.ct:84-90 include 块扩展:`#define NOMINMAX` 前置;Win 臂加 RTLD_NOW/RTLD_GLOBAL/RTLD_DEFAULT 宏 + ctron_w_dlopen(UTF8→wchar LoadLibraryW)/ctron_w_dlsym(空句柄→GetModuleHandleW(NULL))/ctron_w_dlclose(FreeLibrary)/ctron_w_dlerror(GetLastError→FormatMessageW→UTF8,读即清)+ dlopen/dlsym/dlclose/dlerror 四重定向宏;POSIX 臂维持 dlfcn.h+errno.h 原文
- [x] 重建三连 + 发射探针验证:①POSIX:探针程序(#[dlsym] from_handle + dlopen_global + dlerror_str + dlclose)emit→cc→run 绿;②mingw 交叉:同探针 x86_64-w64-mingw32-gcc 编链过(运行归 CI);③smoke --full 全绿
- [x] Commit

### Wave 2: tests/win 冒烟道 + ci.yml windows-smoke job

- [x] tests/win/smoke.sh:双平台脚本(msys2 bash/ubuntu)——hello emit+cc+run/并发自检(spawn/join/chan)/__sync Atomic 钉/dlopen DLL 探针(gcc -shared 夹具 + from_handle 主道;RTLD_DEFAULT exe-export 需 -Wl,--export-all-symbols 登记在册)/CJK 路径三件(源码路径/产物路径/dlopen 路径);本地 POSIX 腿真跑绿
- [x] ci.yml:winpre job(ubuntu:make compiler-c+build.sh+ctc.sh emit 四件预发射 C[cc/chk/emit/drv]+artifact)+ windows-smoke job(needs winpre:setup-msys2 MINGW64→gcc 三件+ctron.exe[--stack,16777216;drv 并链 lib/proc/c_src/ctron_proc.c]→真 ctron build 一例→sh tests/win/smoke.sh)
- [x] Commit

### Wave 3: CI 真跑迭代 + 批终

- [x] push 全部待推提交 → gh run watch:windows-smoke 首跑迭代至绿(预期迭代面:msys2 路径形态/exe 栈/winpthreads/宏碰撞)
- [x] 验证矩阵余项核销:cdylib+DLL 搜索路径(ctron.exe 在场则入 smoke,否则登记挂账);并发排除项明示(coro 臂=CTRON_RT=coro 不入 Windows 冒烟,登记)
- [x] 批终登记(COVERAGE + design 勾 + memory)

## 明确不做

- MSVC/ARM64(J9 已裁 mingw-only x64 先);interp(ctron-cc run)Windows 臂(解释桥 `#if !defined(_WIN32)` 排除维持 β 口径,T45 帧桥整段 POSIX-only);安装器/CDN;coro RT 在 Windows 的移植(winpthreads 有 pthread 但 ctron_rt.c 协程臂未验,排除项在册)。
