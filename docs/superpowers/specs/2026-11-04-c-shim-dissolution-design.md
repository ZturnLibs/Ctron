# C 垫片消解设计(c_src dissolution)—— 算法归 Ctron,地板留 C

> 创建:2026-11-04。状态:**设计方案 v1,待评审**。
> 范围:自举/发布路径上的**手写 C 面**(`lib/*/c_src/`、`compiler/host_shim/`、发射模板内嵌 C)的分层处置与迁移切片。
> 排除(永久非目标):G0 种子(`compiler-c/`,归档冻结)、vendored 第三方(`dist/ctron/vendor/`:mbedtls/miniz;`lib/db/c_src/sqlite3.c` amalgamation 及其绑定本身)、生成物(`compiler/build/*.c` 固定点产物)、裸机 crt0/裸汇编上下文切换。
> 先例依据(本设计不立新约,只泛化既有口径):
> ① `bit_tc64`/`bit_ret64`——64 轮 c6divmod 纯 Ctron 消解 C 位助手(decls 锁 538→539 批);
> ② redis/pg **协议逻辑已在 Ctron**,C 只剩 socket 地板(`lib/db/*.ct` + `ctron_dbredis.c` 函数表勘定);
> ③ gzip 组框(RFC 1952)已纯 Ctron,唯 crc32 一原语复用 miniz(`lib/http/enc.ct` 折中注);
> ④ `std/crypto.ct` 纯 Ctron SHA-256(FIPS 180-4 向量锚)与 `bind.ct` extern 快臂并存——双臂口径已成立。

---

## 0. 现状勘定(实测,2026-11-04)

### 0.1 手写 C 基线

```
find lib compiler -name "*.c" -not -path "*compiler-c*" \
  -not -path "*build/*" -not -name "sqlite3.c" | xargs wc -l
→ 5,857 行(基线;此后每批记 Δ,只降不升)
```

另:发射模板内嵌 C = `driver_emit.ct` 486 段 `println` C 字符串(GC/chan/task/unwind/GUI dispatch),归 §4 提案,不入本基线。

### 0.2 逐件分类与裁决

| 件 | 行数 | 分类 | 裁决 |
|---|---|---|---|
| `lib/net/c_src/ctron_rt.c` | 2092 | 地板(裸汇编 SysV 上下文切换、N:M 调度) | 留 |
| `lib/net/c_src/ctron_net.c` | 1176 | 地板(socket/时钟/睡眠/errno) | 留;成为 S1 归一目标 |
| `lib/tls/c_src/ctron_tls.c` | 405 | 绑定(mbedtls BIO/ctx)+ 可搬(CA bundle 探测) | 薄化(S5) |
| `lib/rt/wasm32/src/wasm_libc.c` | 373 | 地板(wasm32 libc 契约) | 留 |
| `lib/rt/bare/src/bare_libc.c` + `start_arm.c`/`start_riscv.c` | 334+crt0 | 地板(crt0/semihosting) | 留 |
| `lib/proc/c_src/ctron_proc.c` | 233 | 地板(fork/exec/wait/管道/超时) | 留 |
| `lib/db/c_src/ctron_dbredis.c` | 213 | 地板 socket(read/write/close/errno/connect/socketpair),**与 net 地板重复** | 消解(S1) |
| `lib/db/c_src/ctron_dbpg.c` | 210 | 同上 + entropy 转发 | 消解(S1) |
| `lib/crypto/c_src/ctron_crypto.c` | 161 | 绑定(OpenSSL EVP ed25519)+ sha256/hmac 快臂(规范臂已纯 Ctron)+ hex 助手(与 std/enc 重复) | 薄化(S3) |
| `lib/http/c_src/ctron_deflate.c` | 150 | 绑定(miniz)+ crc32 一原语 | 薄化(S4) |
| `lib/fio/c_src/ctron_fio.c` | 115 | 地板(open/read/write/stat/getcwd) | 留 |
| `lib/db/c_src/ctron_entropy.c` | 114 | 地板(getentropy/arc4random/urandom 回退) | 留 |
| `lib/ffi/c_src/ctron_ffi.c` | 51 | 逻辑(cbox 登记表:定容 256 线性表)+ strerror const 冲突垫 | 消解(S2) |
| `compiler/host_shim/cc2_bit.c`/`cc2_str.c` | 47+49 | 解释器内建宿主口(核心内建;派生助手已消解一批) | 留(种子/解释臂链接面,§4 注) |

