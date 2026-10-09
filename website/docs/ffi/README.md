# ffi 域包 —— C 边界错误包装

深层 errno→Result 的首档:把 POSIX「rc + errno」双值惯例折叠成 Ctron 的
`Result`。**消费形态 = `use ffi.{...}`**;规范面 = [语言规范·档位与互操作
](../spec/09-profiles-ffi.md)。参考页:[ffi.md](ffi.md)。

## API

| 函数 | 语义 |
|---|---|
| `sys_result(rc: I64) -> Result[I64, Str]` | rc < 0 = 失败 → `Err(err_str(errno()))`;否则 `Ok(rc)`。典型:`let fd = sys_result(dup(0))` 配 match/`?` 消费 |
| `err_str(e: I64) -> Str` | 错误码 → 人读消息(strerror 经包垫片中转,`str_from_c` 深拷入 arena 转 Ctron-owned) |

## 口径与坑位

1. **域承诺**:|rc| < 2³¹(POSIX 系统调用返回值恒在此域;发射面 Result 载荷
   槽 32 位,大值截断为已知行为)。
2. **errno() 为线程局域**,读「最近一次」系统调用/库调用错误码——包装调用与
   errno 读取之间不得插入其他系统调用。
3. **验收双通道**:包内 test 块 = 解释桥安全口径(不触 Str 返回 extern);
   Err 臂(strerror 消息面)由 `tests/ffi/err_wrap` 编译通道钉死——解释桥
   Str 返回截断是已知缺口,包装层不绕。
4. CBox[T]/cimport/union 指针限定等 C 互操作全族见 `tests/ffi/` 与
   `compiler/test`(ffi 37/37);本包只管错误包装一档。
