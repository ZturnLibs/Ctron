# IP-23 23a 设计稿——JSON 数值模型(分批;2026-10-20 勘定)

> 来源:语言评审提案 IP-23(P0)、§12.6 前置工项(12-db.md:48"行绑定以 std/json
> 数值/布尔保真为前置")。现状勘定基于 lib/std/json.ct(2026-10-20 实测)。
> 关联:R 码 ABI 批(IP-01 错误链的 match/downcast 机制为本设计直接复用)。

## 1. 现状(勘定)

| 面 | 现状 | 缺口 |
|---|---|---|
| 解析 | `jnum_end` RFC 8259 ABNF 严格扫描;数值以**原文串**存扁平条目表(qtag/qval) | 无词法分类(Int/UInt/Big/Float)——分类信息在读取时重推 |
| 读取 | `jget_i64`(jint64 量级/越界门→Err("range"),非回绕)、`jget_f64`(量级窗 \|值\|≤I64_MAX)、`jget_bool`;jv_*/jk_* 便捷面恒经 Result 面 | 无 `jget_u64`(>I64_MAX 正整数不可取);无 BigNum 原文访问(超宽即 range Err);Float 量级窗截断语义未文档化 |
| 写出 | `num_json(v: I64)` 十进制原文;`jw_*` 面仅 I64 标量 | **F64 写出未钉**(语言侧浮点文本格式未定,登记在案);无 U64 写出;无原文保真写回 |
| 派生 | derive(Json) v2.2 容器双臂(plugins 11/0) | 数值字段映射未定义 U64/Big 规则 |

## 2. 数值模型(23a-1)

Ctron enum 即 tagged union,模型一行:

```c
@derive(Error)
enum JNum {
    Int(I64)      // 有符号整数域(含负)
    UInt(U64)     // >I64_MAX 的正整数
    Big(Str)      // 超宽:原文保真(round-trip 不失真)
    Flt(F64)      // 含小数/指数形态
}
```

- **词法分类**(解析期一次判定,存标签位;不新增条目结构——qval 仍原文,qtag 细分
  `n` → `ni`(Int)/`nu`(UInt)/`nb`(Big)/`nf`(Flt);既有 `n` 读取面按"四态皆兼容"
  处理,qtag 精确匹配面(jtagk "n")放宽为前缀匹配 → 存量调用零破坏)。
- **访问器**:`jget_u64`(U64 全域;Int 域正数亦收)、`jget_big`(原文 Str,四态皆可)、
  `jnum_of(entries, path) -> Result[JNum, Str]`(tagged 总口);`jget_i64/f64` 语义不变
  (UInt 超域 → range Err 维持)。
- **downcast 复用**:JNum 变体判别即 IP-01 的 match/downcast 机制(自举面已全)。

## 3. 写出保真(23a-2)

- **整数**:Int/UInt → 十进制原文(直出);Big → **原文原样写回**(round-trip 铁律)。
- **浮点 shortest-roundtrip**:钉死"最短十进制规范形"(parse(write(x)) == x 且在同值
  表示中最短;同长取更近)。实现两径:①移植解释器 df_*(eval_float.ct)规范十进制机
  至 std(纯 Ctron,双宿一致优先);②v0 简化形=量级窗内 %.17g 消尾零(不满足最短性
  时降级①)。**裁决点**:F64 写出格式为语言级钉子(num_json_f64 即规范定义处),
  需与 R 线(ctronr)同批对拍。
- **escape**:无(数值面无转义)。

## 4. 分批与验收

| 批 | 内容 | 验收 |
|---|---|---|
| 23a-1 | 词法分类+qtag 细分+三访问器+derive(Json) U64/Big 字段映射 | 超宽整数 round-trip 语料(U64_MAX/I64_MAX-1/2^64 越界原文)、存量套件零回归(qtag 兼容面) |
| 23a-2 | 写出保真(UInt/Big 直出+F64 规范形钉子) | parse→write→parse 幂等语料、精度边界 fuzz(±0/次正规/2^±1023)、双宿逐字 |
| 23b | db 行映射/web json 中间件切换 | 行绑定链路集成语料(§12.6 兑现) |

**双宿纪律**:种子发射器 trans.c 的 json 无特化(纯 Ctron 面)→ 天然双宿;R 线 ctronr
parity 另册(IP-23 提案原文既有)。

## 5. 裁决点(执行前需裁)

1. F64 写出规范形选择(23a-2 ①vs②;涉语言级钉子,建议 ①)。
2. qtag 细分拼写(ni/nu/nb/nf vs n+i 标志槽)——涉 qtag 直接消费方(文档面)。
3. derive(Json) 对 Big 字段的映射规则(Str 字段直收 vs 专型)。

## 6. 关联

- IP-01 match/downcast(JNum 判别复用);R 码 ABI 批(JNum 经 enum 通道=enumres 形,
  fx 语料 enumres 既有);IP-24(IP-25 s3 前置);§12.6(23b 兑现处)。