**可消解面合计 ≈ 694 行**(S1 423 + S2 51 + S3 ~90 + S4 ~20 + S5 ~60),占手写基线 12%;消解后地板+绑定 ≈ 5.2k 行,其中绑定层(mbedtls/miniz/EVP/sqlite)≈ 900 行,纯地板 ≈ 4.3k 行。

---

## 1. 设计原则

- **P1 双臂口径**:纯 Ctron 臂 = 规范语义(解释臂可跑、可差分);extern 地板臂 = native 快径/绑定。互斥面用 emit 门控(interp 无 extern 运行时——`lib/db/redis.ct` 既有口径)。
- **P2 单向消解**:C → Ctron;手写 C 面只许缩小。确需地板增符号(如 S1 的 socketpair 归一入 net),以「两件重复实现归一为一件」净减记账。
- **P3 地板冻结纪律**:留下的每件 C 文件头部加**地板声明**注释块(所依赖 syscall/ABI 面、语义权威指针、审计口径),此后改动走三线一致 SOP + ledger 申报。
- **P4 消账度量**:`hand-C` 行数按 §0.1 命令随批重测,COVERAGE.md 记 Δ;新增任何 `.c` 须先在本设计登记裁决。

---

## 2. 切片

### S1 db socket 地板归一(消解 ~423 行)

**Scope**:`ctron_dbredis.c`/`ctron_dbpg.c` 全部符号(read/write/close/last_errno/connect/socketpair/entropy 转发)改由 `lib/db/bind.ct` extern 表直指 `ctron_net_*` 同语义符号;`lib/net/bind.ct` 补缺口(socketpair——测试夹具 I/O 用,归 net 地板);两 C 文件删除。interp 面 `x_` 前缀 fd 源 emit 专臂口径不变。
**Verify**:`tests/db` 全绿(redis_replay/rowmap/pool/sqlite);`tests/net` 零回归;ffi 链接口径(use 即链接)不变;`hand-C` Δ = −423。
**依赖**:无。**规模**:S(0.5–1 天)。

**实施增量(2026-11-04 实测修订)**:
1. ABI 适配:db 面 lane 缓冲(`&I64[]`)与 net 地板 `U8[]` 视图不同形——redis.ct/pg.ct
   外呼位改 U8 刮疤缓冲(`[0] ** len` 动态切片重复)。**咬出并修复发射臂 G5-W1
   既有缺陷**:Repeat 切片位非字面量 count 恒折 "1"(长度错 = 静默越界,interp 臂
   正确)——`trans_stmt.ct` else 分支补 `ct_expr` 运行时计数;双臂回归钉
   `tests/09c_repeat_dyn.ct` 随批。
2. net 地板最小增量(P2 例外记账):`ctron_net_socketpair`(AF_UNIX 流对,双端
   O_NONBLOCK 原样)+ `ctron_net_is_wouldblock`(errno 平台值 Ctron 层无表达面,
   redis −2 would-block 面保形所需);bind.ct 两 decl;净 Δ = −423+30。
3. 熵别名迁居:`ctron_dbpg_entropy` 别名(P5-D,避与 std/uuid.ct E5030)由 dbpg 垫片
   迁居 `ctron_entropy.c`(地板,本就保留)。
4. **组合消费即编译器缺陷暴露**:http.client.ct 与 db 两包各自 `use net.bind` →
   同模块不同 keep 集,done-skip 假设"中层不需要已完成模块声明"被破 → 消费体
   E2020/emit 缺 extern(组合探针实测)。修复:`parse_pkg.ct` done 一律照常重载
   走选择性合并(redo 放行到嵌套层;dup 处首胜静默跳过原语义不变,E5030 仍只拦
   入口面新撞名)。
