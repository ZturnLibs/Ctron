# L4 · Bytes 类型设计立稿(§8 read→Result[Bytes] 承诺兑现径)

- 日期:2026-10-07
- 触发:03m_binary_nul_probe(L4 生死探针)自举 interp 面 rc=1——`read_or("/bin/echo")` 读二进制,NUL 截断使 `bin.len` 短计、`byte_slice` 越界。**病根=Str 的 C 串本质(NUL 终结)**:字节面(byte_at/byte_slice/read 长度)在含 NUL 载荷上结构性失真,非单点 bug。
- 现状(1007 实测):host(Java 之外的 C 参考宿主)interp 同病;emit 面 char* 同病。三宿主同源 → 类型级正案。

## 设计决策点(待裁决)

1. **Bytes 形态**:
   - A. 前奏类型 `Bytes`(len 携带,{ptr,len} 值/半值;独立于 Str)——spec §8 `Result[Bytes]` 字面承诺,推荐;
   - B. Str 升级为 len 携带(全局串域重设计,动一切,否决);
   - C. `read_bytes(path) -> (I32, Bytes)` 函数面对(最小面,无类型级字面)。
2. **字节访问面**:`Bytes.len: I64` / `b[i]: U8` / `Bytes.slice(a, n) -> Bytes`(显式 n,无负长);
3. **互转**:`Str.from_bytes(b) -> Str`(NUL 校验,失败 Option/Err)/ `b.to_str_lossy()`(替换 U+FFFD);
4. **emit ABI**:`typedef struct { unsigned char* d; int64_t n; } ctron_bytes;`(栈值/堆外置二选一,MVP=amalloc 堆);
5. **interp 域**:Val 增 tag(V_BYTES→cx 记录 "B" 族已占用→用 "Y")或 V_ARR[U8] 模拟(MVP 速达,精确域列后)。

## 施工切片

- L4-②:interp 字节保真链(read/read_or 返回 Bytes;byte_at/byte_slice 走 len)——探针 03m interp 面翻绿;
- L4-③:emit 面(ctron_read_bytes 运行时+fread 全量);03m emit 面翻绿;
- L4-④:负例族(Bytes 越界/Str 互转 NUL 拒)入 suite。

## 验收

03m_binary_nul_probe 自举双臂 rc=0 且 nuls>0、mid_ok=1;现有 Str 全语料零回归。

## 关联

03m 探针=量尺;FB-1 byte_slice 越界防护(已落库)为 L4-② 的护栏前提;spec §8 read 签名为类型级依据。


## 施工实录(1007)

**L4-③ emit 面已落地(五点先例)**:parse_pkg 名单/sem_calls prel/sem_type prelude_ok 值位门(三名单缺一即 E2020)/trans_expr 五映射/trans_ty "YB" 码+ct_ctype ctron_bytes*+call typeof 五臂/driver_emit ctron_bytes 模板(fread 全量读,bytes_at/slice 越界响亮 panic,to_str NUL 止)。锚 fx_bytes(smoke 扫描 195/0 基线,写后读回形双臂逐字一致)。**二进制 NUL 形 emit 已证**(/bin/echo 101136=精确长,magic 字节 202/190 ✓)。

**interp 面=降级已文档化**(read_bytes 走 C 串域 NUL 截断;写后读回文本形双臂一致,二进制形 interp=4 vs emit=101136)。**L4-②(宿主字节原语)未施工**——升起需 compiler-c 读原语(rt) + interp read_bytes 换 host 通道;03m 双臂翻绿以此为门。


## 施工实录二(1007 续)

**L4-② 自举通道已通**:host 原语两臂(read_bytes_len 装载静态缓冲/at 逐字节,rt_eval)+interp read_bytes 换通道+名单四处(prelude_ok 值位门=第三名单实证)+emit 映射/模板。**interp /bin/echo = 101136 202 4 190 = emit 逐字一致**(全量字节贯通)。

**发现并让名:host 早有 `read_bytes(n)`=stdin LSP 协议内建(rt_eval 1155)**——本设计 face 改名 `read_file_bytes`(全量文件字节)。host 值级四臂(read_file_bytes/bytes_len/bytes_at/bytes_slice/bytes_to_str,V_ARR of V_INT 表示)已入,bytes_len ✓(小文件 4);**残余 triage:bytes_at 返回垃圾值(44380013360,items 内容/生命周期)+ 大文件 SIGBUS(101136 值数组)**——疑 v_arr/items 交互或 arena 块语义,下片首查。

**03m 量尺 v2 已落库**(红=本残余门)。FB-12 双模已实施(argv offset:run=3/裸=1,双模实测 ✓)。


## 施工实录三(1007 续二)

**L4-② 完成**:①compiler-c rt_eval 双原语臂(read_bytes_len 静态缓冲装载/at 逐字节)②interp read_bytes 换 len/at 通道(全量 NUL 保真)③名单四处(prelude_ok 值位门=parse_pkg/sem_calls/sem_type 三处,or2 链程序化生成保平衡)④emit 映射+模板(read_bytes_len/at)⑤让名 read_bytes→read_file_bytes(host stdin LSP 内建冲突)。**三面全绿:host 03m=101136/88499/1(原生臂同)——03m 量尺翻绿**。残余勘验在册:arena 块上 items[0] 被后续覆写(指针值,k 保留)——读/切面改 malloc 旁路(插值 runner 短命,泄漏=设计);arena 交互机理归档待查。


## 施工实录四(1007 终)

**flaky 定性**:03m 宿主探针=suite 子进程上下文间歇败(空输出 rc=1),直跑/ASan 构建均 rc=0 干净——布局依赖的悬垂写类(ASan 分配器下不触发),非确定性逻辑 bug。**下片首查**:host 全量 ASan+压测复现(循环跑 03m×50 找必现形)/或 v_arr 拷贝语义审读。**当前基线**:常规构建 host 03m 直跑绿(101136/88499/1);smoke 213/1(1=同源 flaky 形)。


## 施工实录五(1007 终二)——极小必现形

**语句序依赖实证(确定性,非 flaky)**:同一块内,`assert_eq(bytes_at(h,3), bytes_at(b,3))` 先行=静默 rc=1(tests_total=0 分支,吞输出);两 println(bytes_at 同址读,打印 190/190 **相等**)先行=过。值相等而断言先行即败=eval 状态病(非值错)。上下文要件=完整 main(nuls 全量循环+slice)在前。**下片首查**:host assert_eq 双 eval 的 arena/状态交互(疑第一 eval 破坏第二 eval 的 h/b 绑定可见性,或 cx/env 栈深交互);复现件 /tmp/v_all.ct(过)vs tests/03m(败)仅语句序异。
