# tests/ffi —— FFI 用例(§9.6·§9.8)

规范 §9.8 承诺的 FFI 多文件用例落点(自举发射面专测;解释器无 FFI 口径,不在此跑)。

## 口径

- **行为夹具**(子目录含 `c_src/`):`ctron-emit` 发射 C → `cc` 同批编译 `c_src/*.c`(编译期符号链接,无 dlopen)→ 原生二进制跑 `test` 块。
- **负例**(`*.neg.ct`):`bin/ctron-cc run` 编译失败,诊断含全部 `//@ fail:` 码。
- **lint**(`*.lint.ct`):诊断含全部 `//@ warn:` 码(rc 不判,与 suite.py lint 口径一致)。
- 一键验收:`sh tests/ffi/run.sh`;CI 经 `compiler/test/suite.py` 的 `ffi/` 小节。
- 前置:`compiler/native.sh`(产出 `bin/ctron-cc`、`bin/ctron-emit`)、`cc`。

## C 侧 ABI 契约

`ctron_abi.h` 镜像发射器预发 typedef(`ct_i`/`ct_fn1..3`/`ctron_view_*`);发射面为唯一真源,变更须同步。
C 侧持有 Ctron fn 地址即 `ct_fnK` 裸函数指针(无 env 槽,捕获闭包走 E4042 拦截)。

## 夹具清单

| 用例 | 承诺 |
|---|---|
| `callback/` | C-ABI 回调:裸 fn 名零包装直传 `ct_fnK`;C 侧循环回调、经切片视图回写数组(排序) |
| `repr_c/` | `#[repr(c)]` struct 声明序布局,C 侧同型定义即 ABI 兼容;按值双向传/返、填充布局(24 字节)实证 |
| `str_marshall/` | Str=`const char*` 直通;`str_from_c` 深拷入 arena(Ctron-owned);C-owned 串所有权约定;size_t 垫片注记 |
| `abi_width/` | 定宽整数/浮点/Bool/Str 逐宽度回环(spec §9.6 映射表;Bool 以 int 落界) |
| `*.neg.ct` / `*.lint.ct` | E1001(ABI 串)/ E4041(repr 误用)/ E4042(捕获闭包回调)/ W8050(未标 trusted)/ W8051(非 C-ABI 字段)/ W8052(extern 非 ABI 类型)/ W8053(extern 返回 fn) |

## 性能

`bench/` 为 FFI 边界微基准夹具,由 `compiler/test/bench_ffi.sh` 驱动(Ctron 发射面 vs 纯 C 循环对照)。
