#!/bin/sh
# vendor/deflate/build.sh —— miniz 静态库构建(libminiz.a)
#
# 版本登记:
#   版本  : miniz 3.1.2(最新 release;MZ_VERSION "11.3.2")
#   来源  : https://github.com/richgel999/miniz/releases/download/3.1.2/miniz-3.1.2.zip
#   SHA256(zip)        : f0446d863f9c19926ad9483c523fdc42e42b8d4a6a431d27e09d49c79a140d9a
#   SHA256(miniz.c)    : e2c1aeb66eef9191d8c3feb164db2def2335a61d039bf04ed849f6b042433b30
#   SHA256(miniz.h)    : b53b62ed122e559b8f679e3cb787a0b0035fe87a58f909da0e44931678f4e85f
#   许可  : MIT(LICENSE 随 vendor 入库,登记见 VENDORED.md)
#
# 构建系统决策:cc 直编单文件 amalgamated(上游既定 vendor 形态:miniz.c + miniz.h
# 一个翻译单元,7922 行全量含 zip 归档层——不裁 API 宏(默认全量口径,同 vendor/tls
# 惯例),单 TU 编译 <2s 无裁剪收益)。无配置头、无线程宏:tdefl_compress_mem_to_mem /
# tinfl_decompress_mem_to_mem / mz_crc32 均无状态(压缩器状态为调用栈局部),
# server shim 多 worker 并发调用天然安全(本任务只用这三件,raw deflate 口径,
# zlib/gzip 容器在 Ctron 侧 std/http/enc.ct 组框)。
# 产物:build/libminiz.a(不进 git——根 .gitignore 全局 build/ 规则已覆盖);中间
# .o 入 build/objs/(同上);离线可重建(本脚本无任何网络动作)。
set -eu
cd "$(dirname "$0")"
ROOT="$(pwd)"
mkdir -p build/lib build/objs

SRC="miniz/miniz.c"
[ -f "$SRC" ] || { echo "build.sh FAIL: 缺 $SRC(见 VENDORED.md 拉取口径)" >&2; exit 1; }

cc -O2 -c "$SRC" -o build/objs/miniz.o
ar rcs build/lib/libminiz.a build/objs/miniz.o
echo "deflate/build: build/libminiz.a ← $(wc -c < build/lib/libminiz.a) 字节"
