# tests/http/bench —— P4-D 解析基准(ctron http_parse_head vs picohttpparser)

本地/nightly 性能门禁,**不入 CI 主环**(与 tests/net bench 同款惯例):置
`CTRON_HTTP_BENCH=1` 启用;`sh tests/http/bench/bench.sh` 一键构建 + 公平性
自证 + 采数 + 判门。

## 口径

- **语料**:三报文(GET 无体 162B / POST+CL 体 182B / chunked 响应 129B,
  合计 473B),`bench_parse.ct` 与 `bench_pico.c` 逐字节同文;**公平性由字节
  digest 双侧对 pin 承担**——两侧 digest= 不一致即 FAIL 禁止采数(round 内
  再验一次防漂移);另以 checksum 非零防解析被折叠(DCE)。两侧每请求语义功
  不等(pico 裸 parse 无上限/走私/严格档,ctron 侧全开),公平性由 digest
  同文 + 同语料承担;RED 判定对该语义差(<15% 量级)不敏感。
- **计时**:`ctron_net_now_ns()`(CLOCK_MONOTONIC)与 pico 侧
  `clock_gettime(CLOCK_MONOTONIC)` 同源;热身 1000 轮不计时;正式 N 轮
  (env `CTRON_HTTP_BENCH_N`,缺省 100000;登记采数用 1000000),每轮 3 报文
  = 3 次 parse;ns_per_req = 总 ns / (N×3)。
- **采数**:bench.sh 单次连跑 3 轮,每侧取 **min(ns_per_req)**
  ("3 取最小"惯例);ratio = ctron_min / pico_min。
- **门**:ratio ≤ 2 → GREEN;> 2 → RED + **登记**(计划 §Global Constraints
  口径:解析器工作在 net 层 `&I64[]` 字节道,每字节一条 I64 lane;门禁设定
  按字节宽度,超门即按 **8× lane 税**归因登记,归 P9 I8-typedef 处置;
  **数字照录,不改数、不换语料、不粉饰**)。全请求周期 ≤1.05× 承诺在 P6
  出口复核(本基准只测 parse_head 纯面,不含 IO/构造)。

## 采数登记(2026-09-21,本机 darwin arm64,N=1,000,000 ×3 取最小)

| 侧 | ns/req(min-of-3) | 说明 |
|---|---|---|
| ctron `http_parse_head` | **209**(复跑 204;round 内 209/211/218) | 纯层,&I64[] 字节道,emit 臂 cc -O1 |
| picohttpparser(master 快照) | **46** | cc -O1,char* 面 |
| **ratio** | **4.54×**(复跑 4.44×) | 门 ≤2× → **RED,登记** |

归因(诚实口径):4.5× < 8× —— 实测**劣于逐字节线性外推的悲观界但仍在
lane 税量级内**。宽度税两源:① 每字节 8B lane,同语料工作集/比较宽度 ×8,
SIMD 字节并行全失;② 上限扫描(row-count 类循环)按 lane 计数。I64 lane 上
的解析仍拿到 ~4.8M parse/s ≈ 754 MB/s 语料吞吐(均报文 157.7B),协议半层正确性
面(P4-A/B/C 57/57)不受影响。处置 = P9 I8-typedef(字节道换 I8 窄 lane)
后再对拍本基准;在此之前不为过门改解析器或语料。

## 结构

- `bench.sh` —— 一键门禁(构建/digest pin/×3 采数/判门;退出码 0 门内 /
  1 超门 / 2 环境缺件)。
- `bench_parse.ct` —— Ctron 侧(emit 专臂,不入 tests/http/run.sh 计例;
  digest 用有界模乘,注释详)。
- `bench_pico.c` —— pico 对照侧(char* 直调,字段求和防 DCE 同构)。
- `pico/` —— vendored picohttpparser(见 `pico/VENDORED.md`)。

## nightly 建议

`CTRON_HTTP_BENCH=1 CTRON_HTTP_BENCH_N=1000000 sh tests/http/bench/bench.sh`
(全程 <5s);连同 `tests/http/fuzz/run.sh`(见其头注,nightly ≥30min)
挂 nightly 环,fuzz 不入 CI 主环口径不变。
