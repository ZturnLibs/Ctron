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