5. 消费方换名(夹具/示例):`ctron_db{pg,redis}_{connect,close,socketpair,write,read}`
   → `pg_fd_{connect,close,socketpair,write,read}` / `redis_fd_{connect,close,socketpair,
   write,read}` 包装(x_fd_pipeline/x_pg_fd_session/x_rd_fd/pg_smoke/redis_smoke/
   perf/ctslink 七文件;lane host 参数改 Str,pg_str_bytes 面退役)。

### S2 cbox 登记表消解(~51 行)

**Scope**:`ctron_cbox_{reg,live,take,drop}` 定容线性表逻辑移入 `lib/ffi`(纯 Ctron,容量/扫描口径逐字平移);`free(p)` 走 libc 符号 extern 直声明(无垫);`ctron_ffi_strerror` 保留并登记「Str↔char\* const 限定符冲突」为语言面条目(直声明暂缓,随 FFI spec 修订批)。
**Verify**:`tests/ffi` 30/30(cbox/cbox_dblfree/cbox_useafter 三件差分);drv_parity;发射面 drop hook 链接随 use 即链接机制自动。
**依赖**:无。**规模**:S。

### S3 crypto 薄化(~90 行消解)

**Scope**:①hex 助手(`bytes2hex`/`hexdec`/`hexval`)删除,消费面改 `use std/enc`;②sha256/hmac 快臂保留并标注 `fast/native-only`(规范臂 = `std/crypto.ct`,双臂口径 P1);③ed25519 EVP 绑定 + `rand_bytes` 留(绑定地板,头部加地板声明)。
**Verify**:`tests/crypto` + `tests/crypto_vec` 双臂 parity(纯臂 vs 快臂逐字,FIPS 向量);bind.ct 变化随 decls 锁申报。
**依赖**:无。**规模**:S–M。

### S4 crc32 消解(~20 行)

**Scope**:表驱动纯 Ctron crc32(`lib/http/enc.ct` 或 `std/hash.ct`);`binddeflate.ct` 摘 `ctron_deflate_crc32`;`enc.ct` 折中注撤销;gzip 组框零面变化(已纯 Ctron)。
**Verify**:标准向量锚(`crc32(b"")=0`、单字节 `0xD202EF8D` 族)+ 语料双臂对照(纯臂 vs miniz 臂差分,保留一组对照测试防两臂漂移)。
**依赖**:无。**规模**:S。

### S5 tls CA 探测消解(~60 行)

**Scope**:`ct_ca_probe`(CA bundle 候选路径枚举)移 `lib/tls`(纯 Ctron);BIO/ctx/handshake/close_notify/cert 装载留绑定(地板声明随批)。
**Verify**:`tests/https`/`tests/net` tls 面;CA 缺席负例口径不变。
**依赖**:无。**规模**:S。

### S6(登记不实施)内核逻辑 Ctron 化

发射模板内核(GC 标记/回收、chan/task 队列、unwind 表)中的**纯逻辑子件**理论可随 P1 双臂外移,但内核原语(`setjmp` 寄存器捕获、`__thread`、`__builtin_*_overflow`、裸汇编)无语言表达面——**blocked 于 unsafe/intrinsic 面 spec 提案**(§9 决策,与本设计解耦)。本设计不携,仅登记依赖关系。

---

## 3. 全局门(每片不变)

suite rc=0;smoke 全绿;drv_parity 136/0;对应 lane 测试绿(S1 db/net、S2 ffi、S3 crypto(_vec)、S4 http、S5 https/net);meta_check 全绿;decls 锁变化申报;`hand-C` Δ 记账;`docs/c-rust-divergences.md` 相应口径同步(如有)。

---

## 4. 边界与备注

- **解释器宿主口(`cc2_*.c`)不在消解范围**:它们是 bit.*/str 内建的**解释臂原生实现链接面**(种子宿主 + 自宿 interp `eval_call.ct` extern 消费)。核心内建(位算/时钟/byte_at)位于语言之下,先例 = bit 派生助手消解而 bit 组合原语留口。
- **性能**:S3/S4 纯臂在解释口径无快速宽整型(`std/crypto.ct` 头注既有登记),故快臂保留——消解目标是**语义权威归一**(规范臂裁决分歧),不是删 C 快径。
- **排序**:S1 → S2 → S4 → S3 → S5(按风险升序、独立性排布);每片独立提交 + ledger 登记,可中断可续。
