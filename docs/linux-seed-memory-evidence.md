# Linux glibc 下 seed 解释器内存爆炸——证据包(移交编译器线)

> 日期:2026-09-20 · 状态:未修,发布链已绕行 · 证据:GitHub Actions probe 四轮
> (workflow: `.github/workflows/probe.yml`,可扩展续测;运行记录见 Actions 历史)

## 现象

`linux (glibc) + seed 解释形态` 执行 emit(以 cc_run.ct 为输入)时,进程 RSS 以
**~400MB/秒线性爬升**,~60 秒达 15.4GB(16GB runner 耗尽),平台收割 job
("runner has received a shutdown signal",exit 143)。**四轮运行全部同点复现,确定性。**

对照:**macOS 同命令 30 秒 / 峰值 2.4GB**,发射产物逐字节一致——同一份解释器 C 代码,
仅平台 libc 不同,内存需求差 ~6×。

## 已排除

| 假设 | 结论 | 证据 |
|---|---|---|
| 平台瞬时事件 | ✗ 确定性 | 四轮同点(~60-90s into native.sh/emit)复现 |
| glibc mmap 阈值碎片化 | ✗ 调参无效 | `MALLOC_MMAP_THRESHOLD_=256MB` 下照炸(T1) |
| 编译形态同样受累 | ✗ 有界 | 同工作负载的 prebuilt 发射器(linux 编译)完成 emit 且逐字节正确(发布链 natives 固定点 diff 在用) |
| 小负载即炸 | ✗ 大负载相关 | 相 0 小文件解释(input_cc)1 秒/181MB 正常 |
| probe 环境假象 | ✗ 已修 | 初版 probe 两处脚本错(build.sh 缺失/误用 run 子命令)已排除,修正后仍复现 |

## 关键事实

1. 爆炸点:emit cc_run.ct(12925 行拼接产物);cc_check/cc_emit 相未及测(前相即死)。
2. **编译形态(prebuilt ctron-emit 二进制)在 linux 上有界且正确**——爆炸仅存在于
   解释形态的值模型路径(动态 string/list/env 结构),非发射逻辑本身。
3. 增长形态为线性持续新触内存(RSS 单调),非碎片化假象 → 疑似解释器在 glibc 下
   的**真实过量分配**(分配需求本身 ~6×)。

## 给编译器线的下一步建议

1. 插桩定位:`ctron_amalloc` 加计数器/字节数打印,linux 下跑 emit,对齐分配序列;
   或 valgrind massif(慢但直接)。
2. 重点嫌疑:读文件/字符串增长的 realloc 路径、`ctron_list` 扩容、env 链克隆——
   找 glibc 与 macOS 分配器行为分歧处(如 realloc 大块迁移导致的耦合放大)。
3. 短期缓解已在发布链落地:linux 发布与 CI 走 prebuilt 直编
   (`CTRON_FROM_PREBUILT=1` + `tools/release.sh`,见 release.yml natives linux 臂)。
   解除影响面:ci.yml ubuntu 门禁(现每 push 红于 [3/8] native.sh)、linux 全自举验证。

## 2026-09-21 编译器线接棒:根因闭环 + 归因地图

插桩(已落库,环境变量门控默认零成本):
- `CTRON_MEM_DEBUG=1` → arena 记账(总需求/块阶梯/进度标记/退出逐块对账/尺寸直方图);
- `CTRON_FN_TRACE=1` → 按"当前解释执行的 Ctron 函数"归因分配字节(atexit 出 Top 榜)。

**实测(macOS 同工作负载,seed emit cc_run.ct):**
- 逻辑需求 **25,971MB**,分配 **2.098 亿次**,其中 <1KB 小分配合计 **20,477MB**(占 99.6%);
  逐块对账 used=21,346MB——需求是真实的、线性于解释执行的字符串微操作。
- 机制:永不逐对象回收的 bump arena × 被解释 trans 的海量微分配。**macOS 靠页压缩把
  RSS 隐到 1.5-3GB"侥幸"完成;linux 诚实按触页计数 → 400MB/s 线性爬升 → OOM。**
  这同时解释了 glibc 调参无效(不是分配器驻留,是需求本身)与"编译形态有界"
  (发射产物不经解释器值模型)。

**函数归因 Top(修复地图):**

| 函数 | 需求 | 调用数 | 病灶 |
|---|---|---|---|
| `or2`(lex.ct) | 7,395MB | 11,847,525 | 热路径 De Morgan 辅助函数,每调用帧 ~600B;v0.7 已有 `||` 运算符,历史写法可内联 |
| `ct_struct_tps`/`ct_structs`/`ct_is_enum`/`ct_enum_of_variant`(trans_ty.ct) | ~10,000MB | 各 ~28k | 每次 ~95KB:对 449 decl 全树线性扫 + 临时分配;被 ct_typeof Ident 回退路径高频调用 |
| `nl_set_line_end`/`nl_set_next`(lex.ct) | ~530MB | 各 12,288 | 每调用重建 22 元素 List[Str] |
| `tok`/`rpush`/`ct_env_ty`/`env_bind`/`ct_expr` | ~1,500MB | — | 次级面 |

**已证伪路径(避坑):**
- arena 末次分配原地扩展(拼接零拷贝):**语义不可行**——`let b = a` 共享指针,
  原地扩展破坏值语义不可变性(已回退)。
- char 符号性 / list 克隆风暴 / glibc mmap-TRIM-ARENA_MAX:均排除。

**修复排序(预期收益):** ①lex.ct or2 链改 `||`/`&&` 运算符(≈-7GB);
②ct_struct 查询簇单遍化/预索引(≈-10GB);③nl_set_* 内联(≈-0.5GB);
合计预期把 emit 需求压到 2-4GB,16GB runner 安全裕度内。余项见 FN 榜逐级清理。

## 2026-09-21 续:rt 三件修复落地——需求 26GB → 13GB(−50%),RSS 1.2GB,输出逐字节一致

实现(compiler-c/src,解释器值模型层;门禁 suite.py 0 红 + smoke 我方段全绿 + 输出逐字节一致):
1. **字面量零分配快路径**(rt_core.c str_expr):纯 TEXT 单部件字面量直接别名解析期
   NUL 常量(不可变,别名安全)——比较用字面量("Enum"/类型码等)此前每次求值都
   sb+astr 双重分配;分配次数 2.1 亿 → 4847 万(−77%)。
2. **env/bind 栈纪律复用**(env_push/pop/let):调用帧与绑定节点按 pop 入
   free-list、push 取用;闭包按指针捕获整条链(val.cap),故创建闭包时沿链标
   captured,pop 时被捕获帧只摘链不复用——语义与"arena 不可变"完全一致。
   or2 类微调用每帧 600B → 84B(−86%)。
3. **env_let 同帧同名原地覆写**:循环体逐轮重绑 let/var 不再逐轮新增 bind 节点。

剩余(13GB 构成):ct_struct_tps/ct_structs/ct_is_enum/ct_enum_of_variant 簇仍 ~8GB
(28k 次 × ~84KB,扫描循环内每迭代仍有 ~187B 未定界分配);RA 直方图探针已备
(__builtin_return_address + 计数),-O2 内联致符号化失真,复测建议 -O0 探针构建。
linux 验证:probe.yml 重跑 emit 工作负载 /usr/bin/time -v,峰值 <16GB 即解除 ci.yml 门禁。
