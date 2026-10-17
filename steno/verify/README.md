# steno-verify —— 秘本审计链独立验证器(Stage 0,Ctron 承载)

秘本引擎的审计链/license 独立验证工具:验证 `stenolog.v1` 审计日志(JSONL 事件哈希链 +
Ed25519)与 `stenolicense.v1` 授权文件。**验证器对生产者写出的字节逐字复算,零再序列化**——
这是"谁在何时导出过什么"可自证的信任底座,也是 Ctron 的第一个真实 dogfood 交付物。

- 格式契约:`docs/superpowers/specs/2026-10-17-steno-verify-format-spec.md`(唯一契约源;
  秘本侧 steno-store/steno-license 落地时按本文生产)。
- 验收门:`sh tests/steno/run.sh`(24 例:负例矩阵 + openssl 双向对拍)。

## 构建

```sh
sh compiler/native.sh            # 若 compiler/bin 未就绪
CTRON_STDPATH="$PWD/lib/std" compiler/bin/ctron-emit run steno/verify/src/main.ct > /tmp/sv.c
cc -O1 -w -Ilib/crypto/c_src -I/opt/homebrew/opt/openssl@3/include -o /tmp/steno-verify \
    /tmp/sv.c lib/crypto/c_src/ctron_crypto.c lib/fio/c_src/ctron_fio.c \
    -L/opt/homebrew/opt/openssl@3/lib -lcrypto
/tmp/steno-verify selftest
```

## 用法

```sh
steno-verify keygen --out priv.hex                 # 新密钥对;私钥写盘 0600,公钥打 stdout
steno-verify chain --pub <hex|@file> audit.jsonl [--json] [--quiet]
steno-verify sign-chain --priv <hex|@file> in.jsonl out.jsonl
steno-verify license --pub <hex|@file> lic.json [--machine H] [--now MS] [--strict] [--json]
steno-verify sign-license --priv <hex|@file> in.json out.json
steno-verify selftest
```

退出码:0 合法 / 1 无效(链断/签名伪/绑定不合/过期 strict)/ 2 用法或 IO 错。

## 源码形态

| 文件 | 职责 |
|---|---|
| src/main.ct | 入口薄壳(逻辑全下沉,20480B 直发家族绕行) |
| src/app.ct | CLI 分发/旗标/退出码(argv 经 `#[trusted] extern "c" ctron_prog_args_*`) |
| src/chain.ct | stenolog.v1:core 提取/逐行验证/链环/sign-chain |
| src/license.ct | stenolicense.v1:验证/sign-license |
| src/selftest.ct | 进程内自检(RFC 向量 + 全链自签自验 + 五负例) |
| src/util.ct | hex 校验/键参读取/行切分 |
