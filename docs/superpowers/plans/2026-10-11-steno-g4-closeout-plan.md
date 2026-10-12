# G4 链接旗标/cdylib 收尾批 实施计划(旗标消费已随 G2-W3 落地)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** P1-6 收尾——cdylib 产物形态(秘本垫片库/插件宿主地基)、universal binary(darwin 双架构分发)、rpath 约定成文;-Wl,--stack 可覆盖销账(CTRON_LDFLAGS 已通,G2-W3)。

**Architecture:** 纯驱动面(`compiler/ctc.sh` build 分支):`--lib`(cdylib:darwin `-dynamiclib -install_name @rpath/lib<名>.dylib`;linux `-shared -fPIC`)+ `--universal`(darwin 单遍 `-arch x86_64 -arch arm64`);发射 C 文本零变化。签名/公证不入 ctron(J8 裁决,分发脚本职责);根 ctron 驱动同面随原生驱动器 lane(登记)。

**Tech Stack:** cc 产物形态旗标;门:dylib 形态探针(nm 符号+file 形态+dlopen 加载回环)+universal 探针(lipo -info)+既有门零回归。

## Tasks

- [ ] T1: ctc.sh build 参数(--lib/--universal)+链接行分支(native else 臂)
- [ ] T2: 探针 tests/ffi/dyn_so 扩:dylib 形态构建+加载回环(dlopen_global 吃自家 dylib)
- [ ] T3: universal 探针(lipo -info 双架构)+Darwin 门控(linux SKIP)
- [ ] T4: rpath 约定成文(docs/ffi-analysis 或 dyn_so 头注)+COVERAGE 批终+Commit

## 明确不做

- codesign/entitlements/notarytool(J8:分发链职责);对外 Makefile/CMake 导出;MSVC;install_name 工具后处理(留用户)。
