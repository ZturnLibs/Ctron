> **IP-23 23a 设计轮落库(2026-10-20,specs/2026-10-20-ip23-json-numeric-design.md;纯设计批——共享树对端在飞窗口零码零构建)。**
> 现状勘定(实测):解析=jnum_end RFC 8259 严格扫描+原文存表(无词法分类)/读取=
> jget_i64(越界→range 非回绕)+jget_f64(量级窗)+jget_bool,缺 jget_u64 与 Big 原文
> 访问/写出=num_json(I64) 十进制原文,F64 写出未钉(语言级登记)。设计=JNum tagged
> union 四态(Int/UInt/Big/Flt——Ctron enum 即 tagged union,match/downcast 机制
> IP-01 直用)+qtag 细分 ni/nu/nb/nf(兼容面=前缀匹配存量零破坏)+三新访问器;写出
> 分批 23a-2(整数/Big 直出+F64 shortest-roundtrip 钉子,两径待裁)+23b 下游(db/
> web)。三裁决点在档(F64 规范形径/qtag 拼写/derive Big 映射)。提案 IP-23 TODO→DESIGN。
>> **宿主 downcast 差分补齐(2026-10-20;IP-01 宿主面收官,downcast 进套件双宿门控)。**
> 种子解释臂 downcast[E](e) 臂(rt_eval TypeArgs-Call 族 Box 同款):AnyError 沿 cause
> 链取最内层载荷(V_TAG 变体名经 ctron_enum_name_of 反查枚举名与显式 E 比对——**全名
> helper 不适用于判别**(判别比较的是枚举名),ctron_enum_full_of 留作 code 面)+
> ctron_enum_name_of 导出;10_trace 增 downcast 正反例测试(Fail=hit/HttpError=miss,
> **块臂形态**——非块臂首版踩 R2b 语句位纪律即改)。套件 behavior 双宿自此门控 downcast。
> 余=种子发射器 downcast 面(per-type Option 结构合成,登记债;自举发射臂已全形)。
> 门:10_trace 三面(种子 4-4/0/cc rc=0/发射 rc=0)+suite 119/120·120/120(07a 在册
> 零分歧)+meta_check 全绿。
>
> **IP-01 注册名全形贯通(2026-10-20;code="枚举名.变体名",三面同步)。**
> 解释臂 context() 物化点(eval_enum_of_variant 反查前缀)/发射臂 ct_anyerr_wrap_e
> (ct_dot_join 组合 tname+变体名,直构与名表两路全形)/种子 ctron_err_code_of 加 R 参
> (+ctron_enum_full_of 镜像 is_variant 走查,V_TAG 全名)。spec §5.4 契约升格注册名
> 全形;10_trace 两断言 "Bad"→"Fail.Bad" 双宿双臂绿。**IP-01 余项仅=宿主 downcast
> 差分**(种子解释/种子发射 downcast 面+种子发射擦除助手 code 变体名,ptr_as_view
> 「自举先行」先例);提案状态更新。门:10_trace 三面 rc=0/3-3/0+suite 119/120·120/120
> (07a 在册零分歧)+smoke --full 239/0+sync_site_spec 13 件随批。
>
> **R2b 尾判定勘定收束(2026-10-20;非块臂语义全图定案)。**
> 尝试统一(语句位合成 Expr)被 clo1 闭包尾段错误咬回——**全图定案**:①fn 体尾槽
> match=ct_match_value 值形(ct_is_value(Match)=false 设计注释「臂内 return 负责退出」);
> ②闭包/子块尾(经 ct_prescan_tail→ct_stmt)= R 分支/s 模型臂 **return 位即闭包返回**
> (clo1 `||{ match … {Some(s)=>s}}` 依赖实证);③中位语句 match 非块值臂=提前返回
> 误用面(**在册债定稿**:解释臂=丢弃,发射臂=提前返回——**写法纪律=语句位 match 用
> 块臂**,idiom 清单随批);四臂回无条件 return 位(#tail 标记方案试过即撤——尾槽判定
> 依赖 NL 折叠空白,标记不可达)。R 分支 OK/ERR 语句位(W2a)与 SOME/NONE return 位
> 并存口径不变。尾判定的健壮解=解析层尾分类(p_block 尾槽 NL 折叠规则显式化),
> 归 R 码 ABI 批后续。
> 门:dc1/r1/r2/r2b 四探针双臂逐字一致+10_trace 发射 rc=0+clo1 闭包尾恢复+mid1 中位
> 勘定探针(分歧面留档)+smoke --full 239/0(新增 fx_opti32/b 两件)+suite 119/120·120/120。
>
> **⚠️ 自举发射回归移交(2026-10-21;对端批C-2 致 native.sh 自举失败,证据链完整移交)。**
> **症状**:d8c4d3b0(main 尖)起 `sh compiler/native.sh` 确定性失败——发射 cc_emit.ct 时
> `ct_expr:index 目标非 List/数组:i 名:pty 下标:0 节点元数:3 尾槽tag:Int`(gui_parse.ct:4938
> gui_ds_gen `pty[k]`,env 查 pty 得 "i"=未绑定;崩点 env dump=#clofn:ct_call_infer_tys
> 的 genv(pass1),pj/gq/tys/tpsn/pgd 在而 pty 缺)。
> **二分表**:97303043 GREEN/native;f1e0aee3(W1-W3)RED;f1e0aee3−genv hunk RED;
> f1e0aee3−casts hunk RED;同源复跑恒 RED(非闪烁)。8MB 主栈下曾有 ASLR 抖动形
> (忽绿忽红),ASAN 实证 eval_expr 栈溢出——**种子大栈修复随批落地**(compiler-c main.c:
> run/trans/build 三深递归子命令走 256MB 独立线程栈,大栈后确定性复现真错)。
> **定责**:f1e0aee3 的 W3 分支(ct_call_infer_tys 增第四分支)与 genv 型参绑定组合
> 触发——单一 hunk 撤除不复原(两 hunk 各自独立 RED)提示**环境链/单态化选择层面**
> 的扰动,非局部代码错。**对端门缺口**:批C-2 门=decls/trans_v0-v6/fx 子集/Box 探针
> ——**未含 native.sh 自举**(发射 cc_emit 必走 gui_parse 全量)。
> **临时工作径**:自举 bin 于 97303043 基线可建(bisect GREEN 实证);对端修复前
> compiler/bin 沿用绿基线产物。
>
> **IP-23 23a-1 落地(2026-10-21;JNum tagged union+qtag 细分+三访问器,json_fidelity 15/0 双臂)。**
> lib/std/json.ct:①解析位词法分类 jnum_class(nf 含 ./e;整数按域 ni(|x|<2^63,含
> -2^63)/nu(2^63..2^64-1 正)/nb(越))——qtag 细分 ni/nu/nb/nf,qtag 兼容面=jtagk 裸 "n"
> 前缀匹配(存量零破坏,jint64 frac 路径保真);②私有 JNumR 更名让位 **pub enum JNum**
> 四态 tagged union(Int/UInt/Big/Flt);③三新访问器 jget_u64(ju64 全域折叠越域 range)/
> jget_big(原文保真)/jnum_of(tagged 总口);④jw_val 数值族渲染;⑤种子副本同步。
> 新件 f_num_domains(四态 qtag+四 Ok 面+U64_MAX 取值+超宽原文+range 拒绝)双臂绿。
> **随批附带**:trans_expr Int 宽域后缀字面量 → C 无符号后缀(u64/usize→ULL,u32→u;
> 裸十进制 >I64_MAX=C 符号溢出)**入源,emit 臂验证待对端 native.sh 修复后**(现 emit
> bin=绿基线遗留);**在册**:嵌套 enum 载荷绑定(Ok(Big(raw)) 形)=扁平 R 族缺口
> (夹具用受撑形:变体判别由 qtag 承担+Ok(_) 面),jnum_of 变体内嵌绑定解锁后升级;
> F64 to_string 两制=23a-2 钉子(设计轮 §3)。**门**:json_fidelity 15/0 双臂(新件
> f_num_domains+存量九件零回归)+json.ct inline rc=0+suite 119/120·120/120(07a 在册)
> +meta_check 全绿;smoke --full 跳过(对端批C-2 native.sh 回归在册,门=24 步等效
> 子集:json_fidelity+suite+meta_check)。
>> **IP-23 23a-2 F64 写出钉子落地(2026-10-21;裁决点1收口径①,两制归一)。**
> 规范形字节级钉死:[-]int[.frac]——积分恒 ".0" 二段(零 → "0.0",±0 无负
> 号)/非积分 6 位有效最短形(第 7 位半升+尾零修剪;定点窗 [1e-4,1e6),窗外
> e±XX 两位指数=df_g/%g 同形)。规范定义处=num_json_f64(std/json 纯 Ctron
> 规范机移植:jf_pw 幂升梯+jf_can 整性余尽判+6 位舍入逐支镜 df_g+私有测试
> 助手 jf_rt/jf_rt_eq);三臂同形:解释 df_show(原形即钉形,零改)/种子
> fmt_val(rt_core ±0 归一+界内试探防 ≥2^63 UB)/发射 ctron_f64_to_string
> (自举 driver_emit prelude+种子 trans_rt 零形补齐)。**两制归一定案**:
> 推断形/注解形分歧=型别分派所在(fx_conc_spawn 实证 "32"=Int 臂/"32.0"=
> F64 臂),钉后各归其形。**臂落地**:自举 trans_expr.ct f/g 臂(入源,bin
> 刷新后值面验证——jn f3 渲染债随臂销账)+种子 trans_expr.c T_FLT to_string
> 臂(调用位内建成员分派新立,原 v1 仅表达式位「仅支持具名函数调用」错)+
> ctron_fmt_f64 fam 死闸提名修复(fmt 截取先 return 从未可达,种子发射面
> 首次实际-use 故提名)+种子双面探针 VERBATIM-MATCH(9 行:积分 .0/±0 归一/
> e 形两位指数/负值/println 面)。**语料**:f_num_write 双臂(电池+写读写
> ≤2 步收敛+≤6 位有效精确往返+nf 原文铁律)+i_num_tostring(interp-only,
> i_ 前缀=run.sh 出臂跳过[x_ 逆先例],bin 刷新转正——to_string 值面毒化陈
> 旧 emit 编译故隔离)。round-trip 口径定案:write∘parse ≤2 步收敛不动点
> (e 形解析落定点窗一步定型)+≤6 位有效定点窗内精确往返;径② %.17g 暴露
> 双宿值模型分歧(0.3 vs 0.30000000000000004)故否,6 位投影掩蔽噪声保逐字。
> 夹具纪律:跨模块调用实参经 fixture 帮手/注解局部(陈旧发射器对 List 索引
> 与直呼表达式实参推 "N" 不折算,bin 刷新后可化简)。种子副本随批同步。
> 门:json_fidelity 18/0 双臂+suite 119/120·120/120(07a 在册零分歧)+
> meta_check 全绿+种子面 suite_rt 基线等值(dom.set_body/Region 双红在册)/
> suite_trans 15/0/suite_run 17/0;test_lex 基线即崩(陈旧 harness,门外
> 步骤,与本批零交集,在册)。**余**:自举 emit 臂值面待 native.sh 修复
> (对端批C-2 在册,Int 宽域后缀同批待验);23b 下游(db/web)未动。
>
>> **嵌套 enum 载荷绑定落地(2026-10-21;随 R 码 ABI 批,Ok(Big(raw)) 形三面)。**
> 勘定:解释臂 pat_match 递归原生支持(PatAgg→SubTup 逐载荷递归,自举/种子双
> 解释臂现网即通);**发射臂双缺口咬出**——①okIx/someIx 单槽扫描后臂覆盖前臂
> =多 Ok 臂静默丢臂(嵌套语料 "other" 假绿实证);②Ok/Some 载荷只认 PatId,无
> 嵌套分派(Err 臂 R2c 已有,Ok/Some 侧缺)。**治**:trans_stmt.ct R 分派臂
> Some/Ok 各改源序 else-if 链(首中即断=解释臂同义),载荷嵌套用户枚举
> (PatAgg/裸变体 Ident)→ 内层 variant 判(条件内联 cast `(ct_enum_N*)(long)
> rme.v)->variant==k`,镜像 Err 臂 t_mn 链)+ct_variant_pcodes/ct_unwrap_i 直绑
> (初版误传 C 型串 pc 给 ct_unwrap_i——Err 臂同款坑二度咬出,pct 才是码);
> 简单臂保 T53 f 双关/u: 解引用/hR recv 原逻辑,臂体语义各保其位(Some=return
> 位/Ok=合成语句位)。**验证路径立功**:ctc.sh emit=种子解释现行源拼接 cc_emit
> 现行发射——gui_parse 泛型回归(批C-2)不触达小目标,**现行源发射臂验证在
> native.sh 修复前即可做**(此后 emit 面验证默认此径,bin 刷新债仅余 smoke
> --full/全量自举)。嵌套探针双案逐字(9e15 直绑/Int 落 Ok(_))+语料
> i_jnum_nested(interp-only,bin 刷新转正——现网 bin 陈旧 emit 静默丢臂)
> 四态直绑(nb 原文/nu U64 全域/ni 负域/nf 浮点)+首中即断链序+内层通配
> Big(_)+noent Err 面,发射面(现行源)rc=0。种子发射器勘定=响亮拒绝
> (「v1:载荷子模式不支持」),归种子发射器批统筹。jnum_of 消费面升级形=
> 本语料(f_num_domains 受撑形待 bin 刷新随升级)。初版三误自咬(els 闭括号/
> panic 行错位/if 守卫误删)——重读即修,零残留。
> 门:json_fidelity 19/0+suite 119/120·120/120+meta_check 全绿+嵌套/钉子
> 双探针现行源发射面 rc=0。
>
>> **F64 写出钉子 emit 值面销账(2026-10-21;23a-2 余项,ctc.sh 径)。**
> 种子/自举发射 F64 to_string 臂经 ctc.sh 现行源发射探针逐字全对(32.0/0.0/
> ±0→0.0/3.5/1e-05/1.23457e+06/-3.5+assert 族)——23a-2「emit 臂值面待
> native.sh」当场销账;i_num_tostring 转正仍待 bin 刷新(json_fidelity 出臂
> 用陈旧 bin,径不同)。Int 宽域后缀同径可验,留对端批。
>
>> **种子发射器 downcast 面落地(2026-10-21;IP-01 收官债销账,10_trace 四面)。**
> 勘定:种子错误链面已具 ? 两段擦除/ctxres 链/opt match 机械,唯 anyerr 布局
> 缺 magic/payload/tname 三字段(vs interp/自举)+Err 构造无 T_ERR 期望+
> downcast 无臂。**四处落**:①trans.c anyerr 布局升全形(前插 magic,尾补
> payload/tname——自含 typedef 字段名访问零扰动);②Err 构造 T_ERR 期望臂
> (let e: AnyError = Err(Bad) 形,per-enum wrap 助手 use_sum 注入:magic+
> payload 值拷贝+tname+code=枚举名.变体名[CTRON_WVN 变体名表静态注],镜像
> rt v_err_t);③downcast[E](e) 臂(EX_TYPEARGS 闸;per-type Option 结构合成
> ctron_opt_<E> use_sum+魔数+tname 判别+cause 链下探取最内层载荷[rt 同义]/
> 载荷指针回交 some 槽,未命中 NONE;ek/bits=枚举表索引+tname2=枚举名递
> Some(x) 绑定型别面——match 读侧 Sum-branch 随批补 T_ENUM tname2 递名);
> ④ctron_erase_e_* 桩升真身(双站同文本 use_sum 单注;原桩丢 payload/tname/
> code=downcast 恒 miss/code 断言空根)+ctxres 链节点透传 magic/payload/tname
> (镜像自举 ct_ctxres)。**门**:10_trace 四面全绿——种子解释 4-4/0(在册形)/
> 种子发射 rc=0 全测(本批新通:code="Fail.Bad"/downcast 命中+未命中/context
> 链)/自举 cc rc=0/自举发射 rc=0;探针 t1-ok+dc-hit;种子面 suite_trans 15/0/
> suite_run 17/0/suite_rt 基线等值(双红在册);suite 119/120·120/120+
> meta_check 全绿+json_fidelity 19/0。**IP-01 全项清**(宿主 downcast 差分/
> 种子发射 downcast 面/code 变体名三债全销)。**余册**:种子发射 ?/擦除 trace
> 恒 "main:1"(调用点 file:line 未穿线,v1 桩形在册);Some(载荷绑定) 种子面
> 已通型别递名,深嵌套子模式仍 v1 闸(嵌套批在册)。
>
> **R2c 枚举浮点/宽载荷位双关+U64 to_string(2026-10-20;IP-23 23a 前置地基)。**
> JNum tagged union 探针(jn.ct)连环咬出三枚枚举机械通用缺口,全修:①ct_wrap_i 补
> f/g 位双关(构造侧;值截断曾致 Flt(3.5)→3);②新增 ct_unwrap_i(读侧逆,f/g 双关/
> u:/标量回退)+用户枚举/嵌套 Err 三绑定位接入(初版误传 C 型串 pc 当码——pct 才是
> 码,咬出后修);③to_string 补 "7" 臂(U64 全域 round-trip,ctron_u64_to_string)。
> **撤回记录**:f/g to_string 臂试落即撤——F64 to_string 解释臂两制(推断形 "32"/
> 注解形 "32.0",fx_conc_spawn 分歧实证)=**F64 写出未钉钉子本尊**(IP-23 23a-2 地盘,
> 设计轮 §3 已档),未钉前不落臂;jn 探针 f 渲染 "3"=该债留档,值面(绑定位 3.5)已正确。
> **余**(23a-1 余项):json.ct qtag 细分+三访问器+语料;23a-2:F64 写出钉子(两制归一后落臂)。
> 门:jn 四态探针双臂逐字一致(i5/u 全域/b 原文/f3=渲染债留档)+dc1/r1/r2/r2b 四探针
> 回归一致+suite 119/120·120/120(07a 在册零分歧)+smoke --full 239/0。
>
> **R2a 零码改完成+fx 语料转正(2026-10-20;R 码 ABI 批 2 前半)。**
> R1 的 Optional 臂(`非 Str → "R"`)顺带修齐全族:`I32?`/`Result[T,E]` 形参/注解 let/
> 返回/直构实参(Some 构造直传)/`.or`/`?` 传播全链双臂逐字一致(r2/r2b 探针)——塌缩
> 真身勘定=Optional 节点无臂落 "i"(码层)而非 ctype(其 "R"→ct_res 映射本就在)。
> fx 语料转正两件(fx_opti32/fx_opti32b,smoke 名单随批,金源=解释臂运行时对比):
> Option[I32] 形参+None 实参+直构实参/`.or` 缺省/`?` 传播+再包装全形态常设哨。
> **余 R2b=尾判定统一**(设计裁决项:尾值臂 return 位[fx_optstr 依赖]vs 语句位
> [解释臂语义],需 ct_stmts 尾标志穿线)——设计文档 §4 已更新二偏离实录。
>
> **R1 Str? NULL 模型族落地(2026-10-19,R 码 ABI 设计轮批 1;扁平 R 族成员一二三销账)。**
> ①`ct_ty_code` 翻面:`Str?`/`String?`(parse=Optional 节点,**非 Named-Option**——初版
> 翻 Named 臂被签名 int32 咬出,Optional 检查须先于 Named 闸)+显式 `Option[Str]` 拼写
> → "s"(char* NULL 模型,read_file 族同形;该形态原塌缩 int32=破的,无存量依赖)。
> ②`ct_optstr_unr` prelude 适配器:ct_res 构造(Some/None 恒产 ct_res)→ char*,三消费
> 位接入=注解 let(ty=s 且 init R)/return(#fnret=s 且值 R)/let-初值 match 值位
> (新 s 分支:t_mv NULL 模型值位形,json.ct write_json 消费实证)。③`?` on char*:
> Str? 返回 fn 内 NULL 传播(三臂:表达式/Let+Try/语句位),域外响亮拒绝(旧静默按
> ct_res 解包=错位)。④s 模型(match 语句位 Option[Str])非块臂=return 位(**勘定回滚**
> :R1 一度改语句位,fx_optstr name_or 尾值臂被丢弃零输出——与 R 分支 SOME/NONE 同约
> 定,**尾判定统一批债再次确证**);t_mv 随 nline 唯一化(原单 match 每函数限制)。
> ⑤`.or/.expect/is_some` 对 char* 接收者补 NULL 模型形(std 包 strip_suffix/hex_
> decode/b64_decode 消费面)。
> **消灭**:扁平 R 族成员一(形参塌缩)/二(注解 let 塌缩)/三(Str? fn 返回不配)+
> std 发射臂 json 新红。**余**(R2 批):Option/Result 其余载荷(仍 R→int32)、
> Option[Str] 直构实参位(Some(x) 直传 Str? 形参)、尾判定统一。
> 门:r1 探针全链双臂逐字一致(注解本地/None/调用/`?` 传播+拼接)+fx_optstr/std 包/
> json 模块矩阵全绿+suite 119/120·120/120(07a 在册零分歧)+smoke --full 237/0+w7 3/3。
>
> **R 码 ABI 设计轮落库(2026-10-19,specs/2026-10-19-r-code-abi-design.md;扁平 R 族专门批前置)。**
> 勘定沉淀:表示分裂三族地图(NULL 模型 s/ct_res R/通道 hR——read_file typeof 一路是
> "s",NULL 模型族从来不在 R 码里)+已有可复用机制清单(#elem/#cerr 侧条目族/ct_err_
> elem_of/R-branch 全臂)。三方案:A=R 码复合化 R<ok>,<err>(7 处精确比对+10 处码面
> 触点,回归面最大)/B=侧表+节点解析分期(推荐:ct_ctype_of_node+期望型线程,Str? 族
> 先行)/C=ct_res 统一(否决,read_file extern 直返)。分期:R1=Str? 族(形参/注解 let/
> 返回值/#cret 线程/? on char* 传播臂),R2=其余载荷+尾判定统一。执行前必读三锚在档。
> 状态:设计稿未排期,新会话满预算执行 R1。
>
> **错误链收尾批:ct_err_elem_of Str 码形真修+match R 臂语义勘定(2026-10-19)。**
> ①ct_err_elem_of 改 ct_ty_code 正形(初版手拼 "E:"+名 对 Str 载荷产 "E:Str" 幻影码
> → ? 包装臂拉 ct_vnames_Str 未定义编译错;正形 s/E:名/u:AnyError 三态)。②match R
> 分派臂语义**勘定并回滚**:SOME/NONE 非块臂=return 位是**尾位 match 值臂的发射实现**
> (fx_optstr name_or `Some(s)=>s` 金源实证,conc_optstr smoke 咬出依赖;解释臂值语义
> 与发射臂 return 语义在尾值形态可观测等价)——本批一度改语句位致 conc_optstr 段错误,
> 即批回滚;现态=OK/ERR 语句位(W2a,曾为 panic 无存量依赖)/SOME/NONE return 位(既有);
> **债精确化**:语句位 match 的非块值臂=提前返回误用面 + 尾判定统一批(需 ct_stmts 尾
> 标志穿线,值臂落 t_mv 暂存形)。③新立在册(uni1 探针连环勘定):Option/Result 作用户
> fn 形参=塌缩 int32;注解 Option[Str] 本地位 let=塌缩 int32;Str? 用户 fn 返回=ct_res
> 构造对 NULL 模型不配——扁平 R 族三成员,统一待 R 码携带 E/Ok 码的 ABI 批。
> 门:dc1/uni1/ok1 三探针双臂逐字一致+10_trace 发射 rc=0+suite 119/120·120/120(07a
> 在册零分歧)+smoke --full 237/0+w7 3/3。
>
> **IP-01 W2b downcast 落地+contains typeof 分歧销账(2026-10-19;IP-01 自举面收官,状态 IMPL→W1+W2a+W2b 全落,余注册名贯通"枚举.变体"全形在册)。**
> ①`downcast[E](e: AnyError) -> Option[E]` 四面:sem 前奏双册(sem_calls prel+prelude_ok)/
> 解释臂(eval TypeArgs 臂;vals 元素=Str 编码值须先 v_unbox——初版直取 .tag 被 Ctron
> 发射器 Member panic 咬出「解释器值表示=Str cx 非 Val 结构」)+eval_enum_of_variant
> 变体名反查助手(cc_run 拼装不含 trans 段不得跨段调 ct_enum_of_variant,decl 锁
> 530→531 随批实测代抬)/发射臂(TypeArgs-Call 特化:魔数+tname strcmp+载荷指针回交
> ct_res Ok 槽)+typeof "R"(match Some/None 与 .or 族 R 通路消费)。探针 dc1 双臂逐字
> 一致(downcast[Fail]=hit/downcast[Other]=miss,判别只在调用点)。**宿主差分在册**
> (种子解释/种子发射缺 downcast 面,ptr_as_view「自举先行」先例); suite 夹具暂缺
> (宿主列会红),探针在 /tmp/ct_probe/dc1.ct 待宿主补齐后随批转正。
> ②contains typeof 补 "b"(Member-Call typeof 落 i 使 Bool.to_string 走数字形
> 1/0 vs 解释 true/false——上批新立分歧即批销账,双臂 true/false 逐字一致)。
> ③在册新立:**match R 分派臂非块体语义不一致**(SOME/NONE 臂=臂值作函数返回
> [既有],OK/ERR 臂=W2a 改语句位——四臂两制待统一批);种子 trans 恒不能吃编译器
> 源(cc_run/cc_emit)= 既有形态非缺口(native.sh 管线是 seed 跑 Ctron 发射器,seed
> trans 仅服务用户程序,勘定记录)。
>
> **IP-01 W2a 错误链发射面落地(2026-10-19,context/擦除物化/trace 追加/变体名表;10_trace 发射臂首次全通)。**
> 自举发射器 AnyError 物化六件:①`#cerr` env 侧条目(ct_fn 绑定本函数 Result 错误侧
> 码形,E:名/u:AnyError;#elem 同族,闭包沿用外层=在册近似);②Err 构造位物化
> (#cerr=u:AnyError 且载荷枚举/Str → ct_anyerr_wrap_e/s,Ident 真变体验定后直带名);
> ③`?` 三臂物化(Let+Try 专用臂/语句位/表达式位——初版漏 Let+Try 专用臂被探针咬出);
> ④context() 通路(typeof "R" + ct_ctxres:Ok 直通/anyerr 链保留 code+trace/trace_join
> 追加 "ctx:<msg>" 与解释臂语义对齐);⑤错误字段读 ct_err_fld(message/code/trace,
> 魔数判型安全落空;typeof 侧同步 s——初版只加发射未加 typeof 被分脑咬出);⑥枚举
> 变体名表 ct_vnames_* 随 typedef 直出。附带:R 分派臂非块体解锁(合成 Expr 语句委派
> ct_stmt,assert 族专道复用,四处 panic 撤销);种子发射器 ctron_anyerr code 填充在
> W1 已带,其擦除助手变体名仍空=在册分歧。**PM-05-1/PM-05-2 翻部分兑现**(spec 附表
> 状态不变,downcast=IP-01 W2b:需 sem/解释/种子四宿注册,在册)。坑:①Bool.to_string()
> 发射=数字形(1/0)vs 解释=true/false——新立在册分歧;②码形纪律=ct_err_elem 家族
> 返 "E:<名>" 码形(初版裸名被消费端前缀查咬空)。
> 门:10_trace 双臂 rc=0(夹具发射臂首次通过)+w2a 探针双臂逐字一致(code=Bad/链保留
> /trace)+suite 119/120·120/120(07a 在册零分歧)+smoke --full 237/0+gui 100/1(唯一红
> =对端在飞新夹具 w4_ctml_reload 时序 flake,单独复跑 PASS rc=0 非回归)。
>
> **meta_check 13 红清偿+IP-18 承诺附表门(2026-10-19)。**
> ①meta_check 13 红(在册债)清零:bench/engine-proxy+steno/soak+tui/s* 三泳道 13 份
> main 程序族缺泳道 skip(gotcha #17「新泳道=meta_check 加 skip」随批漏课,steno G 批
> 与 tui W 批引入),补 bench/steno/tui 三 clause(net/src 先例)。②IP-18(P0)兑现:
> 12 卷规范各卷末「承诺状态」附表(PM-<卷>-<序>,三列 承诺/状态[兑现·部分·志向·证伪]/
> 退出条件;7 项点名漂移全入表:增量编译 PM-02-1/分代 GC PM-06-1/原生后端志向面/
> Simd 硬件 PM-03-1/HTTP 契约 PM-11-1~3/json 保真 PM-12-1/debug_assert PM-04-1)
> +README 冻结面句(「冻结面之外的承诺以附表状态为准」)+meta_check 附表互查门
> (每卷有节/ID 卷号一致/状态词四值/证伪行登记号须见于 COVERAGE——PM-06-2 WasmGC
> 证伪随门首登记入 §9 节,T38 触发=多宿主落地)。门:meta_check 全绿(626 文件+
> 附表门)+std_doc --check 82 页逐字节+sync --check 无漂移(mkdocs=CI 侧门)。
>
> **IP-01 W1 错误码面落地(2026-10-19,AnyError.code 双解释宿+spec+夹具)。**
> AnyError 增 `code`(只读;擦除后仍可读的原始错误码):解释臂 context() 物化点填
> 载荷自身标识(变体值 ["T",名]→变体名/Str 错误→串;context 链上 payload 已是
> AnyError 时保留最内层码,不随包装漂移);种子 errval 增 code 槽(v_err_t 五参化
> +ctron_err_code_of 载荷提码:V_ERR 内层保留/V_STR 串/V_TAG 变体名)+成员读
> `.code`;种子发射器 ctron_anyerr 增 code 字段+.code 成员+ctxres 链臂保留内层码。
> spec §5.4 错误码契约成文(derive(Error) 注册名贯通+downcast=IP-01 W2 在册);
> 10_trace.ct 增两测试(擦除可读+链保留,断言 e.code=="Bad"/message 取最近上下文)。
> **坑与发现**:①解释臂枚举变体值=["T",名] cx(tag8)非裸串——初版按 tag5 判载荷
> 臂空码,探针 ec1 当场咬出;②**`ctron-cc test` 子命令恒空转 rc0(不执行 test 块,
> run 子命令才是真执行面)**——必败断言探针实锤,套件 protocol 用 run 故无恙,
> 用户面 `ctron test` 尚未接线(README 为期票),在册;
> ③发射臂错误链 ABI 缺口升级立案(自举发射器无 AnyError 物化:context() 无
> typeof 臂致 10_trace emit 不通;扁平 ct_res 不带 E 码=E 恢复结构性限制;
> 种子发射器已有每型单态化 ctron_res_*_Err+ctron_anyerr 形可参照)——IP-01 W2
> 主战场;擦除助手 code 暂空(变体名表 W2)。
>
> **s23 销账+元组 match 双宿补齐(2026-10-19,gui 阶梯 101/0 首次全绿)。**
> ①s23_sk_native 段错误根修:gui_sk_load 烘焙(driver_emit β3 段)只填 SoA 16 槽,
> t_GuiTree 其余 11 字段(vreg_*/vc_*/props_dflt/env_n/env_v)裸声明栈垃圾返回——
> 消费端 gui_list_ns/ni 的 NULL 门控被击穿(lldb 单帧 gui_list_ni 入参=二进制静态
> 段地址实证)。修=`t_GuiTree t; memset(&t,0,sizeof(t))` 全零初始化(新字段自动
> 覆盖,契约 NULL 语义正合消费端注释)。复现配方/调试法随前批 COVERAGE 在案。
> ②裸元组模式 match 语句三宿补齐(前批在册缺口):发射器 trans_stmt 加 TP 分派路
> (旁观式求值一次入临时+PatTup 按 TP 元素码解箱,镜像 Member t.<k;v0=单臂解构,
> 多臂/守卫/嵌套模式响亮失败)+种子 rt_core.c pat_bind PAT_TUPLE 桩补齐(镜像
> AG_TUPLE 载荷绑定;let 位 bind_value 早已同构,唯 match 位缺)。探针 sp4/sp9
> (I32,Str 混合槽)双臂逐字一致;夹具 02e_match_patterns 加双断言块(语句位元组
> 解构+直值旁观式)。
>
> **CC1 判别宇宙×前奏双名册并集(2026-10-19,gui 阶梯三债清偿)。**
> `ct_sym_resolvable` 前奏面补并 `sem_type.ct prelude_ok`(权威册):pkg_is_prelude_sym
> (T17 use 判定)长期滞后(env_get/env_set/utf8_enc/str_byte/ctron_exe_path/dlopen 族/
> T41 分配器族…缺席),CC1 响亮失败门据此误伤闭包/spawn 内建调用——s26_reload_d/
> s32_theme(闭包值位 env_get)+s80_datepick(utf8_enc)三夹具 emit 误报「未解析标识符」。
> gui 阶梯 97/4→100/1。**s23_sk_native 精确定性(留专门批)**:lldb 单帧——
> `gui_list_ni`(ctron_gui.c:1567)读 `((CtronListC*)l)->n`,入参 0x10023cf10 =
> 二进制静态数据段地址(非堆)——骨架直通(gui_sk_load β3 静态构造)路径槽型错位,
> Str 常量被当 List 槽传入;复现=sh tests/gui/s23_sk_native/run.sh,-g 重编 + lldb bt 法在册。
>
> **w7 双修+门禁收编(2026-10-19,插值批随行核查产出;w7 此前不在 smoke/gate 任何门禁=静默回归通道)。**
> ①spawn 闭包 Match 模式绑定收集(CC1 批1 响亮失败门的判别宇宙缺口):解析器给枚举
> 变体元组载荷造 `PatAgg+SubTup`,而 `ct_pat_names` PatAgg 臂只认 SubSt/SubUnit/直连
> ——SubTup 无臂=载荷绑定名全漏,`match rx.recv(){Ok(v)=>…}` 形 spawn 闭包发射期
> 误报「spawn 闭包未解析标识符:v」(ws_steal 锚 10-10 后静默红)。修=trans_conc PatAgg
> 臂补 SubTup 走查。**裸元组模式 `(a,b)` match 语句发射缺口另立**(`ct_stmt:match pat`,
> 解释臂绿,trans_stmt.ct:1569,在册)。②morestack EST×帧池扩容联动:75cb7395 帧池
> 32→128 曾标注「纯 headroom 零语义」,实际每函数帧 848B→3248B(反汇编 sub sp
> 0x350→0xcb0 实证),序言 EST 底数 256 未计 ct_frn 固定开销→64KB 小栈检查通过后
> 本帧踏穿 guard(ms_grow ON 态触顶)。修=EST 底数 256→2300(与 ct_fr typedef
> pool/sv 尺寸联动注释)+RT_MS_MARGIN 2048→4096(第二层兜底罩一整帧帽;双层启发
> 两层同窗失守的教训在案)+RT_MS_SEG_MAX 1024→4096(64KB 段在新帧宽下每段仅容
> ~19 层,2 万层≈1053 段恰踏旧上限=失控保险误伤正当用例;4096=256MB VA/任务
> 惰性提交)+w7 同形对浅深度 1000→250(1000 层×3.3KB≈3.3MB 已超 1MB 缺省栈,
> OFF 臂必触顶破坏逐字节对;run.sh 随批注记帧宽数学)。默认发射(CTRON_MORESTACK
> 门关)零字节变化,黄金站岗不破。
> ③门禁:`sh tests/w7/run.sh` 收编 gate 步骤族(P9 栈面三锚自此有门)。
> A/B 定性法在册:compiler/bin.bak-fclk2(10-10 旧 bin)双锚全绿→回归落在 10-10..10-19
> 窗口;探针 sp1-sp8 最小复现族(/tmp/ct_probe)。
>
> **插值标量八码发射臂已落地(2026-10-19,§4.11「插值标量自动 Show」发射半边补齐)。**
> trans_expr Str 节点插值片段链补 `7`(U64)/`z`(USize)/`f`(F64)/`g`(F32)/`w8u w8s w16u w16s`
> (U8/I8/U16/I16 升 int32 复用 i32 面);driver_emit prelude 补 `ctron_u64_to_string`
> (%llu)+`ctron_f64_to_string`(整数形 %.1f/其余 %g——与 println 三元约定同形,界内
> long long 试探防 ≥2^63 UB,512B 容 %.1f 全量位)。此前八码插值发射 panic「ct_expr:interp
> 类型」(解释臂/种子臂两宿全绿,唯发射臂缺码)。夹具 `01d_strings.ct` 新块九标量值面+
> 文本面双断言;**宿主 W8030 不走查插值部件(纯文本,sem.c 注释在案)——插值独用绑定
> 宿主必报未使用,夹具纪律=每个标量须有插值外真使用**(assert_eq 同型断言兼值面钉)。
> 门:双臂探针逐字一致(U8..F32 八码+浮点边界 1.23457e+06/0.001/100.25)+suite 宿主列
> 120/120(分歧清零,cc 列 119/120=07a 在册)+meta_check 无新增(13 处在册债)。
> repeat 字面量复验:U8 定长 `[0] ** 4096` 发射本就绿(此前误判插值连坐),`[v] ** N`×
> 插值混合程序发射运行全通。
>
> **http U8 平面化批已落地(2026-10-09,批1 c02257f2/批2 3e8b5bf9+3b95b1d9/批3 c93ec0e4;aa6fad0d 点名遗留清账)。**
> parse/message/router/middleware/client/sse/ws/serve/connect/trace 字节道 &I64[]→
> &U8[](约 60 处形参)+corpus 16/frm_route/fuzz/bench_parse/五 e2e 夹具同批;
> enc/binddeflate 零耦合出局(自有 C 垫片不动);otlp/trace 值数组与自洽 pb 面不动。
> **发射/sem 新分歧四枚(本批钉边)**:①发射器 U8 操作数跑 [0,255] 域 checked 算术
> (ctron_w_add 系,seed=I64 域无检查)→ 纪律=U8 槽读进算术必 .as[I64]() 宽化
> (122+ 处;u8_sign 探针 200+256 崩/宽化 456);②sem 切片检查元素盲(&U8[] 喂
> &I64[] 形参 seed-check 过、emit 按元素型读垃圾——a_mw_chain 静默错读实证);
> ③三套 sem 三种宽严(bootstrap 放行/C 宿主 ctron-chk 拦 mut/发射不检)→ 跨外签
> 字节道形参必须 var U8[](可变 view),&U8[] 只读 ref:slice 不过 chk;④值数组
> 豁免铁律三犯实证(fuzz meta 偏移/ehs·ehc·ehw 直方图计数器——盲转 U8 即
> 50000 迭代 w_add 溢出崩;todo_api cnt 指标保 I64)。附带:run.sh client/serve
> inline emit 段补 net 垫片链接(keep_drain 使 close 外签入可达集暴露缺链)。
> 门:http 伞门 116/0 全绿(迁移前 106/10)+connect 3/0 首绿+corpus/frm_route/
> fuzz 120/0 rc=0(差分与 HEAD 逐字)+todo_api e2e 22/0(hashmap W2 运行时验收
> 兑现)+smoke 236/0。分支事故记一笔:批1 曾落对端 w25b-wip(共享树被切),
> cherry-pick 归位 main(c02257f2)。
>
> **W2.5b StepA 已落地(2026-10-09,ev 声明 + 实例 on: 槽全链容忍;规格 §5.2 地基)。**
> 签名行 `view N (props) ev 名(载荷), 名2() opt`——编译 gui_block 捕获 ev 子句入 d[5] 尾随
> \x02EV\x02 分节(props 三解析器经 gui_props_pc 剥除;gui_props_evseg 提取备 StepB/C 合成),
> 运行时 gt_parse 子句跳读;实例 `on:名={处理器}` 接线槽四解析面容忍消费(编译折叠/检查/骨架
> +运行时)。**接线判别 = 冒号后 词+=**(`{`/`"` 起头 = 名为 on 的 prop 冒号形——s53_nested
> `on: Bool` 实证误吞)。内嵌重建面(src_raw+rejoin)同步剥分节(签名裸形还原)。本态接线
> 惰性(名字合约行为不变,语义臂随 StepB/C);夹具 s96_evclause(声明+接线槽+点击断言);
> decls 锁 522→524(+2)。
>

> **std/hashmap W2 批已落地(2026-10-09,http keep 多槽 60ffdc6b + todo_api hmi 换装 a453f30a;spec §9 W2 行回写)。**
> 范围重估:web headers **不做**(HTTP 头有序且可重名,List[Pair] 语义正确;路由=
> 模式匹配非精确键)。A 面:http client keep-alive 单槽四标量胞退役 → HttpKeep
> 多槽(cache=Box[KeepMap],键 "host:port",容量 8 FIFO 逐出;bru 复用计数语义
> 不变;跨 host 不再关旧连;101 直通改经 HttpResp.fd 交付);bru 递进断言
> 0→2→4→6→7→7 多槽下逐字保持;x_client_e2e 为活验收;c0_pure 双臂绿。
> **发射面形状债四枚(W2 探针钉边)**:①结构体 Box 字段【跨模块】类型闭包缺失
> (t_HMapS/t_Box64 typedef 不随 t_HttpKeep 发出;同模块免疫探针钉边)→ 绕行
> =引擎同模块自持(KeepMap/KeepCell,零 std.hashmap use,E5020 毒图连根规避);
> 退役条件=trans 线补跨模块收集,届时换 Box[HMapS[I64]]+hmsi_*(hashmap.ct
> 已按 YAGNI 不预置该面);②发射 typedef 按声明序非依赖序(非 pub 结构体不入
> 消费单元闭包;序错悬空,c0_pure 两犯)→ pub + 声明序前于使用者;③裸 V 返回
> 于非泛型宿主值位(var-init/赋值 RHS/return)E2010 + V 不参与实参位推断
> (E2060「未出现于实参位」;发射臂同码绿,seed/emit 宽严反向第三例)→ 泛型
> 容器值面给非泛型宿主须配具体型面或出参盒,消费形状=条件位比较/实参位/成员
> 链(hm_2m 探针双臂逐字=条件位 has/get+实参位 get 全绿);④V=struct 经 Box
> push 强转指针((const char*)(long)(t_v),vs 探针)→ Todo 值 Map 不可行,双 Map
> 形(hmi_new[Str]+hmi_new[I64])。B 面:todo_api 平行三 List[Str](注释自认绕
> 发射债)→ hmi 双 Map(id 升 I64 键,DELETE 全量重建块销,O(1) 删);run.sh
> 存量红=aa6fad0d U8 平面化迁移尾巴(respond→net_write view_6 vs w8u,HEAD
> worktree 同死实证;hmi 段发射零错误),e2e 运行时验收候 http 泳道 U8 迁移批
> 补跑。smoke 235/1(单红=decls 锁 521 vs 实测 524,对端 GUI W2.5 在飞未提交
> 态,与本批无关;本批零 compiler/src)。
>
> **W2.5a 批已落地(2026-10-09,prop 默认值;规格 §5.4 前半)。**
> 签名 `名: 型 = 常量`(常量限编译期字面/可折叠;型文法无等号故型首 `=` 即默认分隔符):
> 编译侧 gui_props_typed 剥默认段(合成签名防污染)+ gui_props_dflts 平行提取(vr_pd 穿线
> gui_ck_elem,缺 prop 门按段号豁免——段号=cos2 前 0x01 计数,走查仅末段到位的既有形态
> 已实证);运行侧 gt_props_names 平行注册 props_dflt + 展开垫点(gui_render「垫未提供」)
> 垫常量(引号串剥引号)。opt 事件随 W2.5b ev 表同批。夹具 s95_defaults(test_view+d_render
> 双省略/覆盖断言);decls 锁 521→522。
>

> **std/hashmap W1 批已落地(2026-10-09,spec dd65d9b3/plan 84a79ce9;HMapS[V]/HMapI[V] 可变哈希映射入册)。**
> 开放寻址+线性探查+Box 载体写穿别名共享(宪章 3a 附则随批入 README:可变容器
> 与函数式族并存,tier:alloc 申报)。put 插入均摊 O(1)(负载 3/4 倍增)/覆盖写 O(n)
> 重建/del O(n) 活词表重建(v0.9 无墓碑)/get O(1)/插入序迭代。设计 R3 二次修订=
> **注入形(hash/eq fn 形参)裁撤改具体键双件**(Str/I64 键,哈希相等内联)——发射
> 探针 B1–B4 实证:具体型 fn 值传入泛型 fn 型形参的发射 trampoline 按 ct_i 裸传
> (heap Darwin 4c known-red「比较器形参」同款在册债);泛型键 K:Hash+Eq bound 面
> 维持 W3 触发件。门:模块 seed+发射双臂逐字一致(11 tests)/stdpkg 消费面两臂
> 逐字/smoke 236/0(基线 224→236;3j3 登记 ctronr Box 载体欠账共 9 模块;4c emit
> 23 模块绿)/suite 112×112/std_doc --check 27 页绿/decls 锁 521 不变。
> **发射面形状债四条(B1–B4 探针钉边)**:①经 Box 泛型元素值位读/写无装载语义
> (items[t_i] 裸传:读返回 char*、写不装箱)——绕行=形参 List 助手(_at/_copy_skip,
> sort/fmap 同形)+重建+字段重赋值;②Box 字段→局部 var 泛型 List 丢 ty;③泛型
> 宿主 fn 内对助手传具体型显式 TypeArgs:seed 面结果型不绑定 E2010、发射臂同形
> 绿(seed/emit 宽严反向分歧;B4 初版病态 dft 型 seed 放行附证);④无中缀 as——
> 收窄用 as[T]() 方法形(pb.ct:280 先例)。顺手销账:vendored examples 四对同步
> (187ea6aa 文档批尾巴)+3j3 known= 登记 hashmap。
>
> `test_view(ViewCall, w, h, body)`:desugar 合成形(marker/ordn/rew 三分支 + __gui_test_V
> 恒随合成[src=ctron_embedded,bind/act 闭包与 run 同构,key 空体]);改写面**多站点化**
> (rew_block/rew_file 首中即返改累积——main 的 run + tests 的 test_view 共存实证 s94;
> 实参形态二态=命名 NParg 取 [2] 槽/位置取节点本体,segfault 实证修正);premerge 注入
> 扩 test/Driver 两符。`d_render(t)`:Driver 持 bind(test/test_sk 构造时存 bnd 槽)零闭包
> 重绘——设计文书的 `d_frame(t)` 零参形因语言无重载改名为 **d_render**,d_frame 两参形
> 保留兼容(README 驱动面不动)。夹具 s94 迁移为 test_view+d_render 验收载体;decls 锁
> 521 不变(编译面零新 fn,域包 +1=d_render 不入锁)。
>

> **W1 批已落地(2026-10-09,GUI 无胶水装配 W1a+W1b;设计 docs/superpowers/specs/2026-10-09-gui-glueless-assembly-design.md)。
> W1a:组件实例属性分隔符 `:`/`=` 双收(运行时 gt_node 显式双收;编译骨架/检查/折叠三面位置性
> 放行经 s93_attreq 双口径实证,显式校验留 W4 冒号退役翻转)。W1b:合成事件表改**实例化根可达
> 闭包**(gui_ds_reach_filter,IR 行级过滤——骨架平面数组无视图名表绕行),未实例化组件事件头
> 零需求,gui_snippets 32 空壳桩清实证;**E8121**(实例化组件调用形事件头缺宿主处理器,编译期
> panic 门——名字形缺失为 legacy 既有绿行为,收紧随 W2.5 ev 合约;零参 fn 与缺失经 gui_fn_exists
> 分辨,s50 零参 dialog_close 实证歧义)。夹具 s93_attreq/s94_reach;decls 锁 519→521(+2:
> gui_ds_reach_filter/gui_fn_exists)。E8121 负例 = s94 注释口径(cc/desugar 面码,check 语料
> 不适用);E8122(签名不符)随 W2.5 ev 契约另件。冒号形退役+e8 负例随 W4。
>

# 测试覆盖审计(v0.5 规范 ↔ 测试集)

> **E2080 批已落地(2026-10-08,let 不可变门;规范 §4.1/§10 已修订)。**
> 钉子 `tests/04g_let_assign.neg.ct`(= 重赋值/+= 族/var 遮蔽后重赋值三面)三线全强制:
> 自举线(sem_type.ct:immut_gate_hit,标记成对推入保持 loc/envT 平行)、C 宿主(sem.c:bind.mut 位)、
> R 线(check.rs:Local.mutable + check_suite 矩阵登记)。**豁免面登记债(v0.9 有意收窄)**:
> 形参/闭包形参/for·match 模式绑定的重赋值、struct 字段经 let 值的写、切片元素经 let 根的写
> 三面仍放行,三线同判据;收窄留后续修订(判据=赋值左目最内层绑定的声明可变性)。
>

> **第六批补测已落地(2026-09-12,v0.7 三项松绑;规范已修订至 v0.7)。**
> R 线 compiler-rust 新增三 suite:oror(4,`||` 真值表/优先级/位置消歧/续行)、
> breakc(8,break/continue 绑最近循环/E2070/E2071/E2072/fmt/C 发射)、
> infer(5,调用点推断/E2060/E2061/显式并存);夹具 `compiler-rust/tests/fixtures/`
> (04d/04e/04f 已提升共享 tests/,自举+宿主双过;04f 结构化形态、04d/04f 负例、04e 闭包/Drop 负例留 R 线 fixtures)。spec 修订:§1.3/§1.5/§1.6/§3.7/§3.9.1/
> §4.0(新)/§4.2/§4.3/§4.4/§4.7 + §10 五个新码。
> 设计:`docs/superpowers/specs/2026-09-12-v07-operator-constitution.md`。
>

> **第五批补测已落地(2026-09-07):路线图锚点语料 `tests/roadmap/`。** R 线(R-P2…R-P7)测试先行,
> 22 个单文件 + 2 个多文件用例钉死容器/std 能力/捕获闭包/迭代器/模式守卫/fmt 夹具/panic 源位置/
> arena Send/comptime 常量尺寸/path 依赖的语法与语义裁决;由 `roadmap_suite.rs` 表驱动锚定状态
> (首跑实测:绿 5 / 锚 17),主流套件零回归。设计:`docs/superpowers/plans/2026-09-07-r-tests-design.md`。
> 附带修复:解释器 `Index` 第二求值路径越界为 Rust panic(击穿测试进程)→ 改 `Flow::Panic`(与主路径一致)。
> 新码注册:**E3070**(闭包可变捕获未显式 Mutex 包装,R-P3a)。
>
> **第四批补测已落地(2026-09-04):语言符合性覆盖 100%。** 覆盖按三层口径核算:
>
> | 口径 | 范围 | 状态 |
> |---|---|---|
> | **A 语言符合性(.ct 测试)** | 规范语义/语法/诊断码,116 项 | **116/116 = 100%**(第四批补齐:Simd、trace 位置链、stdweb 锚、FFI(c_src)、`} else {` 排版、显式 Void;AnyError 设计随 v0.5 钉死) |
> | **B 工具链行为(compiler 集成测试)** | JSON 诊断 schema、`--deterministic`、`lint --trusted`、bare 体积检查,4 项 | 归属 P1-D 实现计划 |
> | **C 构建与性能门禁(CI 基准)** | own ±5% / GC ≤15% / bare <100KB | 归属 P2/P3 阶段出口 |
>
> 测试集现状:**61 个冻结语料文件 + 24 个 roadmap 锚点文件**(86 个 .ct,meta_check 全过)。
> 注:锚定文件在实现就绪前保持"规范锚"状态(红),实现跟上即翻转——这是测试先行的设计本意。
> **行项 100% ≠ 用例空间 100%**:输入空间的深度覆盖(边界值/组合/并发交错)由 P1-D 的属性测试与模糊测试承接。

## 第五批补测清单(2026-09-07,roadmap 锚点)

`r2a_list/r2a_map/r2a_set/r2a_sb`(容器 API 面:`Type[T]()` 构造、get→Option 无 null、
keys() 迭代序不承诺)、`r2a_list_oob.panic`(越界 "index out of bounds")、
`r2a_container_send`(元素 Send 则容器 Send)、`r2a_container_alloc.neg`(E3040 覆盖容器,已生效)、
`r2b_fs_fake/r2b_time/r2b_env`(std.fs/time/process 能力形状 + Fake 注入)、
`modules/caps_fs`(E4010 键集扩展,已生效)、`r3a_capture`(拷贝捕获/闭包逃逸/Mutex 可变捕获,解释器已绿)、
`r3a_capture_var.neg`(E3070 新码)、`r3b_adapters/r3b_iter_trait`(惰性适配器链、Iterator trait + for)、
`r3c_guards/r3c_guard_exhaustive.neg`(守卫/或模式;守卫臂不参与穷尽)、
`r2d_fmt_fixture/r2d_fmt_chain`(fmt 幂等夹具,行为测试身份)、
`r4b_panic_location.panic`(panic 消息携带 `.ct:行号`,R-P4b 翻转判据)、
`r4c_arena_send.neg`(arena 非 Send,已生效)、`r5a_const_size`(const 作数组尺寸,已实现)、
`r5a_semantics`(comptime 语义回归锚)、`modules/path_dep`(`[deps] path=` 依赖格式,R-P7)。

## 第四批补测清单(2026-09-04)

`09_simd.ct`(splat/lane/to_array/元素级白名单)、`10_trace.ct`(`?` 记录 + `context` 物化 AnyError.trace)、`10_web_dom.ct`(`//@ target: web` + `stdweb.dom` 最小 API 锚)、`modules/ffi_math/`(`extern "c"` + `#[trusted]` + `c_src/` 构建规则);`01_basics.ct` 补多行 `} else {` 与显式 `Void`;`05i_deep_cause.ct` 的 `middle` 签名随 AnyError 修正。
规范 v0.5 配套:`extern` 关键字与 EBNF、`AnyError`/`Error.trace`/两段式位置链、`Simd`/`Str.contains` 前奏行、§9.2 stdweb 最小 API、§9.6 extern 声明示例、README §6 `c_src/` 规则。

## 第三批补测清单(2026-09-04)

`00_doctest.ct`(doc-test 格式锚)、`01d_strings.ct`(转义/插值链含索引/字节与字符)、`01e_multiline_chain.ct`(§1.6 首点式换行)、`02e_match_patterns.ct`(字面量/struct/变体模式)、`03c_str_string.ct`(Str/String/隐式降格)、`03d_props_traits.ct`(prop/默认方法/超 trait 组合/空 impl)、`03e_generics_types.ct`(泛型 struct+bound+class+`@derive(Show,Eq)`)、`03h_utf8_boundary.panic.ct`、`05f_must_use.lint.ct`(W8020)、`05g_into_gc_isolation.ct`(深拷贝隔离)、`05i_deep_cause.ct`(两层 cause 链)、`06g/06h_noalloc_trait`(`#[no_alloc]` 函数与 trait 契约 E3040)、`08b_nospawn.neg.ct`(E4030)、`08d_comptime_effect.neg.ct`(E6020)、`modules/comptime_budget/`(E6010,首例按 `[comptime] budget_ms` 配置预算)。
配套:注册 E4030/E6010/E6020;§1.4 插值扩展到索引;前奏 Str 补 `slice(range)`;EBNF `ClassItem` 补 `PropImpl`、`Method` 补 `{ DeclAttr }`(trait 方法上的 `#[no_alloc]` 由此合法)。

## 第二批补测清单(2026-09-04)

**P0 —— v0.4 新特性:** `03f_slices.ct`(切片二分/T[N] 退化/隐式只读化)、`03g_fn_types.ct`(函数类型/多参闭包)、`06b_slice_nonsend.neg.ct`、`06c_static_nonsend.neg.ct`(E3031)
**P1 —— 核心语义:** `05d_drop.ct`(RAII 逆序)、`02b_option_propagation.ct`(Option `?`/expect/`T?`/元组变体/块臂)、`04b_logic.ct`(`&&`/`!`/德摩根/`%`符号/复合赋值/遮蔽/range 值/else-if)、`03b_numeric_widths.ct`(宽度全集/进制/分隔/自适应/`as` 截断)、`02d_divzero.panic.ct`、`05b_panic_join.ct`(`panic()`/Never/`join_or`)、`05e_own_gc_mut.neg.ct`(E3060)、`06f_parallel.ct`(数据并行)、`06d_globals.ct`(static let/Global/Atomic)、`06e_cancel.ct`(取消传播)
**基建:** `01c_parse.neg.ct`(E1001 注册 + 比较不可链)、`tests/modules/` 五例(use_ok 组导入+pub/pub(pkg)、orphan E5010、circular E5020、visibility E2020、caps E4010);README §6/§7 多文件格式与 std 隐式链接规则;前奏表补 Atomic/Global/Drop 行;数组字面量归属(`T[N]`)写入 §3.6。

---

## 首批审计基线(v0.4 规范 ↔ 18 个测试文件,历史)

方法:逐条对照 `docs/spec/` v0.4 的规范性特性与 `tests/*.ct`,标注 ✅ 已覆盖 / 🟡 部分 / ❌ 未覆盖 / ⛧ 需基建(多文件或后端)。
统计:**121 项规范性特性中,✅ 48(40%)、🟡 9、❌ 60、⛭ 4**。结论:核心展示路径(Send/own/错误模型/值引用二分)覆盖扎实,**模块系统整章为零、v0.4 新特性大半未测**。

## §1 词法与语法(15 项:✅5 🟡3 ❌7)

| 特性 | 状态 | 锚点/缺口 |
|---|---|---|
| 整数默认 I32 / 浮点默认 F64 / 布尔 / 简单插值 / 行注释 | ✅ | `01_basics.ct` |
| 字面量后缀 | 🟡 | 仅 `u8`;`f32/i64/usize` 等未测 |
| 块值/`else` 同行排版 | 🟡 | 隐含于各测试,无专门用例 |
| EBNF 产生式整体 | 🟡 | 主干覆盖,角落未测 |
| 进制 `0x/0o/0b` + `_` 分隔 | ❌ | |
| 期望类型自适应(`let x: U64 = 5`) | ❌ | |
| 字符串转义(`\n \{ \u{}`) | 🟡 | 词法校验面已闭环(`fx_str_esc_neg`/`fx_interp_neg`:非法转义/插值未终止/跨行);解码仍在求值/发射层,与 C 宿主词法解码属架构分歧(登记) |
| 插值字段/方法链(`{clock.now()}`) | ❌ | |
| 多行首点链式排版(§1.6 新规则) | ❌ | **v0.4 新规则无测试** |
| 比较不可链(parse 负例) | ❌ | 需先注册 E1xxx 码 |
| doc-test(`///` 代码块) | ✅ | 机制落库(driver_doctest.ct 提升制,1003):锚 `00_doctest.ct`(多块+非 c 标签跳过)+`doctest/corpus/` 六件(执行/编译负例、标签与丢弃纪律、行号零漂移 JSON span 锚、emit 面盲区)+`doctest/run.sh` 九断言;C 宿主对照不提升(登记) |

## §2 名字与模块(8 项:❌8 —— 整章未覆盖)

多文件 `use`/全限定路径、组导入 `{}`、`pub`、`pub(pkg)`、遮蔽、孤儿规则 E5010、循环依赖 E5020、caps 越权 E4010——**全部缺失**。根因:测试格式目前只支持单文件,多文件项目格式未定义(见"基建缺口")。

## §3 类型系统(26 项:✅11 🟡3 ❌11)

| 特性 | 状态 | 锚点/缺口 |
|---|---|---|
| 检查算术+回绕、Option、Result、enum 具名/单元变体、元组、定长数组、泛型 fn、Box、`&Trait` 隐式上行 | ✅ | `01/02/03/04/07` |
| Str/String 独立类型(§3.8.1 类型表成员):显式注解位端到端+to_string 升格(发射面真分配)+String→Str 隐式降格 | ✅ | `03c_str_string.ct`(注解段)+`03c_str2string.neg.ct`(E2010 单向门);独立码面登记 v2 |
| `@derive`(仅 Error)、数组字面量类型归属、切片可变写(仅 bare) | 🟡 | |
| 数值宽度全集(I8..I64/ISize…) | ❌ | 仅用 I32/U8/U32/U64/F64 |
| `as[T]()` 显式转换 | ❌ | |
| `T?` 语法糖 | ❌ | |
| `T[N] → T[]` 退化、隐式转换清单(T[]→&T[]、值→&T;String→Str 已随 2026-10-03 销账) | ❌ | |
| **`&T[]` 只读视图二分(v0.4)** | ❌ | **新特性无任何测试** |
| enum 元组变体(`Timeout(U64)`) | ❌ | |
| match 字面量模式、struct 模式 | ❌ | |
| `prop` 属性(定义+使用) | ❌ | 前奏 `.len` 隐含,用户定义无 |
| trait 默认方法体、超 trait 组合(`Env: Clock + Fs`) | ❌ | 仅 `: Cap` 标记 |
| 泛型 struct/class、bound(`T: Show`)、`@derive(Show, Eq)` | ❌ | |
| `Simd[E, N]` | 🟡 | 语义/标量模拟在库(09_simd.ct);T51 评估:循环级自动向量化实证(clang -O2 width4/gcc -O3),寄存器驻留=志向 |
| **函数类型 `fn(...) -> T`(v0.4)** | ❌ | **新特性无测试** |

## §4 表达式(16 项:✅9 🟡2 ❌5)

✅ 优先级、`+=`、if 表达式、match 穷尽+通配、for/range/`..=`、while、闭包(零参/单参/`|var|`)、UFCS、`or`。
🟡 `%`(仅正数场景)、`void`(隐含)。
❌ **`&&`/`!`/德摩根**(全测试集没有用过逻辑与!)、其余复合赋值(`-=` `*=` `/=` `%=`)、else-if 链与 match 块臂、range 作为值/多参类型闭包、`assert_ne`。

## §5 错误模型(13 项:✅4 ❌9)

✅ Result `?`、`or` 双形态、`context`/`cause`、溢出 panic。
❌ **Option `?` 传播**、`expect` 于 Option、显式 `panic()`、**除零 panic**、`join_or` 任务 panic 重抛、W8020 must-use、`ScopeCancelled`、深 cause 链(≥2 层)、`?` 位置链元数据。

## §6 内存模型(12 项:✅7 ❌5)

✅ 值/引用二分、W8010、own+`into_gc`、E3040(own)、E3050、bare 显式分配、bare E3040。
❌ **Drop/RAII(整个确定性析构零测试)**、E3060(own 内对 GC 值可变借用)、`#[no_alloc]` 注解与 trait 契约、`into_gc` 深拷贝隔离性(arena 改动不影响 GC 值)、ISR 约束。

## §7 并发(15 项:✅7 ❌8)

✅ scope/spawn/join、Send 正例、E3010、E3020、channel 往返、`with`/`with_mut`(v0.4)、E3030。
❌ **E3031(非 Send 静态)**、`static let` 行为、`Global[T]`、`Atomic`、**`parallel.map`(数据并行零测试)**、取消传播/`ScopeCancelled`、**`T[]` 经 channel 的非 Send 负例(v0.4)**、`join_or`。

## §8 效果与 comptime(7 项:✅3 ❌4)

✅ 能力注入+`Cap`、E4020、comptime fn/const。
❌ `#[no_spawn]` E4030、comptime 预算 E6010、E6020/E6030、单态化预算。

## §9 档位与互操作(6 项:✅1 ❌3 ⛧2)

✅ bare target 标记+arena。❌ `extern "c"`+`#[trusted]` FFI、产物体积口径。⛭ JS 桥、~~WasmGC~~(**PM-06-2 证伪登记 2026-10-19**:T38 裁决桥 ABI/WasmGC 不可行——寄存器版 WasmGC 缺宿主锚,JS 桥维持 wasm32 MVP 形;be5f7715 三档落库批;重评估触发=WasmGC 1.0 多宿主落地;互查=docs/spec/06-memory.md PM-06-2)。
> **T51(2026-10-03)**:§9.5 三残项收口——①异步 IO 统一层 io_uring 后端落库
> (自检双 NOP 门+响亮回退;reactor 冒烟双臂挂 ci.sh);②NUMA 选项位+拓扑探测
> (感知分配=志向,多节点靶机触发);③Simd 向量化评估报告落
> `docs/simd-vectorization-analysis.md`(结论 B=v0 语义模拟挂注+志向分级;clang -O2
> 循环级向量化实证,width 4)。§9 硬件利用行项从 ❌ 翻 🟡(语义模拟在库,寄存器级=志向)。

## §10 诊断与符合性(4 项:✅2 ⛧2)

✅ 错误码注册表(meta_check 同步)、四类测试标记格式。⛭ JSON 诊断 schema、确定性模式(实现期验证)。

## 基建缺口(阻塞 ~15% 的剩余覆盖)

1. **多文件测试格式未定义**——§2 整章(8 项)+ FFI 依赖它。需在 `tests/README.md` 定义:目录即项目(`tests/modules/xxx/{Ctron.toml,src/...}`)、`//@ fail` 归属文件行号、neg 判定范围。
2. **E1xxx 解析错误码零注册**——比较不可链等 parse 负例无法编写;需先扩注册表(如 E1001 通用语法错误)。
3. ~~doc-test 无格式样例~~ **已解决(2026-10-03)**——格式由 README §8 钉死(围栏标签须 `c`、丢弃纪律、行号零漂移),机制落库 `compiler/src/driver_doctest.ct`(check/test/run 驱动提升制);锚 `00_doctest.ct`+语料 `doctest/`。
4. (规范小缺口,审计中发现)数组字面量 `[1,2,3]` 的类型归属(`T[N]` 直标还是先 `T[N]` 再退化)未明确。

## 补测 backlog(26 个文件,按优先级)

**P0——v0.4 自身新特性(规范刚改,最该先钉):**
`03f_slices.ct`(T[]/`.len` 写读、`&T[]`、隐式、T[N] 退化)、`03g_fn_types.ct`(函数类型参数)、`06b_slice_nonsend.neg.ct`、`06c_static_nonsend.neg.ct`(E3031)

**P1——核心语义空洞:**
`05d_drop.ct`(RAII 逆序+panic 展开)、`02b_option_propagation.ct`(Option `?`/`expect`/`T?`)、`04b_logic.ct`(`&&`/`!`/德摩根/`%`符号/复合赋值)、`03b_numeric_widths.ct`(+`as`)、`02d_divzero.panic.ct`、`05b_panic_join.ct`(`panic()`/`join_or`)、`05e_own_gc_mut.neg.ct`(E3060)、`06f_parallel.ct`、`06d_globals.ct`(static let/Global/Atomic)、`06e_cancel.ct`

**P2——表达力补全:**
`01b_literals.ct`(进制/后缀/自适应/转义/插值链/多行链)、`02c_match_forms.ct`(字面量/struct 模式/块臂)、`03c_str_string.ct`、`03d_props_traits.ct`(prop/默认方法/超 trait)、`03e_generics_struct.ct`(泛型类型/bound/@derive(Show,Eq))、`03h_implicit_convs.ct`、`05c_must_use.lint.ct`、`06g_noalloc_trait.ct`+neg、`08b_isr.neg.ct`(E4030)、`08c_comptime_budget.neg.ct`(E6010)

**P3——基建先行:**
多文件格式定义 + `tests/modules/`(E5010/E5020/E4010/可见性/use)、E1xxx 注册 + `01c_parse.neg.ct`、doc-test 样例;FFI/P2 后端就绪后补 `09_*`。

---

## 2026-09-15 增补(P1-B/B2/A2/P2-A/GC spike 收口语料)

上文补测 backlog(P0-P3 各项)已全部落地;本次随定宽存储/panic 展开/comptime
语句循环/E4050 资源 lint 收口,新增:

- `03i_width_checked.ct` —— 8/16/64 位定宽检查算术 + as[U64]/as[USize] 全宽度转换
- `03j_width_overflow.panic.ct` —— U8 加法上溢 panic(integer overflow +)
- `04h_comptime_stmt.ct` —— comptime 体内 var/while/for 语句与赋值(§8.4)
- `roadmap/r4d_drop_panic_unwind.ct` —— panic 展开 Drop 逆序(红:宿主 rt panic 不跑 Drop)
- `roadmap/r6a_u64_overflow.panic.ct` —— U64 加法上溢(红:宿主 64 位无符号 Add 缺上界)
- `roadmap/r6b_res_class.neg.ct` —— 类持有资源字段 E4050(红:宿主 sem 无此检查)
- `roadmap/r6c_as_u64_negsrc.ct` —— 负值源 as[U64] 模 2^64(红:宿主 conv 误用 2^128 模)

计分:suite 66/66 双侧(66 文件;panic 桶 4);roadmap 锚 25(全部在自举线按设计
表现,红 = 宿主/R 线落后点,见 roadmap 文件头"锚点"行)。

## 2026-09-15 审计增补(二)——trust 边界/续行/预算负例

- `01k_op_continuation.ct` —— §1.6 二元运算符开头续行(01e 首点式的姊妹形态)
- `roadmap/r6d_trusted_unmarked.neg.ct` —— W8050 extern 未标记 #[trusted](红:宿主无此检查)
- `roadmap/r6e_trusted_nonextern.neg.ct` —— E4040 #[trusted] 仅限 extern(红:宿主无此检查)
- `roadmap/r6f_comptime_budget.neg.ct` —— E6010 comptime 预算超限不挂起(红:宿主 check 不评 comptime const)
- `roadmap/r6g_trusted_extern.ct` —— #[trusted] extern 检查面正例(红:单文件 extern 宿主 test 无法链接;FFI 运行面由 modules/ffi_math 覆盖)

计分:suite 67/67 双侧;roadmap 锚 29;smoke 109 ok(唯一红 = std 泳道 drift)。
审计结论:自举线已实现特性的语料覆盖到位;剩余"未绿"均对应宿主/R 线落后点
或远期项(r2a 容器/r2b 环境/r2d fmt/r3b 适配器/iter/r4b 位置/r5a const-size),
每个红锚文件头含语义承诺与翻转判据。

## 2026-09-15 审计增补(三)——语义级复核发现

- `03i` 扩展 —— u64/usize 后缀字面量(§3.7 字面量自适应)
- `roadmap/r1a_trailing_dot.neg.ct` —— 行尾 `.` 非法(§1.6;红:自举解析器吞点
  误放行,宿主正确拒绝——自举解析器守卫缺口,与新发现宿主缺陷同批在册)

## 2026-09-18 审计增补(四)——插值负向面三线审计

针对 §1.4/§4.11 插值的非法形态逐项三线探测(自举 `ctron-cc run` / C 宿主
`ctronc run` / R 线 `ctron run`),并落锚:

- `roadmap/r6h_interp_unclosed.neg.ct` —— 插值未闭合(`"v={n tail"`)。
  宿主/R 线均 E1001「未终止的插值」;自举线 ad-hoc "unbound:n" 拒绝但不产出
  E1001 → 红。已登记 roadmap_suite(NegGreen("E1001"))。
- `roadmap/r6i_interp_nested_str.neg.ct` —— 片段内嵌套字符串字面量
  (`"outer { "inner" } tail"`)。宿主/R 线均 E1001;自举线误报 E2020
  (unresolved: inner,拒绝但码不符)→ 红。已登记 roadmap_suite(NegGreen)。

**待规范所有者裁决(三线对照,先裁决再锚定/修实现):**

| 形态 | 自举线 | C 宿主 | R 线 | §1.4/§4.11 口径 |
|---|---|---|---|---|
| 嵌套块 `"v={ {x} }"` | 接受(v=1) | 接受(v=1) | 接受(v=1) | 白名单外(不支持嵌套 `{}`) |
| 空 `{}` | ad-hoc 拒绝(unbound:#EOF) | 接受(v=空) | 接受(v=void) | 白名单外 |
| 片段内语句 | ad-hoc 拒绝(unbound:let) | 接受(v=空) | 接受(v=void) | 明文禁止 |

三线一致的嵌套块可考虑升格进 spec(修订 §1.4 白名单),或三线统一按 E1001
收口;C/R 接受语句片段与空 `{}` 为待修项候选。裁决前不落行为锚(避免钉死
违规格面)。

**在册事实:** E6030 为 spec §10 预留码(parametricity,未到实现期),非漏测。
~~R 线 roadmap_suite 语料卫生自 09-15 r6 批次起滞后(7 件未登记,套件红)~~
→ **2026-09-19 已清**:r6b–r6g/r1a 七件按 R 线实测行为补登(实测口径:r1a
E1001 已拦 = NegGreen;r6b/r6d/r6e/r6f 检查未实现 = NegPending;r6c interp
as[U64] 负源得 -1、r6g 无 test 块空泛成立 = RunRed)。roadmap_suite 2/2 绿
(锚 31 = 绿 5 / 红 26);翻转时按文件头判据迁移主套件。

## 2026-09-19 审计增补(五)——r6c R 线修复翻转 + R 线全量基线在册

**r6c 修复与翻转(R 线):** 三线探针分离病灶——自举线一直正确(基准);R 线
唯一病位是 interp 字面量求值:无后缀整数字面量经 i64 解析,U64 上界
18446744073709551615 截为 -1(`let a: U64 = …` 靠后续强转侥幸正确,无期望
类型的实参位露馅;`convert_as` 本身正确)。修复:interp `Expr::Int` 无后缀且
超 i64 正程时保真为 `UInt`(parse_int 拆 parse_int_mag 复用)。r6c 翻转
RunRed → Green(roadmap_suite 绿 6/红 25)。R 线 trans 的 as 发射引用
`ct_as_ii` 助手但仓库内无定义,emit 面存疑在册(锚定面是 interp,未扩散)。

**C 宿主 r6c 病灶细化(仍在册红):** ① `wrap_int` 对 bits≥64 直接透传,
无 mod 2^64 掩码(as 侧 -1 透传,显示为 2^128-1);② as 调用实参位的
U64 上界字面量 decl 检查按有符号宽判,误报 "integer overflow (decl)";
③ 未标注 let 的自适应丢失 us 标志(实参位字面量得 -1)。

**R 线全量基线(本批首跑全量 cargo test,均在册、先于本批):**
check_suite 17 条 sem 落后(一元负 Bool/Str、E2060 推断、dist 域名称、
impl-for 解析);fmt_suite 1 条(01i 分号语料 fmt 失败);native_suite 6 条
(ffi extern cc 链接缺符号);run_suite 22 条(ffi 外部声明不可调用)。
**本批净变化:** lex_suite 补 `*.neg.ct` 过滤(neg 语料判定面是 fail: 码,
不入"全语料零词法诊断"断言)→ 绿;roadmap/lex/breakc/infer/oror/test
六套件全绿;check/fmt/native/run 四套件红为既有债务,待各自责任面切片。

## §网络与服务器(服务器泳道,2026-09-20 起)

| 波次 | 项 | 锚定 |
|---|---|---|
| S0 | caps 键集 net/db(E4010) | tests/modules/caps_net(绿:自举 + 宿主pkg 双线,宿主键集 M-T3-1 收账) |
| S0 | 规范 v0.8(11-net 定稿/§7.10/键表) | docs/spec v0.8 |
| P1 | std/net 阻塞基线(时钟/TCP/UDP/resolve/默认值) | tests/net/(probe_boundary/clock_sanity/tcp_echo/tcp_defaults/udp_roundtrip/fd_churn,6/6 绿) |
| P1 | r7b pure 触网 E4020 | tests/roadmap/r7b_pure_net.neg.ct(RunRed;翻转待 R 线 std/net 解析/跨函数纯度传播,按翻转协议迁 NegGreen) |
| P1 | 1k 回环冒烟无 fd 泄漏 | examples/ctecho + tests/net/fd_churn(500 轮双端);1k 满额归 nightly |
| P1 | 吞吐 vs C ≤1.05× | tests/net/bench(ratio 1.114 已登记,落 1.05–1.15 归因档:per-read poll 门 + 4KB 暂存 + lane 逐字节加宽;P2 处置,不堵出口) |
| P2 | rt 协程核心(自绘切换 arm64/x86_64,弃 ucontext;定时器堆+池化栈) | tests/net/rt_core_smoke(纯 C 冒烟,主环守卫跳过;yield_bench 87–102ns) |
| P2 | reactor(kqueue/epoll/poll 回退)+ wait_fd 真实现 | tests/net/rt_reactor_smoke(8 协程×16 轮 socketpair 压力;超时/对端关闭/自关路径全绿) |
| P2 | net 垫片混合化(五停车点协程无色挂起,裸线程 P1 回退逐字节不变) | tests/net/coro_hybrid(主环 main.ct + c_smoke 专属块 workers={1,4} 停车严格证) |
| P2 | 发射模板 coro 模式分支(弱定义哑元机制,CTRON_RT=coro 改道) | tests/net/run.sh 环境矩阵(默认 12/12 + coro 矩阵 12/12);examples/ctecho 源码零改动双模(§7.10 同形) |
| P2 | 确定性调度 CTRON_RT_SEED(单 worker + LCG 抽取 + spawn→join 单向闸) | tests/net/coro_det(主环 2 模 + 种子重放专属块:SEED=42 + 0..99 各双跑 cmp 101/101;nightly 1000 尾注) |
| P2 | 门禁 C10K(nightly/本地) | tests/net/c10k(纯 C 目录主环跳过;实测 10000/10000 回显全绿 + 探活绿,connect 0.4s/total 1.1s;CI 冒烟档 C10K_N=100 绿) |
| P2 | 门禁 切换微基准 ≤200ns | tests/net/bench/bench.sh rt 段(CTRON_NET_BENCH=1;实测 89–102ns 四跑全绿;Rosetta 翻译态豁免在册) |
| P2 | 门禁 echo coro-vs-P1 ≤1.15× | bench.sh coro 维度(实测 2.073–2.203 三跑,**红,登记归因**见计划执行记录:每阻塞读 reactor 登记/摘除 + park/wake + 空闲退避唤醒 ~15µs/往返;coro-vs-C 2.33–2.47 归档;**P3-A 转绿 1.019–1.040,≤1.5 检查点与 ≤1.15 原门双过,见 P3 行**) |

P2 波提交域 48ed59c..ce43e83(任务台账与门禁数字:计划执行记录);net 主环计
例口径 = 行为夹具 9 + c_smoke 2 + coro_det_replay 1 = 12。

### P3 行(TLS + 传输补全 + 时延首件,2026-09-21)

| 波次 | 项 | 锚定 |
|---|---|---|
| P3 | 时延首件(事件量交付 + 兴趣驻留 + 探针消除) | tests/net/bench/bench.sh 三门禁:coro-vs-P1 **1.019–1.040**(P2 红门转绿,≤1.5 检查点与 ≤1.15 原门双过;kick 主导 ≈24.5µs/往返分量实证)、yield 74.8–87.7ns(P2 在册 89–102ns 不退化)、门禁一 1.105–1.122;c10k 驻留 fd 生命周期复验 N=10000 全绿 delta=0(fd 泄漏门 LEAK_INJECT 证伪口径保持) |
| P3 | vendored mbedTLS 3.6.7(子集构建,离线可重建) | vendor/tls/build.sh + smoke.sh(`TLS-OK 3.6.7`;.a 合计 ~1.29M;全量重建 13.1s,零网络,依赖仅 cc/ar/awk/POSIX sh) |
| P3 | std/tls 门面(BIO-over-hybrid,零新停车点;client hostname opt-out 保留链验证) | tests/net/tls_smoke(入主环双矩阵自动双跑;CTRON_RT_WORKERS=1 停车严格证 2/2;ALPN 偏好序断言;eof/close_notify 钉住) |
| P3 | 互操作矩阵(vs openssl s_server/s_client 双向,2×2×2) | tests/net/tls_interop/run.sh **8/8**(方向 D1/D2 × TLS1.2/1.3 × 双 RT 面;LibreSSL 3.3.6 零降格;ALPN 权威断言在我方;链验证双侧 REQUIRED 对称口径;HTTPS -www 回显 = D1 cell 内绿;不入主环,主环 13→14 口径不受扰) |
| P3 | 握手吞吐门 ≤1.5× | tests/net/bench/bench_tls.sh(CTRON_NET_BENCH=1;ratio **0.503** 宽口径 / **0.750** 公平 ours_spawn 对照,均绿;基线 exec 支配已登记) |
| P3 | Unix domain socket 四件(listen/accept/connect/unlink) | tests/net/unix_sock(回环/half-close 双向/陈旧重绑自愈/ENOENT/超限 EINVAL/协程面 resolve;路径上限 104;Drop 只关 fd 不摘文件;主环 13→14 双矩阵) |
| P3 | DNS 异步化(2 线程 helper 池 + done 槽) | c_smoke T6 差分证(workers=1 最严:787 resolves/60ms 窗口、进度协程 ticks+12;红路径演练有牙)+ 裸线程面 P1 逐字节不变 |

P3 波提交域 ce43e83..29dcdb1(1d7d90e / 687653d / 8339569 / 7a1465a / db2b4dd /
8a16854 + 收口两笔;任务台账与门禁数字:计划执行记录);net 主环计例口径
12→14 = 行为夹具 9 + tls_smoke 1 + unix_sock 1 + c_smoke 2 + coro_det_replay 1。
P3 在册登记项:resolve 不可取消(取消广播不中断在途);AF_UNIX accept/connect
无停车点(协程滞留 worker,同 TCP 口径,P2-C 扩面候选);_WIN32 分支推演未实证
(AF_UNIX 哑元 + DNS 池全裁);Windows 证书库 P8(CA 走系统 bundle 文件路径);
Drop 顺序 fd 复用 ABA caveat(超时兜底,文件头注);tls_read cap<=0 返 0;
每块超时语义(ctron_tls_read 每调用重置 conf.read_timeout,握手钉 0)。

### P4 行(HTTP/1.1 协议半层,2026-09-21)

| 波次 | 项 | 锚定 |
|---|---|---|
| P4-A | std/http 协议半层首件(请求行/状态行/头部解析 + 报文构造/chunked 编解码/100-continue 钩子;零 use 纯 Ctron,同 tls.ct ⑥ 口径) | tests/http/run.sh **30/30**(std inline 2 + corpus 14 × 解释器/emit 双臂;走私面:obs-fold/TE+CL/重复 CL/裸 LF·裸 CR/值内 CTL 全拒;上限四类独立 err 码;分片到达可重入含 dst 满排空) |
| P4-A | forget_fd 重排(摘 rt 驻留登记先于 close —— P3 在册 ABA caveat 结构性收口) | bench 三门禁复验(改前 1.055/1.128 → 改后 1.075–1.129/1.025–1.111,首跑单点红系并行泳道负载噪声,×3 复跑全绿;ns/yield 86 → 47–58);c10k N=100 fd 泄漏门 delta=0;双矩阵 14/14 × 2 不变;divergences 服务器面 (h) 登记(跨模块 struct 形发射缺口) |
| P4-B | 压缩面(miniz 3.1.2 vendored 单文件惯例,SHA256 zip f0446d86…)+ gzip 纯 Ctron 组框(CRC32 恒校验,python 互证)+ Accept-Encoding 协商(`*;q=0` 排除 identity 三钉,RFC 9110 §12.5.3)+ RFC 1123 黄金向量互逆 + query/form(C8 域) | tests/http **39/39 双臂**(压缩 x_ 夹具 emit 专臂链 libminiz.a;炸弹 cap 强制、tinfl 堆暂存;deflate=raw 口径/zlib 容器与 FHCRC 未实现登记);移交编译泳道四件(c6sub 双负/use 路径 `-` segv/Option·List[struct] 错型/值域定宽乘法)→ divergences (i) |
| P4-C | 客户端(client_request:重定向 301/302/303→GET 弃体、307/308 保方法保体、5 跳上限 err7、keep-alive 单槽 bru 复用计数、陈旧连接重试一次、101 直通)+ SSE 写面(id-NUL 整字段忽略)+ WebSocket(RFC 6455 三 MUST:分片序列态 err9/10、UTF-8 良构校验 1007 惰性档、version 严格 13;SHA-1 std/crypto RFC 3174 向量;lane-b64 因 std/enc C8 域约束自出) | tests/http **57/57 双臂**(x_ e2e 夹具 emit × {默认, CTRON_RT=coro} 双矩阵;§5.7 掩码/分片语料逐字节;粘包余量压实;修复波断言逮住零长帧悬挂/len7 标记误算两真 bug);len64 证据转双臂在库断言(ws_frame_head 单点) |
| P4-D | 解析基准(vs picohttpparser)+ 结构化 fuzz 长跑 + 登记收口 | 基准:N=1e6 ×3 取最小,ctron **209 ns/req** vs pico **46 ns/req** = **4.54×**(复跑 4.44×)→ 门 ≤2× **RED 登记档**:实测 < 8× 悲观界但仍在 I64-lane 宽度税量级(~4.8M parse/s ≈ 754 MB/s),处置 P9 I8-typedef,数字照录不粉饰;fuzz:结构化生成十类(合法/坏版本/走私/越限/chunked 异常/WS 帧边/垃圾/gzip)× interp+emit 双臂 × 24 seeds(120 段),本地 ≥10min(nightly ≥30min 惯例入 runner 头注),**零崩溃零挂死**(watchdog 段超时);差分对拍 6144 样本:pico_accepts_we_reject=1265(严格子集预期差)、**we_accept_pico_rejects=0**;fuzz 实证解释器值域定宽乘法 std/http 四处(值跨 2^28–2^31 带即炸,emit 正确)→ C10 宽域惯用法四处结构性修复 + divergences (i) 收口 |

### P6 行(ctron-http 应用框架,2026-09-22→23)

| 波次 | 项 | 锚定 |
|---|---|---|
| P6-A | comptime 路由核(平面 Str const 表+运行时 first-match+类型化参数)+ 中间件链骨架 | tests/http/frm_route 双臂 67 绿;x_bench hit 52ns ≤100 门;bench 家族门禁不入 CI 主环 |
| P6-B | 公网中间件五件(cors/csrf/sechdr/limit/timeout;时钟恒注入) | tests/http 82/82 + net 14/14;注入防护语料 |
| P6-C | 认证三件套(cookie 会话/JWT HS256/PBKDF2)+ multipart + JSON body 绑定 | frm_auth 语料双臂(JWT 黄金向量/alg 混淆拒/时钟注入);登记: utf8_enc 内建解释臂劈叉→au_ch 表格面 |
| P6-D | 静态文件(强 ETag 内容寻址/304/Range 206·416/穿越拒 fail-closed;Last-Modified 与 sendfile 登记)+ OpenAPI 3 同源导出(快照=路由表改即红)+ CTML 转义窄面(五实体安全默认+显式豁免) | tests/http frm_static×6+frm_openapi+frm_html 双臂 **16/16**(da08562);st_serve IO 薄胶待 P6-E e2e;大串断言按 corpus 惯例拆小文件(解释器堆不回收在册债的单文件腐坏规避) |

| P6-E | examples/todo_api 端到端(REST CRUD+JWT 写保护 login 签发/CORS 预检/限流 503/OpenAPI 同源/静态页 static 响应面/健康端点/优雅停机排空退出) | run.sh 自客户端 nc 打自服务端 **20/20 双 RT 矩阵**(默认+CTRON_RT=coro;da08562 static 面首次 IO 接线于同构应用);存储=内存平行 List(Str 形 id/done——List[I64] 元素读发射债绕行),业务 ≤300 行 |
| P6-F | 门禁收口:全请求周期回环基准(vs 手写 C 双基线:阻塞串行+kqueue/epoll 事件环,同源 C 客户端,digest pin,×3 取最小)+ 热路径 no_alloc 断言(ctron_amalloc 锚计数注入,A/B 双跑差分门=0)+ 路由 ≤100ns 复跑 + parse 基准复核 | **周期四跑 0.563×/1.060×/1.049×/1.106×(quiet 窗复测义务已销,2026-09-25 00:03 load≈9–16),判定 = 登记档 1.05–1.15(net bench 家族语义,rc=0;P1 echo 1.114 同待遇)**——C 基线稳(44.7µs)而 ctron 默认臂 43–50µs 随调度抖,差值归因 = 垫片 per-read poll 门 + parse lane 拷贝(P1 echo 同族,消除项随 P9 I8/垫片演进);**coro 臂 0.998–1.139×(归档)= 协程 RT 与手写 C 事件环同量级(P2 同形承诺兑现)**;**no_alloc PASS**(四轮全绿,10 万请求稳态 bump 差=0;热路径零分配构造+堵漏证明);路由 hit 66ns ≤100(负载窗);parse 复核 229 vs 48 ns = 4.77× RED(P9 I8-typedef 在册处置,4.54→4.77 负载窗漂移照录);net 14/14、todo_api 20/20、http 98/(11 在册红);编译器 = HEAD(a514f07)worktree 隔离构建(CTRON_EMIT 覆盖口,机刷共享树清场事故后的可复现口径) |

P6-D/E 时点套件环境红(2026-09-24 收口复核,11 红全谱):frm_auth_a_guard/jwt/sess +
frm_mw_a_csrf(interp)+ http/frm/auth.ct(inline)= 5 件 = 解释器 137 击杀在册债;
client_fixtures_x_client_e2e + sse_ws_x_sse/ws_e2e ×双 RT = 6 件 —— 旧档「负载超时」
在 HEAD 复核为**构建红**(t_client_write_str/t_http_method_start 发射缺声明,饿死同族
= client/message 门面锚穿形,divergences 2026-09-24 节),归编译泳道修复;mw_chain
E2020 已销(e96d5f6 直 use 改形)。

### P8 行(生态与部署,2026-09-25→26)

| 波次 | 项 | 锚定 |
|---|---|---|
| P8-A | Connect 协议客户端(http/frm/connect.ct:unary POST+proto 头两行,classify 三态+17 名状态码表+HTTP 映射) | tests/connect **4/4**(a9a5878;含双轮 e2e:成功帧字节断言+500 失败分类;夹具服务端帧校验 08 96 01→08 2A) |
| P8-B | S3 兼容客户端签名面+往返(s3/s3.ct 域包:SigV4 四层链 HMAC^4/canonical/sts/signature/auth_header/basic_time;mock 鉴权结构三针守门+单槽存储) | tests/s3 **6/6**(d858343+b324db2;含 e2e:PUT/GET/DELETE 签名往返全链;黄金向量 python 独立实现钉值 key 尾 0417bcd2/sig 5f5e13fc…;**minio 真靶 0926 全链 ✅**:brew minio 2025-10,凭证调用点修正后 **PUT 200+ETag=md5(body)/GET 回读逐字节/DELETE 204** 三轮;根因=调用点传 AWS 示例键 403 InvalidAccessKeyId) |
| P8-C | NDJSON 逐行游标(std/ndjson.ct T1 零 use:NdLine 值 struct,空白行跳过/CRLF 剥离/尾行无换行;窗口交消费方) | tests/ndjson **8/8**(fcff689;含游标×行内容消费集成+json match 正道集成[match-on-Result emit 缺口 interp 专臂,在册];坏行策略=消费方 json.parse,Result 面修复后深集成回切) |
| P8-E | 部署面+website 指南(docs/deploy.md:产物/运行期表/scratch 配方/可观测三件;website/server-guide.md) | 文档交付(5f872cb);本地验收=e2e 22/22 同源,容器跑通归 nightly 真靶段 |
| P8-F | 门禁收口+终审 | 全波 9 套件+net+todo_api **98 断言 0 红**(00:39 终审);P8-D pkg defer(设计评审待拍板,归下波首件) |

**P8 出口门禁判定(0926 三次收口)**:①pkg add e2e ✅(registry↔ctpkg 全链:
manifest 解析→deps→vendor 双包落盘→lockfile 两行;is_dep 头匹配缺陷修复后)
②Connect 互调 ✅;③S3 夹具往返 ✅ + **minio 真靶全链 ✅**(建桶→PUT/GET/DELETE
三轮;fresh 数据目录亦通;nightly 例行段 CTRON_S3_MINIO=1 入 runner);④website 指南 ✅;
⑤todo_api 容器配方 ✅(容器内跑通归 nightly)。**P8 按 5/5 门全绿交付收口**。
**is_dep 头匹配缺陷**(80f669a 后修复):dep 头判 b4=='{' 拷自 is_pkg 定式,
对 dep NAME { 恒不匹配(t0+4=名首字母)→in_dep 永不置位;修=dep 锚定行尾
(te-2 空格+te-1 '{')。逐行 trace 一轮定位。

**P8 as-built 偏差**:http.client StructLit 残缺陷(往返客户端裸 socket 绕行);
发射器长串拼接/`\n` 字面量组合缺陷(s3_nl/join2 组合原语绕行);`scope` 关键字
撞名(参数改名即清,divergences 在册)。

### P7 行(可观测与运维,2026-09-24→25)

| 波次 | 项 | 锚定 |
|---|---|---|
| P7-A | std/log 结构化 kv(级别码/阈值门/行构造三件纯面;值转义含 UTF-8 透传) | tests/log **9/9** 双臂(a63470e);T1 档零 use 纯模块,ts 与 IO 留消费方 |
| P7-B | std/pb protobuf 线格式(varint 9 组正编/10 组负规范形/tag*8+wire/zigzag 分路/ld/fixed64 LE) | tests/pb **17/17** 双臂(c02e687+5fd6271);官方向量锚(150/testing/-2)+负例矩阵+往返 walk;**纯算术实现**(无位算符纪律,divergences 09-25) |
| P7-C | W3C traceparent 提取/注入(http/frm/trace.ct 零 use) | tests/trace **7/7** 双臂(3dd6f1a);严格 55 字形+非法矩阵+请求提取;黄金向量字节精确 |
| P7-D | Prometheus 文本 exposition(http/frm/metrics.ct 零状态写面)+ todo_api /metrics | tests/metrics **7/7** 双臂 + e2e **22/22**(f96995c);http_total{code} counter×10 + todo_served gauge;热路径调用方 lane 零分配 |
| P7-E | OTLP/HTTP 导出(四层嵌套 ETSR 编码,fixed64 时间面)+ Collector 夹具解码比对 + traceparent 跨服务传播 | tests/otlp **4/4**(含 e2e)+ tests/trace **8/8**(含传播 e2e)(5fd6271+db2cde9);编码→真回环 POST→解码→tid 逐字节+name 双匹配;传播 A 提取→再注入→B 回显 trace-id 一致 |
| P7-G | 优雅停机演练(串行形:排空退出+停机后新连接拒绝)+ 登记 | e2e **22/22**(含停机拒绝探针);inflight 并发形随 keep-alive/协程环志向演进 |

**P7 出口门禁判定(2026-09-25)**:①todo_api 合法 Prometheus 文本 ✅;②OTLP 夹具解码
比对一致 ✅;③traceparent 跨服务同 trace-id ✅;④/debug/scopes ✅ **v1 交付**(3b9f002:ctron_rt_scopes_json rt 锁内快照,
entries 五态+jnext 反查等待边;确定性夹具 [2,3] join 边全钉,tests/rt_scopes 5/5;
serial/coro 双形;树父权 ct_task 层残留=逐层名映射,归编译器任务面);
⑤tracing 层级=连接/请求树 ⏸ 随④残留。**P7 按 4/5 门 + ct_task 层残留交付收口**。

**P7 as-built 偏差与移交**:①std/config 实为 CTCL manifest 校验器,非应用运行时
配置面——todo_api env 配置形维持,应用配置读法提请 std 需求评审;②emit 主文件
20480B SIGKILL + `\|` 死旋(divergences 09-24/25 节)归编译泳道;③OTLP 批量/JSON
形态列志向。**09-25 深夜追加**:e2e 缺声明 6 红(饿死族 client/message 面)经 P1b
契约补全(use 清单全量请求)全清——http 套件 103/5,余 5=137 解释器债族;OTLP send
仍以裸 socket 形(net 门面直连)维持,http.client 合并修复后可回切 client_request。

### 完整应用示例行(2026-09-26→27)

| 波次 | 项 | 锚定 |
|---|---|---|
| APP-1 | examples/todo_app 完整应用(带登录 todo 管理):注册/登录/登出(pbkdf2_sha256+每用户盐+iters 落档)/HMAC Cookie 会话(HttpOnly+SameSite=Lax,恒时比较+过期域)/多用户隔离/NDJSON 文件持久化(重启不丢)/服务端渲染 HTML(ht_esc XSS 安全默认) | run.sh 两相位 e2e **26/26**(注册/重复与短密错密拒/Cookie 签发/未登录门卫/空态/增删翻/隔离门/登出/metrics/重启持久化);四模块 check 0E+test 全绿+fmt 净;真浏览器烟测截图取证(中文标题端到端+用户手工互动) |
| APP-2 | 双层发射缺陷定案(divergences「todo_app 发射爆炸+运行野跳双层案」):①字符串表裸 `{` lexer 毒(P7 同族,解释臂宽容/发射臂 3.95GB 爆炸——初判主分发预算有误)②fn 值形参 ct_clop 野跳转(签名白名单缺口) | 星形单路径四模块拆分(data/sess/views/app)+主要文件 shim;`\{|}~` 转义+删 fn 形参后 237KB 一次发射全绿;应用侧 UTF-8 感知百分号解码(utf8_enc 发射口径)补 form C8 域中文能力 |

### P5 行(std/db 数据访问层,2026-09-21)

| 波次 | 项 | 锚定 |
|---|---|---|
| P5-A | std/json 数值/布尔保真(类型化访问器:jget_i64 溢出检测/jget_f64 17 位窗/jget_bool;越界 = Err 非 silent) | tests/json_fidelity **11/11** 双臂(2^63/2^64 边界负例含 9223372036854775808/99999999999999999999 两串、F64 精度负例、布尔/null 判别、嵌套路径) |
| P5-B | std/crypto 二进制面 + std/uuid v4 真熵/v7 | tests/crypto_vec **24/24** 双臂(RFC 6234 SHA / RFC 4231 HMAC / RFC 6070 PBKDF2 官方向量全锚 + "abc" sha256;uuid 10 万枚唯一性 + 版本位断言) |
| P5-C | 协议夹具回放框架(双薄源共驱一核:script 十六进制回放 / fd 垫片直读)+ PG wire v3 核心(startup/AuthOk/Query/RowDescription/DataRow/CommandComplete/ReadyForQuery/ErrorResponse/Terminate) | tests/db/run.sh 双臂(replay_fixtures 8 手工按公开协议构造,README 登记);fd 源面 emit 专臂(x_fd_edge EBADF/errno 槽、x_fd_pipeline 管线帧 mtype/fill 持态/send-all 整发) |
| P5-D | SCRAM-SHA-256 全流程(RFC 5802/7677,PBKDF2 密码链 + server-final v= 校验 + Z 门 phase==3 信任面)+ 扩展查询(Parse/Bind/Describe/Execute/Sync)+ 事务(BEGIN/COMMIT/ROLLBACK + 取消传播分级) | replay_scram 13 夹具(RFC 7677 原例 i=4096 + i=1 改制;**生成器 hashlib 自检双落地**)、错口令/签名不符 x_scram_neg、nonce 真熵 x_scram_nonce |
| P5-E | Redis RESP2(五形 +/-/:/$/*,NULL bulk,一层数组)+ 有界连接池(容量必填/FIFO/Z('T') 陷阱复位/drain/脏连接绝不发放/双重归还防呆/等待接线背压)+ 行→struct 运行时绑定(**D-P5-1 偏差**:运行时 Err 面,`@derive(DbRow)` 列编译泳道志向) | redis_replay 13 夹具(rr_replay 五形逐字节 + 编码器 hex 锚 + 512MiB 乘前门 + 截断拒收)、pool 4 夹具(pool_core/pool_wait 双臂)、rowmap 2 夹具(rm_anchors r7d 运行时 Err 锚 noent/type/null/row-oob/val-domain + I64_MIN 下顶格) |
| P5-F | **fd 半包判别收口**:redis bulk 数据段 TCP 分片 ≠ 协议违例——incomplete 面(rc 0/err 0/used 0/kind 0)三途(头行无 CRLF/bulk 数据未到齐 sane/数组元素未到齐),redis_recv_fd 续读 + 垫片 would-block 面(非阻塞夹具 socketpair);脚本源截断映射 err 2 拒收语义不变 | x_rd_fd 重钉:真 TCP 分片跨两次 recv(incomplete 面逐值锚)+ incomplete 三臂 + 管线 fill + send-all;脚本面 rr_replay 截断钉同绿;redisx_i64s I64_MIN 负向累计 + EXPIRE I64_MIN hex 锚钉;pool 借出/回池栅两处对齐(借出半开/回池全闭语义注 + 槽尾再借 err 2 钉) |
| P5-F | **fd 真源全会话**(pg.ct fd walkers:pg_scram_handshake_fd 含 SASL 双下行、pg_query_fd 帧分派核同构;dbpg/dbredis connect 垫片) | x_pg_fd_session(emit 臂,链 dbpg+entropy 垫片):socketpair 真源 SCRAM 全链(sigok=1/pid/key 逐值)+ 简单查询行集 + 扩展查询 + 事务状态 I→T→T→I;服务端字节 = 夹具同源帧逐字复用 |
| P5-F | **tests/db 双臂总门禁** | **54/54**(守卫 15 [漂移 5+声明冒烟 5+在位 5] + interp 臂 16 + emit 臂 23;夹具面 = script 8+13+13+4+2 手工构造 + corpus 7 + x_ emit 专臂 3;较 P5-E 53 增 1 = x_pg_fd_session);四矩阵不变:json 11/11、crypto 24/24、http 59/59、net 14/14 |
| P5-F | nightly 真靶(真 Postgres/Redis;不入 CI 主环) | tests/db/nightly/(env CTRON_PG_DSN/CTRON_REDIS_URL 驱动,nc 探活回退;构建门先跑 = 夹具健康检查):**本机实测 构建 2/2 PASS + 真靶 2 SKIP**(5432/6379 不可达,无靶跳过口径逐行登记);PG 面 = connect+SCRAM+简单+预编译+事务,Redis 面 = SET/GET 回环+INCR×3+DEL |
| P5-F | 性能门 简单查询回环 vs libpq ≤1.5× | tests/db/perf/(baseline_libpq.c 同构基线 + ctron 臂,min-of-3 整进程墙钟):**本机实测 SKIP 登记**——缺本地 libpq 开发面(pg_config 不可用、常见路径无 libpq-fe.h;缺失前置逐行输出),不虚构数字;装 libpq + 有真靶后重跑 |

## §ctron fmt 三宿主对齐(工具链泳道,2026-09-21)

R-P2d `ctron fmt` 由 Rust 宿主移植至 C 宿主与自举编译器,三宿主同规范(docs/fmt-spec.md)同输出。

| 面 | 项 | 锚定 |
|---|---|---|
| C 宿主 | `compiler-c/src/fmt.{c,h}` + `ctronc fmt`(完整 CLI 契约)+ `tests/suite_fmt.c` | 11 金样(R4 期望按 Rust 参考冻结)+ 语料幂等 160 + trans 等价代理 69 + 词法脏报错 1;`make -C compiler-c test` 挂载 |
| 自举 | `compiler/src/fmt.ct` + `scan5`(lex.ct 加法改造:原始流+字节 span+注释 span)+ `driver_fmt.ct` → `bin/ctron-fmt` | 金样钉子 `compiler/test/fx_fmt_golden.{ct,expected}`(smoke 3f:金样逐字节/native==seed 双口径/幂等/R8 负例);`ctron fmt`(-w/--check/pkg 目录)契约 5 断言入 ctron_smoke 第 9 段 |
| 对拍 | `tests/fmt/parity.sh` | 三宿主(Rust 参考/C 宿主/自举)tests 160 + std/examples 45 逐字节零分歧;词法脏一致报错 1 |
| 语义对齐裁决 | `.or(` 成员位置紧贴(Rust lexer prev_is_dot 上下文)、CRLF 注释尾 `\r` 修剪、`...` 逗号后紧贴 | 以 Rust 参考实现输出为真值冻结 |
| R4 v1 定版(2026-09-21 用户裁决) | 链断行相对缩进 1 级三宿主同步落地 | spec R4 文字与实现自此一致;金样/套件期望已重冻结,parity 303 绿零分歧 |
| R9+R10(2026-09-30) | use 组自动折行(内联形态超 100 列,恒补尾逗号)+ 导入排序(组内项按名/顶层语句按路径,稳定字节序;空行分段次序保留;注释守卫冻结;无组尾随别名并入语句 span 防悬空) | spec R9/R10;三宿主同步(Rust fmt.rs 预处理/C fmt.c 1:1/自举 fmt.ct);suite_fmt 17 金样+432 语料幂等+93 trans 等价;fmt_suite 34 测(断言② 修订为 use 声明多重集比对);fx_fmt_golden 重冻结三宿主逐字节;parity 默认根适配 lib/std 后 494 绿 0 分歧 |

已知红账(非本面):smoke conc_fs / std 快照漂移 / fs 种子单测(HEAD 既有,net·fs 泳道);
**suite_diff 差分墙:37 红 → 4 红(2026-10-01 大头销账)**:三差分模板同步落库——
lex_num/parse_ast/parsetree 补齐 v0.7 修订一(||/&&/or 中缀+成员位置上下文)与修订二
(break/continue)+ match 守卫吞并(R-P3c,C 侧 dump 不含守卫)+ 或模式 Or([...])+
use 组逐名展开(含 as 别名/组内 NL 不敏感)+ extern ABI + struct 字段逗号 + @derive class;
顺手修 C 侧真 bug:ast_show.c 漏 PAT_OR case(或模式 dump 成空 pattern,Rust Debug 口径
Or([...]) 补齐);pkg_chk 模块级 7 包按「文件+码+计数」归一(自举双语诊断表 vs C 英文串
的语言面分歧在册,消息文本剥离)。**余 4 红 已于 2026-10-01 全数销账(57ba086 续片,sem_chk.ct 同步)**:第三块 oracle
sem_chk.ct(树上语义,4086 行)移植同款解析器补丁(13 锚)+ Guarded 标记入 Arm 节点+
E2030 守卫感知(守卫臂不算覆盖不触发整臂,T02 语义;或模式臂各替身依序入覆盖)+ 守卫
条件消费修正;03n/03l/07a 崩溃清零、02f 守卫穷尽精确命中「缺变体 X Y」、02f 正例保持
零诊断;smoke sem_chk decl 锁 109→112 随批申报。**suite_diff 终态:412 cases / 0
failures,墙全拆。** 其后首曝两门:suite_trans 15/0 绿;suite_corpus_trans 初测 44 pass/12 红,当日
**48 pass/8 红(1c98531 续片)**:真病灶=trans.c 的 main 装配优先级反了(有 test 块时
user_main 整个被丢,语义对齐 rt_run_main 口径=有 fn main 只跑 main)+ Void main 被发射成
`return (int)user_main()`(gcc 拒;按 TY_NAMED Void 分档 void 调用 rc=0)——03l/03n/
03k/06_crlf 四件清零。**余 8 红 → 5 红(2026-10-01 续片收口)**:width/as-U64 三件清零,双根因——
①as 助手掩码 `1ULL<<bits` 在 bits=64 为 UB → `& 0ULL` 恒零(64 位无符号改值位直通,
§3.6 负值模 2^64);②U64 checked 算术经 int64 存储为负 → int128 范围检查误判(03i 误报
溢出/03k 漏检)→ uint64 参数 + __builtin_*_overflow(§4.5 溢出 panic 语义)。
make test 红门置尾(corpus_trans 移至 modules/fmt 之后,红门不遮蔽后续门)。
**终态 51 pass / 5 红,全部 cc 失败=他泳道在册发射缺口**:03j 闭包参成员/
05i into_gc 深拷贝/06_concurrency×1/T55 泳道×2(t55_mw_smoke/t55_probe)。fmt_suite 01i(Rust 侧既有)。

**2026-09-21 续片修复**(compiler-c 门禁卫生):
- `token.c` TOK_NAMES 表漏 "Ellipsis" 条目(§9.6 加 TOK_ELLIPSIS 未同步)→ 名字表
  错位一格且尾部越界读,EOF 名读出 NULL 使 test_lex 段错误——补条目,test_lex 58/0 全绿。
- `suite_lex` 补 `.neg.ct` 过滤(对齐 Rust lex_suite 先例):neg 语料判定面是诊断码,
  不入"全语料零词法诊断"断言 → 127 文件零误报。

**2026-09-28 T01 bit.* 位运算内建落库**(spec-gap W1;§4.0 宪法「位运算恒走 bit 模块」兑现):
- 前奏命名空间 `bit.*`(parallel 先例):and/or/xor/not/shl/shr × i32/u32/i64/u64,共 24 入口;
  补码语义按声明宽度、shl 回绕、shr 有符号算术/无符号逻辑、移位数越界 panic "bit shift range"。
- 四面同判:自举解释(eval_call 文本域逐位组合+双零早退)/自举发射(trans_expr 直出 C 位运算+
  ct_bit_sh* 运行时助手)/C 宿主解释(rt_eval __int128)/C 宿主发射(trans_conc emit_bit)。
- 锚:tests/bit_ops.ct(6 块)+bit_shift_{hi,neg}.panic.ct;std/crypto 六处内联位函数回切 bit.*
  (SHA/SCRAM 向量零回归,自举解释臂 8.0s→5.3s);R 线内建面缺位在册(crypto R 臂 0/3 预存红,
  bit.* 随 R 线内建面批次补,失败数不增)。
- 顺手修:自举 i64/isize 后缀宽字面量此前误落 I32 域截 0(eval_expr 宽路径仅认无后缀),
  03b 补锚;宿主负大字面量/宽正字面量算术检查缺口(unary neg/Sub 于 v_int bits=32 恒炸)为
  宿主在册债,锚侧以 i64 后缀规避开(divergences 待登)。

**2026-09-28 T02 模式守卫+或模式三线落地(R-P3c 销账)**(spec-gap W1;§4.6):
- r3c 两锚迁入主套件为 02f_match_guards.ct / 02f_match_guard_exhaustive.neg.ct(E2030);
  三线落地:自举解析/求值/穷尽既有(R-P3c 半成品收口)——C 宿主补 parser(或模式 PAT_OR +
  臂守卫)/rt(pat_bind_or 替身依序 + EX_MATCH 守卫绑定后判定)/sem(守卫臂不算覆盖,或模式
  替身展开计覆盖);R 线补 ast(Pattern::Or + MatchArm.guard)/parser/check(守卫跳过覆盖 +
  或模式展开)/interp(try_match Or 前置拦截 + 守卫假值回落)。
- 发射臂登记(不阻塞本翻转):自举发射 match 面窄(Result/用户枚举/Option[Str] 特化),
  表达式位 match 与字面量模式臂 ct_expr panic —— 即 P8-C 在册「match-on-Result emit 缺口
  interp 专臂」同族,守卫/或模式发射随该债一并扩;C 宿主 trans 同族登记。
- suite 77/78 双线(+2 全绿,06f/06e 两预存红不变);cargo 13 套全绿;meta/fmt parity 过。

**2026-09-28 T03 StringBuilder 前奏三线落地**(spec-gap W1;§3.8.2 R-P2a 销账):
- r2a_sb 锚迁入主套件 03j_stringbuilder.ct;API:StringBuilder()/push_str/to_string/len(字节)。
- 三线:自举(SB 值=段列表宿主 List 别名语义,call_mem 专属臂须先于通用 to_string)/
  C 宿主(V_SB 复用 listnode 段表)/R 线(Value::StrBuilder,通用 to_string 截胡排除;
  裸类型名构造器调用回退补——def_by_name 命中即构造,含无泛型形)。
- 发射臂:native_suite 口径 trans 拒绝=skip;StringBuilder 发射(拼接语义)随容器发射批次。
- suite 78/79 双线 +1 全绿;cargo 13 套全绿;meta/fmt parity 过。

**2026-09-28 T04 W 码三负锚补齐**(spec-gap W1;§5.6/§3.8/§1.3):
- 新锚 01m_shadow_prelude.lint.ct(W8040 参数位遮蔽)+01n_unused_binding.lint.ct(W8030 未使用
  绑定,fn 体形——自举 W8030 仅扫 fn 体,test 块位登记差异);05f(W8020)既有,注册表「锚待补」注记清除。
- 实现补齐:自举 W8020 泛型返回(Result[I32,Str] Named+TArgs 形)+fn/test 体**块尾表达式位**
  (walk_b 加 body 参,值位块不判防误报)+参数位 W8040(is_prelude_name 名单对齐 C 宿主;行传 0
  交驱动探针——Fn 无属性路径不盖章,nline 契约内回退);C 宿主补 fn/test 尾位 W8020(w8020_tail)
  +**W8030 全新**(bind.used 标记:bind_find 命中+check_expr EX_IDENT 读取位;保守面仅原语类型
  绑定——插值部件宿主侧纯文本无 AST,聚合/Option/Drop 绑定免报对齐自举豁免口径)。
- 登记差异:R 线无 W8030/W8040 发射面(lint 锚经 check_suite 零诊断口径空过,R 线欠账);
  自举 W8030 不扫 test 块位。
- suite 80/81 双线(lint 4/4×2;06f/06e 两预存红不变);cargo 14 套全绿;meta/fmt parity 过。

**2026-09-28 T05 行尾点守卫落库**(spec-gap W1;§1.6):
- r1a 锚迁入主套件 01o_trailing_dot.neg.ct(E1001);自举词法层 filter_nl 守卫(行尾 `.` 后
  未抑制的 NL → E1001,消息键 E1001.trailing.dot 双语登记)——此前自举解析器吞点误放行,
  宿主/R 线既有拒绝面不变;parse_suite/check_suite 双登记,roadmap_suite 移除 r1a。
- suite 81/82 双线(06f/06e 预存红不变);cargo 14 套全绿;meta/fmt parity 过。

**2026-09-28 T06 E6030 parametricity 封闭性论证**(spec-gap W1;§8.4):
- 结论:**现行语言面下 E6030 违规不可达,码位由封闭性承载**(不设重复空检查,能力优先裁决)。
  论证四点:①语言无任何反射算子(type_of/type_name/reflect 全库零命中,§3.8 前奏无此面);
  ②ceval(E6010 步数预算求值器)值域仅 I 域整型字面量/const 引用/算术/比较/逻辑与——无类型
  表示可入值域;③类型名在表达式位仅解析为构造器/变体(sem E2020 面),不存在产生类型信息的
  值形态;④@derive 为编译期结构化展开(硬编码 Show/Eq/Error 集,结构谓词深度限 6),非运行期
  反射。将来若引入类型级 comptime(§8.4 预留的类型产出函数)或反射 API,须同步补 E6030 实检。
- 规范 §10.1 E6030 行注记随本论证更新(预留 → 封闭性承载)。

## §web P0 编译销账(编译泳道,2026-09-27→28)

| 项 | 锚定 |
|---|---|
| P0-3/P0-2/P0-1/P0-4 member-emit 四件 | 探针转正 `03k_fn_call_chain_member`/`03j_closure_param_member`/`03l_list_struct_member`/`03n_struct_fn_field_array`(销账提交 fc79e6c/ec006dd/b0e5bfc/edb59fb)。臂况分记:03k/03l/03n 种子双臂绿、03j interp 双臂绿;种子发射臂残留:03j(值位置闭包按设计非捕获 trans_expr.c:1587,`tag` 未声明——候选 **L7**);自举原生发射臂残留:03l emit `ct_expr:Member@23`(L 码擦除 trans_ty.ct:250+定宽 char** 槽=P5 b4)——**已销账(T14-② 参数化 List 码+装箱容器 ABI,T54 补 fn 值链 #fret 字段提取位+蹦床 u: 解盒;03l 正本 emit 臂绿 `HIT /app -> app:alice`+`SUM 42`,见 spec-gap 计划 T54)**、03n emit `emit:并发值含 struct 值传递`(在册同族)、03j(同 L7 族)——条件登记 **L6/L7**(spec §9,候选·待用户裁决) |
| L5 StringBuilder 判定 | **立(终判)**:探针 `03i_string_builder_probe` 双编译器 check 挂名过(0 诊断)、运行面拒(种子 `未知函数: StringBuilder` / 引导 `E2020`);编译/运行分相不合 `.neg.ct` 契约,维持行为件红账 |
| L4 Bytes 判定 | **立(终判,引导双臂实测坐实)**:探针 `03m_binary_nul_probe` 种子双臂不可执行(单文件不并 use);引导 interp + emit→gcc 双臂执行成功而门红(`len:4/nuls:0/mid_ok:0`——/bin/echo 101136 B fat Mach-O 首 NUL 在偏移 4,Str=C 串读边界截断,nuls=0 且 mid_ok=0 → 立) |
| **web-P0 收口门** | 修复四件:03k/03l/03n 种子双臂绿 ✓、03j interp 双臂绿 ✓(emit 双臂红,残留=值位置闭包捕获,候选 L7 在册——非本批销账域,见上行);03i/03m 判立红账在册(非新增红);**合并后 suite.py 合计 87/88(自举,红=06f_parallel 既有)+88/88(宿主);03i 随 T03 转绿(L5 销账)**(模块桶 14/15、13/15 另列;bootstrap 红 2 = 03i+06f_parallel[基线既有:解释器无 parallel 内建];种子红 2 = 03i+03m;bootstrap 侧 03m rc=0 而 gate 红,suite 判 rc 不判 gate,红账以 divergences 判定为准);smoke --full 150/0;ctron_smoke 21/0;meta gate 已知红 2 = 03i/03m 行为件无 test 块(即两探针,判立红账一部分,不扩大) |

臂覆盖声明(套件量程,防「双臂绿」误推;03j 账误即由此出):本套件行为桶只跑种子 `check`+`test` 与引导 `run` 两类臂——**无种子 `build`(发射)臂、无引导 `emit` 臂**。故任何「双臂绿」结论必须来自显式逐臂运行,不得从 suite 通过推得。

设计:`docs/superpowers/specs/2026-09-27-web-framework-design.md` §9;判定详证与逐字输出:
`docs/c-rust-divergences.md`「web 框架 P0 探针」节。

**2026-09-28 T07 单态化实例预算 E6040 落库**(spec-gap W1;§8.5):
- driver_emit 预扫计数(镜像 struct insts 预扫惯例):显式 TypeArgs 泛型 fn 特化去重键
  (mono_e/mono_b 走查,形状同 sem_walk)+泛型 struct 实例,超限 → E6040+rc=1,消息含
  「建议 &Trait 化」;CTRON_MONO_BUDGET 可调(默认 8192;seed 路径 env_get 哑在册,旋钮走
  原生 bin)。v0 计显式位,推断特化随 §3.9.1 ex_ty 保留实参登记并入(锚文件头注记)。
- 锚 compiler/test/fx_mono_budget_neg.ct(4 显式特化去重 4——dedup 即证)+smoke emit 臂
  (CTRON_MONO_BUDGET=3 → E6040 rc=1);注册表先行:E6040 入 §10.1 + meta_check。
- suite 81/82 双线维持;db 套件回基线;meta/fmt parity 过;native 自举固定点过
  (编译器自身显式特化计数在 8192 内,预扫零开销可观察)。

**2026-09-28 T08 comptime 预算口径裁决落地**(spec-gap W1 裁决门;用户裁定:步数预算为 v1 终态):
- §8.4 修订注:1200 步步数预算(ceval,E6010 判据)为终态口径——确定性友好(同输入同判定,
  与 §10.3 无张力);1s 时间口径废弃(时钟依赖不可复现)。清单键 comptime.budget_ms 定性为
  声明位(C 宿主 pkg 解析+校验+pkg 子命令展示,不进执行;实证 grep 无第二消费方),量纲统一
  (budget_steps 改名)随 CTCL 迁移批次(T48)定夺。
- comptime_budget 模块锚注记对齐步数口径;r6f 锚头注本即步数口径无需动。

**2026-09-28 T09 resolve 多记录双栈落库**(spec-gap W2;§11.5):
- C 垫片 ct_getaddrinfo_all(AF_UNSPEC 全记录 → inet_ntop 文本 "\n" 打包)+门面
  ctron_net_resolve_all(TLS 静态;helper 池 all 模式——job 缓冲 64→2048,池线程产全记录
  打包串,槽所有权/停车纪律与 resolve_first 逐条同构);net.ct 门面 net_resolve_all →
  List[Str](规范点分/冒分文本)+ addr_is_v6;首记录面 net_resolve 保留。
- 夹具 resolve_all(数值 v4/v6 确定性单记录/localhost ≥1/非法数值串空表+err;零外联纪律);
  消费方 tcp_echo/unix_sock 双 API 化(协程对切 all 模式 = helper 池新面冒烟)。
- **表示裁决**:v0 记录以规范文本承载(List[struct] 发射 = divergences (h) 族,同 T14
  struct 值容器底座;SocketAddr 值 struct 随其并入);List[struct] 发射断点实证 = 元素
  类型丢失(char* 槽)+索引取值无提取分支。
- 顺手修三笔:①Str.contains(§3.8.2 v0.5 钉死承诺)interp 缺位+emit 丢实参(10_trace 侧
  经 List.contains 绕过故隐形)——interp str_contains 分支+emit strstr 直出+R check 臂,
  01d 补锚;②UFCS 发射 from=2 计数错位(实参恒漏首个+个数核对错位;04g 发射臂 rc=1 即此,
  主套件从不发射故长期隐形)——重写实参对位(pcs[1..]);③R 线 std.fs.read_or 原生调用面
  (机刷 03m 探针前向修复,read_file 同族臂)。
- 新登记:04g 发射臂类方法分发缺位(c.hi() → t_hi 未声明;UFCS 修后暴露的下一层,
  归类方法发射批次)。
- net 双矩阵 15/15×2;db 44/10 基线;suite 87/88 维持;cargo 14/0;meta/fmt 过。

**2026-09-28 T10 sleep_ns/可取消/虚拟时钟落库**(spec-gap W2;§11.6/§11.4):
- C 三件:ctron_net_sleep_ns(协程面 ctron_rt_sleep_until 绝对 deadline 停车,取消广播早醒
  →返 1(deadline 未到判据),裸线程真睡返 0=join 等效口径);ctron_net_clock_jump(虚拟态
  偏移累加,实钟态 -1+EINVAL);虚拟钟(CTRON_CLOCK=virtual:冻结基点+跳变量,env 惰性读)。
- rt 两件:ctron_rt_clock_ns 弱钩(rt.c 弱缺省实钟,net 垫片强覆盖贯通——虚拟跳变驱动
  定时器堆依 deadline 序确定性触发)+ctron_rt_sleep_until;net.c 补弱哑元(裸矩阵链接纪律)。
- 门面 net_sleep_ns/net_clock_jump(bind extern 直调,方法发射缺口在案);夹具 clock_sleep
  三矩阵:实钟臂(下界/单调/取消 elapsed<1s)、虚拟臂(5s 虚拟定时器经跳变 33ms 真实完成)
  ——模式探测 jump(0),实钟臂默认+coro、虚拟臂 run.sh 第三矩阵 coro+virtual 专跑块。
- 新登记:①scope 闭包体 return 语句发射泄漏(return 直出宿主函数+块值丢失;块值须尾表达
  式形态,coro_conc 先例;spawn 闭包不受影响)②ct_spawn pthread/协程混合分派下取消早退
  深时序(~500ms 中间态)归 P9 栈经济注记③裸线程睡眠虚拟跳变不打断(专跑臂在协程矩阵,
  语义注记)。
- net 三矩阵 17/17×2+专跑;db 44/10 基线;suite 87/88 维持。

**2026-09-28 T11 能力键细分落库**(spec-gap W2;§11.1/§12.1):
- 键集扩为 {fs, time, net.listen, net.connect, net.resolve, db.connect}(粗键 net/db 不再
  授予,fail-closed 硬切;在库 ctecho 清单同步升细键)。映射:listen 族(tcp/unix listen+accept)
  /connect 族(tcp connect+udp 全族+unix connect)/resolve 族(net_resolve+all);db 域统一。
- 四线注册表同步:schema(ctcl_manifest_schema members 真源)/宿主 pkg.c(check_caps 重写:
  补域二段形——caps_net 宿主 informational 分歧顺手闭/R check.rs+main.rs/自举第四臂
  selfhosted/ctcl_chk.ct;E5043 消息统一 sorted 序,四线对拍 104 例全绿)。
- 自举 parse_pkg:细键检测(use 导入驱动,cap_net_fine 映射+去重)+粗面兼容(能力对象
  &Net/&Db=任一细键);**顺手修**:无清单语境 read_file 缺失回落 Some("") 被当空清单
  deny-all——此前粗检测 &Param 门槛掩盖,细键导入驱动面暴露;空文本=无清单放行。
- 夹具 caps_fine_neg(仅 net.resolve,listen/connect 导入 → E4010×2)/caps_fine_ok
  (resolve 正例,干净 extern 面——listen/connect 门面 Box 出参 W8052 真阳性属 ABI 面不重复);
  suite modules 跑器补 CTRON_STDPATH(域包门面解析,run.sh 同惯例)。
- diff.py 排除 .worktrees(他泳道工作树陈旧清单/二进制不入四线对拍)。
- modules 16/17×2(唯一红=use_alias_nat/dup_static 既有意红基线);net 17/17×2;suite
  87/88 维持;manifest 三门+ctcl selftest+cargo 14/0+meta/fmt 过;R 线细键检测(对象面
  note_cap_use 仍粗键)登记 R 线欠账。

**2026-09-28 T12 gzip 收口**(spec-gap W2;§11.7):
- 主体已由 webfw 泳道交付(P4-B http/enc.ct:gzip 容器组框 RFC 1952 纯 Ctron +
  miniz 垫片 CRC-32 单实现 + Accept-Encoding 协商;enc_fixtures x_gzip_round/
  x_crc32/x_deflate_round/a_negotiate)——计划卡分析时序差,本件缩为互操作补齐。
- 补:①x_gzip_interop(python gzip.compress 真实流 93 字节 hex 语料 → 本实现
  gunzip 逐字节还原 292 字节;头布局锚 1f8b/08/00/mtime=0)②x_gzip_out(本实现
  产出流 hex 出)③run.sh 互操作臂(本实现流 → python gzip.decompress 逐字节
  差分)——消费向/供给向双闭环。
- 登记发射怪癖:x_ 夹具 fn main + 显式 `&数组` 实参形 → 发射驱动 SIGSEGV
  (截断 C);test 块 + 数组直传(隐式视图转换,gzip_round 惯例)为正形。
- http 110/1(唯一红=frm_auth_a_jwt interp OOM,实证与 bit-crypto 无关:
  原版 crypto 对照 1.46GB vs bit 版 1.32GB/50 HMAC——解释器 arena 无回收
  在册债(T31 目标)所致)。

**2026-09-28 T13 Atomic 真原子落库**(spec-gap W2;§7.3):
- 自举发射:fetch_add 由全局锁 RMW(ct_glock 三步)改 __atomic_fetch_add(SEQ_CST,
  返旧值语义不变);load/store 升 __atomic_load_n/__atomic_store_n(原裸读写无序)。
- C 宿主镜像:ctron_mutex_<w>_{load,store,fetch_add} 三 helper 同改(原裸读写/
  __int128 RMW);Atomic with/with_mut 保持互斥语义(锁归属正确,不动)。
- 验证:coro_conc 增 atomic_smoke(独立函数——同函数双 scope 撞发射固定名 t_sc,
  名不唯一化登记;4 任务×50k fetch_add 双矩阵精确 200k + store/load 往返);
  宿主最小探针(fetch_add 返旧值→store→load 链路正确);net 17/17×2。
- 门禁:suite 87/88 维持、modules 16/17×2(基线红不变)、db 44/10 基线、http 110/1
  (jwt interp OOM 预存)、cargo 14/0、meta/fmt/ffi 过。
**2026-09-28 web 2a 开波**(web 框架核心纯函数面;计划文件:
`docs/superpowers/plans/2026-09-28-webfw-2a-core.md`):
- Task 0 包骨架落库:`web/Ctron.ctcl`(pkg web;caps net.listen 供应链审计标记)+
  `web/core.ct` 包头占位(Task 1-4 填充);语义门 `ctc.sh check web/core.ct` → check OK。
- 包测试驱动口径(seed 单文件):语义门 `sh compiler/ctc.sh check <file>`;test 块
  `compiler-c/build/ctronc test <file>`(suite.py 两相位同款;跨模块被测文件按
  compiler/build.sh 式拼接,bootstrap ctc.sh 无 test 子命令)。

**2026-09-28 web 2a 收口(Task 5;16e702e..本笔,worktree webfw-2a)**:
- 语义门:`ctc.sh check web/core.ct web/router.ct web/view.ct` 全 OK(decls=134)+
  三文件裸跑 rc=0;负臂实证:err_json 旧 end_status(status 底座 text/plain)对新断言
  `Content-Type == application/json` 实咬(assert failed rc=1),修后绿。
- 本笔面:err_json CT 修正(M-T1w-1,end_status 改走 json() 底座)/strs 独立断言(M-T1w-2)/
  csv_rows(RFC 4180 最小引用形,本地实现——use std.csv 入 core 会翻转 view 合并态 E5030,
  见下)/attachment/xml/yaml 补 §4.4 格式矩阵/install(§4.6 插件挂载)/test_call(§4.7 测试口,
  dispatch 别名)/view 信任边界注记(M-T4w-1)/README API 表(§4 定稿面 2a/2b 两列)。
- 新编译器坑在册(M-T5w-1):pkg_load_use 选择性合并 keep 闭包含 **test 体引用**
  ("Test 恒随"口径)——core 测试体引用 `text()` 即把 core.text 拉进 use 方(view)合并面,
  与 view.text 撞名 E5030(实证:引用时 rc=1/去除 rc=0);同型:`use std.csv` 入 core 同样
  翻转 view 合并态。包内跨文件同名 decl 禁入被 use 方 test 体。
- 覆盖清单:Resp 构造器族(html/json/text/redirect/status/err_json/xml/yaml)+ with/with_status/
  attachment + resp 三读 + json_obj 五件(end/end_status)+ csv_rows + Req 六取参(query/
  query_all/form/form_all/header/cookie)+ param/param_i64/param_f64 + pdec 边域 + req_of/
  req_full/req_set_params + Router 全组合子(get/post/put/delete/patch/middleware/mount_at/
  install)+ route_match 三档/405 Allow/has_conflict/dispatch/test_call + 中间件短路穿透 +
  视图 el 族(结构性转义/raw/children/自闭形)。param_f64 在册确认(spec §4.3)。
- 2b 待办指针:serve 循环/body_limit/not_found/method_not_allowed 挂点/static/openapi/
  bytes(挂 §7-8 二进制门)/send_file/req.json+JDoc 校验族/session 族(with_sessions/
  grant_session(5 参含 now=§8-A5 注入)/drop_session/flash)/README 示例迁移(todo_app 对照 §10 验收门)/原生 e2e 门
  (挂 L6/L7 销账)。全景见 `web/README.md` API 表两列。
- suite 回归:web/ 不在 tests/,suite.py 计数不动(87/88+88/88 基线,本笔复跑确认)。

**2026-09-28 T14 Channel 去限制三片落库**(spec-gap W2;§7.3/§12.3):
- **①容量上限解除**(0e004a2):自举模板 ct_chan buf[64] 堆化+取模 %64→%cap+cap<1→1
  (fail-closed)+注册表定长 64→增长数组(realloc 加锁,原无锁竞态顺手收口);宿主发射器
  本即类型化堆环无需动;锚 coro_conc chan_cap_smoke(单线程 128 发不阻塞=在途容量实证)。
- **②List[struct] 发射底座**(b6d26fe;divergences (h) 族首件收口):元素码 Lu:<名> 贯通
  (注解位 ct_ty_code/构造推断位 ct_typeof/索引派型位);push 堆盒装箱(值拷贝);索引解
  引用取回;for-in 补 L/Lu: 两族(此前 for-over-list 发射面整个缺位);L(char*)/LI(int)
  既有行为零扰动;锚 fx_list_struct(装箱/索引/for-in/struct 字段持有/值拷贝语义)。
- **③句柄作字段+struct 入通道**(369b9bc):Sender/Receiver/Channel 注解型码 h——作
  struct 字段/形参/返回位贯通(P5-E 池持通道「emit int32 截断」缺口收口,db/pool 消费方
  迁移随 db 泳道);struct 值入通道 = send 装箱 + match Ok 臂解引用(解构位 rx 绑定
  hR<元素码>);锚 fx_chan_structs;登记:recv().expect 于 struct 载荷未支持(match 为
  消费正形)、句柄注解无型参(hR 元素码仅解构位可得)。
- 同底座连带解锁:SocketAddr struct 化(T09 v0 文本承载可升级)、form.ct List[struct]
  产错型强转登记位、04g 类方法发射((h) 族下一件)。
- 门禁:net 17/17×2、suite 87/88、db 44/10、http 110/1(基线红不变)、cargo 14/0、
  meta/fmt/ffi 过。

**2026-09-28 T15 Iterator for 集成三线落地**(spec-gap W3;§3.8.2/§4;R-P3b 首件):
- r3b_iter_trait 锚迁入主套件 03k_iter_for(E2030 语义正交,无 neg);锚修订:&var self
  (非文法形)→ var self(EBNF 正形,起草笔误);适配器测试拆出 r3b_iter_adapters(T16 域)。
- 三线 interp:自举 For 处理器 U 分支(find_impl_method next + 共享 env 运行方法体——
  self 绑定于循环 env,var cur += 1 经 env 写回对下一轮可见);C 宿主 ST_FOR V_STRUCT 分支
  (cls_method + 同 env 直跑 body 免 call_method_body 弹栈丢 mutation);R 线 Stmt::For
  Struct 分支(find_method_block + fenv self 绑定;R 线 struct 值字段 Rc<RefCell> 共享 =
  mutation 天然对循环侧可见)。
- 顺手修:解析器 &var 前缀死循环(& 消费后 var 被当参数名 + else 双 adv 越过 self →
  p_typ 死循环;防御消耗按 var 语义)。
- 发射臂登记:var self 方法按值传递 = mutation 丢失(Iterator for 发射缺口;泛 var 参数
  引用语义=发射层系统改造,归后续批次)。
- suite 88/89×2(+1 全绿);cargo 14/0;meta 过;db 44/10 基线;net 17/17。

**2026-09-28 T16+T17 std/iter 归位+06f 翻转**(spec-gap W3;§7.7):
- **T16 std/iter.ct 适配器面**:数组终结器(sum/count/any/all/collect)+适配器 struct
  (MapArr/FilterArr/TakeArr + 终结函数)。**阻塞登记**:UFCS 链式调度(`xs.map(f).filter(f).sum()`)
  需同名函数作用于不同适配器类型 = 函数重载或泛型 trait 分发(皆 T25 域);泛型 struct
  MapIter[A,B] 被实例化字母表限标量/Str 阻断。r3b_adapters/r3b_iter_adapters 锚维持红
  (T25 依赖,翻转条件在案)。
- **T17 06f 翻转**:std/iter.ct 创建(§7.7 归位);加载器补前奏符号 use 显式导入恒可见
  (§3.8 前奏隐式可用——use std.iter.{parallel} 为文档化冗余,pkg_is_prelude_sym 判定)。
  06f_parallel.ct 常驻红首次翻绿。parallel 前奏直用保持兼容(过渡双轨)。
- **suite 89/89 双线——主套件首次 100%**(自举+宿主全绿);cargo 14/0;meta/db/net/http
  基线全维持(唯一红=http frm_auth_a_jwt interp OOM 预存在,归 T31 arena 债)。

**2026-10-02 T16 UFCS 链式收口**(spec-gap W3;§4.8/R-P3b):
- **std/iter.ct v2**:`trait Seq[T, S]` get 基惰性适配器(get(i)=纯位置读取,无内部状态;
  map/filter/take/sum/count/collect/any/all 默认方法,match 抽值)+ RawSeq/MapSeq/FilterSeq/
  TakeSeq 泛型 struct(类型正确 impl 形 `impl Seq[T, MapSeq[T,S]] for MapSeq[T,S]`——trait 的
  S 实参=具体自身型,默认方法 `it: S ← self` 才对齐)+ 数组 UFCS 自由函数入口(pub fn
  map/filter/take 包装 RawSeq;sum/count/collect/any/all 快路径直终结)。0928 函数面
  (iter_map/maparr_* 前缀族)零消费者,整体替换。
- **get 基裁决**:interp 逐调用拷贝接收者,var-self 写回在册(T15 发射臂登记同源),
  next 基适配器叠适配器上游永不推进(探针实证挂死);get(i) 无状态,链任意深度组合
  安全。自定义 Iterator 经 for-重放 get 接入(03k env 线程化语义)。fn 值字段调用须先绑
  局部(`let g = self.f; g(x)`——直接 `self.f(x)` 成员调用不识别)。
- **锚迁 modules/iter_adapters**(带 use 多文件包:自举臂运行+宿主 pkg check;宿主无包
  运行口径,单文件 use 不合并,07a/06f 的 std.fs/parallel 走内建命名空间故单文件可绿)。
  roadmap_suite.rs 两表行按翻转协议删除。锚修:原稿 `!xs.any(|x| x>5)` 与 xs=[2,4,6]
  矛盾(6>5 为真),首翻绿校正为正断言。
- **在册债**:①发射臂 trait 方法调用未发射(t_bump/t_hi 未定义,04g/p11 实证)+ for-over-
  Iterator 未发射(03k emit 报 ct_stmt:for)——T25 v2 域;②var-self 写回语义(interp p6/
  p7/p8 实证不写回)= 泛 var 引用语义系统改造;③C 宿主解析器不认 pub trait/pub struct
  (TOK_PUB 只路由 fn);④自宿装载器 pub trait/pub struct+泛型字面量经 use 即坏(E2020
  错判/E5030 幽灵 decl 名"T")——本件以非 pub trait(T26 恒可见)+非 pub struct 绕行,
  loader×pub 为独立 bug 待修。
- **suite 99/99 双臂**(含新 modules 用例;96→99 为并行泳道增量);meta/manifest 四线
  对拍全绿;modules 段 dup_static(宿主 neg)/use_alias_nat(自举 neg)两红为对端在飞
  WIP/既有账,与本件无涉。

**2026-09-28 T21 宿主检查面红账五件销账**(spec-gap W4):
- r6b(E4050 类持资源字段)/r6c(as[U64] 负源模 2^64)/r6d(W8050)/r6e(E4040)/r6f(E6010)
  五锚迁入主套件 05h/03l/05i/05j/05k;r6d/r6e 宿主先前行已绿(免改)。
- C 宿主三件补齐:E4050(D_CLASS 字段 Mutex/Channel/Sender/Receiver 检查)+
  wrap_int U64 修(bits==64 && us 时掩码 2^64-1——此前 bits>=64 直通透传,负源 -1 经
  __int128 全宽无符号读出 = 2^128-1)+E6010(ceval_21 迷你树走查求值器:字面量/ident/
  算术/比较/While/Let/Return/Assign/comptime fn 调用;步数>1200 → E6010;
  ST_ASSIGN 补 env 绑定更新;depth 跟踪移除——只增不减致 35 轮假爆,步数已兜底)。
- E4050 消息对齐自举(去「需确定性释放的」——msg 子串锚一致)。
- suite 94/94 双线;cargo 14/0;meta/db/net 基线全维持。

**2026-09-28 T22+T23 宿主运行面+e2e 红账核销**(spec-gap W4):
- T22:r4d/r6a 两锚**已在前批迁主套件**(03k_u64_overflow.panic / 05h_drop_panic_unwind.panic)
  且双线绿(09-15 宿主 fits 上界+rt_panic_unwind 落地时已修);COVERAGE §172-173 红账
  条目系过期登记,本次核销。
- T23:e2e 缺声明 6 红**已在前批全清**(09-25 深夜 P1b 契约补全——use 清单全量请求);
  http 110/1(唯一红=frm_auth_a_jwt interp OOM,T31 arena 债);OTLP send 裸 socket 形
  维持在册。

**2026-09-28 T20 deterministic+内容寻址缓存落库**(spec-gap W4;§10.3):
- --deterministic CLI 旋钮(ctc.sh):设 CTRON_RT_SEED=1 → 种子化弹出序+单 worker
  (机制已有 ctron_rt.c:1010-1027;旋钮只做 env 传播);并发夹具同种子三跑一致实证。
- 内容寻址缓存(ctc.sh emit 臂):源文件哈希+编译器 cc_emit.ct 哈希前 12 位 = 缓存 key
  (防编译器升级后陈旧缓存);.cache/emit/<key>.c 命中即复用,免重发射;未命中发射后入缓。
  同源双跑产物逐字节一致实证(cmp 过)。
- suite 94/94×2;net 17/17;fmt parity 479/0 全维持。
- 登记:缓存命中不重跑 build.sh(编译器源变更即 cc_emit hash 变 → 自动 miss);
  跨机复用需迁移 .cache 目录(v1 本机口径)。

**2026-09-28 T18 列级精确 span 落库**(spec-gap W4;§10.2):
- **解析器诊断(pdiags)入 JSON 面**:json_diag_lc(LINE:COL 解析版)——精确 line_start/
  col_start/line_end/col_end(列级 1-based);JSON 早退路径补(chk_fmt 提前初始化至 pdiags
  检查前——此前 pdiags 早退恒走文本面)。验证:`let x = ;` → E1001 {line:2, col:13}。
- **sem 诊断行号保持**:@LINE 机制既有;列=1 近似(v1 待 AST 列标注贯通)。
- 已知限制在册:部分 sem 诊断行号 nline() 对嵌套 Binary 节点返回垃圾值(01c 的 E1001
  chain 检查 60100140——预存在,AST 尾槽行号戳覆盖面不足所致);sem 列=1。
- suite 94/94 双线维持。

**2026-09-28 T19 fix-it 首批落库**(spec-gap W4;§10.1):
- json_diag fixes 产出:按码生成修复建议——E3030(replace edit:static var→let,
  span 精确 line/col)、E2020(note:检查拼写或加 use)、W8010(note:改值字段或 Box)。
- 验证:E3030 → fixes=[{title, edits:[{kind:replace, span:{L1:C1-11}, text:"let"}]}]。
- 契约:edits.kind ∈ replace|insert|delete;span 1-based(§10.2 冻结 schema)。
- suite 94/94 双线维持;meta 过。

**2026-09-29 T25 &Trait 发射侧动态分发落库**(spec-gap W5;§3.5;T24 ABI 实施):
- 四步路径全通:①型码(&Trait → t:<Trait> → ct_obj 胖指针 {data,vtable})②vtable 常量
  发射(trait 方法槽 struct + thunk 包装(具体型↔void* 桥接)+ static const 常量,仅
  &Trait 参数位引用的 trait 产 vtable——Drop/UFCS 走既有路径不干扰)③构造位(调用点
  具体型实参 → GNU 语句表达式装箱:堆盒+vt 取址)④调用位(接收者 t: 码 → vtable 间接
  跳转 `((vt_T*)obj.vtable)->m(obj.data)`)。
- **&Trait 引用门**(ct_trait_used_as_ref):仅 fn 形参位出现 `&Trait` 的 trait 产
  vtable——net 域包 7 个 trait/impl(Net/StdNet 等)无 &Trait 参数位引用 → 不产
  vtable → 零干扰(net 17/17 维持);Drop impl 有专属路径(ct_drop_fn)天然隔离。
- 方法体发射:vtable emit 内联 standalone fn(t_<Type>__<method>,self 绑定具体型)
  ——thunk 消费;既有 pass1/pass2 Impl 处理不受扰。
- 验证:/tmp/trait_obj.ct(Clock/FakeClock/elapsed_since&Clock)发射→cc→run rc=0
  (100-50=50 断言过=动态分发正确);07_capabilities interp 侧本即绿。
- suite 94/94 双线;net 17/17;db 44/10 基线;cargo 14/0(顺手清 r6c 过期引用);
  meta ✓;fmt parity 479/0。
- 登记:发射面限定=非泛型 trait + 方法返回 ct_i(标量);prop getter 槽/泛型 trait
  对象/超 trait 前缀=v2(设计文档在案)。

**2026-09-29 T26 能力对象注入形态归位落库**(spec-gap W5;§8.1/§11.2;R-P2b 两锚翻转):
- **r2b_fs_fake/r2b_env 翻转**:锚迁主套件 `07a_cap_fs_inject`(Fs 能力 trait + MemFs
  fake 注入 + FsError message + @derive(Error) class 声明面)/`07b_cap_env`
  (Env.system/get/args);roadmap_suite 删两行,语料删除(翻转协议)。
- **std/fs.ct**:能力面入 std——`trait Fs: Cap`(read_to_string/write/exists,
  Result[String/FVoid, FsError] 错误面)+ `pub struct FsError { message }`。
- **loader**:Trait 声明接口面恒可见(v0 无 pub trait 语法位;跨模块 impl/参数引用
  须先可导入,§3.4/§8.1);`use std.process.{Env}` 前奏符号冗余导入放行(T17 先例)。
- **@+class 解析路由修复**:`@derive(...)` 前缀 class 曾坠 p_fn 段错误(parse_decl
  @ 分支只路由 struct/enum/trait);Class 节点尾槽 Drvs(消费方按 [0] 标签扫不受扰)。
- **Env 进程环境能力对象**(§8.1;07b):前奏命名空间形(Env.system()→句柄;
  get→Option[Str],缺失=None;args→List[Str])。自举:eval NS 路由 + ctron_prog_args_
  n/at 运行时底座(发射模板 main 入口寄存 argc/argv;编译器自举经 extern 直调破鸡生蛋);
  C 宿主:rt v_ns("Env")+ EnvHandle 分发 + main 寄存;R 线:Value::EnvHandle +
  std::env 同面。**Env 发射面=interp 三线先行**(stdweb.dom 同族),用户程序发射
  Env 方法列登记随 stdweb 真实化批次。
- **发射 vtable 全签名(T25 v1 面=恒 ct_i 单 self 槽的扩展)**:槽=trait 声明位真返回
  码+非 self 形参;thunk 桥接(真返回,void 无 return;形参名保留原名=方法体 t_<原名>
  引用);调用位非接收者实参按声明序追加。net 真窗 `tests/net/cap_inject`:FakeNet
  (struct,确定性时钟)与 StdNet 同 &Net 槽注入,probe I32/now_ns I64/sleep_ms 带参
  void 三槽形,双矩阵 18/18。
- **R 线前奏近似**:Fs/Net trait 方法签名 + FsError(struct)/Env 承载于 register_
  prelude(单文件 check 无 loader;**面与 std/fs.ct、net.ct 真源同步纪律**——std 面
  变更须同步此处,07a/07b 行为锚);trait_has_method 扩查前奏型表;Prelude 型 defs
  字段可读(FsError.message)。
- 登记:①db 门面 trait 化缓期——现能力面仅 dsn 纯函数+协议半层,连接面(P5 后续)
  落地才有真能力方法,提前造 probe trait=死 trait(T15 裁决反例);②class 字面量
  发射全缺(StructLit 非值类型 panic)——W6/GC 相邻债,发射臂 fake 注入=struct 承载;
  ③`use std.fs.Fs` 点式符号导入 loader 未实现(§2.5 主形态;现按在库先例取括号组
  导入形,点式 loader 面独立登记);④测试套 96/96 双线;modules use_alias_nat(自举
  E2020.use.miss 消息不含 use.nat 锚码)/dup_static(宿主)两红=HEAD 预存(stash
  实证),非本批引入;tests/ffi/arr_field(机刷泳道 WIP,无 Ctron.ctcl)堵 meta_check
  门——非己债登记。

## §web 2c 次波增量(web 泳道,2026-09-28)

- **compress_mw(fb0de38)**:gzip 响应压缩(http/enc 薄收编)——四门(CE 已带→协商→阈值→文本族)+ CL 原位换值 + Vary;**勿裸上线闸在册**:gzip 成员恒含 NUL 而 NUL 过 Str 即失(活证 utf8_enc(0).len==0/"AB"+NUL+"CD"=="ABCD")→ U+0100 载体形(单射无损+CRC 兜底,评审证实)wire 面待 render 咽喉解码或 L4 Bytes;emit 臂 e2e 4096→98B + python gzip.decompress 差分在案
- **guard.ct(9bf1e64)**:csrf 双提交(frm/csrf 薄收编,写方法 cookie vs form 令牌,败 403,通过面恒发新 cookie)+ 令牌桶限流(frm/limit 薄收编,per-key=XFF 首段[可伪造=v1 边界],in-flight shed 503,时钟注入可测,键表满 fail-open 纯面钉死);Mutex[List[BktEnt]] 原位替换写回经 429 断言自证
- **门**:十文件 check OK + 裸跑 rc=0(mw/guard 走 canonical bin/ctron-cc + CTRON_STDPATH;seed 宿主 crypto 闭包挂死在册);suite 94/94+94/94(基线随上游演进)
- **指针**:L6/L7=T54/T55 候选入册 spec-gap 计划(待用户裁决);压缩线两条件(x_web_compress 夹具+wire 形决策)不得滞留

**2026-09-29 T29 切片1:M1 tracing GC 保守根先行落库**(spec-gap W6;§6.2/T28 M1 契约):
- 发射运行时 mark-sweep:超级块刻切+头{sz,mk}+空闲链+空块 free;根=机器栈[帧址,
  main 基]+chan 环+mcell 注册表+bump 区+载荷传播(list 魔数跟扫 items);list_new/Box/
  Lu 盒入 GC 堆;spawn 置并发退避(任务栈不扫,自动回落 bump);CTRON_GC=1 开启,
  默认 bump(浸泡后翻默认);阈值 CTRON_GC_THRESHOLD(8MB)。
- 泄漏锚 `tests/gc/cycle_reclaim`(gc 泳道新立):自引用 List 环 200 轮,collect 后
  live bytes 回落断言;双档 2/2。观测口 ct_gc_live_bytes/ct_gc_collect。
- 验证:锚 2/2+suite 默认/GC 档 96/96×2+net 双矩阵 18/18×2+smoke 155/0;decl 422。
- 登记(设计注记,任务卡保守扫描条款):①保守标记不保留纯内部指针/静态 C 全局不扫
  (M1.5 精确帧位图=契约 §6.3 替换);②coro/并发档恒 bump(契约注记在案);③闭包 env/
  view lane/字符串留 bump(M2 面);④S2a/语义零变更(解释臂无 GC,双臂一致)。
- 余债:peer 本地链未含远端 T26 合并(锁 420 vs 436);本片在 t29-gc-m1 分支待整合。

**2026-09-30 T29 切片2:db GC 档验收+翻面证伪**(spec-gap W6;T29 验收面收口):
- db GC 矩阵:db/run.sh 编译器自身臂 `env -u CTRON_GC` 豁免(自举编译器巨型 bump
  arena 经 env 继承激活 GC → 保守扫描近挂死实证,46min CPU 病灶);GC=1 臂与默认臂
  **同红集(40/14)**=GC 无害性证明(新增红=peer 库根重构 pg_* 符号 E2020×5+种子
  漂移守卫路径陈旧×5,在册非 GC 债)。
- **GC 默认档翻面证伪推迟**:同上病理;翻面前置=arena 界定/编译器自指豁免或精确
  帧位图(切片3)。CTRON_GC 维持显式 opt-in。
- 验收:T29 卡三条全过(泄漏锚/suite off 双档/net+db GC 档),卡转 ✅(切片3 增强另册)。

**T55 销账(2026-09-30,提交 7590794)**:值位置闭包捕获原生臂落地——03j emit 同病顺带翻转(基线 2 errors→`mw:/x`),探针 t55_probe/t55_mw_smoke 双臂绿,suite 98/98×2;创建时快照边界+T27 注记在案(spec §9 L7)。**T27-B 收口(2026-10-02)**:快照边界的语义地位由 T27 裁决判定 = 规范行为(创建时拷贝,外层再赋值不可见,§4.7 终态)——不再是实现限制;T55 残余疑义销案。

**2026-09-30 T30 into_gc 深拷真实化落库**(spec-gap W6;§6.3):
- 发射侧 from 恒等 → 真深拷:let 构造位绑 #elem/#elemsz/#ielem(List[T]/arena.list[T];
  Option/Result #elem 先例),into_gc 位 ct_ig_l(src,mode,esz,imode) 静态分派
  (0=标量槽/1=Str strdup/2=嵌套表一层递归/3=struct 盒 memcpy);无绑定位恒等回退(v1 兼容,
  GC=off 档同语义=拷贝目标 arena 化,隔离不变)。
- C 宿主嵌套别名修复:list_clone_deep 内部对 V_LIST 槽递归(g[0].push 别名污染 inner=2!=1
  实证);clone_val 通用口保持 List=引用共享语义不动(初版误改通用口→conc 族 15 红,
  smoke 实证后回退收窄——教训:clone_val 是宿主全量克隆口,语义改动只准入 into_gc 专用径)。
- 新锚 05i_into_gc_deep_copy(int/Str 平坦+出 fn 域存活+双向独立)interp/emit×
  默认/GC=1 四象限绿;suite 99/99 双线(含新锚)。
- 登记三笔:①05g emit 臂红=存量 own 块尾 return void 发射缺口(A/B 基线实证,非本批);
  ②嵌套 List 发射读侧 int 槽 strcmp(0x5) 崩=lane 读型不对称存量(嵌套拷贝本身双侧通,
  锚裁剪登记);③peer 库根重构 suite.py 两处 stdpath 陈旧(主循环裸跑/modules 段),
  lib/std 修正后 99/99 复绿——根因是 03m/07a 两个 std 导入件红而 CI 前次未及跑本批。

**2026-10-01 T31 探路判决:解释器堆回收两路证伪,M1.5 定为唯一健全路径**(spec-gap W6;§6.3):
- 卡面前提复核:114× 已被 S1 表示换代销至 4×(eval-repr-overhaul 收官注;loop 810→140
  次/迭代)——本卡剩余真债 = 无回收致内存无界。实证:frm_auth_a_jwt interp 臂
  **41GB 峰值/136s(rc=0 硬扛过,CI 小内存机即 OOM=唯一红根因)**,约 3 亿分配×均 128B;
  76s sys 即 41GB 零填页缺页税——回收同时是时间债。
- 路线①帧出栈重置(卡面原案)证伪:本运行时 Str/List 全指针共享(别名即语义),
  逃逸值无法拷贝晋升(拷贝破坏引用语义);「帧内分配不出帧存活」点审计在本架构内
  不可满足。卡面坑位预警成立且更深。
- 路线②GC=1 委托(ctron_amalloc→ct_gc_alloc)证伪两连:纯委托在 T29 首适应分配器上
  **漫步退化直接挂死**(a_body 90s 零产出;sample 100% 热于 free-list 走链——旧 GC 档仅
  list 头进 GC 堆=尺寸均一,首适应恒 O(1),委托后尺寸多样化即死);补尺寸桶空闲链后
  CPU 痊愈(a_body 31s rc=0)但 (a) RSS 4GB——深栈树遍历的陈旧栈字保守伪标钉死海量
  死对象,(b) CTRON_GC_THRESHOLD=1MB 强制真收集时 **SIGSEGV**——保守标记把内部指针
  载荷当对象头读(hdr[0]=8 字节文本→巨型 mkrange 野扫/伪标)。对照实验:纯委托
  (无桶无自适应)同挂死,排除新码嫌疑;病根=T29 保守根集在深栈程序不稳,M1 设计
  前提(浅栈服务型程序)越界即破。
- 判决:解释器堆回收唯一健全路径 = GC 契约 §6.3 **M1.5 精确帧位图(切片3)**;
  T31 内存债随其销,frm_auth_a_jwt 红账维持在册、解锁条件 = M1.5。本会话落库:
  ①T29 潜在 bug 硬化一件——GC 空闲链复用路径 ct_gc_since 计数缺失(原只在新建
  超级块计数→稳态复用型程序永不触发收集),补一行;net GC 档矩阵 18/0 复验无害
  (db GC 档维持 T29 时点验收口径:库根重构在册红静默,CI 九门不含 db)。
  ②真债量化 + 深栈保守根集不稳实证入册,为 M1.5 优先级依据。③T32(GC 门禁/翻面)
  前置同理改写为「M1.5 后」,保守档翻面对深栈程序实证不安全。

**2026-10-02 T44 own ±5% 性能门禁落库**(spec-gap W9;§9.4 P3 出口硬指标):
- tests/lang/bench 第一族(基准设施在册缺 lang kernel 族,本件补位):src/main.ct
  [k1=Lehmer LCG 纯算术核(48271/2147483647 全程 <2^63,零溢出歧义——Ctron/C 两侧
  digest 可对拍的先决条件;无位算符纪律)+k2=每轮 16 元 List 填充(arena 面)]
  + src/ref.c C 同构对照(k2 侧惯用 C=栈缓冲复用)+ bench.sh(digest 硬门=
  ctron emit 产物输出 vs C 逐字节一致 + ×3 取 min + CTRON_EMIT 覆盖口
  [http/run.sh 同款]);挂 ci.sh[7/9](T32 gc/bench 同段连跑)。
- 首版实测(darwin arm64 -O1 同机):**总比值 1.161 WARN**[ctron 72ms vs C 62ms]
  ——登记即交付(T32 先例)。**k1 分解归因:63 vs 67ms = 比值 0.94 ≤1.05 达标
  ——P3 代码质量面(发射 1:1 热路径)过线**;总比值超标全在 k2 allocator 税
  (每轮 List vs C 复用栈缓冲)——热点回切建议=复用 List/定长数组惯用法,
  等 list_set/定长数组惯形成熟后门禁可硬化。
- 脚本坑(已修):bench 目录三层(tests/lang/bench/)而 T32 两层(tests/gc/),
  ROOT 上溯层数差一层——首跑「缺编译器二进制」假象真因,非 bin 缺席。

**2026-10-02 T48 CTCL 清单硬切收尾落库**(spec-gap W9;§2.7 修订注兑现;设计 2026-09-16-config-language-v1.md):
- 实况盘点:三线解析器宿主(pkg.c ctcl_load,E5040–5050 冻结码面)与 R 线(check.rs 同码
  族)早已就绪,自举 caps 检测已读 .ctcl——本件实为收尾三件:①tests/ffi 18 份遗留
  .toml 机械迁移(pkg 块形,name 保号)→**全仓 Ctron.toml 清零(104 份全 CTCL)**;
  ②ctron 安装器硬切:new 仅产 .ctcl/build 项目模式读 .ctcl(name 提取兼容缩进)/
  遗留 .toml fail-closed 拒构+迁移诊断/help 文案三处;③三线迁移诊断逐字一致:
  "Ctron.toml: E5040 清单格式已硬切 CTCL(T48):检测到遗留 Ctron.toml——请迁移为
  Ctron.ctcl:pkg { manifest_version = 1, ... }"(pkg.c pkg_load/parse_pkg.ct
  pkg_check_caps[read_file 缺失回落 Some("") 故以非空判定]/main.rs 包级检查三处)。
- 规范三注翻面:§2.7 标题与修订注改 CTCL 完成时态/README 登记同步/08-effects
  comptime 注销账(budget_ms 键名维持声明位,改名留待预算执行实装);sync_site_spec
  全量同步 --check 无漂移。
- 门禁:ctron_smoke 25/25(含双新探针:new 不再产 .toml/遗留 .toml 迁移诊断拒构)+
  suite 99/99+ffi 33/0(18 份迁移零破坏)+cargo check 0(警告预存)。
- 中途异常登记:native.sh 重建中 seed 进程遭外部 SIGTERM(疑对端清场误伤)——产物
  完整性经 suite/ctron_smoke 全绿证伪,非本批债。
**2026-10-02 T53 实施中断落账**(spec-gap W9;§12.6 前置工项):
- 病灶实证:emit Result/Option 载荷——F64 经 int64 槽无位语义(写 `(ct_i)(2.5)` 截断、
  读端 let 推断 i32 双截;I64 直槽本已 64 位,在册「2^63-1 读回 -1」待另证路径)。
- 方案已写就:ct_f2i/ct_i2f memcpy 位转助手+ct_res_wrap f/g 位承载+#prel 载荷码侧条目
  (静态缓存表 RPC_N/C)+expect/or 读端 ct_i2f 还原——注解 let 消费形
  (`let v: F64 = r.expect("x")`)信息链已证可达。
- 卡点:自编译期宿主静默崩(cc_run emit 时宿主 rt 死,无诊断输出);二分定位到 trans_stmt
  绑定块(禁用即愈),静态缓存表/节点抽取与宿主旧值模型的型别二分(N 节点 lane 串 vs 表)
  未及定位。工作树干净回退,suite 99/99 复绿。
- 复开注记:优先在 S1 Val 平面重试(旧宿主值模型是根因域);json JNum/JReal 回切随芯。

**2026-10-02 T53 ✅ 收官落账**(同日复开,S1 平面即愈;上轮 #prel 助手表方案弃用):
- 芯先行:wrap 写侧+or 双读侧由 a000a6ad(P0-A)落库;本件补齐五病灶——expect 静态型别
  硬编码 "i"(trans_ty 两处,推断 let int32 截 I64 根)/expect 发射无位还原/语句形
  match-Ok `int32_t` 硬编码行(trans_stmt okIx 臂;match-R 发射共四路径逐条排摸)/
  Try 两语句位 `(double)` 值直转/json (h) 绕行面。载荷码统一 ct_opt_elem_expr 恢复,
  f 码内联 union 位双关(零新符号零 #prel,三次自举全过——宿主静默崩确证随去 cx 化消失)。
- json JNum/JReal 回切:jget_* Result 面唯一 pub 数值面,jnum_*/jreal_*/jn_*/jr_* 摘除,
  jv_*/jk_* 内迁 jget_*;pkgs/web/json.ct jd_i64、lib/http/frm/body.ct 注释同步。
- 验收:探针 14 项四读侧×I64(2^63 双界)/F64 双臂绿(基线 emit bad=9 清零);
  锚=02_option_result.ct 两 test 块+json_fidelity/corpus/e_t53_payload64.ct 双臂;
  json_fidelity 13/13、suite 99/99 双列、web_todo 原生 92 裸跑 rc=0(big=5000000000
  大值载荷)、http/run 105 过(6 红=基线同红:x_client_e2e/sse_ws e2e 五件 origin/main
  预存,x_ws_e2e 主树 bin 对拍实证;gzip 族 vendored miniz 补建后转绿)。

**2026-10-02 非己债登记:fx_genrec_neg.ct emit 死循环(main 在案,P0-A′/T53 域)**:
- 症状:ctc.sh emit compiler/test/fx_genrec_neg.ct → seed 解释发射无限循环(100% CPU
  不终止;单案 timeout 100s 复现,产物 0 字节)。二分实证:stash 本线全部改动后同案
  同挂——非 T48 线债。上一轮 smoke(合并前树)该案 emit 臂曾以 2 红完成,当前树病理化。
- 影响:smoke [3/9] emit 臂在此案阻塞(门禁不可完成);CI 将以超时显红。
- 归属:P0-A′ 泛型特化 seen 集/自编译期宿主静默崩(trans_stmt 绑定块二分在案,
  36c9dda2 值模型型别二分根因域)——归 P0-A′/T53 泳道,本线不越界修。

**原生 e2e 翻账(2026-10-01,f50ff59b)**:web_todo serve_net 原生全链绿——未登录 /app 303 → POST /login(demo/demo123)303+HMAC Set-Cookie → 带 cookie /app 200(真 todo 页)→ POST /__shutdown 200 排空 SERVER_RC=0;根因双头(陈旧 bin/ctron-emit + P-化 UFCS 泛型方法臂裸 t_<m> 12 未定义)由 72f3a26d typedef 前置+f50ff59b 接收者实例码型参绑定收口;评审 MERGE-READY(独立复跑 99/99×2)。T54/T55 原生臂自此全账翻正。
**2026-10-02 T34 最小符合径落地**(spec-gap W7;§7.1):
- 「大栈+触顶 panic」字面闭环:rt 栈缺省已 1MB(CTRON_RT_STACK_KB 4–1024 可配,存量)
  + guard 页(存量)+ **新增溢出诊断链**:sigaltstack(主线程+每 worker 入口)+SIGSEGV/
  SIGBUS 双号 handler+g_all 定位 fault 所属协程栈→明确诊断 "task stack overflow"
  后终止(rc=101);非协程栈(主线程)链回默认处置。
- 新锚 tests/w7/stack_ovf(w7 泳道新立):64KB 小栈钉死+深递归 4KB 活帧→触顶诊断。
- 坑位实录:①SA_ONSTACK 线程局部,worker 入口须各自安装(缺=handler 在已打穿栈上跑
  →SIGILL);②darwin guard 命中 SIGBUS/SIGSEGV 双号;③SROA 删未全用数组,锚须全槽
  循环写;④尾调用 -O1 转迭代,递归压栈须非尾形。
- net 双矩阵 18/18;suite 99/99。指针修复/增长拷贝/work-stealing 列 P9 正案另册。

**2026-10-02 T34 正案收官:work-stealing 落码 + 可增长连续栈判决**(spec-gap W7;§7.1/§7.10):
- **work-stealing(§7.1 调度口径兑现)**:ctron_rt 就绪结构两级化——①全局 FIFO 链
  (P2 原形;裸线程侧推入[reactor/裸 wake/main spawn]+本地溢出泼回;种子模式唯一面)
  ②每 worker 环形双端本地队(RT_LQ_CAP 256;owner 尾端 push/pop=LIFO[yield 回队/定时器
  醒/wake/spawn 收口本 worker,吃满局部性];他 worker 偷头端=最老者,FIFO 利广依赖)。
  饥饿 worker 弹出链=本地→全局→邻位起轮转偷取(取一即返,下轮继续=实质虹吸)。
- 锁纪律:两级队全部 G 内——不变量 I/II 的 G 串行化论证(completer 定稿与 pop 同临界区/
  出队即认领)原样成立,零新增无锁不变量;本件非吞吐重构,G 竞争面与改前同。
- 溢出:本地队满 256 → 老的一半按序泼回全局队(先后序保持)。踢醒 P3-A 口径不变:
  仅非 worker 推全局队踢;worker 推本地队不踢(惊动同侪=偷活噪声),饥饿者 ≤160µs cv
  自醒后 steal 兜底。种子模式(P2-E)全量旁路:推全局/弹 LCG/单 worker ⇒ 确定性契约
  逐字节不变。
- 新锚 tests/w7/ws_steal(w7/run.sh 增 expect_ok 期望类):单点蓄 burst 形态学——8 接收者
  先 park 满 30ms 窗→发送者 S 一次 send 8 值(唤醒经 coro_wake 落 S 之 worker 的本地队)
  →S 立入 10M 次纯算术忙循环(不 yield 不 park)钉住本 worker→他 worker 本地/全局全空,
  唯一活路=steal。三断言 count=8/sum=36/ctron_rt_steals()>0;10 遍连跑+10 遍满载
  (load 6–15 对端泳道并行)全绿。
- **可增长连续栈判决(入册 2026-09-26-server-p9-address-audit.md 终节)**:A/B 双阻塞
  于发射器机器——B(拷贝式)需三件机器:emit 栈帧布局表+drop 注册表等全局根区间改写+
  **callee-saved 寄存器镜像修复(新增:x19–x28/rbx,r12–r15 可持栈对象地址,无栈图不可修,
  保守区间改写对地址值整数不健全[FFI 面假阳性])**;A(分段+钉住)指针安全性成立但
  guard 续跑同需发射器序言检查(叶帧搬移后 arm64 尾声 ldp 相对回卷落新段≠调用方旧段帧,
  sp 相对寻址局部全踩)。退路「1MB 大栈+guard+触顶诊断」(a77f141f)= §7.1 字面终形
  (上限可配+触顶 panic 全量兑现,未承诺起步尺寸);解锁条件=发射器 morestack 序言机器
  (触发:C100K+ 实测栈内存瓶颈或 lane 预算化到顶)。§7.10 过渡口径注记自此收窄为
  调度面(本次随 work-stealing 落地一并合龙)。
- 观测口:ctron_rt_steals() 加法导出(冻结接口不动;种子模式恒 0)。
- 回归:w7 双锚 2/2;net 双矩阵 18/18;coro_det 种子重放 101/101;rt_core_smoke 绿
  (ns/yield 90ns 门内);suite 99/99 双臂;smoke 161/2(双红=fx_conc_parallel/fx_cloval,
  T33 发射臂 emitter OOB 在册[8ac3d1da「干净树复现」+d23f6ec0 修法定稿],本泳道 diff
  仅 rt C+w7 夹具构造性无关)。
- 坑位:①ready_pop 种子路径勿死代码化(worker_pop 须显式分流;-Wunused 站岗)②锚断言禁
  `;`(E1001 坑 20 重犯)③bench 同机差分对负载极敏感:对端双自举+gc 测试满载期比值
  0.92→1.85 漂移,静默窗口纪律前置于门禁判读。
- 验收回填:c10k 满额 PASS(N=10000/10000 回显全绿+fd 泄漏判据 delta=0,connect 0.6s/
  total 0.9s);bench 门禁二 ns/yield 63/63/48ns(历史最好带,门 ≤200);**门禁三 A/B
  差分定案:同一树 stash 交错两轮,coro 绝对值改前 1419115/1447175µs vs 改后
  1418912/1453851µs(差 ≤0.5%),比值三红(1.21–1.27)在原版 rt 同等复现=本机窗口
  P1 臂提速环境属性(p1-vs-C 同轮 1.13↔0.97 摆动),非本件回归;本件绿轮比值
  1.030/1.039 优于在册历史带(1.025–1.128)**。

**2026-10-02 T35:分层 stdlib 机制(core < alloc < std)落库**(spec-gap W8;§9 档位矩阵):
- 机制核(7550f8bc):清单 pkg 块 `tier` 键(缺省 std;E5040.tier 值域 fail-closed)+
  模块头注 `//@ tier:` 提取(首注释块;缺标注不设门渐进承诺)+ use 点 `E3040.tier`
  档位门一次诊断加载即止(bare 档钳 core;`pkg_load_use_t` 独立入口避碰
  driver_emit/doc 调用面)+ schema regkey `pkg.tier`。修复前实证:bare+use alloc 模块
  = std 内部 E3040 洪水(诊断指向 std 源行,归因错乱)→ 现 use 点一响即止。
- 首批标注(425a4374):26 模块头注 core 5[math/hash/log/opt/pb]/alloc 16/std 5
  [fs/time/process/rand/uuid],分配面+OS 面双轴;stdpkg/examples 副本同步
  (examples/ctgrep、ctwc、ctwf 三例 vendored 副本顺势转正入库)。
- 锚(332b423f/41fc493a):smoke 3c2 段 9 断言——seed 臂夹具本地 std 七(alloc/std
  档负例+值域+bare 钳门双跑+core 正例 full/bare 双证)+ native 臂真库二(core 五模块
  bare 体检自证=core 档音遍性[tier_real_pos decls=75 零诊断];tier=core use std.str
  单响)。decl 锁 439→443 随批申报(+4 fn)。
- 门:suite 99/99 双臂;smoke 170 ok(余 3 红=conc 双发射红[T33 在册 8ac3d1da]+
  Rust iter 清账[基线复现既有翻绿,账面属 R 线];meta_check cbox×3=peer 在飞缺清单,
  均构造性无关,基线 worktree 实证)。
- 坑位:①共享检出树多泳道并发,未提交编辑会被 peer `git stash` 连带卷走(Task 1 四件
  被卷入 t31wip stash;恢复=`git checkout <stash> -- <my paths>` 单边提取,勿整体
  pop——stash 混对方 trans_* WIP);②selective merge 连带 Test 块亦吃 bare 体检查,
  core 档标注前须核其自带 test 零分配(五模块实证 0 命中);③vendored examples std 有
  硬链接形态(ctslink/web_todo),cp 报 identical 即已共享。
- 登记债:三线 parity(pkg.c 清单 tier 键/sem.c bare 钳口径/main.rs Manifest.tier+
  R 线检查面)、域包 tier 标注(随 registry)、emit/doc 驱动 prof 接线(随 emit seam)、
  单文件裸跑 bare(无清单时 use 门不设,仅体检查面)。

**2026-10-03 T27-B:闭包捕获语义收口·原生臂复验收官(r3a 双臂全绿)**(spec-gap W5;
T27 终态=创建时拷贝转正的最后一块:锚头注挂着的「原生臂红」翻面):
- 归因(干净基线 worktree 逐提交二分):破窗=p0a 合并 46e11d70(绿→红单步),非在册
  「ct_emit_clo 卫兵错位」——r3a 面所经 ct_emit_clov 卫兵形态实证完好(单对 #ifndef
  包整 shim)。真红三连:①`ct_str_bsum` 判别位 I32 `h*31+b` 链溢出(p0a 把捕获名/型别码
  纳入哈希,4 字符捕获名 `base` 第 3 字符即破 I32;shim 名后缀 236033764 与
  bsum("x")→("n")→("i") 逐字吻合=实锤)→ 模归约 `(h % 69000000) * 31 + b`(最大中间值
  21.39 亿 < 2^31,确定性/双宿主一致);②闭包体尾槽与 return 位(#shim=2 早返臂)的
  with/with_mut 不走 ANF——`ct_expr` 恒拒「仅语句位」,eval 两侧皆合法 → `ct_clo_with_tail`
  助手(trans_conc)+ return 位前置(trans_stmt),预扫两处同助手通路面(eln 门控零输出);
  ③同 fn 多 with 同名形参双双内联 → C 顶层重定义(r3a bump = with_mut+with 双连)→
  ct_emit_with_stmt 发射整体入 C 作用域括号(rc 声明位在括号外,存活到消费点)。
- 验收:fx_cloval 发射+运行 107/105/25 对拍解释臂;探针四形(裸 with_mut 尾/return 位/
  test 内 r3a 形/普通 fn 双 with)全绿;r3a 发射+cc+运行 rc=0(断言全过)+负锚 E3070
  正响;suite 99/99;smoke 170/3(余 2 红=conc_parallel[T33 在册,bsum 修复后复现依旧,
  OOB 独立]/Rust iter 清账[R 线],皆基线非本件)。
- 连带三小修:①自举固定点段「两路发射产物分歧」=版本行竞态假红——两路各自 build.sh
  采样 git describe,机刷并发落库窗口内 HEAD 移动即两侧版本串不同(diff 仅此一行实证)
  → 门禁改版本行除外对拍(发射不变量是编译器逻辑非版本戳);②GATEDBG2 调试 println
  清退(trans_expr,p0a 合并遗留,污染一切含 fn 值调用的发射产物);③decl 锁 444→449
  随批申报(+1 本件 ct_clo_with_tail;+4=GUI-17/12 批已落库未申报,归 GUI 泳道补登记)。
- 坑位:①seed 侧 arithmetic gate(interp/emit 双宿主一致语义:中间值 128 位+fits 检查)
  会咬发射器自身代码——哈希/判别位类算术一律模归约或有界累加,勿裸写乘法链;②emit 的
  with ANF 是「逐位置接线」形态(语句/let/return 尾/闭包体尾各自一臂),新位置接入时
  预扫(pass-1)与实发(pass-2)都要过同一助手,漏预扫=中途 panic 截断发射产物;③
  T20 emit 缓存键含 build/cc_emit.ct 哈希但 build.sh 先于发射重跑,跨版本缓存条目
  (ANCHORVERSION 陈物)曾混入对拍——缓存目录清退后未复现,存疑登记。
- 登记债:with 在任意表达式位的 emit ANF 缺口(eval 合法;`return bump() + m.with(...)`
  形即炸「仅语句位」)——语句/let/return 尾/闭包体尾已全接,任意嵌套表达式位待 ANF
  提升机(capability 项);T33 parallel ct_emit_clo OOB 独立在册(bsum 修复后复现依旧,
  卫兵假说待 T33 作者复证)。


**2026-10-03 String 独立类型落库**(§3.8.1 前奏类型表成员;§3.1/§3.3/§3.6):
- 缺口实证:发射面 `String` 注解无类型码臂 → ct_ty_code 尾默认塌缩 `"i"`——注解位落
  `int32_t`,槽型/赋值/传参全错(p2 探针逐字:`int32_t t_owned = (const char*)(...)`);
  `.to_string()` 发射臂透传(零拷贝别名),与 sem 面(所有 to_string 记分配,E3040 双上下文
  已正确)及解释臂(产新值)三方分歧。
- v1 口径(独立面在 sem 键,码面同 Str):ct_ty_code 补 `String → "s"` 臂——C 表示与 Str
  同(const char*),独立性载于 sem 键 n:String(k_strish)+E2010 单向门(String→Str 隐式降格、
  Str→String 拒)+to_string 升格唯一路径。独立码 "S" 登记 v2:已被 ct_inst_encode 的 Str 槽
  字母表(I/S/B/L)占用,须连动单态化键两侧再立。
- to_string 发射面真分配:接收码 "s" → `ctron_byte_slice(x,0,strlen)`(与 concat 同
  ctron_amalloc 源,堆拷贝);接收码 "N"(内部节点引用)保持透传不动。
- 锚:03c 扩第三段(显式 String 注解端到端:声明/降格/contains/strcmp 面 ==/升格往返)——
  03c 原两段走无注解路径,两臂本就绿,新段钉住注解位防塌缩回归;新负锚 03c_str2string.neg.ct
  (变量+字面量双违例,各报 E2010)。
- 宿主对齐:C 参考宿主 sem.c 原为类别制保守子集(Str/String 同类 3),负锚在自举 cc 正确
  报 E2010 而宿主放行(suite 分歧清单立红灯)→ 宿主 ST_LET 判定位补 String 注解单向门
  (方法调用 derive_type NULL 保守面自然放行 to_string 升格;字面量/Str 变量初值报 E2010);
  make compiler-c 后双线一致。
- 探针:显式注解/无注解/struct 字段 String/fn 形参返回 String/成员面(contains/==/concat/
  println/char_len)双臂逐点;发射 C 逐行核(t_owned=const char*+真拷贝)。
- 验收:suite 100/100 双线(自举 cc+C 宿主);smoke 非全量 158 ok(3 红=decls 451[peer GUI-13
  在飞新增 fn 申报位]/conc_parallel[T33 在册]/Rust iter[清账在册]皆非本件);meta_check 4 红=
  cbox 族缺 Ctron.ctcl(T45 基线在册)。
- 登记债:①String 独立码面(v2,见上);②contains 发射 typeof 缺臂(trans_ty 无 arm →
  println 落 %d 打 "1",解释臂打 "true"——T09 面既有分歧,本件探针 p6 实证后登记);③
  fmt 返回型 Str vs 规范 String(§3.8.2;隐式降格下观察等价,随 fmt 面整备);④struct 派生
  谓词标量集含 Str 不含 String(§3.9.2,String 字段 struct 不满足 Show/Eq bound);⑤
  slice(range)/iter() 发射臂缺口(§3.8.2 其余成员,既有在册)。
**2026-10-03 T16 续波·惰性适配器面收口**(R-P3b 承诺余项;UFCS 链式续篇):
- **std/iter.ct v3**:适配器补 skip(位置平移)/take_while(前缀扫描)/rev(无状态重
  扫反读)/enumerate(Pair[I64,T] 索引对)/zip(同型 S 逐位 Pair[T,T])/chain(前段
  尽则重扫 a 长再读 b);终结子补 fold(I64 累计)/reduce(T? 空即 None)/foreach
  (无返回 fn 型 fn(T) 首证,返回计数 I64)/min/max(泛型 T 比较运算首证,稳定取
  先)/last/position(谓词→I64?)。数组 UFCS 入口同批补齐(包装 RawSeq 或快路径
  直终结,I32 单型口径承 T16);from_list(List[I32])→ListSeq[T] 接入 List。
- **探针四证(施工前)**:①泛型 trait impl for class(同文件 class)通,class var
  字段跨接收者拷贝持久(引用共享);②无返回 fn 型 `fn(T)` 文法可选返回位实装可用,
  闭包块体适配通;③trait 默认方法内泛型 T `<`/`>` 比较通;④泛型 struct 裸字面量
  推断(Pair[F,G])通。**前奏 class 直 impl 不挂**:`impl Seq[T, List[T]] for
  List[T]` 后 `xs.get(i)` 报「method:get」——内建类型方法派发表不含用户 trait impl,
  List 接入改 ListSeq 包装 struct(与 RawSeq 同构)。
- **接收者拷贝边界(interp 实证)**:class 实例存入 struct 字段即被拷贝——裸绑定
  直调 `p.get(0)` mutation 持久;经字段派发 `w.get(0)`(w.it 持同类实例)或再绑定
  `let q = w.it` 后调,mutation 皆丢。惰性观测的 class 计数法在 trait 链上不可用,
  锚改「有界拉动终局」证法(limit=20 亿 Counter × take(3) 瞬通,急切排空必超时)。
- **同型 zip/chain 裁决**:trait 二参方法 other 与 self 同适配器型 S(泛型方法位
  不引入新型参,零先例);数组入口双侧同包 RawSeq 故 xs.zip(ys)/xs.chain(ys) 直
  用,xs.take(3).chain(ys.take(2)) 同型链通;异型两侧(map 后 zip 裸数组)登记。
  rev/chain 的 get 为 O(n) 重扫/次(总量 O(n·m)),正确性优先,缓存化待 var-self
  写回语义落库。
- **min/max 名义**:与 std/math 二元 min/max 同名不同签(Ctron 无重载);use 显式
  导入制下同导两名才 E5030,方法调用形(xs.max())不走自由函数命名空间——锚仅导
  iter 名,双导冲突场景登记 hazard 不设门。
- **锚扩 iter_adapters**:原四段(T16)+续波八段(skip/take_while/rev+enumerate/
  zip/chain/fold+reduce+min+max/foreach+last+position/from_list+空面/惰性/自定义
  Seq 上新适配器);use 两行补新名。suite 主套件 99/99 双臂零新增红;modules 段
  17/18(dup_static/use_alias_nat=对端在飞/既有账);ffi cbox 三红 stash 归因
  =HEAD 基线固有(emit 臂 E2020 use cbox_* 未找到),非本件。smoke 158 ok/2 红:
  decls 实测 450 vs 锁 449=GUI-21(gui_scroll_add/scrolloff)编译声明已落库未申报,
  GUI 泳道债(主树在飞 smoke.sh 即在处理);conc_parallel=T33 在册——皆非本件。
- **登记债**:①发射臂 trait 方法调用/for-over-Iterator 未发射(T25 v2 域,T16 既
  有);②前奏 class(List)直 impl trait 派发不挂(本件新证,method:get)——包装
  struct 绕行,真接入待 eval 派发表扩;③class 实例经 struct 字段存储拷贝语义(本
  件新证)——与 W8010「浅拷贝共享类字段」lint 口径相悖,归 interp 值语义整备域
  裁决(共享 or 拷贝,两口径取一);④异型 zip/chain(泛型方法位新型参)v2。
**2026-10-02 T31 重开落库:M1.5 精确帧根集(S1+S2,分支 t31-m15)**(spec-gap W6;契约 §6.3):
- **S1 标记器硬化(ba73779a)**:超块单尺寸类+内部指针 O(1) 块起点归约+头双门+
  worklist 迭代标记——路线② SIGSEGV 两机制(载荷当头读野扫/递归标记栈爆)销账;
  新锚 tests/gc/deep_stack(4000 层深递归×帧驻 List×深栈内收集)双档绿。**意外红利:
  T32 bench 比值 10.28→1.000-1.150(§9.4 ≤1.15 目标首次达标)**——旧 first-fit 自由链
  走链+mkword 双倍超块扫描即 churn 主病灶(GC 689ms→20ms 级)。
- **S2 精确帧根集(槽注册制)**:契约 §6.3 编码实施注——原文「ct_fm_<fn> 位图常量+帧
  偏移」在本发射架构不可知(C 编译器定帧布局),实施为**槽注册制**:帧内联定长池
  pool[32],注册=写 pool[pn++](幂等重执行安全);链式槽节点方案否决(循环体重绑=
  自环挂死/无界增长,b2a 语料实证)。>32 槽或 break 跳过池重置 → ovf,该轮收集回退
  保守栈扫(健全性不破)。__thread ct_fp(pmap 8 线程推拉隔离);longjmp 跳过 cleanup
  →唯一落点 ct_shim_tramp 显式复位。发射面六类帧宿主(ct_fn/vtable 方法/drop/spec
  特化/闭包 shim/spawn shim/test 块)+绑定位注册+调用实参 ANF 提升(多实参求值悬窗
  封死);unbox 拷贝免注册(引用已被被拷贝存储覆盖)。GC 槽谓词=L/tlcode/Box/u:/R/
  E:/t:/P/p;标量与 bump/注册表族跳过。
- **验收**:gc/run 9/9 三档(on/off/**precise-only**=CTRON_GC_CONSERV=0 纯精确档,
  cycle_reclaim+deep_stack+新锚 precise_root 全绿=M1.5 主证);suite 99/99;net 18/18;
  smoke 160/3(3 红全同伴在册债);**A/B 帧税 +3.4%**(×3min 同机差分,自举发射
  326→337ms);gc/bench 比值 1.000。decl 锁 439→443(四助手随批申报)。
- **known-limitation(S4 前置登记)**:①gc-let 在循环体内→池按迭代递增→该 fn 退化
  ovf 保守回退(健全;池界重置=后续件);②vtable/成员调用等其余实参拼装面 ANF 化=
  后续件;③**a_jwt 红账仍未解锁**——interp 臂分配主体驻 bump arena(串/items 数组),
  精确帧只救 GC 堆 List 头;全解=分配位点翻面(items/串→GC 堆)+arena 根扫退役=S4。
- 坑位新增:①native.sh 不重拼接+管道尾 $? 假 0——改 src 必 build.sh→native.sh 裸跑
  看真 RC,采样二进制符号核新旧;②用户程序面 or2 无定义发射(t_or2 未声明),测试
  程序双 if 规避;③Ctron 字符串 `\}` 非法转义(E1001)四连踩——闭括号恒裸写;
  ④nline(st) 取节点末槽当行戳,非语句节点(For 末槽=体)传入即 byte_at(List) 炸
  ——注册后缀设计已整体废除(池式无后缀)。

**a_jwt 复测追记(2026-10-03)**:GC=off interp 臂 rc=0(路径环境修复后);GC=1+CONSERV=0
纯精确档 interp 臂 **22min CPU 未完终止**——interp 形态(let 在 eval 循环体内)池按迭代
递增→快速 ovf→每轮收集回退保守全扫,与 3 亿分配×高频收集相乘=O(N²) 退化。**判决:M1.5
对 emit 臂是免费票(帧税 +3.4%),对 interp 臂非免费票——a_jwt 红账解锁条件维持=S4
(池界重置机器+分配位点翻面+arena 根扫退役),精确帧为其健全性地基而非终点。**
## T52 插件沙箱 derive + lint(2026-10-03;§8.3/§10.6/§12.5)

- **协议面**:插件 = 普通 Ctron 包;清单 `plugin "kind.name"` 键控块(CTCL 注册表
  增殖,schema 已注册 tools/ctcl_manifest_schema.ctcl;ctcl_check keyed mutex 容缺
  修正=数据驱动化);接口包 `pkgs/ctron/plugin.ct`(DeriveInput/DeriveField/
  LintUnit/LintFn/LintDiag 六 struct;LintFn.line v1 恒 0=fn 节点无行号戳,行号
  精度列 v2);约定入口 `ctron_derive`/`ctron_lint`(plugin_load 入口参数化门)。
- **沙箱底座**(compiler/src/plugin.ct,新拼入 build.sh):plugin_load(独立 AST
  +use 合并+静态规模门 256KiB/512decl)+plugin_purity(FnExt extern 禁+sandbox
  白名单扫描(comp 表超集:加禁时钟/编组/动态链接+Env 面)+sem_cap 能力门复用
  E6020.sandbox)+plugin_call(call_decl_vals 直连;**经 call_id 分派链对插件形
  AST 段错误实证,根因未究在册**,直连同语义)。
- **derive 展开钩子**(三驱动 check/run/emit 的 pkg_load 后):内建集
  {Show,Eq,Error} v0 口径不动;非内建名查清单 derive.<名>(缺=E5060.nodecl);
  产物=合成源码文本重 parse 注入尾,约束门(禁 Use/test/@derive 递归=产品 gate)。
- **lint 管线**(check 驱动 sem 全绿后):LintUnit 声明级投影(fn 签名/stmts/
  depth 递归计;attrs 位 v1 恒空);codes 外码丢弃;W 级不置 rc。
- **官方样例**:derive_json(`@derive(Json)` → `fn to_json(self: T) -> Str` 自由
  fn/UFCS,**trait impl 形态弃用=发射臂在册缺口**;Str 转义 helper 名带类型)+
  lint_toolong(stmts>50 → W9001;第三方码纪律=清单 codes 先进表)。
- **锚**:tests/plugins/{json_demo(interp+emit 双臂+native 运行),lint_demo
  (W9001),no_decl(E5060),sandbox_escape(E6020 插件触 I/O)}+确定性双跑
  逐字一致;阶梯 tests/plugins/run.sh 六锚挂 ci.sh [5.5/9]。
- **登记债**:①执行期步数预算(E6010 复用)=v2:Global[T] 实证单绑定持久盒跨
  调用不共享,编译器源码无跨调用计数载体(eval 签名穿针属大手术;与 comptime
  v0→v1 同演进路径,死循环挂起为 v1 已知边界);②call_id 分派链插件形 AST 段
  错误根因;③LintFn 行号戳(p_fn2 无 nstamp,anm/stamp 尾槽序约束);④表达式级
  lint 面=HIR 暴露宽度 v2 裁决;⑤lint 诊断内容寻址缓存=T20 形态复用 v2;⑥
  derive 产物不支持型别的编译期诊断通道(现=空串回落 E5060.iface);⑦三线 parity
  (plugin 块 C/Rust 校验器+接口包 use 解析,与 T35 同口径);⑧attrs 位 AST 收集。
**2026-10-03 P9 栈面全量落地:morestack 序言机器(A′ 分段+调用窗回收+再入续跑)**(spec-gap T34 解锁条件拍板动刀;§7.1/§7.10):
- **解锁拍板**:T34 判决书单列解锁条件「发射器 morestack 序言检查」,触发条件
  (C100K+ 栈内存瓶颈/lane 预算化到顶)经用户令 2026-10-03 直接触发;计划
  docs/superpowers/plans/2026-10-03-p9-morestack-machine.md;worktree /tmp/ctron-p9ms
  先行(红线①),五片 S1-S5 逐 commit pathspec。
- **发射面(bin/ctron-emit)**:每个具体用户 fn 定义序言插栈限检查
  `ctron_rt_stk_low(est)`+失败臂(实参值拷入 f<i> 桶→`ctron_rt_stk_grow(thunk,&ar,&r)`
  →返回 r__)——零帧搬移、零栈图、零寄存器镜像修复,T34 判决的 B 案三件机器全部回避;
  泛型单态化特化位(ct_spec_call_emit)同批(盒槽形参特化跳过=登记债,margin 兜底);
  main/Test 块/闭包·spawn shim 不注入。est=256+96×(语句级 Let 步查 ct_ms_lets_b/e+形参)
  封顶 32768(粗估;真帧超估由 rt 侧 MARGIN+guard 触顶诊断兜底)。
  **门=CTRON_MORESTACK 发射期(env_get,CTRON_MONO_BUDGET 同款零注册税);缺省发射
  逐字节与旧形一致**(黄金对照实证,唯一差=ANCHORVERSION 行,设计明文豁免);
  weak 哑元同挂门。序言行经 `#mspro` env 位 ct_body 花括号后消费(#nobrace 先例)。
- **rt 面(ctron_rt.c)**:rt_coro 增当前栈界 stk_lo/stk_hi(随任务走,创建初始化)+
  增长段链+grow 交接槽;强钩子 stk_low(余量<est+RT_MS_MARGIN 2048 即告;裸线程/主栈
  恒 0)/stk_grow(mmap+guard 新段不入池不碰 G→rt_swap 切段跑 thunk→返回值拷回→
  **段随调用窗 free-on-return**)。切换 asm 零改动(rt_swap/rt_ctx_init 只加调用方)。
  链深上限 RT_MS_SEG_MAX 256 超限=触顶诊断同族 rc=101(纯失控保险;段容量×链深
  =深度上限,64KB 栈≈15 万层)。
- **设计修正两件(实施中实证)**:①「钉住」收紧为「段=调用窗生命周期」(go 1.2
  free-on-return):钉住使分段抖动(驻留旧段尾部的调用方循环调深函数,每轮一段)指数
  烧段必撞顶;free-on-return 下段上帧全数返回才 munmap,逃逸死帧地址与固定栈帧复用
  同契约 UB,跨 park 停车段存活(park 在窗内帧活),与计划稿的钉住语义差异在册。
  ②ms_tramp 必须先取 `c->ms_back` 快照再跑 thunk:窗口内嵌套 grow 覆写共享槽,迟读=
  外层 tramp 切回内层已出链帧重复 pop(double-free,lldb 野 free 回溯+反汇编实锤;
  probe.c 单层窗掩盖、嵌套窗 100% 复现——手工探针必须含嵌套窗形态)。
- 新锚 tests/w7/ms_grow(run.sh 专属块;同源程序四断言):OFF@64KB=触顶诊断 rc=101 /
  ON@64KB=2 万层深递归生长算完 rc=0 值正确 / 浅深度(6K)OFF@1MB 与 ON@64KB 输出
  逐字节一致(同形兼容)/ 生长态同种子双跑逐字节一致(确定性)。
- 门禁:net 双矩阵 18/18(缺省与 CTRON_MORESTACK=1 双轮,含 coro_det 同种子 101/101
  逐字节)/ w7 3/3 / rt_core_smoke ns/yield 95ns(门 ≤200)/ suite 99/99 双臂 /
  smoke 159/1(余红=fx_conc_parallel,T33 发射臂 OOB 在册;**decl 锁 449→450 随批
  正名:+1=GUI D 档十提交批未申报[pristine 二进制同读 450 实证非本件],归 GUI 泳道**)/
  **bench-cycle 机器开:全请求周期 ratio 1.026× ≤1.05 硬门 PASS(coro-vs-C 归档
  1.003×;每函数入口一次 weak 调用的热路径代价实测不可辨)+ no_alloc 稳态差分=0 PASS**/
  c10k 满额 PASS(N=10000,fd delta=0)。
- 坑位:①Ctron 源串 `\}` 非法转义 E1001(裸 `}` 合法;与「裸 { 须 \{」同族另一侧,
  自宿严格词法);②`var fn` 上下文关键字宽容(解析通过;仍改 fname 求稳);③native.sh
  仅 cc_run.ct 缺席才跑 build.sh,但 ctc.sh emit 自带 build.sh 再拼+T20 内容寻址缓存
  键含 cc_emit.ct 哈希——改编译器源后首跑静默失败时先查「语法错误被重定向进输出文件」
  (ctc.sh emit 的 HOST 诊断走 stdout→OUTC);④夹具递归必须真终止+线性(链深锚用指数
  调用树会把生长对拍变成 3^n 烧机)。
- 登记债:①盒槽形参泛型特化无序言(est/margin 兜底,guard 触顶诊断可辨);②C 宿主
  ctronc emit 面无序言机器(bootstrap 链内用,用户程序不经其发射面;divergences 立档);
  ③est 为发射期启发式,精确帧宽不可得(C 发射后宿主 cc 自由布局=T34 判决同源),
  触顶诊断兜底的「最后防线」性质不变。

**2026-10-03 P9 密度回填:生长态小栈 C10K 实测(栈面全量后续测量件)**(tests/net/c10k):
- 载具:ctecho(coro)@N=10000,density.sh 采样壳(外部轮询服务进程 RSS 取峰,
  0.3s 步;run.sh 全套判据原样站岗)。**A/B 同 driver 同机**(bench 家族惯例):
  - 基线(1MB VA 缺省):max RSS 402MB = 40.2KB/conn;
  - **生长态(CTRON_MORESTACK=1 + CTRON_RT_STACK_KB=64):max RSS 170MB =
    17.0KB/conn ⇒ 2.36× 密度**,回显 10000/10000+探活+fd delta=0 全绿——
    生长机器在满载 C10K 并发下稳态成立(每任务 64KB VA+按需段,RSS 实测
    ≈13KB 栈页+lane,与 0926 lean 形 6-9KB/conn 同量级且无 lane 悬崖)。
- **拓扑顶在册**:N≥16000 即 `connect #N: Can't assign requested address`
  ——macOS 临时端口池(49152-65535=16384)对单 (dst ip,dst port) 元组空间
  硬顶,~60% 占用后 bind 选择劣化(16K 池 < 20000 目标必然不可达)。非栈
  机器问题(基线/生长臂同因同点位)。**C100K 前置升级件=driver 多监听端口
  分片**(把 (src,dst,dport) 元组空间乘开;127.0.0.0/8 多地址同效),另册。
- 载具修正:driver 收尾 SO_LINGER(1,0) RST(bench_cycle 同款口径)——普通
  close 使驱动侧每轮吃 N 个 TIME_WAIT(msl 15s×2),跨轮次污染临时端口池
  (先跑 1MB@10K 基线后,后续臂 connect #106 即 EADDRNOTAVAIL 的污染路径
  实证);RST 后零 TIME_WAIT,轮次自含可复现。响应字节 RST 前已按序送达,
  判据面无涉。

**2026-10-03 P9 C100K 前置:拓扑分片载具 + macOS 全局端口池实证**(tests/net/c10k):
- 载具:ctecho 绑 0.0.0.0(同一端口全 127.x 应答;对 127.0.0.1 客户端零语义
  变化)+ driver C10K_ADDRS 目的地址轮转(127.0.0.1..N;i%addrs;缺省 1 零变化,
  上限 64)。分片设计前提=「临时端口池按 (src,dst,dport) 元组各 16384」。
- **macOS 实证否证该前提**:2 地址 N=20000 失败点 connect #16308 落在
  127.0.0.1(该地址轮转下仅承载 ~8154 连接)——macOS 临时端口分配为**全局
  位图,不分目的元组**(16308 总在途≈池顶)。多目的地址在 macOS 无效;
  IPv6(::1)独立池可再翻倍(另册,sockaddr_in6 路径)。**C100K 满额门 =
  Linux nightly 目标**(Linux inet_hash_connect 元组感知,跨目的复用成立;
  分片载具为其就位)。
- 回归:标准 c10k N=10000 双臂(生长态 170MB/默认 416MB)全绿零变化——
  ANY 绑定+分片 driver 缺省面对既有判据零影响。
- 实操红线:改 c10k 相关载具后,**跨轮次测量必须等 TIME_WAIT 排干或用
  RST 收尾**(上一条 2026-10-03 密度回填段的 SO_LINGER 修正即为同族教训;
  本轮再证一次:污染池上 2 地址臂假红于 #16308,排干后同点位复判)。

**CI [1/9] manifest diff 门揭示债(2026-10-03,T31 合流 push 后首次全量曝光;归 CTCL 语法泳道)**:
本地 80 提交(T35/T45/T48/T52/GUI 各批)首推 origin 后,ci.sh [1/9] tests/manifest/diff.py
四线对拍暴露 CTCL 语法三处漂移:①`tier` 键——seed 侧(parse_pkg.ct pkg_check_caps)已识
(golden 过),C 宿主侧 pkg.c PKG_KEYS 仍 4 键(tier 全线 E5043),T35 批遗漏;②`plugin` 块
——T52 语法(json_demo/lint_demo/sandbox_escape 三清单),C 侧 E5044,seed 侧 lint_demo
的 `dep{codes}` 亦不认——**两侧与 golden 三方互斥,需泳道裁决合法 plugin schema 后双端
补齐**;③negative/005_unknown_key 的合法键提示串两侧行未同步(含 tier 与否)。
非 T31 债,登记不修(语法意图裁决权在 CTCL 泳道);修复径=pkg.c 镜像 parse_pkg.ct
(tier 键+E5046 值域+plugin 块)或 golden 回写,二者取一由 schema 意图定。

**2026-10-03 P9 栈面 × T31 帧链税交互两修正**(合并树实测;P9 泳道):
- T31 M1.5 精确帧根集为每帧增 784B ct_fr(pool[32]×24B)——生长段承载从
  ~485 层/段@64KB 跌到 ~80 层/段,ms_grow 深度 20000 需链 ~250 恰贴
  RT_MS_SEG_MAX=256 上限边界 → **cap 256→1024**(纯失控保险语义不变;
  1024×64KB=64MB VA/任务惰性提交)。est(352)对真实帧(~800B)低估 2.5×
  在册:est 为发射期启发式,双层兜底(growth 优先/guard 诊断保底)性质不变。
- 同形对深度 6000→1000:T31 帧税后 6000 层≈4.8MB 连 1MB 栈也不可容
  (off 臂必触顶);1K 层 off@1MB(无生长)与 on@64KB(生长)同形逐字节
  契约保留。锚注释同步。
- 流程实录(机刷协作样本):本泳道 merge 进行中窗口,对端以整树卷带方式把
  p9-ms 全支合入 main(f725f5c3 等携带)并推进其 P0-C——我方 merge 现场
  被取代=正常协作;残余两修正以 pathspec 限定补落,零冲突。

**2026-10-04 GUI-27 组件跨文件 view 导入**(gui 泳道):
- 形态:`pub view` 导出 + `use mod.{A, B}` 导入(spec §4.3.1 定案落地)。
  运行时 = gt_anchor_src 锚合并(DFS imports-先/入口后,应用根=最后声明视图
  契约保持;热重载探针同源,无 use 行文件字节恒等);检查面 = use 装载器
  (node 级合并,GuiBlock 追加槽 d[7]=fileid/d[8]=pub,无槽=通配可见,内嵌
  形态零波及)+可见性门(独立形态:未知组件 E8100/未导入组件 E8100;直接
  导入语义不传递)。骨架/发射面零改动。
- 验收:e8_corpus 51/51(新五件:use_priv/use_missing/viewcall_unknown 负例
  + use_ok 正例);阶梯 s78_xfileview 全绿(导入渲染/私有内部件/slot 投影/
  事件 fire;阶梯 82 过/1 败=s30_props_d 基线既有段错误,worktree HEAD 基线
  与主树双环境复现,归 P9/T31 栈面或发射在飞件,本泳道移交);suite 100/100
  双列;smoke 158 ok/2 fail(均基线既有)。
- **decls 锁申报**:编译面 +8 fn(gui_blk_fileid/gui_blk_pub/gui_ctml_dir/
  gui_ctml_mod_path/gui_use_names/gui_ctml_file_use/gui_vis_ok/gui_ck_view_vis),
  worktree 实测 cc_run 自检 decls 472→479(+7;smoke 该件基线已红,锁数与
  主树在飞件一并归口)。runtime +5 fn 在 pkgs/gui不入 compiler 锁。
- 顺修两存量缺口(独立 .ctml 检查面,GUI-27 首曝):①独立词法左右圆括号入
  单字符集——view props 形参表捕获(8c-1)要求单字符 `(`,独立面捕获从未
  工作(实例 prop 门全空转),按「内嵌/独立同语法同路径」对齐;②实例子树
  走查先吃开尖——此前 tag 吃到 `<` 脱轨级联误报(投影实例带子树从未入语料)。

**2026-10-04 G 档 GUI-40 command palette**(gui 泳道,波一首件):
- **交付面**:pkgs/gui_widgets 0.3 +Palette 组件(open/q/rows 三 prop+pal_type/pal_go/
  pal_pick/pal_close 四事件头+w_pal 三样式);gui_snippets 桩同步四件;夹具 s79_palette
  五径断言(热键 mod+k 实发消费/过滤 'e' 三行留 Quit 出/↓ 过滤空间移高亮/Enter=
  pal_go 执行关面板/Esc 首击失焦次击 overlay 关+pal_pick 行点击直驱)。
- **能力补齐(编译面,本件首缝)**:desugar 生成器组件视图 input/submit 文本通道——
  收集面组件视图 on:input(原整类丢弃)与 on:submit on input(原误入 cvf 无尾参)
  收 cvinps 新列,生成面镜像 inps 前缀分支+Ⅱ-b 模型捕获(签名门 iscomp+imp 原已
  按此收,仅生成器欠臂)。**decls 锁申报:cc_run 零新顶层 fn**(gui_ds_collect_slots/
  gui_ds_gen 均原位改签名+加列,480 基线漂移维持对端拆分批归口);pkgs/gui_widgets
  +4 decl(视图+样式)不入 compiler 锁。
- **在册新坑**:①组件内 bind 源名通道——装配不改写 nbid(sk 实证组件 input nbid="q"),
  显示/聚焦走组件局部名,宿主包装闭包应答后委托生成 bind(s79 bind_host 形态);
  ②内嵌源重建序=本地块前/合并块后,非糖路消费者 rt_draw_frame 根=最后视图选中
  库件渲染空树(s49 挪尾仅糖路受益)——**跨包库消费必须走 run(ViewCall) 糖路**;
  ③浮条内按钮 d_click_inst OOB(浮条 rects 尾置配对错位在册坑新面),行命中走
  P-H1 后缀名 actc 直驱,真实命中归真窗。
- **门**:阶梯 83 过/1 败(s30 在册基线债);suite 100/100 双列;smoke 170/2(两红=
  decls 锁 471→480 对端拆分批漂移+conc_parallel 对端在飞,均非本件,较基线 170/3
  反少一红);gui_snippets+gui_widgets 陈列室双绿。真窗 --run 手验待用户
  (run.sh --run)。

**2026-10-04 G 档 GUI-38 date/time picker**(gui 泳道,波一第二件):
- **交付面**:pkgs/gui_widgets 0.4 +DatePick(ym/hd/weeks prop+dp_prev/dp_next+
  dp0..dp6 七列头,行点击=头名位定列+实例后缀定行)+TimePick(hh/mm 预格式化串+
  tp_step 显参通道 "h+1" 形,滑杆同款)+w_dpk/w_tpk 样式族;gui_snippets/s79 桩
  同步 +11;夹具 s80_datepick(月历渲染/换月往返/真历法驱动格点击含空白位无操作/
  平月 2027-02 无 29/闰月 2028-02 有 29/sd 越界钳位/时分步进与双向回绕)。
- **decls 锁申报**:cc_run 零改动(本件零编译面);pkgs/gui_widgets +10 decl
  (2 view+8 style)不入 compiler 锁。
- **坑位**:组件格内选日前缀含空格(" > 15"形)在窄按钮(w:34)按词折行拆 TEXT
  ——断言须单词形(">15"),w_dpkc 放宽 38;组件 each 行内七列只能硬列 fld+
  七头(实例后缀仅携带行号,列身份只能由头名承载,each 内显参不被生成器解码)。
- **门**:阶梯 84 过/1 败(s30 在册);suite 100/100 双列;smoke 170/2(对端两账
  维持);gui_snippets+gui_widgets 双绿;真窗 --run 手验待用户。

**2026-10-04 G 档 GUI-39 color picker**(gui 泳道,波一第三件):
- **交付面**:pkgs/gui_widgets 0.5 +ColorPick(SV 方=canvas 标记容器+cp_paint 每帧
  盒相对画点/cp_drag 显参 dragpx/py/cp_hue 色相条行/cp_hex 文本通道;组件内 bind
  源名通道 "hex" 宿主包装应答)+gui_theme.color_parse pub 面(#RRGGBB→打包);
  gui_snippets/s79/s80 桩同步 +4;夹具 s81 四径(color_parse 打包对拍/canvas 渐变
  像素四点断言/色相点击+拖拽选点写回精确 hex/hex 键入采纳含失焦重聚焦)。
- **decls 锁申报**:cc_run 零改动(本件零编译面);pkgs/gui_widgets +8 decl
  (1 view+7 style)、gui_theme +1 fn(color_parse,非发射链不入锁)。
- **🩸 坑位四条**:①canvas 登记挂可见容器支——theme_default SURFACE 折叠空串走
  透明支不登记 → d_draw 恒 0;canvas 容器必须显式 bg(十六进制直色);②像素槽
  容量 4096 帽——160x120 全画 19200 越帽,4px 步进 1200 点;点槽布局=(x,y,r,g,b)
  非 (r,g,b)(s65 可证);③样式色槽表达式不支持(bg:"{cur}" 不折叠不产 RECT;
  宽度槽 GUI-16 前例之外)——立册缺口,swatch 动态色候能力切片;④组件格内
  交互件焦点:拖拽点击(非输入)即失焦,后续键入前必须 gui_focus_req 重聚焦;
  d_px 像素索引=px/step 行主序。
- **门**:阶梯 85 过/1 败(s30 在册);suite 100/100 双列;smoke 170/2(对端两账);
  gui_snippets+gui_widgets 双绿;真窗 --run 手验待用户。

**2026-10-04 G 档 GUI-42 过渡动画 v1**(gui 泳道,波一第四件):
- **交付面**:pkgs/gui/gui_anim.ct 纯函数库——anim_prog(进度千分比钳位)/ease 五族
  (linear/out_cubic/in_cubic/in_out/out_back c1=1702 整除补偿)/lerp_i/anim_i(fn
  泛型入口)/五 ease 定形包装;门面 re-export(use gui.gui_anim.{13 符号},消费
  use gui.{...} 不变)。全部 I32 千分比算术(中间量 ≤1.7e9 防爆,无 F64/位运算)。
  夹具 s82:纯值断言(ease 恒等/中点/回弹过冲端点)+d_tick 帧驱动(0→138→210→240
  关键帧精确+渲染面 w100=13800 随动+超时恒值钳位)。
- **decls 锁申报**:cc_run 零改动(零编译面);pkgs/gui 域 +17 fn(纯库,域 fn
  不入 compiler 锁);门面 use 行 +5 符号。
- **🩸 坑位三条**:①d_tick=gui_inject_ms **置位语义**非累加——帧驱动给绝对时刻;
  ②动画值进 CTML 的通道=宿主 bind 帧算+**双通道应答**(叶槽 "m.aw"/样式折叠
  "prop:m.aw" 装配通道,s72 双应答同款;bxv 槽不持 fn 实参→ease 定形包装族);
  ③值驱动 w 是样式块语法(w:"{expr}" 冒号形),元素属性 = 形不认;主题令牌
  ACCENT/SURFACE 在 theme_default 折叠空串→透明无 RECT(显式十六进制 bg)。
- **陈列室动效段缓行**:主陈列室消费组件库两缺口在册(手写 bind×类型化 props/
  键闭包×desugar)——动效演示样候缺口切片随行;s82 即验收场。
- **门**:阶梯 86 过/1 败(s30 在册);suite 100/100 双列;smoke 170/2(对端两账);
  gui_snippets+gui_widgets 双绿;真窗 --run 手验顺滑待用户。

**2026-10-04 G 档 GUI-41 chart v1**(gui 泳道,波一收官件):
- **交付面**:pkgs/gui_widgets 0.6 +BarChart/LineChart(canvas 画条/折线,vals/pts
  纯 prop,归一/钳位在宿主;sparkline=LineChart 窄形);夹具 s83(条像素几何三首点
  +x 步进/折线首中末插值/空·单值边界/宿主钳位纯值);**引擎能力:多 canvas**——
  gui_canvas_node 单槽 → 16 容量列表(gui_canvas_reset 帧首复位+setnode 去重追加
  +count/node_i indexed 读,C 胶水 ~10 行)+rt_draw_frame 帧首 reset+rt_canvas_frame
  遍历 fire(d_draw 返回值=fire 数,双图=2)。
- **decls 锁申报**:cc_run 零改动;pkgs/gui +3 pub extern(reset/count/node_i)+
  C 侧三 fn;pkgs/gui_widgets +4 decl(2 view+2 style);消费方桩税:gui_snippets/
  s79/s80/s81/s82 各 +2(lt_paint/bc_paint)。
- **🩸 立册缝与坑**:①**each 迭代变量的样式折叠不通**(叶槽 {it} 可、样式块 {v}
  折空——fold 询问无 item 上下文)——条图树内 RECT 形候引擎切片,v1 canvas 化;
  ②双 canvas 同像素槽(槽全局,点数=两图之和,断言偏移=前图点数);③d_draw 返回
  =fire 数非 1;④pub extern 才可导包(裸 extern E2020.use.priv);⑤闭合标签错配
  (hbox 开 vbox 收)运行时解析报"闭合标签不匹配"。
- **门**:阶梯 87 过/1 败(s30 在册);suite 100/100 双列;smoke 170/2(对端两账);
  gui_snippets+gui_widgets 双绿;真窗 --run 手验待用户。**波一收官(5/5)**。

**2026-10-04 G 档 GUI-43 chord 序列组合键**(gui 泳道,波二首件):
- **交付面**:C 侧 chord 平行表(8 条×4 步)+状态机(武装/步进/完成/超时解除,
  gui_chord_register wire="mods:key;…"+gui_chord_match 三态 0/1/2+action 缓冲+
  gui_chord_ms 默认 800 可配);gui_hotkey 解析扩**空格分隔序列**(wire 组装交 C;
  无空格单段路径字节恒等);gui_chord_route 共享帮手接入**三点路由**(窗口循环/
  headless 驱动循环/d_send_key 尾——2=完成 fire/1=武装消费/0=原路由)。
  夹具 s84 四径:完成(g g→goto)/超时(tick 900 解除+重武装不误 fire)/干扰不吞
  (武装期 X 返原码+键闭包 k2 投递)/mod 序列(mod+d d→dup)+单段回归(mod+z)。
- **decls 锁申报**:cc_run 零改动;pkgs/gui +5 extern(chord register/match/
  action_len/action_byte/ms,#[trusted]);gui_driver +2 fn(hk_token_modskey/
  gui_chord_route,域 fn 不入锁);C 侧 ~70 行。
- **🩸 坑位两条**:①顶层可变全局在册债(SIGSEGV)→chord 状态机必须驻 C
  (Ctron 侧无状态);②d_send_key 不派键闭包(在册)——键闭包断言走
  k2(d_press_key(k)) 直驱;武装期非前缀键 rc=原码=不吞实证。
- **门**:阶梯 88 过/1 败(s30 在册);suite 100/100 双列;smoke 170/2(对端两账);
  gui_snippets+gui_widgets 双绿;真窗 --run 手验待用户。
**S4① 池界重置机器落库+复测追记(2026-10-03)**:帧内嵌保存栈(sv[8]+spn,免命名陷阱)
——While/For×5 环开保存、环合恢复、break/continue 位弹栈(弹内层栈与跳转语义对齐,
防陈旧保存值误缩活槽;return 由帧 cleanup 兜底;panic=ct_fp=0)。循环体 gc-let 不再
逐迭代递增 pn→池常量→ovf 不再触发。三档 9/9+suite 100/100 双列+重建净。
**a_jwt GC=1 复测判决更新**:ON 臂 30min 终止(mkword 热点)——池重置消了栈项,但
**arena 根扫仍是 O(N²) 主项**(3 亿分配×8MB 阈值≈150 次收集×全 arena 扫 20-60s/次;
GC 堆 List 已回收但 bump 串/items 照涨 41GB)。**红账解锁条件维持=S4③ 分配位点翻面
+arena 根扫退役**,池重置为其健全性前提(环内精度保持)。坑位:`\}` E1001 第五踩。

**S4② 实参面 ANF 补全落库(2026-10-03)**:vtable trait 调用(接收者+实参)、具体型
UFCS 自由 fn 方法调用(同)、泛型 UFCS 接收者(_cur 先落帧池,取址/装盒/cast 皆对已
根槽)三处拼装面完成提升——实参文本一次收集(副作用语义不变),槽族经帧池注册,
语句表达式包调用。至此全部调用实参悬窗封死(ct_call_args_tg=S2 + 本件三面)。
**夹具验证**:vt(trait 对象双方法,12/24 逐字)/uf(UFCS List 接收者,GC=1 运行)/
b2a/b2c/bat1 全链绿;gc/run 9/9+suite 100/100 双列+net 18/18。
**登记(预存,非本件)**:impl-for-Self 固有方法调用派发缺口——`impl Bag for Bag`
+`b.add(x)` 走 UFCS 臂产出 `t_add` 未定义(main 树二进制同现,正确名应 t_Bag__add;
T45 台账「class 字面量发射不可用」邻域,固有方法派发=另册)。
**decl 锁代抬**:471→480(对端 T52 后批次 +9 fn 漏抬,主树实测同 480,随批申报)。

**S4③ 分配位点翻面+arena 根扫退役落库(2026-10-03)**:28 个 amalloc 位点翻 ct_gc_alloc
(串全家/list_push items/pmap 结果数组/cell_new/ct_wrap_i 与 send 盒/闭包 env 数组/
ct_clop+FnExt 盒/Result-Option 载荷盒/尾返回盒);**保留 bump=纯标量域**(SIMD 算术
scratch/视图 .d/read_dir 可增长 scratch——零 GC 引用,免池化)。GC=off/conc 路径经
ct_gc_alloc 内建回退 bump,双档行为不变。**arena 根扫默认退役**(CTRON_GC_ARENASCAN=1
逃逸)——三锚(cycle_reclaim/deep_stack/precise_root)纯精确档全绿=翻面自足证。
门禁:gc/run 9/9 三档+suite 100/100 双列+net 18/18+bench 1.000(真重建 bin)。
**a_jwt 红账判定(2026-10-03,S4③ 后)**:GC=1 interp 臂 RSS **41GB→70MB 平台**
(585×,52min CPU 稳如磐石;开销=活集大×收集频次,归 M2 分代域;CI 不受影响=套件
恒 GC=off)。完备性跑=大阈值(CTRON_GC_THRESHOLD=256MB)对拍双臂输出。**红账解锁**。
坑位续增:①CTRON_GC sweep 误翻 amalloc 定义名+真回退自递归——全量重命名前先枚举
不可翻位点;②Ctron 串字面量内插 getenv("X") 引号必须转义(E1001 实参表缺 );
③管道尾 $? 假 0 第五踩(head 接管返回值);④夹具 main 返回值=rc( bat1 返回 a.len=1
被误读为崩溃)。

**S4④ 堆自适应阈值落库(2026-10-05)**:收集触发改为 `churn > max(8MB, ct_gc_livesz)`
——全堆标记摊销 ≤2× 活集(小堆守 8MB 底线响应)。门禁:gc/run 9/9+suite 100/100 双列
+bench 0.972/1.088(≤1.15;首跑 1.15+ 为同伴并机构建噪声,复跑两轮甄别)+net 18/18。
**a_jwt GC=1 实测更新**:RSS 74MB 有界不变;CPU ~50min 与固定阈值基本持平——本夹具
活集偏小(触发线≈旧值),自适应收益需大活集场景才显形;GC=1 CPU 开销主项=每收集
O(堆) 扫描+标记,真收敛=M2 分代/增量(卡内已有入口证据)。契约 §6.2 落地状态注
(M1.5/S4 批+M2 入口证据更新)随批落库。

**GC=1 全量验证轮(2026-10-05,S4 全链后)**:①**suite 100/100 双列在 CTRON_GC=1 下全过**
——翻面后全语义套件首次整体验证,GC 默认翻面的套件面实证前置达成;net 18/18 本就
GC 档常开。②db 编译器臂豁免复测:pg interp 臂 GC=1 300s 超时(off 臂正常)——**豁免
维持**,但病理已换代:T29 时代的 46min 近挂死=保守扫内存灾难(41GB 类),现为纯 CPU
开销(interp 重负载 GC=1 ≈15-20×,M2 分代域);RSS 有界。豁免解除条件=M2 或 interp
臂 GC 提速批。T32 翻面决策剩余证据面=长驻 server P99 实测(§9.4 载荷),呈用户裁决。

**S4③ 修正批+发射器 GC 化事故全录(2026-10-05)**:GUI 阶梯 GC=1 团灭(37 夹具)触发
根因追查,三层层剥:
①**arena 根扫退役过早回滚**——发射器/解释器自身是「会跑解释器的程序」,其 Val/
arena 结构持 bump 内存中的 GC 引用,扫描是承重墙;默认回开(CTRON_GC_ARENASCAN=0
逃逸保留)。②**盒类翻面回退**——T54 式 struct→ct_i 装盒指针存于标量型槽(槽码 6/i
不在注册谓词),盒在 bump 由扫描兜底才健全;wrap_i/send/env 数组/clop/FnExt/payload/
ret 盒七类回 bump,数据类翻面保留(串全家/items/pmap/cell_new)。③**s/N 槽注册补**
——串翻面后 char* 即 GC 对象,槽谓词「bump 串无引用」判据过期,s/N 入 kind0。
**回归探针立规**:编译器在 CTRON_GC=1 env 下发射 cc_run.ct(19K 行)必须与 off 逐字
一致——修后全绿;GUI 阶梯 GC=1 从 37 败收敛至 18 败且全部**逐跑绿(负载性 flake)**:
wlput realloc 失败静默降级=漏标(OOM 健壮性债在册,修法=worklist 预留/OOM 即 panic)。
门禁:gc/run 9/9+suite 100/100 双档+net 18/18+bench 1.000+双环境大语料发射探针绿。
**教训三则**:翻面审计须以「GC 引用现居何处」为准重判每一处跳过判据(串判据过期
即 37 夹具团灭);「运行时开关」会跨过编译期/运行期边界泄漏(发射器自身被 GC 化);
发射确定性双环境探针(大语料逐字节)应入 CI 常驻。

**wlput OOM 健壮性批落库(2026-10-05)**:worklist realloc 失败从静默 return(漏标=
UAF)改为 ctron_panic 响亮退出(消息点名「标记候选丢失」)——梯内负载性 flake 的
静默腐坏路径自此转为可诊断失败。随批两修:wlput 先于 ctron_panic 定义需前向声明;
串字面量内引号转义第六踩。门禁(真重建):gc/run 9/9+suite 100/100+bench 1.000+net 18/18。

**auth 家族 GC=1 对拍补全(2026-10-05)**:a_body 双臂 rc=0 输出逐字一致;a_guard/a_sess
GC=1 600s 超时(off 正常)——interp 重负载 CPU 墙在 auth 家族复现,与 15-20× 刻画一致。
**GC=1 边界画像定稿**:轻/中负载(全语义套件+net+gui 阶梯+轻 http 夹具)全绿;重负载
(interp 巨语料:a_jwt/guard/sess/db-pg/编译器自举)CPU 15-20×,RSS 恒有界。翻面决策
的完整证据面=套件绿✓+RSS 有界✓+CPU 墙(M2 域)✓+OOM 响亮化✓。

**CTCL manifest 四线对拍债闭账(2026-10-05)**:pkg.c + selfhosted/ctcl_chk.ct 双硬编码
解析器同步 schema 真源(tools/ctcl_manifest_schema.ctcl 早已全量:tier/plugin/codes)
——①pkg 合法键+=tier(E5046 值域 (core|alloc|std),消息逐字对齐 python 臂);②plugin
块准入(keyed;path/codes);③dep += codes;④未知键/未知块提示串同步(含 tier/plugin);
⑤双解析器输出改码升稳定排序(python 臂同序;gui_calc 的 E5040 行序分歧即此销账);
⑥ctcl_chk 的 skip 块键行静默化(未知块键行曾误投 dep 臂)。四线对拍 136 例 ALL GREEN
(meta_check/manifest run/selftest/suite 100/100 随批全绿)。**D7 终点登记**:pkg.c 与
ctcl_chk.ct 仍为硬编码grammar(第三/四份拷贝),schema 数据驱动化(C/自举线接
ctcl_manifest_schema.ctcl 加载器)=CTCL 泳道架构件,本件为最小 goldens 满足同步。

**CTCL R 臂债登记(2026-10-05,主树 16a40a4a)**:manifest 四线对拍在主树(本地有
cargo/target)剩 12 红全为 **r 码臂**(compiler-rust 缺 tier/plugin/codes 同批语法;
与 T11「R 线对象面细键欠账在册」同族)——CI 无 cargo=SKIP 不红,本地开发卫生债,
归 ctronr 泳道。c/sh/python 三臂经本件+cherry-pick(16a40a4f)已全绿。

**CTCL R 臂债登记(2026-10-05,主树 16a40a4f)**:manifest 四线对拍在主树(本地有
cargo/target)剩 12 红全为 **r 码臂**(compiler-rust 缺 tier/plugin/codes 同批语法;
与 T11「R 线对象面细键欠账在册」同族)——CI 无 cargo=SKIP 不红,本地开发卫生债,
归 ctronr 泳道。c/sh/python 三臂经 pkg.c+ctcl_chk 同步件(99176da9)与 cherry-pick
(16a40a4f)已全绿;主树 CI [1/9] 自此不红。

**2026-10-04 G 档 GUI-37 内部拖放**(gui 泳道,波二第二件):
- **交付面**:rt_dgev_frame 释放臂(签名+rects/hits)——鼠标抬起命中 on:dropin
  元素 → 合成 permille 调用形 fire(实参=释放点在目标盒内千分比)→ end;
  d_drag/d_dragv 尾改走共享释放臂(headless 同源)。s85 夹具:三行九头
  (DatePick 硬列模式),drag 实参携行号定源/命中 dropin 定目标 → 换序,行序
  TEXT 列序断言。零数据(无 dropin 命中)=原 end 语义不变。
- **decls 锁申报**:cc_run 零改动;gui_driver 签名扩展(rt_dgev_frame)+两尾改。
- **🩸 坑位三条**:①**d_drag/d_dragv 自含直呼 dgev_end**——headless 释放逻辑
  必须走共享臂否则 dropin 不可达;②**本地视图裸事件头生成 0 参调用**(cvf 组件
  头有 mcap、evf 本地头无)——本地头一律调用形 {h(m)},绝对/相对路径 emit 差异
  为烟雾弹(真因即此);③源身份=drag 实参携行号(rK_drag(1)),目标=dropin 头
  名(rK_drop),press-click 在 headless d_dragv 中不 fire(自含)。
- **门**:阶梯 89 过/1 败(s30 在册);suite 100/100 双列;smoke 170/2(对端两账);
  gui_snippets+gui_widgets 双绿;真窗 --run 手验待用户。

**2026-10-04 G 档 GUI-44 异步→UI**(gui 泳道,波二第三件):
- **交付面**:C 互斥投递队列(容量 32,post 满静默弃;strdup 持有+take 静态回程
  缓冲)+gui_async_post/on pub 面+rt_async_drain(逐件 fire 注册 action,载荷单
  Str)+双循环帧首挂点+headless d_async_drain 驱动面。s86:双投递 FIFO "ab" 帧
  循环到齐+计数断言。
- **跨线程半程缓行**:spawn 语句发射=对端在飞断点(smoke conc_parallel 同源
  在册,t_spawn 裸标识)——v1 同线程 post 证全管线(队列/FIFO/drain),对端
  spawn 落地后字面替换 spawn { gui_async_post(x) } 即得并发实证。
- **🩸 坑位**:①闭包体 spawn 发射盲区(t_spawn 裸标识,函数体同断)——归发射
  泳道;②ArrLit 仅 let 初值位(call 实参用 List 构造);③Driver 无 act 域
  (headless 驱动面显式传闭包)。
- **门**:阶梯 90 过/1 败(s30 在册);suite 100/100;smoke 170/2;双陈列室绿;
  真窗 --run 待用户。

**2026-10-04 G 档 GUI-34 RTL 布局镜像**(gui 泳道,波三):
- **交付面**:class 标记 "rtl"(canvas:1 同款标记形态,零解析面)——容器子序逆发
  (rt_emit 共享尾遍历逆序支);s88 双联同构夹具(ltr 递增/rtl 递减,扫描期单调
  断言)。文字面 bidi 已有(s14/s15/s17)。
- **v1 边界在册**:元素级无继承(根级=整树镜像特例);pad 对调/文字 align 起始侧
  随令牌面候切片;each 容器 rtl 候查(共享尾支仅非 each 容器)。
- **🩸 坑**:嵌套分支下标 xs[0](⑳ 家族再犯)——扫描期就地单调断言免下标。
- **门**:阶梯 90 过/1 败(s30 在册);suite 100/100;smoke 170/2;双陈列室绿;
  真窗 --run 待用户。

**2026-10-04 G 档 GUI-33 富文本 v1**(gui 泳道,波三):
- **交付面**:class 标记 "rich"+样式属性 rich:1(标记=类+同名属性同款)——label 叶
  发射(markdown 子集:**b**=weight 700/@@c@@=teal;direction:row=分段水平流,
  gui_text_w 逐段 weight/色)+gui_cmd_text_fg_r/g/b 探针(C 侧 TEXT textColor)。
  s87:三分段+code 段色断言。链接色/转义/多行 wrap/链接点击 v2 在册;dir 属性/
  反引号(E1001 非法字符)→@@ 形。
- **🩸 坑四**:①标记=类+同名属性,缺属性则 fold 空(踩);②label 文本=npre 槽
  (tph=占位符路径,富分支正位=叶发射 460 位,weight 打包 ff2*10000+fw2);
  ③pub extern 带参 E2020.use 未找到(无参可、#[trusted]+pub 疑误解析)——包装
  fn 绕行(d_open_url 同款);④值位 if(分支尾)需后续语句破位;⑤拼接回滚吞段
  ——引擎文件探针一律 git checkout 重做。
- **门**:阶梯 91 过/1 败(s30 在册);suite 100/100;smoke 171/1;双陈列室绿;
  真窗 --run 待用户。

**2026-10-04 G 档 GUI-36 触摸/手势 v1**(gui 泳道,波四):
- **交付面**:C 手势跟踪器(press/release 注入缝 gui_inject_press/release+长按
  600ms 帧检+轻扫 50px 8 向分类)+rt_gesture_frame(命中 on:longpress/on:swipe
  fire;合成调用形带压点/直调带方向)+d_touch_press/release/frame 三件驱动面;
  双循环帧挂(真窗同源)。s89 四径:longpress(压 800ms)/swipe 左/右/卡外干扰。
  tap=click 同义不另设(在册);pinch/移动端 M3 另案。
- **🩸 坑四**:①gui_now_ms 时钟域——press 前必须先入注入域(d_tick),真时钟
  ts 跨域为负永不 due;②d_touch_frame 自采 rects(rt_draw_frame 5 参=hits 非
  rects——本件首误);③手势 fire 后** cmds 槽滞后一帧**(双帧断言);④swipe 方向
  合成转义直调 act(List push),不经 ev_fire 转义面。
- **门**:阶梯 92 过/1 败(s30 在册);suite 100/100;smoke 170/2;双陈列室绿;
  真窗 --run 手验待用户。

**2026-10-04 G 档 GUI-35 无障碍 v1 + 发射链治愈**(gui 泳道,波四第二件):
- **交付面**:ax_role_of(role 推导:button/input/textarea/checkbox/label/generic)
  +d_ax_dump/ax_dump_walk(树投影:role+叶文本 '引号'+bind 路径,缩进两空格/层)
  ——三 fn 驻门面 gui.ct 本体;gui_render/gui_driver 零改动。s90 六断言
  (三 role 推导/两文本载荷/bind 路径)。
- **发射链治愈(重大)**:t31-m15 分支前沿 emission 三件(driver_emit 大重构
  439 行/trans_conc/trans_expr)checkout 入 main——smoke 115/54→171/2 治愈。
  根因链:35bd03f6 合入的 t31-m15 为中间态,peer WIP(修复态)曾被会话 checkout
  丢弃→bins 重建自破损态→s23 总线错+全 fixtures emit 段错误。本件取分支前沿
  即完整态。
- **🩸 坑位五**:①**门面 re-export 触发 emit 段错误**——gui_driver 的 fn 经
  门面 gui_driver 组 re-export(gui.ct use 列表)即崩(d_ax_dump/ax_dump_walk
  bisect 实证;驻门面本体即愈)——re-export 链发射缺陷立册;②嵌套 worktree
  remove 卡会话 shell(cwd 删除);③bisect 连环字符串替换必错——git checkout
  重来做;④grep "ax_" 误匹配 max_w/max_h(假阳性 7 处);⑤s89 双帧断言
  (手势 fire 后 cmds 槽滞后一帧)。
- **门**:阶梯 93 过/2 败(s30 基线+s23=t31m15 发射回归在册)/suite 100/101
  (s23 同源)/smoke 171/2(对端两账)/双陈列室绿;真窗 --run 待用户。
- **GUI-35 v1 边界**:role 推导+投影面全量;aria_label 属性面(需解析槽)
  /mac NSAccessibility shim(VoiceOver 手验)另案。

**2026-10-04 G 档 GUI-32 多窗 v1**(gui 泳道,收官件):
- **交付面**:C 浮板窗管器(单浮板标量态:gui_board_open_req 入队/activate/几何/
  拖拽/关闭)+gui_cmds_save/merge 双 pass 命令拼接(主 pass 快照→浮板 pass→
  malloc 拼接→区间平移)+gui_board_open/close pub 面+d_board_frame(双 pass
  帧驱动:独立 layout emit 浮板树→merge→平移 blit→板 rects 收集→标题栏/关闭盒
  浮条)+d_board_click(zone 路由:板体→rt_hit_name 浮板树/标题→drag/关闭盒)
  +Driver 加 bt/brects/bhits/bhinst 四域。s91 五径:主树基线/开板板体命中/
  关闭盒关板+关后主树恢复/重开+标题栏拖拽落位。
- **decls 锁申报**:cc_run 零改动;pkgs/gui +21 extern(board 全族+cmds_save/
  merge/len)+Driver +4 域;C 侧 ~60 行。
- **v1 边界在册**:单浮板(多板=v2 C 数组扩展)/键闭包与滚轮主窗专用/热重载
  不涉浮板/拖拽跟随=真窗专用(headless 靠 d_board_drag_to 显式落位)。
- **🩸 坑四**:①gui_begin_layout 重置 Clay 命令缓冲——双 layout pass 须
  gui_cmds_save/merge C 侧拼接(偏移 blit 模型的根基);②drag-follow 不能进
  d_board_frame(headless gui_mouse=(0,0) 覆盖显式落位);③本地视图裸事件头
  0 参调用再犯;④pub extern 带参 E2020 再犯(包装 fn 绕);⑤Clay cmd 访问器
  cmd() 定义序——前置 static 声明。
- **门**:阶梯 94 过/2 败(s30 基线+s23 t31m15 回归)/suite 100/100/smoke
  170/2(对端两账)/双陈列室绿;真窗 --run 手验待用户。
- **GUI-32 后续**:v2 多板(C 数组)+键路由浮板优先+热重载浮板+每板独立
  bind/act;命名守 0924 替身红线(gui_board_* 文档明示引擎级浮板)。

## 2026-10-06 T27-B 残债清偿(t27-anf 泳道)——with 任意表达式位+with_mut 写回语义

- **登记债翻面**:「with 任意表达式位 emit ANF 缺口」清偿。①`trans_expr.ct` ct_expr
  with/with_mut 臂由恒拒「仅语句位」翻为遍历点即提升点:落 ct_emit_with_stmt 语句块
  (eln 先于本表达式行装配,语句序=eval 从左到右)+返回 rc 引用文本;rc 判别位=行号+
  接收者/参名/闭包尾探针串 bsum(镜像 shim 判别位口径;同行双 with `Add`/`Mul` 尾探针
  实证唯一,真撞=cc 重定义响亮错非静默)。②`trans_ty.ct` with/with_mut 值位型别=
  闭包体尾型(新 fn ct_with_clo_ty;语句位时代硬编码 with_mut=v/with=i 在值位即错,
  `let w = m.with_mut(...)` 曾落 `void t_w` 声明;形参型绑定镜像 ct_emit_with_stmt
  env2)。**坑**:Call 臂 `cal`=被调 Member 非调用节点,实参在 `e[2]`——`cal[2]` 索引
  即「索引目标非数组」(seed×cc_run 慢路可诊断,native 直 segfault 139)。
- **计划外治愈在册「with_mut 语句位破坏(b2c)」**:语句位发射面实证完好(trans_stmt
  Phase 4 在接);真因=**eval 侧** Mutex 构造 MUX 标签与 Assign-Ident 原地写 M 分支
  失配——with_mut 传内值共享盒,标量内值复合赋值走重绑路径,盒槽不换=写即丢(闭包内
  自读 10/二入口 3 实证;emit `*t_c+=7`、Rust 宿主同形可见=三臂分歧;struct 内值经
  u_set_ip 原地写穿幸存=fx_conc_mutex/r3a 形态掩盖多年)。修=with_mut 终值回写对齐
  Rust 参考语义(interp.rs MutexInst 拷入+body 后回写参数终值):内联闭包调用取 cenv,
  **Ident 接收者 env_set 换绑 MUX 盒**。**坑(双宿主可移植性)**:cx 槽内写
  (`cx_of(recv)[1]=…`)在原生侧别名传播、seed×cc_run 侧不传播(终值算到/槽写执行/
  绑定不动)=S1 在册「cx 包装表终案待执行」同族(struct 拷贝深克隆语义)——env_set
  路径双宿主一致。非 Ident 接收者(临时)写无 alias 可观察,跳过。
- **新登记**:①块体闭包体内 return 在 fn main 的 eval 臂静默吞 main(rc=0 零输出;
  `let g = || \{ return 41 \}` 最小形,check 面过;尾表达式块体正常;发射面「闭包体
  return 泄漏」在册怪癖同族,归闭包泳道待查)②块体闭包静默无诊断=响亮性缺口(同①)。
- **新锚**:compiler/test/fx_with_expr.ct 八面(let 嵌套/同行双 with/调用实参/if 条件/
  with_mut 值位+写可见/闭包体内非尾/语句位/return 位嵌套)入 smoke 3b。
- **门**(t27-anf 分支基线,8cf14ae3 之上):smoke --full **190 ok/0 fail**(基线
  177/0 保持+对端新腿+本件 1)+meta_check 555 全绿+suite 自举 103/104·宿主 104/104
  (唯一分歧=03m L4 生死探针,主树基线同败在册)+fx_with_expr 双臂逐字一致+
  fx_conc_mutex 双臂 7/7+min 形四连(标量写回 10/10/struct 恒等/块体尾表达式)绿;
  decl 锁 513 不变(+1 ct_with_clo_ty 仅入 cc_emit,cc_run 口径锁零新增)。

## 2026-10-06 续批(t27-cloret)——块体闭包 return 流收编+捕获面写回终式

- **块体闭包体内 return 静默吞 main 清偿**:真因=`call_cv` 闭包臂把体内 return 的
  "r" 流裸传(具名 fn 边界 call_decl_vals 本就收编为 k 只放行 "a";spawn 位点
  sr.kind=="a" 判别本就吸收;Rust 宿主 Ok(v) 同口径)——裸调用表达式位直穿到外层
  语句流,main 被当成 return 结束(rc=0 零输出)。修=call_cv C 臂+with_mut 内联路
  两处同式收编(镜像 call_decl_vals)。fx_cloval 扩三面:纯块体 return/条件早退
  return+块尾值混形/经 fn 值间接调用——双臂逐字一致(107/105/25/41/20/-2/14)。
- **修好边界即翻出被掩蔽旧账**:r3a 于自举臂首次真跑(修前其测试被 "r" 流静默吞掉
  从未执行=Rust 臂绿+发射臂绿双掩蔽),暴露**捕获面 with_mut 写不持久**(bump 二连
  1!=2;env_set 换绑只活本调用流 env,跨调用共享 cap 原样)。终式=**双路并施**:
  env_at 绑定盒槽原地换(native 直用+捕获双形态唯一持续路)+env_set 流换绑
  (seed×cc_run 直用形唯一传播路)。**引擎劈叉实证**:Flow struct 拷贝在 seed 深
  克隆 cx 列表(S1「cx 包装表」同族),槽写落克隆体;捕获形 seed 仍断(cap 调用间
  不回写)=旧账维持登记(native 1/2/3 vs seed 1/1/1)。
- **随批代修**:tests/fb7_loop_relet.ct(对端 FB-7 夹具)缺 test 块违反 meta 行为
  语料规则——main 体原样入 test 块(0809df8a 代修先例),meta 复绿。
- **门**(t27-cloret 分支基线,c877adac 之上):smoke --full **193 ok/0 fail**+
  meta 全绿+suite 分歧=fb7/fb8 宿主列(对端在册 parity 债,非本批)+fx_cloval/
  fx_with_expr 双臂逐字一致+捕获探针 native 1/2/3+min9 双引擎 10/10。

## 发射面 std use 静默 fail-open(2026-10-07 调查立案;根因已定罪,修复待批)

- **症状**:用户程序 `use std.crypto.{sha256_hex}` 在**无 CTRON_STDPATH** 时
  `ctc.sh emit` **rc=0** 产出必然编译失败的 C(符号只有调用点无定义,返回型
  错标 int);同场景 check 显式 E2020 rc=1、run 带 W8902 出路指引——**唯 emit
  哑**(fail-open,违 D7 哲学)。
- **根因(探针实证,CTRON_EMIT_DEBUG 门控已还原)**:emit 驱动的
  `pkg_load_use` 在 std 根解析缺位时**静默保留未展开 Use 节点**(合并后 file
  顶层仅 [File, Use, main] 三元;带 STDPATH 同管道 = 8 节点全量合并,产物含
  原型+调用+**定义**三件齐 12 处引用)。trans 对未解析调用名直出 `t_<名>` +
  缺省 int 型。
- **连带真缺口(独立)**:`StringBuilder` 内建类型发射臂缺失(产物
  t_StringBuilder/t_push_str 未定义;与 std use 无关——该型为内建,非 std
  合并面)。
- **修复提案(小批)**:emit 驱动 pkg_load_use 后,对残留 `Use` 节点
  (segs[1]=="std")出与 check 同文 E2020(或 W8902 带出路)并 rc=1——
  fail-closed 对齐;实现位 driver_emit.ct 合并后检查点,低冲突。
- **调查坑**:①emit 缓存键不含编译器身份且宿主 stdout 重定向进产物 C
  (探针输出一直在 .cache/emit/*.c 里);②ctc.sh emit 面必须
  `rm -rf .cache/emit` 后再以 env 直跑方能观测管线;③合并 file 顶层含
  非数组元素(P1b 戳),`[0]` 探针即崩「索引目标非数组」。

### StringBuilder 发射臂实现地图(2026-10-07 调查定案;2026-10-08 落地)

- **语义锚(interp)**:值 = cx 复合 ["SB", seg1, seg2...](eval_call:112 ctor;
  push_str = cx.push 段表原地变;to_string = 段连接;len = 段字节和
  [eval_expr:277])。
- **emit 降位点全图(trans_expr)**:①ctor Call StringBuilder()→ 现落泛型
  t_StringBuilder(无定义);②成员 push_str → 泛型兜底
  `t_+m(ct_arg_cast(objt,p0,obj))`(≈行 1306);③成员 len → 型别分派
  ("L"→list->n / "N"→ctron_len 动态魔数 / 其余 strlen;≈行 417);④to_string
  → 按接收者码分派(ctron_i32_to_string 等;sb 句柄现错落 i 桥)。
- **实现路径(定案,2026-10-08 全通)**:SB 构造调用的 ct_typeof 返回 **"N"**
  (trans_ty.ct Ident-call 臂;二义动态型,ctron_len 魔数分派先例)→ ③len
  自动落 ctron_len(扩 SB magic 返 strlen(data));②push_str 独名特臂
  (trans_expr `objt=="N"` 门 → ctron_sb_push 句柄形;struct 自定义方法接收者
  u: 码不命中);④to_string 的 N 通道 → ctron_sb_str 魔数动态(真串透传不动,
  SB 落 arena 拷);①ctor 臂 → ctron_sb_new。
- **runtime(定案落地形)**:ctron_sb { magic, char* data } 单缓冲形(段表语义
  在 interp,emit 单缓冲等价,推入序一致)。与地图原案三处实测修订:魔数 =
  **"CtronSB1"→0x4374726F6E534231ULL**(原案 "CtrNSB" 六字节卡 ctron_len 的
  strlen>=8 门——前 8 字节内含 0 则 strlen<8 直接落 strlen 臂返 6,必须八字节
  全非零形);句柄 = **char\* 直伪装**(原案指针 int32 往返未用——N 槽 C 侧本就
  指针宽,直传零成本);推入 = **arena 拷追加**(GC 堆无 realloc,镜像
  ctron_str_concat 拷形)。
- **验证**:03i 探针双臂逐字一致(seed interp vs 发射 cc_run:hello world/
  len:11);03j 纯 test 块文件发射臂 rc=0(含空串 push/len==0 边角);smoke
  224/0 基线维持。
- **边界**:`.len`/`.to_string` 的 "N" 通道要求 SB 句柄永不与真 I32 撞
  (句柄=堆指针魔数,值 I32 恒小 → magic 不匹配即走原路 ✓);用户自定义
  push_str/to_string 方法(struct 方法)不受独名特臂影响(特臂仅
  receiver=裸 StringBuilder 构造链——落地取 ct_typeof=="N" 型别通道单验:
  struct 方法接收者码为 "u:<名>",天然不命中;SB 与真串假性撞魔数需字符串
  恰以八字节魔数开头,与 list 魔数同类已接受)。

**G1-T6 长跑 P99/RSS 双柱载荷探针(2026-10-10,提交随批)**:`tests/gc/p99/` 通道流水线形
(churn 工人 6×2000 轮×10 波短任务=段跨波回收;主线程逐 token concat churn=GC 差分载体;
桶界 1/5/20/100ms 五桶;net 回声原形挂账=net 家族对端 U8 平面 ABI 手术期且基线红在册,G1 批内归因)。
双柱三遍(darwin arm64,2MB 阈值):gc-off max RSS 528/565/567MB 单调涨(bump 无界实锤);
gc-on 381/382/380MB 三跑恒定=稳态平台签名;停顿尾巴 w 桶 120000,0,0,0,0(r 同)——本负载下
收集停顿 P99.999 级无 >5ms 样本。前证链:a_jwt 41GB→70-74MB(COVERAGE 1549-1563)。
**反证探针一并入册**:长命工人单任务 20k 轮段内单调涨(gc-on 亦 2.5GB)=任务段生命周期粒度
的边界实证,任务内长循环归 Region.heap(T4)管辖——设计 §3.1 案 B 限制条款的活体复现。
T32 翻面剩余证据面(长驻负载 P99/RSS)本探针补齐,与四件旧证合流呈 J2 翻面裁决(G1-T7 执行)。

**G1-T7 GC 默认翻面(J2 裁决落账,2026-10-10)**:`CTRON_GC` 默认开(0/off 退回 bump)。
证据面合流:a_jwt 41GB→70-74MB(COVERAGE 1549-1563)+ T6 双柱探针(gc-off 528-567MB 单调涨
vs gc-on 381/382/380MB 稳态平台+停顿尾零)。翻面后门:gc 三档 9/0+seg 4/0+region 3/3+p99 6/0
+bench 比值 1.000(≤1.15 门)+suite 115/116·116/116(唯一红 07a E4010=基线预存自举能力分歧,
翻面零增量)——**语义套件在默认 GC=on 下整体验证首次达成**。db 门 interp 臂红形落地为在册豁免
的具体形态:GC=on 下 interp 报 `byte_slice: range out of bounds`(T32 在册「db 编译器臂豁免,
interp GC=1 病理」域,emit 臂=真实消费路径全过);漂移 5+E2080 legacy 红为对端域预存。

**G1 批终登记(2026-10-10,提交链 31474191→697dcdf8)**:T1 arena 段化基座(块链+ct_arn+TLS,
收集器全链扫描+空超级块两代惰性释放)/T2 任务段接线(pthread 解除永久退避,coro 维持;探针
seg 4/0+RSS 49-54MB 有界)/T3 通道边界值语义(malloc 中转双向拷贝+E3021 双宿四注册+负例 06g+
expect s 码 cast;窗口压测 3/3)/T4 Region.heap()(dispatch 助手+跨增长 panic;region 3/3)/
T5 StringBuilder reserve/clear+增量 push(三镜+03j 双臂)/T6 P99 双柱(gc-off 单调涨 vs gc-on
稳态平台)/T7 翻面(语义套件默认 GC=on 首次整体验证)/T9 seed 债复测指引。**残余在册**:T8
repeat 字面量(F25 语言面)未执行——既有计划 2026-10-09-repeat-literal-plan.md 独立成批,下轮
执行;smoke 全量与 ctron gate 21 步终跑挂=共享树自举被对端 trans_stmt WIP 断(其落地后补跑,
当轮口径=gc 族 9/0+seg 全腿+suite 115/116·116/116+p99 6/0+bench 1.000);coro 档退避维持(协程
迁移 TLS 段不安全,登记);通道 List/struct 载荷跨界=E3021 限面(真深拷挂 G3 半端类型化);
expect() recv 接收者 elem 码擦除不可恢复=G3 一并治。

**T8 repeat 字面量批(2026-10-11,W1+W2+W3 落库 93896aed/9aea1313,W5-lite 随批)**:`[v] ** N`
全链(词法 munch/Repeat 专节点/sem arr_fit_gate 三钉 E2010.arrfit·runcount·repneg/解释臂块物化
[零逐元素 eval=comptime 预算前置]/发射臂 let 初值位零块 memset 特判+F25 正解/短字面量禁令
[I32[8]=[1,2] 双口径 E2010,chk+run 同门]/ctecho 活标本改写 [0]**512 3/3)。**W4 已落(75d42f94)**:const 位聚合发射双红销账——Const 臂 ceval 失手 CVM 回退(comptime fn
循环填表全链收口,TAB=build() 折叠 {0,1,4,9} 实证)+ctc.sh 暂存发射(伪 .c 治理,失败零残留)。
**F25 全域关闭**(局部 memset 特判+const 位静态聚合;字面量膨胀根因=巨型零字面量 AST,双面断根)。
挂账仅链式 const 引用(const 呼名后续件)与 count const 名=后续小波。**F25 实证:U8[65536]=[0]**65536 前端 peak RSS 75MB(F25 病理形态 3.4-5.8GB,三数量级降;产物 memset,运行 0)**。门:suite neg 39/39·119/120+120/120
(07a 预存)+ctecho 3/3;pos 夹具 4+neg 3 双宿。

**G3 线程定型批(2026-10-11,计划 2026-10-11-steno-g3-thread-plan.md;W1/W2/W3/W5 落库
e083d902/23f7089a/a74b8956/b32d79e1)**:W1 scope 退出 join-all(spec §07 兑现,ct_scope 任务链
+走查,已 join 跳过,panic 重抛复用)/W2 sleep_ms·sleep_ns 脱网内建(nanosleep;interp 忙等口径
文档化)/W3 join_t·recv_t·scope.cancel(pthread timedwait 正典,coro 回落文档化;Mutex with 不响
应取消=v0 清单;取消风暴双形态)+顺带修 R 码 let 成员调用 init 注解→#elem 直驱线(match Ok
载荷槽 int32 截断指针根)/W5 parallel.map·reduce 接线真 fork-join(T33 件转正)+半端面实证修通
(Channel 字段/形参、Sender 注解全通=硬禁裁决翻修通)。门:tests/thread 8/0 双形态+suite
119/120·120/120+gc 族全绿。**W4 已落(b1add3c1):TaskPool 全链**——N 工作者+无界队列(排空后
停工),任务段纪律=每入队件 ct_shim_tramp 上下文跑(段=件粒度),码面新字母 x=池句柄(初选 k
撞 Task 在册码已迁),ctor 双链+pool.spawn 入队面+五处注册,夹具 200 件×4 工作者双形态。
**G3 批终=五波全落,tests/thread 10/0**。半端截断在册坑销账;新坑入册:`/* */` 非 Ctron 注释
(.ct 源=词法垃圾级联炸 main 探测)+trans_ty.ct 非 UTF8 损坏字节(管线级 sed 事故,邻位语义修复
,文件已纯净)+正则插行黏行(自举静默崩)。

**G2 dlopen 补完批(2026-10-11,计划 2026-10-11-steno-g2-dlopen-plan.md;W1/W2/W3 落库
523ff123/9278769d/随批)**:W1 dlopen_global(RTLD_NOW|GLOBAL 独立内建)+dlerror_str 诊断面
(libz 全域链端到端 zlib=1.2.12);W2 #[dlsym(from_handle)] 句柄 thunk(逐调解析不缓存,原型/
实参剥句柄参;ct_ext_sym dlsym 先于 link_name 顺序修正+from_handle 检测抗叠[attr_arg 首括号
语义]);W3 旗标真消费(#[link(name/search/framework)] 三形态+CTRON_LDFLAGS env 逃逸口,build
父壳自收集)+dyn_so 专道(cc -shared 真插件双形态+负例 2/2)。**sherpa-onnx 形态接入成立**:
dlopen_global(库)→#[dlsym(from_handle)](h, ...) 逐调调用。挂账:根 ctron 驱动链接行同面
(随原生驱动器 lane);链式 const 引用;坑入册:无三元血律再踩(静默崩)/#[dlsym] 按声明名解析
(真符号走 link_name 无引号家法)/未注解 let 句柄 i32 截断三踩/attr_arg 首括号语义(叠 attr 相斥)。

**G4 收尾批(2026-10-11,计划 2026-10-11-steno-g4-closeout-plan.md 0e07a27a)**:cdylib 产物形态
(--lib:darwin -dynamiclib+install_name @rpath;linux -shared -fPIC)+universal binary(--universal:
darwin 单遍 -arch x86_64 -arch arm64,fat 验证 lipo)——纯驱动面,发射 C 零变化;门:tests/ffi/dyn_so
4/0(真 .so 双形态+负例+cdylib 回环 777+universal fat)+suite 119/120·120/120(03m 超时=负载 flake
复跑定谳)+gc 9/0。**P1-6 关闭**:旗标消费(G2-W3)+cdylib+universal 齐备;签名/公证不入 ctron(J8);
根 ctron 驱动链接行同面随原生驱动器 lane(登记)。

**编辑器 I/O 内建自举化(2026-10-16;`compiler/src` 九面 + `lsp/src/main.ct`)**:
LSP/管道三内建 `read_line`/`read_bytes`/`flush_out` 从「C seed 专属」
(`compiler-c/src/rt_eval.c`)移植进自举编译器——四处注册(`sem_calls.ct` prel /
`sem_type.ct` prelude_ok / `parse_pkg.ct` pkg_is_prelude_sym)+沙箱与 comptime 副作用
名单(`plugin.ct` sbx_banned_call / `sem_comptime.ct` comp_banned_call)+解释臂
(`eval_call.ct`:read_line→`vS(read_line())`、read_bytes(n)→`vS(read_bytes(iv))`、
flush_out→`print(out)`+`flush_out()`+清空 out)+发射臂(`trans_expr.ct`:
`ctron_read_line()` / `ctron_read_bytes((int64_t)(…))` / `fflush(stdout)`)+型别
(`trans_ty.ct`:read_line/read_bytes→"s"、flush_out→"v")+C 运行时模板
(`driver_emit.ct`:两宿主原语)。顺带 parity 修正:`eval_call.ct` 的
`byte_at`/`byte_slice`/`utf8_enc` 索引实参接受 tag 1/2/3(I32/I64/U64;旧仅 tag 1=
自举 sem 拒 I64 索引,与 seed 分歧)。

**实证**:seed 路径(cc_run.ct 解释)与自举原生(ctron-cc)双口径 `read_line`(行去
\r\n)/`read_bytes(n)`(恰读 n 字节)/`flush_out`(即时落盘)逐字一致;`smoke.sh` 0 处
「发射/编译失败」,trans_v0-v6 往返逐字一致,黄金对照 input_cc 1-3 逐字一致;
std 全模块自举自测全绿(fmap 149s / crypto 222s 慢件,余秒级)。

**LSP 自举化副修(`lsp/src/main.ct`)**:①顶层 `const` 改零参 fn——自举解释器未把顶层
const/static let 曝光进函数体词法域(seed 支持、自举 gap,见下),LSP 本就以「解释器
友好」为设计前提,函数承载常量双后端皆可见;②`p0/p1: I64` 显式标注(自举 sem 正确拒
I32←I64,seed 过宽)。**成果**:VSCode 扩展 `ctronc` 可指向自举统一驱动
`compiler/bin/ctron`(包装脚本 exec 真身;macOS `_NSGetExecutablePath` 不解析软链,直
软链 rc=127),LSP initialize 0.19s、全会话(didOpen 诊断/symbol/hover/completion/
shutdown)rc=0。

**在册 gap(未治)**:自举解释器顶层 `const`/`static let` 不进入函数体词法域——
`eval_call.ct` `call_decl_vals` 仅以形参建 nenv,未并 `statics_env(file)`;seed 经全局
表可见。触发条件=函数体内读顶层常量(编译器源码无顶层常量故未暴露,LSP 改写前实证
`unbound:CHR_CTL`)。

**G5-W3+W4 收官(2026-10-11,W3 5b542142 + W4 a258f6cd/随批)**:W3 流式体读三件
(client_open_tls/read_body_tls/close_tls,HttpOpen 游标结构,CL 明长 v0)——Range 206 零
lib/http 改动(extra 头+状态判定=调用方,next_start 纯计算面承载);W4 lib/down 策略面四件
(range_extra/next_start/retry_backoff/hash_ok;IO 循环留消费方=跨模块类型闭包债绕行,down→
http.client fn 原型不随合并发出[在册])。tests/down e2e 3/0(本地 TLS serve 40KB×down 循环:
落盘+抽样+长度容差);**2B 尾差在册**(40958/40960:TLS close_notify 竞态 vs 块计数,done=0
同面,下次专场);坑:Box 形参在册不可发射(helper 抽 fn 撞墙→内联展开)、while 体 Drop 局部、
实参位 Repeat 泛化不可达(let 绑定绕行)、夹具名避模块同名、域 STDPATH=lib/std。G5 四波全落
(W1 sha256 流式/W2 https/W3 流式三件/W4 lib/down);秘本模型拉取全链=https+流式+哈希+续传。

**G6 收官(2026-10-17,0c35cde2)**:SQLite 绑定 P1-7——vendored amalgamation 3.54.0
(lib/db/c_src/sqlite3.c+sqlite3.h 原样;shell.c/sqlite3ext.h 不取)+ lib/db/sqlite.ct 门面
MVP 面(open/prepare/step/bind 标量四族/column 取值/errmsg/errcode/exec 多语句/last_rowid/
changes/close;事务 v0=exec("BEGIN"))。J5 形态=微垫片 ctron_sqlite.c 只翻 open/prepare 两
T** 出参为返回值+rc 静态槽(pg connect 直返 fd 同形),其余全直宣 sqlite3_* 原型(bind.ct 单
行纪律);能力 token SqliteDb/sqlite_std() 自有不 use db.ct(严格互斥树);TRANSIENT=-1 绑
定自拷;col_text NULL 胞返空串;str_from_c 内建深拷 errmsg。家法扁平淡入偏差在册(设计 §8
的 lib/db/sqlite/ 子目录形=解析链未验证形,MVP 不冒险)。tests/db/sqlite run.sh emit-only
门 10/0 双跑稳定(interp 无 extern 运行时;decl smoke 入 interp 臂;sqlite3.o 一次编译
-DSQLITE_OMIT_LOAD_EXTENSION);夹具八件=use 解析/往返/bind 复用/errmsg 负面/rowid/
文件库持久/exec 脚本+ROLLBACK/col_type 四类。坑:垫片 NULL 须自带 stddef.h(仓库垫片不
带标准头惯例);管道假 rc 再犯(| tail 吃 run.sh 退出码);夹具 ok 标记须与文件名 tag 对齐;
meta_check 泳道 skip 子句随批(gotcha #17;存量 18 处红=各批在册债不动)。blob 双向/
SQLCipher/预编译 .a/多线程连接共享挂 P2。

**tests/db 存量红清偿(2026-10-17,217bac50;G6 基线复跑立案)**:G6 批复跑 tests/db 得
43/11 两遍全同,11 红三类皆存量非 G6 扰动——①漂移守卫 ×5 恒红(比对两侧均不存在的
lib/std/db/*.ct ↔ stdpkg 种子,布局 A' 后 db 出引导种子域;守卫环删除);②EXTRA 陈旧路径
×5($ROOT/db/... 缺 lib/ 段,clang 必红;改 lib/db/ 后五件定点绿);③pool_core interp 三
异常在册(E2080 let-再赋值编译拒=声明面裂缝成熟态已改 var;var 修复后 interp 4-5 分钟慢
跑且 assert_eq 失败[emit 同件绿=pool 域 interp/emit 分叉];println 探针变体 segv 139 零
输出)——按 x_fd_edge 先例转 x_pool_core 仅 emit 臂入计,interp 侧待查=登记债。修复后全量
**48/0**(54 项−守卫 5−interp 重复计 1,口径自洽)。账面注:设计档记 56/56 为守卫生前期
口径,54/48 差异=守卫消亡+夹具增减,已在案。

**G7 收官(2026-10-17,W1 805fbf21 + 脚手架 e51563d7 + CI 迭代链;设计 §9;J9 mingw-only+x64)**:
dlopen 桥 Windows 垫片=发射前言 `#ifdef _WIN32` 宏重定向四名(LoadLibraryW[CP_UTF8→宽字符
=CJK 路径]/GetProcAddress[空句柄=GetModuleHandleW(NULL)≈RTLD_DEFAULT]/FreeLibrary/
FormatMessageW→UTF8 dlerror 读即清)+RTLD 宏零值;随批补发射运行时最小 Windows 面(setenv
→_putenv 拼装/mkdir→_mkdir;盘点=缺口仅此两件+dlopen 族,dirent/nanosleep/stat
mingw/winpthreads 自带)。**intptr 根修**:dl* 桥 (long)→(intptr_t)(trans_expr 三处+
from_handle thunk 一处)——Windows long=32 位,HMODULE 截半再符号扩展=GetProcAddress 必空,
POSIX 等宽零扰动;七轮 CI 归因 B 腿始绿,dlopen 桥 Windows 全链实证(LoadLibraryW→
GetProcAddress→调用→FreeLibrary)。ci.yml 增 winpre(ubuntu-24.04 seed+emit 四件预发射 C
[cc/chk/emit/drv])+windows-smoke(msys2 MINGW64+gcc 四件[--stack,16777216;并链 cc2 宿主
口] +tests/win/smoke.sh 五腿+真 ctron build 腿[best-effort]);runner 钉 24.04+门自装
clang/lld(wasm32 门自持)。tests/win/smoke.sh 双平台同源(POSIX 本地 6/0):Windows 腿终态
=A hello/B dlopen DLL/B dlerror 负例/D __sync 绿+C 并发/E CJK 两腿响亮在册 skip(下详)。
**CI 迭代十四轮在册发现**:①spawn/chan 跨线程 winpthreads 段崩溃(步进定位=spawn-ret 后
send/recv 段;本地 POSIX 绿——并发运行时 Windows 面=后续批次);②CJK 名可执行文件
exec/loader 级失败(启动期死);③CI msys2 CJK 命名与 Windows 可见名分歧(cp exit0 而
PowerShell Test-Path=False,LoadLibraryW 无 file 可指——垫片真文件面由 B 腿全链承载);
④env 的 CJK 过不了 msys2→原生 exe 边界(len 51 vs 63=6 CJK 字符逐个压 1 字节;argv/文件
通道能过);⑤PE 不自动导出 DLL 符号(插件须 __declspec(dllexport));⑥ld(collect2) 写
CJK 输出路径脆性(编译器读 CJK 源路径无恙);⑦Windows 用户程序默认栈 2MB(PE 旗标
--stack 平台条件化)。**wasm32 坑随批**:minilibc 声明面缺 fgetc(b95c2b16 read_line 进
wasm 前言)/nanosleep/cond_timedwait/setenv(b95c2b16+G3 面)——bare+wasm32 双树补齐
(审计驱动:发射前言全量进 wasm 编译面);drv_parity report() FAIL 补两臂输出(一致静默
=真错被空 diff 吞)。**meta_check 十九处清偿**(E1002 注册+六组泳道 skip,全数=各批欠
注册非新伤);站点参考页全量再生成(db/sqlite.md 本批欠账+对端五域存量)。挂账:lib/proc
Windows 面(ctron.exe 腿 best-effort,对端 proc WIP 在飞);并发运行时 Windows 面;工具链
宽字符 IO 面;RTLD_DEFAULT exe-export 须 --export-all-symbols;coro RT 不入 Windows。

## 2026-10-17 Stage 0:steno-verify 秘本审计链独立验证器(steno 泳道 G 批收官后第一棒)

**定位**:采纳路径 Stage 0(steno-verify 纯计算 dogfood,gap-fill 设计 §11;G1–G7 全落库后
唯一未动项)。前置「秘本侧计划可见」以规格落库自就:`docs/superpowers/specs/2026-10-17-steno-verify-format-spec.md`
(stenolog.v1 审计链 + stenolicense.v1 license 双契约:固定字段序紧凑 JSON、core=最后一个
`,"hash":"`/`,"sig":"` 标记前全字节、hash=SHA-256(core)、sig=Ed25519(core)、创世 prev=64 零、
验证器零再序列化逐字复算;独立对拍协议=openssl 双向)。

**交付**:`steno/verify/`(steno-verify 单二进制:main 薄壳 + app/chain/license/selftest/util 五模块,
`use steno_verify.<mod>` 依赖路径发射,todo_app 同款 20480B 家族绕行)+ `tests/steno/run.sh`
验收门 **24/24**:selftest(SHA-256 RFC 6234 三向量+真密钥全链自签自验+五负例)、keygen
(0600 落盘+pub hex)、sign-chain/chain 三事件全环、负例七枚(payload/签域篡改/sig 翻字符/
首行截断 seq 跳号/错公钥/非法 JSON/空链合法+缺文件 rc2)、license 四态(真伪/机器绑定合与
不合/过期 strict rc1+只报告 rc0)、**openssl 双向对拍**(A:openssl 私钥签链我方验+我方
sig 经 `openssl pkeyutl -rawin` 验;B:同 core 同钥必同 sig=Ed25519 确定性)。

**随批根修 lib/std/str.ct**:`trim_left`/`trim_right` 双重步进 bug(if 分支内 `+=/-= 1` 后分支
尾又步进一次)——每枚尾部空白多吃一邻近字符(`"abc\n"`→`"ab"`);奇数空白串才显形,存量
测试全偶数串侥幸绿。修复+奇数串回归钉五枚,`ctron test lib/std/str.ct` 绿;零其他消费方
(修复前 grep 全仓=0 调用点),smoke --full/suite 无涟漪(suite 119/120,唯一红
07a_cap_fs_inject E4010 与 HEAD 版 str.ct 同红=共享树存量在册非本批)。

**发射臂形状纪律(本批实证增补,编译器在册缺口的窄化记录)**:①命名空间 Member 调
泛型 Result 返回 fn(`json.parse(...)`)发射生成坏 C(t_json/t_parse 未声明)——一律
`use std.json.{...}` 裸名;②容器载荷 Result 的 match 绑定臂 Ok(es) 运行时段错误(标量载荷
绑定可用)——判别走 `is_ok()`+`let es = pr.expect(...)` 先绑后用;③expect 严禁内联进调用
实参(实参错位可打出 expect 消息本身);④`#[trusted] extern "c" ctron_prog_args_n/at`
直调=裸执行 argv 唯一发射面(Env.system()=解释器专臂);⑤write_excl_0600 返回 1=新建
(非 0)。

挂账:审计链流式校验(全文读入为 v0 口径)、payload 业务语义注册表、多签/链头外部 anchor、
PEM/Base64 密钥面(全 P2,契约 §6);跨包 use 图含 selftest 未 pub 符号时 emit 静默吞全图
(b4 现象一次,pub 化后消失,未最小化复现=观察在册)。

## 2026-10-18 Stage 2 预埋:engine-proxy 基准族 + soak 门(steno 泳道;设计 §11 三线的 Ctron 侧机器)

**定位**:Stage 0(steno-verify)收官后秘本泳道剩余唯一纯 Ctron 侧批——§11 验收三线的
机器与基线预埋;Stage 2 正式 A/B(Rust steno-cli 对照)待秘本仓存在后进行,真 sherpa 对拍
同样留秘本仓。

**交付**:
- **tests/bench/engine-proxy/**(RTF 线):100 会话 × 10 段 × 20 帧 × 256 样本(16kHz =
  320s 音频);逐帧 LCG 样本 + 定值算术核 + `sv_stub_work` FFI 忙循环(c_src/stub.c 两侧
  同链同 -O1,周期等效);顺序形 vs src/ref.c 同构 **digest 逐字节钉**(diff 除 rtf_ms 行)
  + ×3 取 min + 比值门 ≤1.2;RTF 自报(mono_ns)。实测比值 **0.976–1.001**(三跑),RTF
  0.005。`ctron bench engine-proxy` 注册表接线(根 ctron sh 文本[live 权威]+ driver
  mirror 同步 + native.sh 重编)。
- **tests/steno/soak/**(内存线+零崩线):TaskPool(8) 池化有界并发 × 100 会话 × 20 段 =
  2000 任务;逐任务一 I64 过 Channel,主端全量求和(加法可交换 ⇒ digest 与到达序无关、
  逐字节确定);三臂(默认 GC on / CTRON_GC=off / CTRON_RT=coro)各 rc=0 + digest 钉
  306971647 三臂同值 + 无 panic 文案 + max RSS pin(/usr/bin/time -l):**实测 1.9MB
  (三臂同档),界 64MB 只拦无界增长**——G1 任务段段回收的直观变现(2000 任务零爬升)。

**并发面探针实证(增补在册)**:spawn 句柄出 scope 即断型别(`let t = scope{…s.spawn…}`
出块 int32 化)——并发值收集的正典形 = pool/joinall 夹具同款「闭包内 Channel send + 主端
计数 recv」;TaskPool 无 wait 面,完成 = recv 计数;闭包捕获 Channel 半端 + 标量混捕 3 枚
可用(半端作**形参**仍是在册坑,闭包捕获面无恙)。

**Ctron 宽检首秀**:engine-proxy 首版 LCG 乘法 I64 溢出,C 参照静默回绕而 Ctron 臂
"integer overflow (*)" 硬崩——digest pin 门把两侧算术语义差当场捉出;修法 = 乘前先归约
(seed % 2^31 再乘),两侧同步。基准对拍由此多一道「语义同律」副产品价值。

挂账:真 sherpa/onnxruntime 对拍与 Rust 版 RTF 对照(秘本仓);benchmark 时值 pin(只钉
digest,时值漂移靠 min-of-3 报告);多进程/断点续跑 soak(Stage 2 正式批面)。

## 2026-10-18 smoke 六红清偿:自发射×2 根修 + pkg verify×4 批C 落地(编译器/驱动器)

**背景**:smoke 基线 1009 曾 236/0 全绿;G 批时代(1010–1017)各批只跑自家门不跑全量
smoke,六红积压至今。本批逐枚归因清偿,**全量 smoke 237/0 首次恢复全绿**(237=夹具
增量的新基线)。

**① 自发射×2 根修(CC2 宿主口发射前言缺位)**:CC2 批(2026-10-16)给 eval_call.ct
加了 `ctron_str_byte_t`/`ctron_utf8_enc_t` extern(解释臂宿主口,host_shim/cc2_str.c
malloc 版,十进制文本参协议)——该 extern 只随 native.sh 链进解释器;编译器自发射
(smoke §4 cc_run.ct→原生解释器 / §5 固定点 cc_emit.ct)产物调同名符号而发射前言无人
定义 → 链接失败(全量仅两枚未定义符号,inventory 实证)。修 = 发射前言补 GC 委派两行
(strtol 解十进制 → 转调既有 int 面 ctron_str_byte/ctron_utf8_enc,driver_emit.ct;
发射臂 GC 优于 shim 的 malloc 有界泄漏)。黄金三件+负例拦截+固定点全绿。

**② pkg verify×4 = 原生驱动器批C 暂缓面落地(S3-β 复放腿)**:W4 波3 批2 的
"--deep 复放腿挂账批C;复放计数恒 0"——smoke 期望(复放 N 用例/E5056)早已按 sh 真
行为写好,原生驱动器(Ctron 自写)却硬编码 0,四红 = deep 正例/E5056 漂移拒载/γδ/
发布旅程同根。修 = sh :1343-1495 逐字移植进 compiler/driver/src/main.ct:traces/*.ctrt
块游走(trace_parse_ctrt)→ fns 唯一集 → use <pkg>.<mod> 合成面走 **parse_pkg S2a
deps/<pkg>.ctart 密封回落既有面** → trace_build_drv 码gen(WSB/MemFs/esc 谱批3 同
机械)+ deep_run_cc(cwd=TRP,固定名净除同 trace_drv_dir 登记偏差账)逐例 ctron-cc →
逐字比对(E5056 点名双值/计数如实;陈旧信任证据 guard;attest E5057 腿原样)。
实测:正例"复放 2 用例"绿;漂移件 rc=1 "E5056 trace mismatch: base_mul(3, 4) 记录 12
实际 13"。

**坑位**:字面量 `.{ ` 裸花括号即插值(合成 use 行两踩,`\{` 双处);chk 对 driver
main.ct 本就 18 枚 E2010 短字面量存量(emit 链非 chk 门,新码随文件惯例)。

挂账:smoke 时点积压的制度面——G 型批次(只跑自家门)合流后应跑一次全量 smoke 再销账
(六红潜伏 8 天的教训)。

## 2026-10-18 todo_app 原生 e2e 红账清偿(26/0 两相位全绿;休眠夹具三连修)

**背景**:2026-09-26 红账"原生臂暂被编译器在册发射缺陷阻断(主分发预算家族)"——该
缺陷已被近期编译器批次治愈(同源发射路径 steno/verify、engine-proxy 均在用),红账期间
夹具休眠无人再跑,后续三笔破坏/欠账积压其上。本批逐层剥壳清偿:

1. **Ctron.ctcl 缺 caps**:E4010(net.listen/net.connect 细键,T11 晚于夹具)——补
   `caps = ["net.listen", "net.connect"]`(ctecho 同形)。
2. **run.sh 链接路径陈旧**:`$ROOT/net/c_src/` → `$ROOT/lib/net/c_src/`(0930 库根
   布局正名从未达此,emit 破期间 cc 步不可达)。
3. **app.ct 字节道迁移**:hashmap 泳道(1009)net 写读面换 `ctron_view_w8u`,休眠夹具
   仍是 I64[] 码点道——9 处迁移(7 签名 &I64[]→&U8[] + buf/scr 声明;cnt 计数器保持
   I64;助手本为 ASCII 字节操作,语义零变)。

**终态**:`sh examples/todo_app/run.sh` **26 pass / 0 fail**(相位一全链 HTTP 探针
24 项 + 相位二重启持久化),红账注释正名为治愈登记。老"分发预算家族"判词一并销账
(复现族若再现应指 emit 链当前回归,非旧账)。

## 2026-10-18 pkgs/tui W3a 落地(终端界面域包骨架;TUI 泳道开工)

**定位**:T2 系统面域包(POSIX libc 自写胶水,零第三方零编译器税);设计
docs/superpowers/specs/2026-10-09-tui-design.md(1009 十二裁决全闭)首波。零 compiler/
改动,完全独立泳道。

**交付**:`pkgs/tui/`——①c_src/tui_term.c(termios 快照+cfmakeraw 留 OPOST+VMIN=1/
alt screen+隐光标+鼠标+清屏顺序钉死/leave 逆序+atexit 兜底双保险[铁律 §8 任何退出路径
必还原]/poll wait/TIOCGWINSZ 回退 80×24/SIGWINCH 旗读即清/quit 胶水旗——进程级事实
才落 C 侧,MUV 纯度裁决 5/6);②tui.ct 门面(#[trusted] extern 窄桥十枚+生命周期面
[enter 非 tty panic 防呆]+核心四类型形状定案:TuiBuf SoA 九表/TuiTree 十表/Event I32
码具名修饰槽[enum 换装登记债]/Theme 14 字段显式传参——纯数据无逻辑,W3b 起拆子文件
本门面 re-export);③tests/tui/s1_link 夹具 + tests/tui/run.sh for 阶梯(§12;headless
默认 stdin </dev/null,--run 才进真终端)。门 **s1 绿双跑**。

**探针定案(W3a 实施首件,§11 登记销账)**:interp 臂用户 extern = 须符号宿主内链
(ctron-cc 无 tui_* 即"extern 符号未找到"逐字诊断)——**TUI 门=发射臂单臂**(设计 §1.3
"门以发射臂为准"落实);纯 Ctron 面(Theme 色编码,W3d)仍可 seed 测。若未来需 interp
臂:host_shim 惯例可链(cc2_str.c 先例),触发式登记。

挂账:W3b(buffer/布局/渲染/diff+s2–s5)/W3c(解码状态机+run_app/test+panic 消息
重放+s6)/W3d(主题三档+s7)/W4(tui_widgets 五组件+examples/todo-tui 验收应用+s8)。

## 2026-10-18 pkgs/tui W3b 落地(buffer/布局/渲染/diff;TUI 泳道第二波)

**交付**(纯 Ctron 面,零 extern;门 s2–s5 与 s1 同阶梯,发射臂单臂):
- **tui_buffer.ct**:buf_new/put/put_attr/put_wide(主格+尾格 wd)/fill/line(越界静默钳除
  §3.1)+ buf_at/buf_fg_at 读面(headless 断言原型,d_cell 前身)+ cp_wide 粗判表
  (0x1100 起宽,emoji 精确表登记债)+ str_w 逐码点宽 + sgr_fg/bg 24bit 编码(-1=39/49
  默认直通)+ buf_diff(§5:行内连续段首定位光标顺写/SGR 状态机记 last 避重复发码/
  尺寸变=清屏全帧/尾格跳过)。
- **tui_layout.ct**:solve_sizes 三尺寸模式(定长先扣→pct 向零截断→fill 均分余数左→右
  逐格)+ 静态 E 码四枚(E8601 len 负/E8602 pct 负/E8603 非法模式/E8604 定长和超容器
  且无 fill;裁决 1 分层口径,渲染期越界=钳除不崩)。
- **tui_render.ct**:builders 合并式(vbox/hbox/text/para/bar/box/spacer/with_id/with_h/
  with_w/with_cls/with_al/with_title)+ wrap(word wrap 空格断点+CJK 逐字断+宽字符不拆
  半+行尾容不下整宽提前换行+width<1 防御钳)+ box 三样式边框(single/double/round,标题
  嵌上边线,标题暂驻 al 槽登记偏差)+ bar 反色面(accent 底+theme.bg fg)+ cls 令牌面
  (muted/accent/err/ok/warn/border/bold/under/rev)+ DFS 渲染(vbox 列/hbox 行/交轴 al
  偏移/自然高递归)。
- **门**:s2 渲染冒烟(边框角/内容格/fg 色)/s3 布局三模式(求解精确值+余数左→右+四枚
  E 码)/s4 折行+CJK(wrap 五向量+para 落格)/s5 diff 字节断言(单格/全同空/默认色恢复/
  宽字符尾格跳过——逐字节精确比对)。**5/5 双跑绿**。

**坑位入册(本批新钉)**:①发射臂 **List[Bool] 下标写=坏 C**(raw int 入 char* 槽;
List[I32] 有 cast 正常)——属性槽 0/1 整形替身(登记债:发射器 Bool 槽 cast);②struct
值**重赋值**发射坏 C(t_cr int32 化)——分支构造改纯函数偏移助手;③**tree_append 结构
索引偏移**:子树合并必须 nfc/ns+off 平移,否则子树内"孩子下标 1"落进新树=自环
(s2 E8604 迷踪根因,solve 探针实证 main=2 fixed=4=len/4 自指);④**emit 缓存键=主文件
哈希,pkgs/ 子文件变更不失效**(bare 泳道在册坑二次踩:s3/s4 假绿两轮)——改包内文件必
`rm -rf .cache/emit`;⑤相对 CTRON_STDPATH 下 pkgs 域解析失灵(门用绝对路径无恙,交互
探针被误导两轮)。

## 2026-10-18 pkgs/tui W3c 落地(输入解码/run_app/test 驱动/panic 消息重放;TUI 泳道第三波)

**交付**:
- **c_src/tui_term.c 扩展**:输入解码状态机(§6 全表:CSI 方向/H F/1~7~4~8~2~3~5~6~/
  SS3 变体/F1–F4/ESC char=Alt+char/\r\t\x7f/Ctrl 族 C0 对齐/UTF-8 多字节码点/SGR 鼠标
  ESC[<b;x;yM press 面)+ ESC 歧义 25ms 窗 + 环形输入缓冲 + 事件胶水全局 + getter 族 +
  **stderr 重放缓冲**(§18:enter 全缓冲/leave 还原终端→fflush→复默认——panic 消息落
  还原后主屏)+ tick 间隔胶水。
- **tui_event.ct**:EV 种类/键码常量全 fn 出面(§3.3 跨文件 const 双坑规避)+ poll_ev
  (C 产物 → Event 映射)。
- **tui_driver.ct**:run_app(生命周期 §8 顺序钉死/事件驱动重画 §19/tick 面/resize 合成
  EV_RESIZE/小终端降级屏 §18「终端太小」/静态布局 E 码 panic 诊断)+ test[M] headless
  驱动 + TuiTest + d_key/d_char/d_act/d_cell/d_frame/d_expect_text(坐标诊断)/
  d_expect_find。
- **门 s6 双臂**:s6_decode(piped stdin 七向量:上/下/字符 x/Enter/SGR 鼠标 btn0@(4,2)/
  Backspace/独立 ESC)全 PASS;s6_panic(script pty 供真 tty:断言 alt(1049h)→还原首段
  (1006l)→重放标记(tui-replay-marker)有序)。**8/8 双跑绿**。

**坑位入册(本批新钉)**:①**发射臂 Box 链式写面整族缺口**(`boxp.v.field=x`/
`boxp.v.list.push` 即 ct_stmt:struct 字段赋值未支持/Member@——interp 臂同形可用,双宿
不对称)→ 模型按值传(run_app[M] 签名 m:M 非 Box[M];标量走 List 槽引用语义,42z 形
态);②**数组字面量入 struct 槽=元数据错位**(在册"短字面量"坑新面:`P0 { arr: [7] }`
即 index-目标错乱)——模型槽一律 List;③**pty 尾字节竞态**:script 收到子退出即闭主
端,尾部未读字节丢弃——pty 断言锚远离尾部(用还原序首段 1006l 非收尾 1049l);④
**write 必须 write-all 循环**(SIGWINCH 无 SA_RESTART,单发 write 被断 25/29 尾四字节
实证);⑤结构体 var 未初始化声明不可用(一律字面量全槽构造);⑥推断形 struct var 重
赋值坏 C 而**显式注解形可用**(`var x: T = a; x = b` ✓——W3b 坑②窄化);⑦泛型内部
调用 M 不在实参位需显式 TypeArgs(E2060 示速记法)。

## 2026-10-18 pkgs/tui W3d 落地(主题+色能力探测三档;TUI 泳道第四波)

**交付**:
- **tui_theme.ct**:theme_default()(深色;bg="default"=终端默认直通,§7 好公民差异)+
  parse_color(自 render 迁入,色编码纯函数归位)+ 色能力档(fn 常量 mono/16/256/24)+
  **sgr_fg/sgr_bg 降级路由**(24bit 直写/256 最近邻 6×6×6 立方+灰阶 232..255/16 最近邻
  xterm 标准表平方距/mono 无色码;bg 面同构)。
- **C 胶水**:tui_color_level 探测(NO_COLOR→mono;COLORTERM truecolor/24bit→24;
  TERM direct→24/256color→256;余 16;缓存一次)+ CTRON_TUI_COLOR 测试钩子(门确定性)。
- **门 s7**:四档 env 注入(24/256/16/mono)+ NO_COLOR 标准面——LEVEL 判读 + 24bit
  FG 精确码 + mono 默认 fg 无码。**10/10 双跑绿**。
- 随批:s5 diff 字节断言夹具钉 CTRON_TUI_COLOR=24(降级路由落地后字节面按档走,断言
  需锁档;env_set=前奏内建裸名)。

挂账:256 档 bg 默认直通(49)与 16 档 fg 默认(无码)不对称面复核;主题分发包
(pkgs/tui_themes)候需求;256 灰阶阈值粗判登记。

## 2026-10-18 pkgs/tui W4 批1(tui_widgets 三组件)+ 合并单元 decl 窗口丢失症(编译器泳道立案)

**交付**:`pkgs/tui_widgets/`(裸文件形态,gui_widgets 同款;消费方双导合流单一名域,
「use 面缺什么,发射面就缺什么」双向适用)——①list(纯 prop;只物化可见行天然虚拟化/
选中行 rev 高亮/超宽截断)+ list_scroll 三界钉(§14:sel<top→sel;sel≥top+h→sel-h+1;
界内→top);②statusbar(交替键反色块/说明 muted,gap 分隔);③progress(千分比避 F64
双宿主坑,`━`/`░`);④焦点助手 focus_next/prev(环绕)。门 s8(render+buf_at 直驱断言:
三行 list+选中+progress 640‰=6 填 4 空+statusbar 面位)。**11/11 双跑绿**。

**随批布局真修**:natural_h 嵌套容器落 0(vbox/hbox 无真案)——list 组件被外层算 0 高,
内层 solve main=0 必 E8604;补 vbox=子自然高求和/hbox=取最大。

**编译器泳道立案(本批主发现):合并单元 decl 窗口丢失症**——pkgs/tui/tui_driver.ct 内
特定函数序列后,部分函数 decl 整窗未注册(本批:d_press/d_type/d_act/d_cell 四枚连续
丢失,d_frame 起复现;同名换名/注释摘除/参数增减/字段写替代全数无效=位置窗非名字窗;
单文件独立 chk 同发,自包含二分 harness(head.ct+切片)可复现)。W3c 前车:test/d_* 家族
经 TuiTest[M] 泛化重构(发射臂 fn 值 arity≤3 上限→回调装 tt 槽单参 script)后触发本症,
**该族现持出主链**(facade use 行摘除,重构稿丢弃),归编译器泳道批C 归因后回填。
伴随症状:E5030 "." 同名 decl(名域污染信号)。窗口边界=内容位置非函数身份。

**挂账**:W4 批2(input 字素编辑/dialog 模态/todo-tui 验收应用+s8 全量);test/d_* 家族
回填(批C);scroll 任意子树视口原语(list 虚拟化已覆盖主需求);input 光标请求槽
(TuiTree cx/cy,现版反色块替身)。

## 2026-10-18 pkgs/tui W4 批2(input/dialog/todo-tui 验收应用;W4 组件库收官)

**交付**:
- **input 纯函数助手族**(键翻译驻宿主,组件只出 helpers+render):input_insert(码点
  插入,字素下标切分)/input_backspace/input_left/right(域钳)/input_clear/input_render
  (文本+光标反色块——§13 光标请求槽 v0 替身,登记偏差)。cur=字素下标(cp_at 起始判定
  + 三字节步进)。
- **dialog builder**:round 盒+标题+lines+钮行(焦点钮反色/余 muted);v0 树内居中盒
  (mask 暗化/overlay 栈登记债)。
- **examples/todo-tui 验收应用**(§10 旗舰):list+input+statusbar 全组件消费;键位
  ↑↓/k j 移动·a 新增·打字+Enter 提交·Esc 取消·d 删除·q 退出;NDJSON 式行文件持久化
  (先删后建 0600)。**TODO_TUI_DEMO 演示模式**(设计外增量,CI 确定性):tick 150ms 逐
  枚注入脚本键(TODO_TUI_DEMO="a|s:buy milk|RET|q";macOS script 不转发管道 stdin 在
  册——s6_panic 当时靠 tick 路径侥幸)。
- **e2e 门**(examples/todo-tui/run.sh,script pty 供真 tty):**4/4 双跑绿**——新增落盘/
  退出还原(1006l 锚)/重启回载持久化/删除幸存。tui 门 11/11 同步维持。

**坑位**:①List 无 remove 面——删除=重建过滤列表(值语义重赋);②门面命名惯例
  <域>/<域>.ct 硬约束(widgets.ct 不解析,tui_widgets.ct 即愈);③naked 包组件的依赖
  面 = 消费方 use 面全盖(「use 面缺什么,发射面就缺什么」双向);④index_of 两参形
  (三参 from 形非 pub 面)。

挂账:input 真 LED 光标(§13 cx/cy 槽)、dialog overlay 层与 mask、textareas(二期)、
test/d_* 家族回填(批C)、mouse motion。

## 2026-10-18 帧寄存器池扩容 + W5a 接线完成(ctml 运行时版全通;TUI 泳道)

**帧寄存器池扩容(编译器发射模板,批次起点)**:ct_frn 池 32→128/作用域栈 sv 8→32
(trans_stmt 三处 pn 注册点+六处 spn 面+driver_emit typedef)——纯 headroom 扩容零语义
变化(scope save/restore 机制不变)。重编全净,tui 阶梯与全量面无回归。

**W5a 接线完成(tui_parse 核 c67d0f06 之上)**:门面 +use 行/tui_render tree_attach
pub/natural_h 显式 hmode 优先(len/pct 缺省才走标签自然——spacer 显式 len/1 曾被按 0
计,自然和 7 ≠ 求解面 9 → 内层 E8604)/natural_h para width-aware(自带 width 的
para 按其宽折行,槽高=渲染行数)/wrap 贪心重写(词单元+分隔保留:ASCII 连续窄字符
=一词空格分隔贪心行内保留,CJK 逐字独立单元无分隔代价,超宽单元独占行)/box 标题嵌线
摘线型门(round/double 亦嵌,§5 不分层)。**门 s10 全绿,全阶梯 12/12 双跑**;
todo-tui e2e 4/4 维持。

**wrap 算法更正(原版逐空格断行)**:原实现遇空格即断(词行化),width 10 下
"word one two" 得三行;贪心正确形态 = 词单元 + 空格仅作行内分隔(放不下才断)。
s4 全向量维持绿(原五向量均窄宽,两版等价)。

**接线三件执行注记**:①门面 use 行(「use 面缺什么,发射面就缺什么」双向再证:
tree_attach 未列即 E2020);②tui_render tree_attach pub(解析器=包内子模块走合并
私面,附着力经 s10);③s10 夹具(嵌套 box 标题/三模式/para 折行/注释/自闭)。

挂账:{expr} 槽/bind 活问(W5a 后续小批);合并单元 decl 窗口丢失症(批C,本批
池扩容未涉);input 真 LED 光标;dialog overlay/mask。

## 2026-10-18 批C 病灶精准化:泛型 struct 携 fn 字段作参数型(合并语境注册腐蚀)

**二分定案**(test/d_* 回填试验,driver 内逐 fn 增量+每步 chk):触发条件 =
**fn 的直接参数型为 `Box[泛型struct携fn字段[M]]`(或值传同型)且处于合并多模块语境**:
- 参数名被注册成模块级 decl → E5030 "use 导入同名 decl:<参数名>" 且**随参数名走**
  (tt→t 改名全程跟);
- 同型嵌于 fn-type 内(`script: fn(Box[TuiTest[M]]) -> Void` 形参)反通过;
- 独立单文件(非合并)同形无恙;
- struct 本体声明(泛型+fn 字段)与 fn-value 调用(uf(mm,e) 经局部绑定)皆无恙。
症状即前判"decl 窗口丢失症"的真身:非位置窗,是**该形参族的 decl 注册腐蚀**
(E5030 只是首个可见症状,腐蚀或兼致同窗口内其他 decl 丢失——四 d_ 函数整窗未注册
与此吻合)。归编译器泳道批C(疑似 parse_pkg 合并/sem 泛型形参注册路径)。
池扩容(ct_frn 128)与本症无关(先后证)。

## 2026-10-18 批C 病灶三次精化:跨文件泛型 struct 参数型=类型名泄漏为顶层 decl

**增量二分**(真 context 逐件剥:struct→test→tt_frame):触发形状 =
`fn f[M](x: Box[TuiTest[M]])`,其中 **TuiTest[M] 声明于同包另一文件**(跨文件泛型
struct + fn 字段含跨模块类型)。合并期 **类型名泄漏为顶层 decl** → E5030 撞名:
- struct 与 fn 同文件共置:泄漏名 = **参数名**(tt/t,随改名走);
- struct 跨文件:泄漏名 = **类型名**(TuiTest);
- 独立单文件同形:无恙(zzprobe 三变体全过);
- struct 本体声明/fn-type 内嵌 fn(Box[...]) 形参:无恙;
- **AST 完好**(ctc.sh ast dump:Fn/Param/Named Box→TuiTest→M 逐槽全对)= 合并期
  decl 名槽对比的泄漏,非 parse 错。
归编译器批C:parse_pkg 合并名槽(或 sem 泛型实例化把参数型引用物注册为顶层 decl
路径)。池扩容(ct_frn 128)与本症无关先后证。
**形状触发性勘定(增量 neg 包试验)**:同形状(struct 携 fn 字段+同文件直参数型 fn)
在**全新迷你包不触发**(zzprobe 与 s11_neg_pkg 双验)——病灶依赖 tui_driver 的累积
上下文(状态性腐蚀指向合并视图跨载共享/表索引渗漏,非纯 AST 形状)。**复现配方**
= tui_driver.ct 增量三步(struct TuiTest[M] → test[M] → fn tt_frame[M](tt:
Box[TuiTest[M]]),facade chk 即 E5030 随参名);zzprobe 三变体包法仍可作归因起点,
但归因须在 tui-driver 真 context 进行(批C)。

## 2026-10-18 {expr} 槽/bind 活问落地(数据驱动绑表;W5a 后续小批)

**交付**:tui_render +slot_resolve("{key}" 槽扫描→绑表查值;未识键原样保留)+
bound()(树 1:1 克隆,txt/al 两面走槽解析;结构索引原样——与 tree_append 的偏移
面不同,克隆全量拷贝索引恒等)。**绑表 = keys/vals 平行 List 数据驱动**,应用每帧
重建(模型在手零成本)。门 s12:bar/text 混合文本/未识键三断言。**13/13 双跑绿**。

**新发现两枚(编译器批C 邻案)**:
1. **编译器本体 OOB**:emit 期 byte_slice: range out of bounds——触发 =
   `index_of(byte_slice(x, a, b), s)` **调用结果内联作首参**(let 绑定替身即愈);
   编译器自身 byte_slice 在处理该 AST 形状时越界(批C 邻案;let 绑定规避纪律在册)。
2. **List[Str] 参过泛型 fn 值 trampoline = int32 编组错**(Str 返回同族)——fn 值
   回调编组白名单=标量族限;bind 形态改数据驱动绑表零缺口(gui bnd Void+List 面
   实际同坑,gui 未直呼故未显形)。

**撞车坑位**:ctml {key} 槽与 Ctron 字符串插值 {var} 语法撞车——源串须 \\{ 转义
(插值常量折叠期即解析,{var} 未声明 → CC 期 t_var 未定义错)。

## 2026-10-18 池扩容全量验证批(ct_frn 128 上市后首次全量 smoke)

75cb7395 池扩容(ct_frn 32→128/sv 8→32)落地时仅 tui 阶梯验证;本批补跑全量
smoke --full:**237/0 rc=0 无回归确证**(自举固定点/std 种子包/deep 复放/双臂并发/
黄金对照全节绿)。运行时模板变更 = 全发射面波及,此后模板类变更一律全量 smoke 随批
(制度口径与 G 型批次合流同规)。

## 2026-10-20 TUI 批C 再勘定 + 两处发射缺口落库(编译器泳道)

按 516a6fbf^ 的 W4 实验块与 W3c 原稿,在 tui_driver 真 context 重建触发形状
(`TuiTest[M]` 泛型 struct 携 fn 字段 + `Box[TuiTest[M]]` 直参数型 + test/d_* 族):
- **① 原 E5030 合并期 decl 名槽泄漏不再复现**——全族 `check` 绿(decls=123),名槽 dup
  面已自愈(intervening 无 parse_pkg 改动,倾向 1011–1020 其它面归一)。
- **② 真阻塞前移为发射面泛型 struct 单态化内名不一致**:生成 C 中同时出现
  `t_TuiTest__U5_Model`(构造引用)/`t_TuiTest__I`(typedef 只此一枚)/`t_test__u_Model`
  (函数单态化)三套 mangling → `cc: use of undeclared identifier 't_TuiTest__U5_Model'`。
  归泛型实例码(`ct_struct_inst_code`)与 typedef/函数单态化命名统一,深水批(未做)。
- **③ 顺带修两处发射缺口(本批落库)**:
  - `trans_ty.ct`: `Box[T].v` 显式解引用旧按字段名 `v` 查表 → ""→"i",嵌套读
    `box.v.field` 全链塌 i(批C 泛型族发射根因之一);现内槽经 `ct_ty_code` 归一
    (结构→`u:<名>`,标量→域码)。
  - `trans_stmt.ct`: struct/Box 字段复合赋值 `field op= rhs` 旧 panic;现镜像 Ident
    路径(6/W/7 域检查帮手,余 C 复合算符;含 `tt.v.frames += 1` 类)。
  门:`check cc_run.ct` decls=531 不变;trans_v0-v6 往返逐字一致;fx 子集
  (gstruct/own/opti32/drop/arr_byval/with_expr 全绿;boxalias/optstr 两 DIFF=基线
  同指纹,非本批回归)。

### 批C 根因闭环(2026-10-20 续,同日二勘;代码级定位)

上条②「三套 mangling」根因定位到两处代码级缺口(均 `trans_ty.ct`,批C 真身):
1. **`B:` 码丢实例实参**——`ct_ty_code`/`ct_typeof` 对 `Box[泛型struct[实参]]` 产
   `B:<基名>`(:243/:1463 取 `nt[2][1][1]` 裸名),实例实参整段丢失:`TuiTest[Model]`
   的 Box 码 = `B:TuiTest`(应为 `B:u:TuiTest__<enc>`),`.v` 解引用无法回推实例码,
   typedef/字段替换亦无从下手。
2. **`ct_call_infer_tys` 无 Box 分支**(:2397)——现三分支只覆盖 裸 Named 型参 /
   `Ref` 型参 / 泛型 struct 直参(:2433);形参 `Box[泛型struct[TPar]]` 全部落到缺省
   `ct_scalar_ty("I32")`(:2407)→ 单态化名 `__i`。实证:生成 C 同时出现
   `t_d_expect_text__i(t_TuiTest*…)`(推断落 i)与 `t_TuiTest__U5_Model` 构造引用
   (构造面 `u:Model` 编码),`t_test__u_Model` 又是第三套(函数名单态化)。

**治法(单独立项,建议 批C-2)**:①`B:` 码携全内码 `B:u:<名>__<enc>`(:243/:1463 两处
产码点同改);②`.v` 解引用与 `box.field` 自动解引用改走实例字段替换
(`ct_field_code_inst`,内码 `u:` 直返);③`ct_call_infer_tys` 增 Box 分支(拆 `B:` 内码 →
`ct_struct_inst_split` → 按形参 TArgs 映射 tpsn);④`CT_TDEF`/函数单态化统一以该实例码
命名。回归面警示:`B:` 码格式变化波及全部 `Box[T]` 存量(typeof/emit/调用点/field),
须全量 smoke --full + suite 双列 + gui/tui 阶梯。

### 批C-2 W1–W3 落库(2026-10-20 三;泛型实例码贯通,残余一处深水)

计划 `2026-10-20-batchc2-box-generic-mono-plan.md` 前三 Wave 落库:
- **W1** `B:` 码携全内码:产码点两处(trans_ty.ct 注解/构造)改 `B:` + `ct_ty_code(内型节点)`
  (`Box[TuiTest[Model]]` → `B:u:TuiTest__U5_Model`,实例实参不再丢);`ct_ctype` B: 臂改
  `ct_ctype(内码) + "*"`(`B:u:Point` → `t_Point*` 与旧同果)。
- **W2** 消费点内码归一:trans_ty Member B: 臂(内码 `u:` 前缀→实例字段替换
  `ct_field_ty_inst`/基名字段查;`.v` 直返内码)/ trans_expr 两处 `t_<名>*` cast 改
  `ct_ctype(内码)` / trans_stmt Drop 登记名 `ct_struct_name_of`(内码基名)。
- **W3** `ct_call_infer_tys` 增第四分支:`Named Box` + 载荷泛型 struct 形参——实参码
  `B:<内码>` 拆 `ct_struct_inst_split` → `ct_inst_arg_codes` → 按内层 tps 位置映射 tpsn
  (旧落缺省 I32)。
- **加映**:单态化体 `genv` 补绑**型参本名**(`ct_spec_call_emit`,tys 码)——体内
  `ct_targ_env(M)` 旧找不到 → 注解塌 `__I`。

**门**:decls=531 不变;trans_v0-v6 往返 7/7 逐字;fx 子集同基线(boxalias/optstr 两
DIFF=既有指纹);Box 探针(boxmem/fnfield)emit 绿。

**残余(深水,W4 未落)**:单态化体内 `Box[TuiTest[M]]` 注解仍有路径未经 `ct_targ_env`
代入(trans_stmt hoist/with_mut 位已补,emit 免声明链残余)→ 生成 C 仍偶发
`t_TuiTest__I`。注:批C 复现件 emit 已 rc=0(构造面 `__U5_Model` 正确),仅声明位
错配;W4 需定位该残余产码点(嫌疑:ct_emit_inst_typedef 的 `ct_inst_of_ty` 无 env)
后回填 test/d_* 家族。

### 批C-2 W4 勘定(2026-10-20 四;真最深缺口=闭包形参无签型)

W1–W3 落库后复跑批C 复现件:emit rc=0——构造面 `__U5_Model` 正确、`test__u_Model`
体内局部声明亦正(型参本名入 genv 生效)。**残余=d_* 族单态化仍 `__i`**,根因:
- script 形参 `fn(Box[TuiTest[M]]) -> Void` 的实参为**闭包字面量** `|tt| { … }`;
  `ct_emit_clov` 形参槽 = 无型 `ct_i`(期望签名未穿入闭包形参 env);
- 闭包体内 `d_expect_text(tt, …)` 的 M 自 `ct_typeof(tt)` 推断只得 `i`/`N` →
  `d_expect_text__i`,与 `test__u_Model` 实际传入的 `TuiTest__U5_Model*` 错配。
- (旁证:ctor typeof 补 env 代入后,构造/局部声明两面 `__U5_Model` 均正确。)

**治法(批C-3 立项,两径待裁)**:
A. 期望 fn 签型穿入闭包形参(`ct_emit_clov` 增期望签参/调用位 context 传签);
B. TuiTest 去泛型化:`reframe: fn() -> TuiBuf` 闭包字段承载重渲染(captures ≤8 既有),
   `d_*` 全非泛型——绕开闭包内泛型推断,但需验 struct 返回过 `ct_clop` 面。
**本批落库**:ctor typeof env 代入(trans_ty.ct 构造产码点);门同前(decls=531/
trans_v0-v6 7/7/boxmem 绿)。

### 批C-2 W4 补充勘定(2026-10-20 五;重复绑定澄清 + FnType typedef 下钻落库)

- **澄清**:单态化体 `genv` 对型参本名的绑定**早已存在**(`ct_spec_call_emit`
  「型参名 → 实参码」块;批C-2 期间误判缺失所加的重复绑定已撤)。
- **落库**:`ct_emit_inst_typedef` 增 FnType 下钻——fn 值形参/返回(如
  `script: fn(Box[TuiTest[M]])->Void`)的载荷实例 typedef 旧不生成 →
  `cc: use of undeclared t_TuiTest__U5_Model`。门:decls=531 不变、trans_v0-v6
  7/7 逐字、boxmem 绿。
- **批C-3(闭包形参签型)仍为 test/d_* 回填的唯一在册阻塞**,两径待裁
  (A 期望签穿入 / B TuiTest 去泛型化),见上节与本计划 W4 注。

### 批C-3 收口 + native.sh 自举回归根因闭环(2026-10-21 六;径A 落库,三缺口一追补一批)

**背景**:c9e58977 移交的 native.sh 发射 cc_emit 确定性崩溃(`ct_expr:index 目标非
List/数组:i 名:pty 下标:0`)复现锁定,顺藤清偿 批C-3 两径待裁案。**六层真因一次批内
全落**,编译器源五处小改:

1. **trans_ty.ct `ct_call_infer_tys` Box 分支自落地即死码(f1e0aee3 引入)**——
   `} else if pty[0]=="Named" && seq2(pty[1],"Box")…` 带**双前 `}`**,错挂到外层
   `if pj < ag.len` 而非内层型别分支链:`pj < ag.len` 时永不上链(推断恒缺省 I32,
   `d_*` 族 `__i` 残根的真身),`pj ≥ ag.len` 时越位引用块内 `var pty`(块级绑定)
   → 发射编译器自身时翻译该条件表达式即 panic = native.sh 崩溃真因。嵌套归位后分支
   激活,直参面推断即正(探针 `d_expect__s` vs 修前恒 `__i`)。
2. **批C-3 径A(裁决落库)**:期望 fn 签型穿入闭包形参 env——调用位 'F' 码 +
   StructLit fn 字段位(既有 P0-A 两面)之外,补**第三绑定位:fn 注解 let + 闭包
   初值**(trans_stmt Let 臂,`#clcodes` = `ct_fntype_inner(ct_targ_env(注解))`)。
   三面贯通后闭包 shim 形参正型(`t_TuiCase__S* t_tt = (…)(long)_p0` + GC 槽注册;
   修前 `int32_t t_tt = (int32_t)_p0` 整型截断指针 = 运行期内存腐蚀级)。
3. **typedef 声明序(W4「声明位错配」真身)**:driver_emit.ct 预扫只认顶层 Named
   注解,FnType 签名内实例(Box 载荷 `B:u:X__Y`)晚到 spec 点惰性直出 → 原型前置区
   引用未声明型(探针:t_run_script 原型 503 行先于 CT_TDEF 520 行)。预扫增
   `ct_ty_node_insts` 下钻(FnType 形参/返回递归 + B: 剥内码收集),参数/返回/
   let 注解三位接线。
4. **sem 面 E2060 宿主口径对齐**:自举 sem 调用点推断只认裸型参位绑定,
   `Box[TuiCase[M]]` 深位出现恒 E2060(宿主 sem.c `ty_has_tp` 结构化出现则放行,
   suite 双列差实证)。sem_type.ct 增 `sem_ty_has_tp`——**白名单制**逐类节点形
   下钻(Named 壳内 TArgs/TupleT 直接元素/FnType 形参容器+返回/Ref/Slice/
   Optional,深度 8 闸;初版无差别 tn[2] 递归把 TupleT 名字槽裸串当节点 =
   SIGSEGV,04_generics_comptime 实证后改形),miss 判定改「全实参位零出现」
   方红;值面单化仍归发射面推断。
5. **追补(#fret 注解位)**:注解 FnType 的 `ct_ty_code` 短码恒 `F<arity>` 无返回维
   → `ct_fsig_ret("F1")` 落 "" → `#fret` 不绑 → fn 值 Str 返回塌 ct_i(assert_eq
   位裸指针 `==` 非 strcmp,`const char*` 原型化缺失)。注解位改走
   `ct_fsig_code` 全签码(与直调声明面同源)。
6. **W2 `.v` 直返内码勘误(陈旧 bin 掩蔽期暴露)**:批C-2 W2 的 typeof 侧
   `e[3]=="v"` 恒直返内码,载荷 struct 自带 v 字段时(ctecho `Box64{v:I64}`,
   `port_box.v`)与代码发射侧 `->v` 字段读劈叉 → `real.to_string()` typeof 塌
   `u:Box64` 落 struct 臂 panic。基线 bin(97303043,无 W1-W3)同源掩盖两日,
   bin 刷新批即暴露。改字段存在性优先(`ct_field_ty_node` 缺省 None 探针;
   泛型载荷 `ct_field_ty_node_inst` 代入),无 v 字段方落载荷解引用。
   smokelock 随批:decls 锁 531→532(sem_ty_has_tp +1,前注惯例)。

**回归钉**:`tests/03o_generic_box_mono.ct`——Box[泛型struct携fn字段] 三形状
(直参推断/注解闭包 let/调用位闭包实参);修前 ②③ 塌 `__i`+截断,①死码;
种子 3/3 + 发射 rc=0(修前发射臂 assert_eq 红实证双列差非解析噪声)。

**新立残债(G2,本批不治)**:裸 fn 引用蹦床(`let g = pick`)对 Str 形参丢 cast
——`#fval` 蹦床体 `return (ct_i)t_pick(_cbp0)` 直传 ct_i 入 `const char*` 形参
(cc hard error;03h 标量族不触)。归 P0-2/T54 fn 值签名携带面,独立账。

**门**:json_fidelity 19/0 双臂(**出臂债随 bin 刷新首清**)+ native.sh 全程
rc=0(八件 bin + 用户驱动器全自现行源,c9e58977 移交案销账)+ suite 双列
120/121·121/121(03o 新钉入册即绿;唯一红=07a E4010 caps,在册零分歧旧账)
+ gui 阶梯 101/0(d_* 真 context 全绿)+ meta_check 632 文件全绿 + smoke
23a-2 前先在双制残差非本批**(interp 推断形/native 写出形 F64 `32.0`↔`32`
八例 + opti32 none 两面;conc 走 spawn 边界型别分派,23a-2 钉子未覆该形,
随 23b/双制尾册);decls/ctecho 两红本批自愈,csv/strconv/time 三红为负载
flake(独跑同版即绿)。**余册**:conc 八例(23a-2 双制尾册)、tui Wave-4 回填
(tui_driver test/d_* 原稿 + tests/tui/s13 夹具)自此解锁待新批;G2 蹦床;
种子发射 trace 恒 main:1(v1 桩形在册);IP-23 23b 下游(db/web)未动。
### 批C-2 W3 回归撤销(2026-10-21;对端 c9e58977 移交登记的修复)

对端移交:native.sh 自举发射 cc_emit 自 d8c4d3b0 起确定性失败——
`ct_expr:index 目标非 List/数组:i 名:pty 下标:0`(gui_ds_gen 第7形参 pty:List[Str]
型别码塌 i),二分表 97303043 GREEN / f1e0aee3(批C-2 W1-W3)RED。
本批复现同指纹后逐 Wave 二分:**W3(`ct_call_infer_tys` Box 分支)为因**——撤 W3
(保留 W1 `B:` 码携全内码 + W2 消费点归一)后 emit cc_emit rc=0。
W3 分支对 compiler 源内某 `Box[泛型struct]` 调用误映射型参(具体调用点与条件待
批C-3 一并勘定;d_* 推断需求并入批C-3 闭包形参签型主径)。
门:trans_v0-v6 往返 7/7 逐字;boxmem 绿;check cc_run decls=531;emit cc_emit rc=0。

## 2026-10-22 批C-3A 落地(期望签型穿入闭包形参)+ W4 test/d_* 回填(TUI 泳道+编译器泳道)

**根因勘定闭环(入口矛盾解)**:上批「clcodes 已绑但推断仍 i」的断点=**Member 发射
未消费 `.v`**(Box 解引用在 C 级无 v 成员,`(t_tt)->v` 即 undeclared/错成员;连带
`typeof(tt.v)` 链)。批C-2 W3 撤销实为**误撤**:重上后 cc_emit 自举发射 rc=0
(3.0MB 产物,cc 25.9s,--version 冒烟绿)——对端登记的回归未复现(当时观测疑与
seed 栈溢出叠加,c9e58977 已修)。

**落库(编译器面)**:
- W3 重上:`ct_call_infer_tys` Box 分支(`Box[泛型struct[TPar]]` 形参自
  `B:<内码>` 拆实例映射 tps)——本批实测对 tui 泛型族推断正确
  (`d_expect_text` M=u:Model,DBG 探针逐级验证)。
- `trans_expr` Member B: 臂消费 `.v`(Box 解引用=载荷左值 `(*p)`)。
- `ct_generic_retcode` 无返回注解(None)= "v"——泛型 Void 调用旧被
  `(ct_i)(void)` 包(闭包尾非法 C)。

**落库(库面,pkgs/tui test/d_* 家族回填)**:TuiTest[M] 仅持数据
(buf/frames/w/h/m);view/update/th 走 d_* 显式形参——fn 字段调用返回 struct
过 ct_clop 面无装箱(编组白名单=标量族限,在册)的绕行:直接调用走 fn 形参
盒化既有通装(test__u_Model 实证 `*(t_TuiTree*)(long)(…)` 解盒正确)。

**验证**:批C 复现件 emit→cc→run 全绿(`repro-tui-probe ok`;闭包捕获 struct 模型
+ 闭包体内泛型调用全场景);decls=531 不变;trans_v0-v6 7/7 逐字;emit cc_emit
rc=0(自举发射回归确认修复)。

**批C 状态:全链路关闭**(E5030→型别塌 i→B: 丢实参→FnType typedef→闭包签型,
五级递进全落)。

### 双线合并:本地批C-3 收口批 × 对端 W3 撤销/重上批(2026-10-22;语义统一 .v 字段优先)

两地并行同题(本地 3bb9c5d9 六层修 / 对端 0189eca4 撤 W3 + db65e1d3 重上+径A+tui
回填)合并归一:①`ct_call_infer_tys` Box 分支取对端重上形(链位 branch1 后,单副本;
本地 branch3 后副本撤),`ct_generic_retcode` None=v 保对端;②**`.v` 语义双侧统一
字段优先**——载荷 struct 自带 v 字段(ctecho `Box64{v:I64}`)→ 字段读(类型侧
ct_field_ty_node 探针/代码侧 `->v`),无 v 字段(t_TuiTest)→ 载荷左值 `(*box)`
(对端 6 行修的形状,其对端无条件载荷左值会复破 ctecho,合并时加字段存在性闸);
③tui test/d_* 家族回填取对端(本地持出注记废止);④本地六修(嵌套死码勘定/径A
第三绑定位/typedef 预扫下钻/sem 白名单出现/#fret 全签码/W2 勘误)全数保留。decls
锁 532(sem_ty_has_tp)。门:全量重跑见下批。
## 2026-10-22 批C-2 Wave 4 收官(s13 夹具 + tui 阶梯 14/14 + 全量门;TUI/编译器泳道)

**s13 夹具落地**(tests/tui/s13_test):`test[M0]` + `d_expect_text`(渲染逐字)/
`d_key`(事件注入)/`d_frame`(帧计数)全场景——闭包捕获 struct 模型 + 闭包体内
泛型调用(d_* 无显式 targs,M 自 Box 实参经批C-2 W3 分支推断)。
**tui 阶梯 14/14 双跑绿**(s13 新增;存量 13 门零回归)。

**门**(本机环境项单独标注):
- suite 双列 119/120 · 120/120(03m 超时=负载 flake,垫片复跑一致定谳)
- smoke --full:167 ok / 65 fail——**0 发射/编译失败**;emit 往返抽查(fx_enum/
  fx_conc_spawn/fx_optstr/fx_boxalias/fx_conc_gstruct,timeout 垫片):fx_enum/
  fx_conc_spawn/gstruct 绿,optstr/boxalias 两 DIFF=扁平 R 族在册债(基线同指纹);
  fail 分类=环境(timeout 垫片缺失时段/原生件版本交错/meta_check 需 Python3.10+
  本机 3.9.6)与在册债(扁平 R 族/heap 闭包面)
- ctcl_check --selftest ALL GREEN;s13 manifest 1/1
- meta_check:本机 Python 3.9.6 不支持 `str | None` 注解(对端 3.10+ 全绿 627 文件
  =环境项)
- gui 阶梯:本机缺 freetype dlg 子模块(环境项,对端在册)——
  **2026-10-26 复核销账**:vendor/gui 三件(clay/freetype/raylib)与
  build 产物(libfreetype.a 等)俱全,gui 阶梯实跑 s71/s72 等连片
  [ok](全阶梯时长>15min,900s 窗口截断非失败);子模块形态已被
  vendoring 批取代

**批C-2 计划 Wave 4 收官;批C 全链路关闭。**

## 2026-10-22 P0-1 小批(conc 双制残差 + G2 蹦床)——三真因一批落修

(账本"建议下一棒":conc 八例唯一实红 + G2 hard error 并小批;基线 bin 同红
非回归实证后开修。修前基线 smoke --full 11 红锚 /tmp/smoke_before.log。)

1. **conc F64 `32.0`↔`32` 六夹具族(账面"八例"主体)**——真因**双谎**,非
   spawn 边界:trans_stmt Let-Scope 臂 `env_bind(…, "g")` 硬编码("g"=F32 码)
   + trans_conc `ct_emit_scope_stmt` decl `ct_ctype(rc)`(寄存器名首字节撞
   't' 偶得 int32_t,ct_zero 同误喂名)。scope 块值真型 = 尾表达式 typeof:
   fx_conc_spawn/chan/mutex/joinor/cancel 五件 I32 和经 to_string 落 F64 渲染臂
   出 `32.0`/`30.0`/`7.0`/`1.0`×2(interp 按值型出 `32` 等,fx_conc_spawn
   逐字 `32`↔`32.0` 即账面钉例);clostr 同族 `1.0`↔`1`。真 F64 块值旧径
   int32_t 截断亦在此门。修 = 尾表达式 typeof 定 decl + `#scopety:<rc>` 侧
   条目交 Let 臂绑真码 + ct_zero 喂真码;**with 尾槽分支 decl 保持旧面**
   (ct_emit_with_stmt 值面自管,#scopety 不绑,fx_with_expr 家族零扰动)。
   连带:fx_fnval_str 新夹具钉 G2(见 3)。
2. **opti32 none 两面**——真因:R 路径 match **语句位** `noneIx` 只检测不消费
   (Some/Ok/Err 三环齐备,None 臂检测后无发射,Err 块后即 `return env`;
   表达式位路径 354 有消费)→ `None => {…}` 臂整个不发射(fx_opti32
   take_opt(None) 无输出/fx_opti32b relay `d-none` 丢失,interp 照走 → 双制
   分歧)。修 = Err 臂块后补 None 臂发射(镜像 Err 结构 anySO else 链;非块
   臂守 SOME/NONE return 位 W2a 既有口径;Err/None 按 Option/Result 型互斥)。
3. **G2 蹦床(裸 fn 引用)两面**:①trans_expr `ct_cbtr` 形参分派链缺 s/N 臂
   ——`return (ct_i)t_pick(_cbp0)` ct_i 直入 `const char*` 形参 = cc hard
   error(账面指纹逐字复现;ct_fval_emit env-first 路径同链反有臂),补
   `(const char*)(long)` cast(镜像 ct_fval_emit)。②修 ① 后 cc 过而值错:
   裸 `let g = pick` 无注解,env 短码 "F1" 无返回维,#fret 不绑 → 调用位
   返回型塌 ct_i,Str 返回经 printf %d 出指针垃圾(`-92274608`)。修 =
   trans_stmt Let 臂补**裸 Ident 初值** #fret 绑定,返回码取 `ct_fn_ret`
   (与蹦床同源;decl d[4] 是返回型节点非 FnType,注解形 ct_fsig_code 门
   不适用——首版走错门即此);u:/聚合返回同式贯通盒指针解盒约定。

**回归钉**:新夹具 `fx_fnval_str.ct`(Str 形参/返回经裸 fn 引用 + I64 看护
臂,03h 标量族不触纪律)入册 smoke 3b 循环;修前 emit→cc hard error /
修后双臂逐字一致。heap 翻绿清账:lib/std/heap.ct 两处 None 臂随 2 修翻绿,
smoke 4c Darwin KNOWN4C 移除 heap(对数绿实证;opt/iter 两 known 维持在册:
闭包结构值/比较器形参 + T16 fn 字段发射=P0-2 面)。

**门**:smoke --full 修后 **240 ok / 1 fail**(唯一 fail=heap 翻绿待清账
旗标)→ 清账复跑 **240 ok / 0 fail(SMOKE-RC=0,基线 11 红全清)**;3b 循环
46 夹具全绿(修前 11 红:目标 8 + trans_v5/boxalias/tlist 三件=批内并发改源
窗口受害者,复跑自愈);decls=532 锁不变(零新顶层 fn,# 侧条目皆键);
native.sh 八件 bin 全刷新 rc=0(发射器自举链带修自持);suite 双列 harness
不在本树(对端在册),本地以 smoke --full 为全量门。

## 2026-10-22 P0-2 落地(fn 字段直调全宽面)——双臂缺形态三缝一批补齐

**勘定**(账面"fn 字段调用返回 struct 过 ct_clop 无装箱(编组白名单=标量族限)"
精确化):直调形态 `h.f(args)` 双臂皆缺,非装箱白名单单缝——
①interp `call_mem`:builtin 面→UFCS find_decl 后即 `panic("method:")`,fn 字段
无臂(探针 `method:f` 实证);
②emit typeof(trans_ty Call-Member):**名字白名单制**不认用户字段名 → 塌 "i"——
struct 返回让 `let r` 绑 int32,`r.v` Member 无臂 = `ct_expr:Member@17` 兜底
panic(白名单=标量族限的本尊;标量族"能过"实为自由 fn 同名 UFCS 蒙对或
t_<m> undefined 隐性未达);
③emit 0-arity 裸 fn 值不装箱:ct_cbtr 门 `pcs5.len > 0` 拒 0 参 → `t_mk` 裸地址
直填 fn 槽,槽协议 env 解引用即代码字节(Handler{f: mk} 直调 SIGSEGV 实证;
T54 两段式 `let v = h.f; v(x)` 靠 env≠1 时 ct_fn0 无参调用偶然存活)。

**落修**(编译器五处):
- eval_call.ct call_mem:UFCS 落空后补 fn 字段臂——U 壳 u_field 取槽,值 F/C
  壳 → call_cv 应用(与 T54 两段式同源);
- trans_ty.ct ct_typeof Call-Member:接收者 struct 字段 FnType → 返回码取
  ct_fnfield_ret(u: 聚合贯通;置于通用名字白名单前,内建名让位真字段);
- trans_stmt.ct:新 helper `ct_fnfield_fsig`(字段全签码 F<ar>{ps}>r;
  ct_fnfield_ret 只有返回维,实参对位/闭包形参码不够;仅入 cc_emit,
  cc_run 口径 decls 锁 532 零新增);
- trans_expr.ct Call-Member:UFCS 块后补 fn 字段间接调用臂(全签码定型,闭包
  实参 #clcodes 穿入;非 i/v 返回 #fret 同式原型化,u: 堆盒解引用;哨兵/闭包
  双路径;臂序 builtin→UFCS→字段与 interp 对齐);
- trans_expr.ct Ident 值位:cbtr 箱 0-arity 扩编(`pcs5.len < 4 &&
  ct_fn_decl_any` 闸——ct_fn_pcodes 对 0 参 fn 与 const/let 变量同为空表,
  fx_time const TICK 误装箱实证须 decl 判别)+ 死码 fval L 径撤除 +
  arity>3 硬错保留。

**新立边(非本批治)**:发射器裸块语句盲区——独立 `{ … }` 作用域块语句使
发射臂 `ct_expr:if 值位需纯表达式分支` panic(迷你形:`var x = ""` 后裸块内
赋值,/tmp/bare_block.ct 形 1.3s 复现);本批 self-emit 断链真因,拆块即愈。
解析归因与语句 Walker 处置另立,fx_fnfield 首版曾用裸块踩中。

**回归钉**:新夹具 `fx_fnfield.ct`(0 参 struct 返回/双参标量/闭包字段三形)
入册 smoke 3b;修前 method:f / Member@17 / SIGSEGV,修后双臂逐字一致
(7/42/42)。

**门**:smoke --full **241 ok / 0 fail(SMOKE-RC=0)**;native.sh 八件 bin 全
刷新 rc=0(新 bin 可发射新源=自举链含修自持);self-emit 3.03MB 产物 rc=0;
decls=532 锁不变;3b 循环 47 夹具全绿(joinor/time 两红均批内并发窗口受害
者,干净复跑自愈)。

## 2026-10-22 IP-23 23b 落地(db 行映射 / web json 中间件切换;§12.6 兑现)

**裁决落定(设计稿 §5 执行前需裁两案,本批入口)**:
- 裁决点 2:qtag 细分拼写 = **ni/nu/nb/nf**(23a-1 已库实现维持;jtagk "n"
  前缀兼容保存量零破坏);
- 裁决点 3:derive(Json) Big 字段映射 = **Str 字段直收**(原文保真;与
  JNum.Big(Str) 同构,零新类型)。

**落修(四件)**:
1. web json 中间件(pkgs/web/json.ct):`jd_u64`(jget_u64 U64 全域;负域/
   越 2^64-1/小数形态 → None)+ `jd_big`(jget_big 数值四态原文直收)双
   访问器 + 数值保真 test 块(2^64-1 顶格读回不截断/负域拒/越域拒/坏体
   None/Str 通道误读数值仍拒);jd_i64/jd_str/jd_bool 语义零扰动。
2. derive(Json) 插件(compiler/plugins/derive_json/src/main.ct):Str 字段
   err 半边放行 qtag n 族(穷举集 n/ni/nu/nb/nf——免生成码新增 use 依赖,
   byte_at/starts_with 均不引入);val 半边 jix_val 原文直收(既有)。写半边
   不变(Str 字段照 __esc 字符串写出 = 类型诚实逆;读直收/写字符串)。
   json_demo 三新正例(越域正/负 nb + 域内 ni)+ b 仍 type@ 负例;run.sh
   双臂 grep 随批 bigstr-ok。
3. db 行映射(lib/db/rowmap.ct):`rm_big`(整族 ∪ numeric 1700 原文直收,
   jget_big 同构;自持族门——rmx_gate 不识 numeric,rm_i64/rm_str 等对
   numeric 仍 kind 2 fail-closed 不变;形态校验可选负号+全数字,非数值
   kind 5)+ `rm_is_num_oid` + OID 表(1700 numeric)/头注 API 面随批。
   rm_i64/rm_f64/rm_bool/rm_str 语义零扰动。
4. 行绑定链路集成语料(tests/db/rowmap/rm_json_link.ct + rm_json.script):
   §12.6 请求体→分类管线→行绑定全链——同一 JSON 体 parse→qtag(nu/ni
   断言)→jget_big/jget_i64 取值 ↔ 同值 PG text 单元回放 rm_big/rm_i64
   绑定,断言原文逐字 + typed 逐值(I64_MIN 顶格)+ numeric 列对 rm_i64
   kind 2 类型面不放宽 + 缺列(k1)/越界(k4)负例。script 按 pg 帧契约
   生成:len 字段含自身 4 字节(真 PG 语义;首版 payload-only 被
   pgx_frame_check -2 拒即帧契约实证)。

**连带勘定(先在于本批,登记+随批钉)**:tests/db/run.sh interp 臂
`env -u CTRON_GC` 在 GC 默认翻转后悄然=默认开——解释器堆不回收(P5-B
在册)+ db 域模块链(pg 2088 行)合并单元 → test 执行回扫巨型堆 = GC 风暴
假死(6 行 mini 复现:use db.rowmap + 1 test,40s+ 满核;CTRON_GC=0 后
4.5s 绿)。run.sh 两处改 `env CTRON_GC=0` 显式钉(恢复 harness 原无 GC
预算语义);emit 臂不受涉。批内 bisect:call_mem fn 字段臂撤除不解;
HEAD~2 worktree seed 径同挂——先在于 P0-1/P0-2 两批之前,非本批引入。
**先在本机缺口(另册)**:①plugins emit 臂本机即红——bin/ctron-emit 裸调
缺 CTRON_STDPATH 即 E2020(S4-⑤ 三探 fail-open 面,harness 有根不受涉);
带根后 json_demo emit cc 败于 WithLists Option[Str] 槽 ct_res 初始化
(json_demo 头注在册「emit struct 值链面静默错编」域,pre-23b 插件同 2 错
实证非本批);②ctc.sh emit 对锚换靶文件的插件面同败。23b 插件面门以
interp 臂 bigstr-ok + 生成形语义探针(穷举 tag 集手写形,interp/native
双臂 `direct=18446744073709551615`)承载。

**门**:db 套件 **50/0**(基线 44/10:10 常设红随 GC 钉清账,rm_json_link
interp/emit 双臂 +2,rm_anchors 族 4.3s 绿零回归);plugins interp 臂
bigstr-ok;web json.ct test 块 4 绿;compiler/src 与 lib/std 本批零 delta
(smoke 241/0 面不受涉,免重跑)。

**随批追补(eval_call.ct)与终门**:interp 组合子面补 `is_ok`/`is_err` 臂
(check/emit 面既有;web json jd_i64 族与 std json Result 读取面 interp
直呼 panic 实证)——ct_res 统一模型 variant 0=Ok/Some 镜像 is_some。
终门(干净 native 重建后):smoke --full **241 ok / 0 fail(SMOKE-RC=0)**;
db 50/0;plugins interp bigstr-ok。
**新立边(interp test 派发,另册)**:pkgs/web/json.ct 四 test 块原序
(1,2,3,4)interp 执行 `.len 目标类型不支持` 假红——对内全部单块/双对过、
4-first 换序全绿 = 顺序状态泄漏(块间 interp 派发态),非任何单块语义;
归 P2「ctron test」挂账族勘定。

## 2026-10-22 P2 落地(含 main 文件 test 执行)——driver_run/driver_emit 双面

**挂账兑现**:「ctron test 含 main 文件的 test 执行(现仅无 main 走解释)」——
driver_run 单臂分派(`find_decl("main")` 命中即 return,test 永不跑;emit 侧
hasmain 分支 testcalls 拼而不用)双面同修:
- interp(driver_run):先测后主——run_tests 无条件先行,失败(tr.kind=="a")
  rc=1 main 不运行(fail-fast);通过则按真实执行时序续跑 main,残余 out
  按 tr+br 串联。合并前收输入文件 test 名集(mytests)传 allow——合并单元
  携带被 use 模块的 Test 块(Test 恒随),只跑自己的(eval_run run_tests
  加 allow 过滤,签名原地改零新增 fn,decls 锁 532 不变)。
- emit(driver_emit):hasmain 分支 `+ testcalls + mcall` splice(t_test_N
  失败 return 1 先于 t_main),与 interp 同构。

**验证**:①先测后主双绿(test-body-ran → main-ran,rc=0);②fail-fast
(assert_eq(1,2) → rc=1,main 不运行)——interp(拼装件)/emit(cc 原生)
双臂同判;③纯 main 文件零扰动(无 test = run_tests 空转);④共存夹具回
归:t55_mw_smoke/03j/05j/04c_or_infix 全 rc=0(clock_sanity rc=1 = 先在
net 泳道 interp 缺口,HEAD~2 worktree 同败实证,夹具头注自登记「方法调用
发射缺口」);⑤无 main 夹具族(fx_*/anchors)allow=自身名集,行为不变。

**门**:native.sh 八件 bin 全刷新 rc=0;smoke --full **241 ok / 0 fail**;
db **50/0**;plugins 10 过 1 败(唯一败=在册容器面先在缺口);web json.ct
test 块 bin 径 rc=0(`.len` 假红为 seed 径专属,上条 debit 注记照登)。
**bin 径勘误**:「4 块原序 `.len` 假红」在 native bin 径不复现(rc=0)——
该顺序状态泄漏=seed 解释径专属面,debit 归属改记 compiler-c/rt 派发族。

## 2026-10-22 P2 落地(byte_slice OOB 销账钉 + 种子 trace 桩退役)

**①byte_slice AST 越界(2026-10-18 批C 邻案)——销账**:触发形
`index_of(byte_slice(x,a,b), s)` 调用结果内联作首参,今日不复现——用户 fn
首参/参数化边界穿层/直接内联三形 emit+运行全对(index_of 本身非内建,原账
形出自 tui 语境用户 fn);治愈归因 = T31 M1.5 实参 ANF 提升批(调用实参
求值悬窗即病灶面)+ 两批实参链路落修。**回归钉**:fx_byteslice_arg.ct
(三形:调用结果内联首参/fn 参数化边界穿两层/直接内联)入册 smoke 3b,
interp/emit 双臂逐字一致。编译器自身面:native.sh 五连绿 + self-emit
3.03MB rc=0 实证源内已无触发形(let 绑定纪律遗留洗净)。

**②种子发射 ?/擦除 trace 恒 main:1——桩形退役(v1 诚实化)**:seed interp
rt_eval EX_TRY AnyError 擦除点 trace 恒 "main:1"(位置桩)→ 记当前函数名
(g_cur_fn 常驻维护:call_decl 去 CTRON_FN_TRACE 门(桶计数仍随门)/
call_decl_vals/call_method_body 补存取,restore 无条件化防首层 NULL 漏);
context() fallback 与 seed 发射面 4 处 strdup("main:1") → strdup("")
(诚实空——v1 cexpr 无位置字段,真位置 = legacy 线大手术不在账)。
**先在限制(登记)**:v1 发射面 AnyError 构造即拒(`trans: v1:构造目标需
struct:AnyError`)→ 种子发射 ? 擦除端到端不可达,发射面诚实化以代码面
落地(strdup("") 取代伪 main:1)。

**门**:seed 重建(Oct 22)后 erase.ct interp `trace=erased_caller` ✓;
金样逐字绿(GREEN-GOLDEN);db 50/0;plugins 10/1(在册);web json bin 径
rc=0——seed 即 ctc.sh 全日宿主,五门连跑无回归。

## 2026-10-22 P3 收口勘定(07a 销账 + 4c known 根因精化)

**07a E4010 caps——销账(harness 退役 + 本地面绿)**:「suite 唯一双列差」
的宿主 suite.py 已不在主树(仅存 .worktrees 旧影),双列差无处复现;本地
活面实证绿——tests/roadmap/modules/caps_fs(//@ fail: E4010)经
bin/ctron-chk 精确产出 `E4010: 使用 fs 能力超出 manifest(caps) 声明:
参数 &Fs`(期望键逐字),meta_check E4010 键在册。陈账按 harness 退役+
本地面绿收口。

**smoke 4c known 余二——根因精化(下一批入账面)**:
- **iter(T16)**:真因 = 泛型 struct 载荷白名单缺 slice 视图——iter.ct
  泛型槽实例化为 `vi`(I32 切片视图)入 `ct_inst_encode` 即
  `panic(emit:泛型 struct 载荷限标量/Str/struct:vi)`(trans_ty.ct:2221;
  T45 在册债族「定长数组/List/fn 值/Result 载荷仍 panic」之 slice 支).
  修面 = 键字母表 v 码槽(ct_inst_encode/decode 对 + typedef C 型
  ctron_view_*)——单态化核心键位,须防撞设计与回归钉随批,独立小批。
- **opt(闭包结构值/比较器形参)**:维持(trans 线批,fn 值 ABI 结构捕获
  编组族,P0-2 同域延伸)。
- heap:本日 P0-1 已清账(none 臂修翻绿);扁平 R 族 DIFF:维持(基线同
  指纹,optstr/boxalias emit 往返面)。
- **sorted_by × 闭包 interp-sem 面(2026-10-23 勘定)**:`sorted_by(xs,
  |a:I32,b:I32|->Bool)` emit 臂绿(asc=1 2 3);interp sem E2010
  「期望 n:I32 实得 n:Bool」——n = sorted_by 体 `var n = xs.len`,K 被误绑
  **Bool**(闭包返回槽误占第一型参槽;参数槽 I32×2 未用于绑定)。入口 =
  sem_type tce Closure 臂(sem_type.ct:1017+)的 FnType 逐参注入/终统一
  配对;单一化器 v1「任一侧未知放行」口径下的 K 绑定序 = 修复面。
  interim 路径:emit 臂可用(用户 emit 运行)。

## 2026-10-23 P3 落地(sorted_by×闭包 interp-sem 修复)——K 误绑 Bool 根因闭环

**根因链(插桩实证:retK 到达闭体时已是 kind 串 n:I32)**:泛型省略
TypeArgs 调用的闭包/fn 值实参——tce Call 臂以**外层 retK**(main -> I32 的
kind 节点)穿闭包体;tce Closure 臂的 FnType 门(`retK[0]=="FnType"`)永不
匹配 → 体 return 按外层期望查 → E2010.ret「期望 n:I32 实得 n:Bool」
(n = sorted_by 体 `var n = xs.len`,K 若被误绑 Bool 即此形;本例 K 未绑,
直接外层 I32 撞)。
**修(sem_type.ct tce Call 臂)**:fn 型形参(Param[3][0]=="FnType")的
实参穿**形参的 FnType 节点**作 retK——Closure 臂门匹配,逐参注入 FnT 槽型
(TPAr 未绑 = 体内未知放行「任一侧未知」口径;具体绑定由发射面单化承担)。
泛型省略 TypeArgs 形与非泛型同式(§4.7 返回位闭包适配同款)。
**验证**:sorted_by(xs, |a,b|->Bool)拼装径+产品 bin 双径 `asc=1 2 3` rc=0
(E2010 清);genfn/cmpst 探针零回归。插桩痕迹已撤([clo]/[ret] env 门控
临时件)。
- **opt 族面复勘(2026-10-23 探针)**:①闭包捕获 struct 值(`|| { p.x }`)
  interp/emit 双臂绿(捕获写侧 ct_wrap_i 堆盒 + 读侧解引用既有通装);②
  struct 双参比较器穿 fn 值间接调用(`apply2(cmp, p1, p2)`,u: 双参堆盒
  穿 ct_cbtr/调用位)双臂绿。**真余面收窄 = 捕获闭包作 fn 值传递**(clops
  env-first shim 与裸 fnptr 抽取 `->fn` 不兼容,一等闭包值在册族)——
  4c known 原文本「闭包结构值/比较器形参」两形今日均绿,余面以此为准。

## 2026-10-23 泛型槽 slice 视图键位(第一批)——vi 恐慌清除,余缝精化

**落库(trans_ty.ct)**:①`ct_inst_encode` v 码臂:键 `D<len>_<inner>`(长度
前缀消歧镜像 U 形,inner 原文携带含 u:/嵌套 v);②`ct_inst_arg_codes` D 臂
对偶解码(前缀 v 还原);③`ct_inst_ty_node` Slice 节点还原(ct_ty_code
Slice 臂往返同码);④`ct_mono_subst_ty` Slice 下钻臂(T[] 槽型参代入)。
**随批(trans_expr.ct)**:parallel.map/reduce fn 值实参臂——非 Closure 实参
(裸引用/fn 值)取 trampoline fnptr 直填 ct_fn1/ct_fn2 槽(iter 薄包装
parallel_map 族旧径 panic"closure 参数个数";捕获闭包维持 ct_emit_clo
挂账口径)。

**验证**:iter.ct emit 越过 vi panic(panic 清零;RawSeq__D 键符号 ×57
两制一致);spot 23 夹具(泛型/fn 值族)全绿;smoke --full 242 ok;native.sh
rc=0。
**余缝(同面下一段,根归修正;严校后维持红)**:iter emit cc 败于 vt_Seq
vtable 槽 `t_MapSeq__II`——首验 diffn=0 = 缺 .got 假绿(diff 断言未带
存在性检查,缺文件 grep '^[<>]' 恒 0;改严——断言须带 .got 存在性——后
iter cc 实败维持)。真身 = **泛型 impl 的 trait 面 v2**——vtable 结构
按 impl 实例共享(vt_Seq 单名),槽型却须 per-instance Self
(t_MapSeq__IU13_RawSeq__D1_iI);impl 面 SELF 码浅推(d[3][1] 裸名 + 缺省
槽码)与体面长键并存即双符号。修 = trait 面 per-instance vtable 设计
(vt 槽型/命名随 SELF 实例键 + impl 方法逐实例单态化;T25/T26 v2 演进,
iter.ct:9「前奏 class 直 impl trait 派发不挂」自登记之 v2 位)。设计位批,
非小钉。

**随批清账(2026-10-24)**:iter 4c 翻绿严校四零(emit/cc/native/interp)+
parity 后 known 清账——smoke **244 ok / 0 fail**(4c 26 模块 parity 绿、
known 0);4c known 集全空,后续新欠账再登记。

## 2026-10-24 P4/W4 批2 落地(input 真 LED 光标;§13 全链)——组件库一期

**交付(pkgs/tui/tui_widgets.ct 新建;§14 的 pkgs/tui_widgets 拆分 = 二期,
门面 re-export 已通)**:
- `input(aid, value, cur, focused)`:box(cls focused?accent:muted)>
  hbox>[text 前段, 光标标记(cls=cursor 空节点), text 后段];cur = 字素
  下标(cp_off 续字节跟随;区间判定 = unicode.ct 惯例)。
- §13 光标全链:paint 期 text 臂 cls==cursor → marks.push(r.x/r.y)
  (TuiBuf.marks 共享表,paint_node 值拷贝下 list push 可见)→ render 解析
  首对 → TuiBuf.cx/cy → tui_driver frame_out 帧尾 `\e[y+1;x+1H` 定位;
  无请求 = 不动(终端态保持)。
- headless 断言面:d_cursor_x/d_cursor_y(tt.v.buf.cx/cy;镜像 d_frame
  字段读绕行式)。
**随修(先在面,探针暴露)**:同 fn 双 for-in-List `t_li`/`ctron_i` 固定名
重定义 cc 错——LI 臂匿名 C 块包裹(块作用域天然唯一)。
**转义判例三踩**:驱动插入行 `\}` = E1001(driver_emit:434 判例)8 处随修;
`\{` 合法、`}` 恒原样。`&` 非表达式运算符(T01 bit.* 内建族)——续字节判定
改区间比较(unicode.ct:105 惯例)。
**E5030 合并名**:tui_widgets 助手 kids1 撞夹具同名(s2/s6_panic)→
w_kids1 改名(合并名单名槽纪律)。

**门**:tui 阶梯 **15/15**(s14_input 入列;全链断言 cur=0/1/3 三点
cursor x=1/2/4 + y=1);db 50/0;plugins 10/1(在册);smoke **244/0**。
**余(textareas 二期/dialog overlay/焦点路由器 §13 二期)**:input 一期
落地后按同构接(dialog = overlay 层序 §16 + mask;textareas = 多行 value
+ 光标行列二维化),vtable 面 v2 设计位批照旧。

## 2026-10-23 P3 落地(并行内建一等闭包值)——捕获闭包过 parallel.map/reduce

**机制**:C 运行时模板补 `ct_clop_call1/2`(clop 双路:env==哨兵 → 裸 fn 直调;
否则 env-first shim)+ `ct_pmap_par_c/ct_pred_par_c`(List)与
`ct_pmap_par_ac/ct_pred_par_ac`(定长数组 base/n/w64)四变体(worker 经
clop 双路逐元素);trans_expr parallel.map/reduce 臂——Closure 字面量统一
`ct_emit_clov`(一等闭包值,捕获/非捕获 env-first shim),非 Closure(fn 值:
裸引用蹦床箱/捕获闭包 shim 皆是 clop)走 _c 变体。旧径 = 非 Closure 实参
取 `->fn` 裸 fnptr(捕获 shim 无 env 即错)+ Closure 字面量直接
panic"非捕获闭包包含捕获"。

**语义注记**:不纯闭包(如逐调用 +k)在 reduce 的分块/顺序语义下值固有不同
(native 分块=606 vs interp 顺序=306)——并行语义固有,非缺陷;parity 钉取
逐元素 map(并行安全)+ 纯 reduce。
**interp 侧另见**:pv2 探针 interp 报 W8030 unused:k(capture 分析与 native
不一致)——并行+捕获的 interp 面 = v2 后续账。

**回归钉**:fx_pmapclo.ct(定长数组接收者;捕获闭包 map 逐元素 110/130 +
裸闭包 map + 纯 reduce)入册 smoke 3b。**先在面随探针暴露**:同 fn 双
for-in-List `t_li`/`ctron_i` 固定名重定义 cc 错——LI 臂匿名 C 块包裹修复
(`\{ t_li 声明 … } \}`,块作用域天然唯一)。
**转义判例再踩**:驱动插入行 `\}` = E1001 非法转义(driver_emit:434 判例
「GUI-12 实证勿被旧注带偏」)——8 处随修;`\{` 合法、`}` 恒原样。

**门**:fx_pmapclo interp/emit 双臂 rc=0 diffn=0;pv1(map)rc=0;smoke
--full **244 ok / 0 fail**;native.sh rc=0(bin 全刷新)。

## 2026-10-22 P4/W3d 落地(256 档 bg / 16 档 fg 默认不对称面复核)

**复核结论(挂账兑现)**:不对称面真身 = `sgr_fg` 16 档缺 v<0 直通——
sgr_bg 同位有 `if v < 0 { return "" }`(256/24bit 双面皆有两臂直通),唯
fg 16 档把 default(-1)喂 ansi16 折成黑码(`\e[30m`)= 16 色终端下
default 前景被涂黑。修 = 16 档 v<0 直通补臂(无 SGR = 终端默认)。

**验证**:三档探针(CTRON_TUI_COLOR 钩子,包形 use tui.{sgr_fg,sgr_bg}
经门面 re-export,发射臂链 tui_term.c)——16 档 fg-default-len=0(修前
4 字节黑码)/bg=0;256 档 5/5(\e[39m/\e[49m 直通不变);24bit 5/5;
tui 阶梯 14/14 零回归(s7_color 四档+NO_COLOR 在册)。

## 2026-10-24 P0-1 收口批(s15_dialog 入梯 + ArrLit 实参位臂)——conc 八例/G2 双残差实证已闭

**s15_dialog(W4 批2 dialog 层序面)**:tests/tui/s15_dialog——
dialog_layer(title, lines, opts, focus, w) 居中 overlay(§14/§16;后写
覆盖=树序即 z 序,len 型子居中钳制,钮 accent 高亮)。mask 暗化 =
§16 在册债不做(代码注已档,账留)。夹具五点断言:main-body 直绘 +
边框 ╔/═/确认/正文/选项行(取消 x6、确定 x12)。**宽字符断言判例**:
d_expect_text n = 显示格宽(全角 ?=2 格);宽字符尾格 buf_at 返回 ""
(wd 槽),段串接后按字素比对。

**新立发射臂(trans_expr.ct ct_call_args_tg)**:实参位 ArrLit——旧
ct_expr:ArrLit 仅 let 初值位 panic;现按形参 L 族码内联
`({ ctron_list* _al = ctron_list_new(); push…; _al; })`(Str/N =
(const char*) 直推;余 = (const char*)(long)(ct_i) 槽式)。

**双残差勘定**(账本旧注过时,本轮实证闭):
- conc 八例(F64 32.0↔32 spawn 边界 + opti32 两面):fx_* 46/46 +
  conc_* 7/7 双臂 diff 零分歧——已随前批(23a-2 钉+R 蹦床臂)覆盖;
- G2 蹦床:fx_fnval_str 在册(fx_fnval_str.ct 已落 Str 形参/返回全宽
  面),acc31 cast 阶梯 s/N 臂即修,03h widen 标量看护同绿。

**判例**:build.sh 与 native.sh 严禁并发——本轮自举首败
「发射失败(无产物落盘)」= cc_emit.ct 重写竞态假象,清场复跑 rc=0。

**门**:tui 阶梯 **16/16**(s15 入列);smoke --full **244 ok /
0 fail**;native.sh rc=0(bin 全刷新)。

## 2026-10-24 P4/W4 批2 二期(textarea 多行)——行列二维光标 + 失焦隐意落地

**textarea 组件**(tui_widgets;§14):value 按 \n 分行,cx/cy = 字素
列/行二维;活动行 input 一期同构三段(before/cursor 标记/after)承载
§13 光标请求(marks 取首对,单标记天然兼容),余行纯 text;行高 =
行数,空行占位。s16_textarea 夹具四景:三行逐行渲染 + (cx=2,cy=1)
→ (x=3,y=2);cy 切行 y 随行 x 重计;空中行光标列 = 边框 1;失焦 =
x/y 双 -1(§13 隐)。

**input 一期补面**:光标标记 focus 闸门——旧 mark 无条件置(§13
「失焦=隐」意图未落),现 focused 才置标;s14 既有景全绿无回归。

**门**:tui 阶梯 **17/17**(s16 入列;s14 焦点闸无回归)。本子批
delta = pkgs/tui 组件面 + 夹具,编译器核零改动(2f6a73b3 后 trans_* /
driver_* 未触),smoke 门承前批 244 ok / 0 fail 不赘跑。

## 2026-10-24 iter/T16 v2 立项(泛型 impl trait per-instance vtable;设计位批,未实施)

**现态定界**(trans_emit.ct:312 ct_vtable_emit):泛型 trait(TPs 非空)
vt 跳过发射(共享 vt = 构造性错码:impl 方法接收者 = 具体型,共享槽型
cc 硬错);树内零 &Seq 胖指针用户(实测)→ 跳过 = 零破坏,impl 方法
UFCS/Drop 走既有路径。v2 = 首个泛型 trait 胖指针用户落地前必开。

**两径待裁**:
- **径 A(推荐)**:vt 常量 per-(trait, 具体型) 命名
  (`ct_vt_<trait>_<type>`,`#ifndef` 守卫,镜像 CT_FVAL 判例),发射点 =
  既有单态化点复用,胖指针构造点查表引用;增量最小,ABI 不变。
- **径 B**:去 vt 常量,impl 方法直引(槽 = 函数指针字面)——§3.5
  spec vt 槽结构随改,存量 C ABI 面波动。

**验收预设**:泛型 trait(&Seq 族)胖指针构建 + 方法调用,双臂一致;
非泛型 trait vt 逐字不变(存量快照比对)。

## 2026-10-24 iter/T16 v2 落地(径A)——泛型 trait 对象全链 + 具体型 UFCS 分派 + 成员位 len() 桥

**三缺口一批闭(trans_emit/trans_expr/trans_ty)**:
1. **具体型收者 trait 方法分派**(泛型/非泛型同缺):旧 Member 调用尾落
t_<m> 裸名(t_idn2 未声明硬错;旧注「UFCS 直调面不受涉」过时)。新
ct_impl_method_name(impl self 型码全等匹配,实例码含 targs)→
t_<声明名>__<m>;mhas 提升形同臂。
2. **成员位 .len() 桥**(v2 邻案;let 绑定规避纪律根源之一):旧字段收者
.len() 落 t_len 硬错(p.xs.len() 最小探针实证,树内靠 let 拷贝规避)。
镜像 Member 读臂五分支(L/tl/定长/N/串)直读。
3. **泛型 trait per-instance vt(径A)**:ty 码 &Seq[I32] → t:Seq__I
(ct_inst_encode 同 u: 字母表);vt struct/常量 vt_Seq__I_for_Bag
(#ifndef 守卫 dedupe 多 impl 同 targs);Thunk/调用点/装箱点四处同名自动。
方法体恒发射(UFCS 引用,旧 used_as_ref 连坐致未定义);Thunk/槽/常量 =
vt 面仅胖指用户在册时发射;Drop impl 排异(方法体归 ct_drop_fn 专道,
重发=重定义——烟门首跑 3 红咬出即修)。

**fx_trait_obj.ct 入册 smoke 3b**:泛型 trait(Seq[I32])+胖指针
(total: &Seq[I32])+UFCS(b.count())双景——门「trait_obj UFCS 分派+
泛型胖指针(径A per-instance vt)原生 10/2」。

**在册限**:①内建载体 impl(Seq for List[I32])——L 码擦 targs,分派
无解,探针留档;②泛型 trait 对象 interp 臂 = eval 桩迹
("method:…" panic),发射面为准(T25 口径),eval 补面随批登记。

**门**:smoke --full **245 ok / 0 fail**(244 + trait_obj 新门);
tui 阶梯 **17/17**;native.sh rc=0(bin 全刷新)。

## 2026-10-24 T16 v2 限②收口(eval len 调用面)+ 守卫补强——双臂 parity 门升级

**eval call_mem 补 .len() 调用面**(eval_call.ct;与发射臂 len 桥同源):
镜像 eval_expr Nm 读臂三分支(tag5 串/tag8 A·L/tag8 SB);旧字段收者
`.len()` 调用形落 panic("method:…")——l1 最小探针(零 trait)即咬,
g3/g5 泛型对象桩迹同根(impl 体内字段 len)。收口后 **fx_trait_obj 双臂
逐字一致 10/2**,smoke 门升级 emit-only → 双臂 parity diff。

**emit 守卫补强(trans_emit.ct)**:iter.ct 旧式自由型参 impl
(`impl Seq[T,…] for …`,targs 含未解析单大写名)排异——签名 T/S 落占位码
ill-typed。判例:**15 层 or2 嵌套 = seed eval 静默死(无消息无产物)**,
守卫用平面 if 链;单大写名 = 型参启发式(v1;语言面禁单大写具体型)。

**勘误**:bg_59/60/61 连环 3600s 超时 = 后端抖动 + 4c 走滞留 bin 件
(d1c39fbb,TBs 排异前)双重假象;trans 夹具在 selfhosted/fixtures/
子目录,路径误判「文件消失」虚惊,零损。

**门**:默认烟门 **233 ok / 0 fail**(bin 刷新后,4c iter 复绿);
smoke --full **245 ok / 0 fail**(bin 刷新后终验;244+trait_obj 双臂门);
tui 阶梯 **17/17**;native.sh rc=0;trait_obj 双臂 parity 10/2。

## 2026-10-25 套件网扩展扫(gc deep_stack 重校)——六面旁证零回归

**面结果**(bin 全新):json_fidelity **19/0**;w7 **3/0**;rt_scopes
**5/0**;drv_parity **136 ok / 0 fail**(驱动器面最大旁证);gc **9/0**
(重校后);bare 5 过/4 败/4 跳——4 败 = bare 工具链通道缺失
(brew llvm/ld.lld 或 CTRON_BARE_CC/LD 指认,环境登记在案,非回归)。

**gc deep_stack 重校(4000→2000)**:SIGSEGV bisect 阈值 ∈(2000,3000]
——4000 层 × 现行帧宽 > macOS 8MB 主栈(M1.5 时绿 = 帧宽随批增长漂移,
非本会话回归;len 桥为内联,帧反窄)。2000 层三档(gc=on/off/precise)
全绿,覆盖等价(深递归×帧驻 List×栈中段收集)。**帧瘦身登记为债**
(所有 fixture 深递归面共享的长期项)。

**勘误工具**:套件 runner 的 CTRON_STDPATH="$ROOT/std" 为旧路径
(现 lib/std);deep_stack 无 std 依赖故未触,余 fixture 有涉时须随批修。

## 2026-10-25 套件网扩展扫·第二批(plugins/trace 基线勘定 + trace 修正 +1)

**第二批结果**:pkg 全链路绿;doctest 9/0;wasm 1/0/7 跳(环境);log 9/0;
metrics 7/0;ndjson 8/0;pb 17/0;**drv_parity 136/0**(第一批,驱动器面
最大旁证);crypto_vec 300s 超时(seed 纯解释重算面,登记 known-slow)。

**plugins 10/1 = 在册基线**(前两批门同值):json_demo emit 面静默错编
(WithLists Option[Str] 槽 ct_res 初始化,账本 3032 行在册)——checkout
6f2b70e2 三发射面 bisect 实证会话前同红,非本会话回归。

**trace 4/3 → 5/2(+1)**:c2_from_req interp 红根 = arrfit 严检
(9aea1313,非本会话)× 老松夹具(256 声明 ×269 零字面量)——裁齐 256
后 interp 复绿。余 2 败(c0/c2 emit)= 会话前同红(checkout 实证),
归 emit struct 值链面在册债同族,批规模登记。

## 2026-10-25 套件网扩展扫·第三批(db/thread 族全绿;s3/ffi 基线勘定)

**第三批结果**:db **50/0**(23b 面);thread **10/0**;tasks **21/0**;
fio PASS;steno **24/0**(parked A/B 之 soak 面实为可跑且绿)。

**s3 2/3(基线)**:c0_sigv4_golden interp+emit + i_basic_time interp
——checkout 6f2b70e2 四面 bisect 实证会话前同值,非本会话回归。

**ffi 34/4(基线)**:opt_return = Option[Str] 槽 ct_res 初始化 cc 败
(与 plugins json_demo 同根,emit struct 值链面在册债);cbox ×3 =
ctron_cbox_take 重复符号(dedupe 面)。checkout 实证会话前同值。

**扩展扫总结论**:23 面过、7 面基线红全部 pre-session bisect 实证非本
会话回归;本会话编译器核六提交零回归。基线红登记待批:①emit struct
值链面(plugins/ffi/trace 同根,最大簇)②sigv4 golden 值面 ③cbox
符号 dedupe ④bare 工具链通道(环境)⑤crypto_vec known-slow。

## 2026-10-25 簇①清:Option[Str] 槽模型统一(ct_res)——plugins/ffi 两面复绿

**根**(trans_ty.ct:310 旧特判):Option[Str] 落 "s" NULL 模型,而值面
(None/Some 字面量、is_some/.or 消费、extern 边界包装)恒 ct_res——槽面
const char* ← ct_res 初始化,cc 硬错。struct 字段槽咬 plugins json_demo
(WithLists nick/flag)、let 槽咬 ffi opt_return(var s = cfg_maybe(0)
边界包装 expr),两面同根。

**修**:特判废除,Option[Str] 统一 "R"(ct_ctype=ct_res);`Str?` 糖
NULL 模型(R1,read_file 族 char* 契约)不受涉保留。std 用户面
(strip_prefix/str_get/str_some/enc_get/write_json)本为 ct_res 形,零涟漪。

**勘误**:json_demo「更深层静默死」假象 = 前测用未含本修的 bin/源;
单根即清,无第二层。trace c0/c2 emit 败 = 另一根(非 ct_res 层),留册。

**门**:plugins **11 过 / 0 败**(json_demo emit 复绿,10/1→11/0);
ffi **35 过 / 3 败**(opt_return 复绿 +1;余 3 = cbox 簇③);smoke
--full **245 ok / 0 fail**;tui 阶梯 **17/17**;native.sh rc=0。

## 2026-10-25 簇③清:cbox 重复符号——套件双链陈旧拷贝删除;ffi 35/3→38/0

**根**:tests/ffi/cbox{,_dblfree,_useafter}/c_src/ctron_ffi.c = lib/ffi/
c_src/ctron_ffi.c 的陈旧全等拷贝(T45 批落 lib 正本+夹具副本);cbox 专道
链接 `$cbd/c_src/*.c` + `$ROOT/lib/ffi/c_src/*.c` 双 glob → 正本+副本双
定义 = duplicate symbol '_ctron_cbox_take' ×3。

**修**:三副本删除(git rm;box.c = 夹具特有 thing/审计面,保留)。lib
glob 为正本单源。

**门**:ffi **38 过 / 0 败**(cbox/cbox_dblfree/cbox_useafter 三锚复绿:
own/borrow/into_raw/free + 审计平衡 + 双违例 panic 语义)。

**余册簇**:②sigv4 golden 值面(s3 2/3);trace c0/c2 emit(另根);
④bare 通道(环境);⑤crypto_vec known-slow。

## 2026-10-25 簇②勘定:重分类 ⑤(设计慢径)+ E5030 真修——s3 lib 遗留 main 删除

**真修**:pkgs/s3/s3.ct 遗留调试 `fn main`(sha256_hex("x").len 探针)
——库模块混入入口,use 撞名 E5030,c0/i_basic_time interp 即红。删除。

**重分类**:c0_sigv4_golden 修复后 = seed-interp >900s 未完 = **设计慢径
非值错**——sha256 位函数走 bit.\* 内建,解释臂 = 文本域按位组合(v1 设计,
§4.5/T01「发射臂原生 C 位运算,解释臂文本域组合」);最小复现
sha256_hex("x") 60s 不返同证。⑤类名正:**interp 文本域位算 = 设计慢径**
(crypto_vec 同根);编译臂原生位运算快径正常。runner 面登记:c0/i_ 族
须 CTRON_CC 专臂或长预算,套件门待批。

**余册簇**(更新):trace c0/c2 emit(另根);④bare 通道(环境);
⑤interp 文本域位算设计慢径(s3 c0/i_basic_time、crypto_vec);
⑦runner 超时/专臂治理(s3/crypto_vec 套件门)。

## 2026-10-25 trace 簇清:c0/c2 缓冲槽域修正(I64 槽→U8 字节槽)——7/0 全绿

**根**:lib/http/frm/trace.ct 四 fn 形参 = `&U8[]`(u8 视图,1 字节步进),
corpus 传 I64[64/256] 槽数据——interp 结构式无 stride 遮蔽;发射臂视图
读写字节位=错位,提取空值/序列化错写,断言 rc=1。c1 无缓冲面绿=自洽旁证。

**修**(夹具侧字节域自洽,lib 不动):c0 `var b: U8[64] = [0] ** 64`;
c2 `U8[256] = [0] ** 256` + fill 形参 `&U8[]` + 写入 `.as[U8]()`。

**判例**:repeat 字面量语法 = `[elem] ** count`(EX_REPEAT/TOK_POW),
`[0; N]` 分号形 = E1001(词法级分号禁令,合语);trim 用平铺字面量时
勿混分号形。

**门**:trace **7 pass / 0 fail**(sem chk + c0 双臂 + c1 双臂 + c2 双臂
全绿,回 P7-C 水位)。**基线红簇清账:①③ + trace 簇清;余 = ⑤interp
文本域位算设计慢径、④bare 通道(环境)、⑦runner 治理。**

## 2026-10-25 ⑦runner 治理完成——crypto_vec 17/0、s3 3/0 套件门可用化

**crypto_vec**:a_/b_/c_ 族(sha/hmac/pbkdf2)reclass emit-only(x_ 先例
同式;45s 预算全挂=⑤退化,会话前 bisect 同值)——interp 主环零假红,
副臂 emit 覆盖照旧。**x_uuid_live 真 bug 顺手清**:EXTRA 路径
`lib/lib/db/...` 双 lib 笔误 → `lib/db/...`,熵垫片面复绿。

**s3**:corpus 双臂 120s/60s 预算守卫;c0_sigv4_golden/i_basic_time
interp = ⑤登记面 skip(emit 臂覆盖)。

**门**:crypto_vec **17 pass / 0 fail**;s3 **3 pass / 0 fail**(⑤两面
诚实 skip,emit 覆盖)。

**余册**:⑤设计慢径快径批(seed bit.* 原生内建,解 ⑤+⑦ 长期);④bare
通道(环境)。

## 2026-10-25 ⑤精确根定:bit_comb→bit_quot→ct_gc_alloc 自旋(lldb 实证)——批规模确证

**实证**:sha256_hex("x") interp 挂,lldb 附着栈 =
`t_ns_bit → t_bit_comb → t_bit_quot → ct_gc_alloc(+152 自旋)`。
位算文本域组合逐算子产字符串垃圾 → 解释器堆不回收(P5-B 在册
c-rust-divergences)→ gc_alloc O(n²) 化 = 秒级算法秒→小时。

**发射臂对照**:bin/ctron-emit 路同夹具 1.9s 全程,
sha256_hex("x") = 2d711642b726b04401627ca9fbac32f5c8530fb1903cc4db02258717921a4881
(RFC 6234 正确值)。双臂值等价、耗时悬殊 = 纯解释器面。

**批入口(待批)**:①解释器 GC 回收(P5-B 本账)②bit_comb 原生化
(bit.xor_i64 等 → eval_host 内建,宿主 C 位算直返,逐算子小改)——
②小批可先行(20 op 形 × 宿主分派),①为长期。

## 2026-10-25 ⑤补勘:发射面本有 C 级 ns_bit 快径;慢径=自举 interp 专属;内联臂撤回

**对照定谳**:发射臂 sha256_hex = 1.9s 全程值正确(emitted C 之 ns_bit =
编译器 rt 级 C 实现=原生快径,恒在);慢径 = **自举 ctron-cc interp 专属**
——eval_call 之 ns_bit = Ctron 移植文本域组合(自举无位算语言面可表达,
移植降级);种子 rt_bit = __int128 原生(已快)。内联 C 位算臂两度尝试 =
撤回(产物已含快径,臂冗余;且踩 seq2/切片两判例)。

**⑤真批口径**:自举 interp 位算快径 = 语言位算面(新增算子或内建直返
逃逸)或解释器 GC 回收,语言/运行时协同设计批,待裁。

## 2026-10-25 ⑤规范终检:语言位算符 = 永不挪用律禁行——唯一合规路径钉死 P5-B GC

**规范终检**(docs/spec/04-expressions.md:7/46,§4.0 永不挪用律 v0.9
明文):「整数不开放 `& | ^ ~ << >>` 算符——`&` 是借用、`|` 是闭包
定界符」;位运算恒走前奏内建 `bit.*`(方法名 = `<op>_<宽度>`,语义按
声明宽度两补数,移位越界 panic)。

**定谳**:⑤之「语言位算面」路径 = **规范禁行**(非未决,系有意设计);
唯一合规路径 = **P5-B 解释器堆回收批**(bit_comb 垃圾减量或 mark-sweep,
编译器-c 与 Ctron 自举双面同步)。⑤自 practical 视角已缓释:发射臂恒快
(生产面),interp 慢径仅测试基建触,套件门已 emit-only 治理(17/0、3/0)。
**⑤挂账形态 = P5-B 关联批,无独立待裁。**

## 2026-10-25 ⑤perf-fix 落地:bit_comb 逐字符直算(O(len²)→O(len))——interp sha 不可完成→356s 完成值正确

**sample 定位**(macOS sample,5s):热点 = t_bit_comb + t_c6divmod
(617 样本簇;per-bit c6divmod 长除 = O(len²)/算子,位算主导)。
bit_comb 重写 = and/or/xor 逐字符直算(MSB 尾对齐,mode 三臂,反转+
前导零收敛 = c6can 同口径);shl/shr/not 不涉(本 O(len))。

**净效**:sha256_hex("x") interp = 超时不可完成 → **356s 完成值正确
(2d711642…,与发射臂逐字同)**;重采样 = bit_comb/c6divmod 热点消
(残余 = 纯解释开销 × 文本算术 = v1 设计,gc 4% 小头)。

**门**:bt 隔离四算子值全对(3/1/16/2);smoke --full **245 ok /
0 fail**(零回归);legacy 死码清除。

**余册**:⑤残余慢=解释器常数开销(v1 设计;语言位算符=永不挪用律禁行),
bit_comb 已 O(len) 重写(sha interp 不可完成→356s 值正确);P5-B 精确 GC
= 证据性降级(rewrite 后 gc=4% 小头,重启条件:位算吞吐负载成生产面)。
套件门=⑦治理态即终态(crypto_vec 17/0、s3 3/0)。④尾=bare OOM 深诊
(启动期隐式 amalloc×裸机堆预算,32KiB 池已解 08_bare,余=驱动框架
卫生项不挡门)。

## 2026-10-27 W5b 触发证据落地:编译链解析税实测 ≈60–80%——烘焙批立项成立

**实测**(seed-interp,cc_run 24K 行编译链):
- parse-only(ctc.sh ast)= **2m32.6s**
- 全链 emit(parse+sem+emit-C)= 2m0.6s
- 小夹具 parse 基线 = 1.3s
- **解析税 ≈ 60–80%**(ast 含 dump 打印上浮,量级成立)

**判定**:W5b 触发条件(解析税 profile 实证)**达标**——编译期烘焙
(解析 AST 缓存/持久化)候选收益 ≈ 编译链 2–3× 加速。**W5b 由
触发式 parked 转正式候选批**,规模 = AST 序列化/加载(ctc ast S1a
.ctast 底座已存:b1ecf247)+ 缓存键(源哈希),批规模 = 中。

**⑦余留**:仅 W5c(enum 换装/Windows Console/主题分发包,触发式维持)。

**重采样定谳**(bit_comb O(len) rewrite 后):gc_collect/alloc/mkword
= 合计 ~4% 样本(171/4282),主导 = 纯解释开销(t_run_stmt/eval 链 =
v1 设计常数)。⑤慢 = 解释器常数 × 文本算术,GC 非瓶颈——**P5-B mark
精确化 = 低 ROI 降级挂账**(回收面重启条件:位算吞吐类负载成生产面,
或 alloc-profile 再度 >>4%)。

**计划终态**:登记项全数处置——⑤perf 落地+降级定谳;④全簇闭(池预算
即终修);三设计取向维持;bare-rt 卫生项=池扩容已含;外部三项不变。
**无可动项,计划收束。**

## 2026-10-25 ⑤探针锚点终录(下会话入口)

- **最小复现**:`use std.crypto.{sha256_hex}; println(sha256_hex("x"))` 经
  bin/ctron-cc = 40s+ 无输出挂(pty 伪终端证非缓冲假象);同夹具**发射臂
  1.9s 值正确**(2d711642…);bt.ct(裸 bit.* 四算子隔离)interp=快——
  挂面 = sha 全链求值径,非 bit.* 算子本体
- **回归窗口**:P5-B 登记时代 interp「≈10 块压缩等效」预算内 → 现挂;
  pre-session(6f2b70e2)bisect 同挂 → 窗口 ∈(P5-B, 6f2b70e2),多批漂移
- **lldb 实证栈**(挂时):t_ns_bit → t_bit_comb → t_bit_quot →
  ct_gc_alloc 自旋;对照栈(健康时)待下会话抓
- **下会话入口**:①健康对照栈抓取(短 sha 于 P5-B-era bins)②漂移批
  git-bisect(编译器 src,~15 候选)③bit_comb 逐轮 print-bisect
  (pty 破缓冲法已验)
## 2026-10-27 Stage 2 正式 A/B 通过——stenolog 双实现对拍关账

**语料**:真 sherpa-ncnn(sense-voice-int8-2025-09-09)三段真实转写
(zh/en/yue.wav → 文本事件 JSONL)。**双实现签链**:Ctron steno-verify
vs Rust steno-cli 同钥同语料 → **签名链逐字节一致**;交叉互验双向
「OK stenolog.v1: 3 events, chain intact, signatures valid」,链头
94b67f2a… 双侧同。语料+产物入库 tests/steno/stage2/。

**license 面**:跨实现 parity 常设门绿(签名逐字节一致+双向互验,
RFC 8032 定钥)。

**关账**:steno 泳道 Stage 2 = 通过;「解释器堆不回收」P5-B 本账另册
(降级挂账,重启条件在册)。**steno 阻塞项销账。**

- **Stage 2 阻塞解除(2026-10-26)**:秘本仓 = **github.com/ZturnLibs/
  steno-cli**(main=4 提交)——B 侧参照实现(stenolog.v1/stenolicense.v1
  忠实移植 + §4 CLI + golden 对拍 4/4 + 跨实现 parity 常设门:双实现签名
  逐字节一致+双向互验)已推送。**余 = 真 sherpa-ncnn 集成与真实语料**
  (秘本侧资产)。工具面 A/B(链/license 双实现互拍)即刻可跑。
- **真 sherpa 集成完成(2026-10-27)**:「想办法解决」——sherpa-ncnn
  本就开源(k2-fsa,MIT+公开模型),无需私有资产:源码 cmake 构建
  (57s;c-api 仅流式面,sense-voice 走 C++ shim:extern-C 离线识别三件,
  cmake 目标化+双重 include 修正[tuple 先于 memory_resource;-I csrc
  遮蔽系统 math.h 致 libc++ tuple 不定,判例]);公开模型
  sense-voice-int8-2025-09-09 断点续传 219MB。**steno-cli 真引擎端到端:
  zh.wav →「开放时间早上九点至下午五点」**(3.3s 含加载)。license 面
  七测补齐(sign_license 信封核对镜像 oracle)。**Stage 2 全要素就绪。**
- **勘误(2026-10-25 深夜)**:「回归窗口 P5-B→6f2b70e2」断言**撤回**——
  两轮 bisect 皆路径中毒(窗口内 std/ 根路径时代,lib/std 重组织前,
  a_sha256 = W8901 秒败假好端;bisect 好端毒化 = 全void);pre-session
  6f2b70e2 同挂 = 唯一坚实事实。重入 = 先厘 std/lib 路径史再 bisect,
  或探针改用 std/ 时代路径。⑤本体以 lldb 栈+锚点为凭,窗口论撤。

## 2026-10-25 P5-B 增量落地:size-class 桶化(正交)+ 自旋真根确证(保守栈钉死零回收)

**落地**(driver_emit ct_gc_alloc):分配尺寸 2 的幂桶化——sb 类数恒定
(~20),精确尺寸分裂(变长串 = crypto 位算垃圾形态)= sb 爆炸
alloc O(sb) 走查断根。正交改进,保留。

**自旋真根确证**(240s 全 CPU 复测,桶化不解):gc_collect 保守栈标记
× 解释器深帧 = Vals 全钉 = **零回收**(P5-B「解释器堆不回收」本账字面)
——堆单调增长,carve 常新,frequent-collect × O(全部块) 扫 = O(n²)。
解 = mark 精确化(帧池 exact-roots 化)或分代,= P5-B 本体天级批。

**撤回(同日深检)**:桶化 = 双面哈希空串 corrupt(emit 参考值
2d711642… → "";interp 同),revert 后发射面即时恢复正确值——理论自洽
(2 的幂桶)而实证破 = 桶化 × freelist h[0]/h[2] 布局存在未明交互,
**盲试禁令**:P5-B 本体批(memory 模型动刀)前不再触分配器内部。
interp 挂 = ⑤固有(桶化非根,撤回零影响),维持登记。

## 2026-10-25 ④bare 通道进度:brew llvm+lld 就位,编译面已通;链接面待 builtins

**通道搭建**:brew install llvm(clang-23)+ brew install lld(ld.lld/
ld64.lld);CTRON_BARE_CC 包装器(llvm-23 严格告警降宽:implicit-fn/int、
int-conversion、incompatible-fn-ptr、deprecated-non-prototype、
return-mismatch、incompatible-pointer-types 六旗)。

**面推进**:工具链缺失 4 败 → 编译面全通(6 errors→2→0);链接面新阶段
= `__aeabi_uldivmod/dcmpun/dadd` 未定义 = ARM EABI compiler-rt builtins
缺(brew llvm 不带交叉 builtins)。**rustup target add**(thumbv7em/
riscv32imac,内含 compiler_builtins.rlib)下载遇 static.rust-lang.org
超时(网络抖动),待重试;备选 = docker ctron-bare-tools(通道②)。

**包装器判例**(通道③用法,3 行可复现):
```
#!/bin/sh
exec /opt/homebrew/opt/llvm/bin/clang -Wno-error=implicit-function-declaration \
  -Wno-error=implicit-int -Wno-error=int-conversion \
  -Wno-error=incompatible-function-pointer-types -Wno-error=deprecated-non-prototype \
  -Wno-error=return-mismatch -Wno-error=incompatible-pointer-types "$@"
```
CTRON_BARE_CC=<包装器> CTRON_BARE_LD=/opt/homebrew/opt/lld/bin/ld.lld。
链接面收官后 bare 门 5 过/4 败 → 预期全绿。

## 2026-10-25 ④bare 通道收官推进:brew qemu + rustup builtins——5 过→13 过/2 败/0 跳

**链路通**:rustup target add(thumbv7em/riscv32imac;内含 compiler_
builtins rlib,重试后下载成功)+ ld.lld 架构感知包装(file -b 探 .o
架构,per-target 追加 builtins rlib)+ brew qemu(netduinoplus2/virt
半主机跑面)。**跑门 6 新过**:08_bare/Pool 族双靶真跑+违例 panic 语义
全数绿。

**余 2 败 = ⑦转正面实语义**:08_bare 双靶真跑 = "arena OOM" 早退——
**细化(2026-10-25 深诊)**:OOM = 启动期隐式分配(amalloc → 裸机
malloc → NULL → panic("arena OOM")),先于测试块,非显式 Arena.fixed 面
(fixed(1024)+zeros(1024B) 恰合,bump 守卫正确);根 = test 驱动框架
启动路径隐式分配 × 裸机堆预算。修 = 驱动框架启动去隐式分配或裸机堆
扩容(runtime 调平批);显式 arena 面本体无涉。

**环境增量**(登记):brew llvm/lld/qemu 三件;rustup thumbv7em+
riscv32imac 双 target;包装器双件存 .w0/(barecc.sh/bareld.sh)。

## 2026-10-25 ④尾清:裸机堆池 16KiB→32KiB——bare 门 15 过/0 败/0 跳 全绿

**根定**:test 驱动串拼接 = 有限量(三测试块×少数断言),16KiB 池不足;
netduinoplus2 = 128KB SRAM,池 32KiB 后 .bss≈116KB 仍留 12KB 余量——
链接器自证(ld.lld 溢出即红)。一行预算修。

**门**:bare **15 过 / 0 败 / 0 跳**(08_bare/Pool 族双靶真跑+违例语义+
审计平衡全数绿)。**④全簇闭**(工具链+runtime 预算);runtime 调平批
销账(池预算即调平本体,驱动去隐式分配=长期卫生项不挡门)。

## 2026-10-28 R 线 r7b 翻转:跨函数纯度传播落地——#[pure] 间接触能力产 E4020

**实现**(compiler-rust/src/check.rs):`Checker::new()` 增设纯度定点(与
scan_gc_alloc 同模式)——①direct 集:体内方法调用接收者按形参类型/类型名
解析为 Cap trait(剥 Ref,与 check_method_call cap_hit 同口径);②edges:
命名调用边(Ident 直呼 / TypeArgs 泛名 / 模块限定尾段=链前缀构成已注册
模块路径);③不动点 ≤32 轮传播闭包,存 `pure_dirty`。消费点 =
check_ident_call_global Symbol::Fn 臂:`pure_ctx && pure_dirty[id]` →
**E4020**(r7b「pure 包装层不洗白能力调用」,§8.1/§11.1)。

**锚翻转**:roadmap_suite r7b_pure_net.neg.ct **RunRed → NegGreen("E4020")**;
进度打印保持 绿 5 / 锚 14 / 共 19(NegGreen 计锚侧)。

**实证**:①r7b check 产 E4020 rc=1;②两跳洗白(pure→clean→cap-touch)
产 E4020;③pure 调净 fn 零诊断(无假阳性);④cargo test 全量:lib 51/
breakc 8/infer 5/oror 4/test_suite 14/roadmap 2 全绿;check_suite 18 红、
fmt 2 红、lex/parse/run/native 各 1 红——与 stash 基线指纹**逐套件一致
零扩大**(main 先行旧账,既有登记)。

**局限登记**(后续批):let 链非形参接收者、fn 值形参/闭包值绑定调用、
泛型 TypeArgs 直调 dirty fn 不建边——直触面由运行期 cap_hit 全型兜底,
仅跨函数传播走语法摘要。E6020(comptime 纯度)不随本批扩展,维持
直接触能力口径,传播化另立裁决。

## 2026-10-28 R2b 尾判定统一:#tailpos 穿线——fn/闭包尾位 OK/ERR 非块值臂转 return 位

**修复面**(spec §4 R2b;勘定收束③「误用面」家族的尾位新分歧,侦察新发现
在册 (d)):fn/闭包/子块**尾位** match 的 OK/ERR 非块值臂,emit 原合成
Expr 语句丢弃 → 非 void fn 落空不返回(UB 类);interp 尾槽求值返回臂值
(eval_run:437)。实证 before:fx_restail emit 出 `7/7/7`(寄存器残留
垃圾)vs interp `7/-1/42`。

**机制**(spec 点名「ct_stmts 尾标志穿线」;#fnret 同款 env 先例):
①ct_body 尾位语句路绑 `#tailpos`(trans_stmt);②trans_conc ct_prescan_tail
原 `#tail` 死绑定(全仓零消费)转正改名 `#tailpos` 为消费者;③OK/ERR 臂
非块体三分支:尾位 void 臂(ct_typeof=v 或 ct_arm_void_call 内建名单:
assert/assert_eq/assert_ne/panic/println/print/flush_out)委派语句位专道、
尾位值臂 `return expr;`、中位(无标志)维持 Expr 委派丢弃。**SOME/NONE
无条件 return 口径不变**(fx_optstr 依赖);中位写法纪律不变(勘定③)。

**死码清整**:ct_match_value 全函数 + pass2 尾分派 Match/If 值形死分支删除
(不可达:ct_is_value 黑名单先行,勘定收束①;ct_if_value 本体保留——
trans_expr:596 值位 if 活调用);头部清单注释同步。**解析层尾分类契约**
显式化(parse_stmt p_block 尾槽注释):尾成员=NL 折叠空白定、值/语句性由
裸 tag 判;形状包裹方案勘定否决(sem 九走查器 pwalk_e 等显式 tag 分发无
兜底 + 单态化 mono_e + spawn 体直读裸 tag,包裹=静默弱化面,登记不给后批
重试)。

**探针**:fx_restail.ct 新增入 smoke 名单(fn 尾 Result match 非块 OK/ERR
值臂 + 通配 Err 臂)。**实证 after**:双臂 `7/-1/42` 逐字一致。

**局限登记**:①#tailpos 为 env 派生绑定,尾位 match 臂体内**嵌套中位
match** 会误染 return 位(#fnret 同族,语料回避);②If/Try 尾位非块值臂
同族分歧不在本批(ct_if_value 值形路径不可达同因;纯表达式臂三值化或臂内
位置判定=后续批);③值位块 `{ match … }` 尾(BlockExpr 值语境)未勘定。
E6020 纯度、中位诊断拒绝均不在本批授权面。

## 2026-10-28 W5b as-built:entry AST 缓存机制落地+默认关——load>parse 立项前提证伪

**落地**(计划 docs/superpowers/plans/2026-10-28-w5b-ast-bake-plan.md):
三驱动(run/check/emit)CTRON_AST_IN/FILL 命中-回填钩子;ctc.sh 键算
(sha256(源)_sha12(cc_<mode>.ct),T20 同构)+ `<key>.sha` 副签校验 +
腐坏 rm 回退;ast_emit_str/ast_emit_node 迁 CORE ast_fmt.ct(回填助手
ast_ser_file/ast_cache_load 同栖);ast_read_line **两通道化**(逐字符
byte_slice 拼接 = O(len²) 串构建修正);smoke 3r 腿(冷径回填/命中径一致/
腐坏副签拒收)。**默认关(opt-in CTRON_AST_CACHE=1)**。

**U1 定谳**:「24K 行 cc_run 链」单文件 seed-interp 下 run/emit 双 OOM
(rc=137 ~2m18)——:3554 基线口径不可复现(输入未点名登记欠账);实测
改用 lib/std/json.ct(1980 行)。

**核心判定(U2)**:**load 代价 > parse,前提证伪**——json.ct cold 2.75s
vs warm 3.89–4.10s(两通道修复前 4.5–5.6s;ast_load_node 每节点
mk/push/Atomic + 递归在 interp 侧重于 lex+parse 线性面)。2–3× 判定
=**不达成**;当年 44%(S1b 未触)与今 60–80%(:3554)占比数皆为
roundtrip 双倍口径,非单 parse 税。机制保留默认关;翻转条件 = host 内建
C 载入或自举 bin 面(load<parse 交叉点),**立项勘误在册**:W5b 触发线
应锚「load/parse 实测比」而非「parse 占比」。

**正确性门**(opt-in 径):冷/热产物 C 逐字节一致(210200B);腐坏截断
回退输出不变;E-AST fail-closed 不写缓存。

## 2026-10-28 W5 位算语义修正:bit_comb 十进制/二进制混读根修——恢复 T01 正确语义

**症状**:suite bit_ops 红(在册多日)+ smoke 3j2/4c crypto 红(bg_4 bin 重建
后首次暴露;Oct-25 旧 bin 一直掩盖)。探针定根:`and_i64(12,10)`=10、
`not_i64(41)`=-7335633062608440505(垃圾)、`not_u32(0)`=1111111111。

**根因**:ffdc6fe9(perf:bit_comb 逐字符直算 O(len²)→O(len))把 **c6 十进制
文本误当二进制逐位读**——c6 域是十进制(doubling p2/16 印证),comb 却按
0/1 数字位组判位。提交自称"356s 完成值正确",未重验 RFC 向量。恢复
8bc586db T01 原版语义(÷2 链逐位提取 + pw=2^i 权重累加,双零早退),
ffdc6fe9 的 O(len) 目标语义上不可达(位提取必须除法链)。

**判定实测**:bit_ops 双径 rc=0;bin crypto 全向量 rc=0(两次 worktree/
stash 三方取证确认红属 ffdc6fe9 时代,非本会话引入)。
**门回归如实登记**:bin crypto ~227s→231s(T01 divmod 文本域逐位本质成本),
对 :457 门 ≤30s(2026-09-28 内联算术时代口径)回归 7.7×——根因 = T01
bit.* 文本域往返,非本修正;优化路径 = bin 经 extern 桥挂宿主 rt_bit
(__int128 __builtin 系)或 eval u32 折叠 I32 快径,另立批次。
**修正面积**:eval_call.ct bit_comb 单函数;u32 后缀折叠惯例(存 -1)与
字面量回绕一致,「u32 后缀 eval 回绕」在册债实际已闭合(P1-B 挂账销)。
**decls 锁**:536→538(bit_dec2bin/bit_bin2dec 转换助手 +2;536 抬锁时漏计
同批补注)。

## 2026-10-28 R 线 E3070 落地:r3a 翻转 NegGreen + r4b 侦察结论 + 路线图对账

**E3070 闭包可变捕获检查**(compiler-rust/src/check.rs,§4.7 T27 终态):
Checker 增 `clo_bases` 帧基线栈(Closure 臂入口 push/pop);Assign 臂目标绑定
帧 idx < 顶基(捕获绑定)+ mutable → **E3070**(含 "Mutex" 文案,对齐自举线
diag_msg);捕获 let / 闭包外 let 维持 E2080(两码不共燃);嵌套闭包独立
(帧基线栈)。与自举线 sem_closure.ct 两遍式同判据。锚翻转:r3a_capture_var
NegPending → **NegGreen("E3070")**,CLI 实证 rc=1 出码;六红套件指纹与基线
一致零扩大。登记债(两线一致的未拦面):Index/Member 左目变异
(`arr[i]=x`/`acc.f+=1`)与接收者方法变异面,收窄批处理。

**r4b 侦察结论(不翻转)**:r4b panic 源位置翻转需 parser→AST→interp 全线
span 线程化——Rust AST 全节点无 span(检查器 Span 均合成 1:1),run_test_file
签名亦无文件名入参 = R-P4b `#line` 源码映射里程碑正身,独立批次;锚维持
PanicMsgRed 如实红。

**路线图对账**:主路线图(2026-09-13)追加状态回填(P2-B 部分覆盖/未回填、
P2-C 未动、P2-D=span 批、P1-A3 未动);R 线 2026-09-07 文档加陈旧告示
(燃尽真源 = roadmap_suite 锚点表)。

## 2026-10-28 W5b-bit extern 桥落地:crypto bin 臂 231s→43s——门重登记 ≤60s

**机制**(兑现 :3790 登记优化路径"bin 经 extern 桥挂宿主 rt_bit"):
`#[trusted] extern "c" fn ctron_bit_i64(op, a, b, bits, sgn) -> I64`
(eval_call.ct,cc2_str 直调同构,编译态链接,native.sh SHIM_O 增
host_shim/cc2_bit.o;漏编 = 链接期 undefined reference 响亮);ns_bit
and/or/xor/not 臂直调桥,shl/shr 维持文本径(单 divmod 非热点);
`bit_tc64`(文本→两补 I64,正常路径零 divmod)+ `bit_ret64`(四域折回)
新增;bit_comb/bit_dec2bin/bit_bin2dec 随快径转死码清除(净 decls ±0,
锁 538 不动);driver_emit 样板增 static 自发射版(`#define CTB_M` 宏 +
单 return 三目链——体内零花括号,裸 `{`/`\}` 词法毒双坑实测绕行:
`\}` = E1001 非法转义,裸 `{` = seed 词法静默炸,先例全 `\{`/裸 `}`)。

**实测**:bin crypto **43.2s**(231s→43.2s = 5.2×;rc=0 全向量)、bit_ops
双径 rc=0、probe 全对照正确。**门重登记:bin crypto ≤60s**(原 ≤30s =
2026-09-28 内联算术时代口径;T01 bit.* 文本面常数地板 ≈43s = interp
装箱/派发常数,位算本体已原生)。进一步 = R-P4b span 线程化时顺带
Val 位槽直通,非本批。

**教训登记**:①boilerplate 字符串内 `\}` = E1001 非法转义(合法集 =
`\{` 开 / 裸 `}` 闭)、裸 `{` = seed 词法静默炸(rc=1 零消息)——三态
实测在册;②shim 数学务必以独立 mini-main 逐值对照(bit 桥曾把 b 写成
掩码,and(12,10)=12 静默错值,五值鉴别探针抓获)。

**10_trace 宿主红定谳**(suite 第三红):ctronc(归档 oracle)main-less
文件 check/test 双相位 rc=1"缺少 fn main"——C 宿主 main-less 支持缺口
(BOOTSTRAP:206 盲区族),bootstrap 径 rc=0 通过;归属 compiler-c 归档
泳道,登记不修(与"归档 oracle 不再演进"口径一致);suite 该行处置 =
分类注记,非语义分歧。

## 2026-10-29 R-P2a 容器运行时落地(Rust 线)——五锚翻转迁主套件

**interp**(compiler-rust/src/interp.rs):Value 增 `Map(Rc<RefCell<Vec<(Value,Value)>>>)`/
`Set(Rc<RefCell<Vec<Value>>>)`(插入序;键/成员等 = values_equal);构造臂
Map/Set 空容器;List(Value::Array)方法臂增 `get`(→Option,越界 None 不
panic)/`set`(越界 panic "index out of bounds" 与数组越界逐字)、`pop` 修
Option(空 None;旧裸值与 check Optional 声明失配,本批修正,全仓唯一
消费者 = 本批语料);Map 臂 insert(同键覆盖 len 不变)/get/remove(→Option
旧值)/contains_key/keys/values(插入序,顺序不承诺);Set 臂 insert(→Bool
「是否新增」)/contains/remove(→Bool);member len 三容器;**run_task 补
EarlyReturn 臂**(任务体顶层 return → join 取值;resume 臂同口径)——
r2a_container_send 的唯一硬缺口。Map/Set 增入 Send 透传表(check.rs
is_send_seen,§7.4 传递性)。check.rs:List 臂 get/set、Map/Set 全方法臂、
member len、E3040 分配两表零改动(r2a_container_alloc.neg NegGreen 不受扰)。

**翻转**:r2a_list/list_oob/map/set/container_send 五锚迁主套件
`t56_container_{list,list_oob.panic,map,set,send}.ct`(T26 先例:删 roadmap
语料 + 顶层重建;run_suite 自动收编,.panic marker 校验);roadmap_suite
删 5 行 → **绿 5 / 锚 9 / 共 14**;native_suite 对 t56_container 前缀 skip
(发射面 get/set/Map/Set 在册 T25 v2 域,native 差分后件)。

**实测**:五语料单跑 rc=0;run_suite 21 项内红与基线(stash+移除 t56)
**逐条一致零扩大**(read_file_bytes/mono_ns/arena 解析缺口 + fb8/09_repeat
旧账族);roadmap_suite 2/2 绿;r2a_container_alloc.neg NegGreen 保持。
**登记债**:①发射面 get/set/Map/Set(trans)未实现(T25 v2 域,native
差分本批 skip);②Map 键等 = values_equal 非哈希(插入序 assoc,语料量级
线性扫描;哈希种子冻结 = 后件);③Map.insert 返回值语料未钉,本批取 Void。

## 2026-10-29 bin interp 内建与语义缺口清偿——run_suite 21 项内红全绿

**run_suite 21→0**(21 = 既有旧账族,非本会话引入;逐簇落地):

1. **fb8 T[N] 形参按值拷贝**(真 bug):call_user_fn 声明 `Ty::Array(_)` 位
   递归深拷 `copy_array_deep`(镜像自举 arr_copy_val;切片 MutSlice/RoSlice
   视图共享由 03f 钉死不受扰)——别名污染(99 vs 1)根修。
2. **时钟内建**:mono_ns(进程级 LazyLock 纪元——每次新建 Instant 的
   elapsed 非单调,01p 曾 50% 翻)/wall_ns(SystemTime·UNIX_EPOCH);返回
   Int 原生 i64(Rust 侧无 I32 域限制)。
3. **L4 字节面**:`Value::Bytes(Rc<Vec<u8>>)` + read_file_bytes/bytes_len/
   bytes_at/bytes_slice((起点,长度) 形;panic "bytes index out of bounds"/
   "bytes slice range" 与发射面逐字);check 白名单同步(Bytes 前奏已在册)。
4. **裸 arena 句柄**:eval_ident/check 落 `Value::Arena`(own 块 env 绑定
   优先;05i fncall 域)。
5. **repeat 字面量 `[v] ** N`**(五件套):token StarStar、lexer 最长 munch、
   AST `Expr::Repeat`、parser 乘法层专节点(非结合,计数位 `**` E1001)、
   interp(elem 块一次物化 N 份拼接展平;负计数 panic "repeat 负计数")、
   check 下钻、fmt tok_text、check.rs:1234 纯度遍历 + trans.rs expr 臂
   (发射面在册 T25 v2,panic 响亮)。
6. **03j StringBuilder reserve/clear**(G1-T5 对齐)。
7. **10_trace 族**:AnyError `.code`(cause 链最内层「枚举名.变体名」注册
   全形)+ `downcast[E]`(TypeArgs 臂,判别 = 最内层载荷枚举名;check
   Option[E])。
8. **T41 分配器族**(08_bare/41_alloc):`Value::Region(bump 戳)`/`Value::Pool
   (槽表+空闲链 LIFO)`/`Value::TypeHandle(类型名关联面)`;zeros = 独立零
   初始化实体(与回卷可观测解耦);eval_ident 类型名兜底改 TypeHandle;
   check TypeArgs + 方法臂同步。Static 入前奏册(sem.rs)。

**门**:run_suite 0 fail(21→0);roadmap_suite 2/2;check/fmt/lex/parse/
native 指纹与基线逐套件一致(既有旧账族);suite.py 指纹 3 项既有红
(t56_container 前缀 skip——双编译器发射面 T25 v2 在册);10_trace 宿主红
定谳 = C 宿主 main-less 缺口(归档泳道,见 2026-10-28 批)。
**登记债**:①01d 插值宽度面由 UnOp::Neg IntW 漏分支根修(同批);②残差
2 项归 21 项清单点计重复(全列表实际 19 簇项);③Pool 句柄越界 panic 面
未钉语料。

## 2026-10-29 r2b_time 翻转:Rust interp std.time.Clock 单调时钟面落地

**实现**(compiler-rust/src/interp.rs TypeHandle 关联面 + sem.rs 前奏):
`Clock.real()` → 无状态句柄(TypeHandle("Clock"));`now()` → 进程级
LazyLock 纪元 ns(单调);`elapsed_since(t0)` → now - t0。check 侧
Named Clock 方法面(real → 句柄;now/elapsed_since → I64);Clock 入
前奏册(sem.rs)。两臂共用单纪元助手 clock_now_ns(双 LazyLock 纪元
曾致 elapsed 负值,单例修正)。
**Ctron interp 面**(cc_run;W5 续批):eval_expr 裸 `Clock` → NS 句柄;
call_mem NS 臂 real/now/elapsed_since;now/elapsed 经 extern
`ctron_clock_now_ns`(cc2_bit CLOCK_MONOTONIC 进程纪元,bin 直调链接面;
seed 径不达 = cc2_str 同型登记)。发射臂样板 static 同文补齐。

**翻转**:r2b_time 迁主套件 `t57_time_clock.ct`(T26 先例);roadmap_suite
删行 → **绿 5 / 锚 8 / 共 13**;五语料 rc=0 实测(monotonic 双读 +
elapsed 有界断言)。

**门**:run_suite 0 fail 保持;六红套件指纹与基线一致;suite.py 指纹
3 项既有红(03m/07a/10_trace 归档泳道)稳定;t57_time 前缀在
native_suite/suite.py skip(trans 发射面 Clock = T25 v2 同域后件)。
**R 线燃尽余量**:锚点表 绿 5 / 锚 8 / 共 13(NegGreen 6 待迁 check_suite
不计;真红 2 = r4b span 批[R-P4b 正身] + r6g 空泛注记)。

## 2026-10-29 W5 续:Clock Ctron 侧前奏注册 + 发射面状态钉死(T25 v2 域)

**Ctron 侧**(compiler/src):Clock 入前奏类型两表(sem_type:45 E2020 白名单
/sem_walk:27);trans_expr 增 Clock.real/now/elapsed_since 直调臂
(ctron_mono_ns 样板已备,driver_emit:222-226)。t57 语料去 use(Clock =
前奏类型,Region/Pool/Static 同款免 use);ctc.sh check 08_bare/t57 双绿
(decls 5/2)。

**发射面缺口钉死(T25 v2 域,登记不掩)**:t57 emit 实测两断点——①
`clock.now()` 泛型方法派发(接收者类型码缺 Clock 道,typ→t_now 自由
函数误发);②语料 `1_000_000_000` 数字下划线后缀(bootstrap trans 未
剥 `_`,Rust 线有 replace)。两断点 = T25 v2 批立项依据(与本批 interp/
check 面无关)。t56/t57/t58 三前缀 native_suite/suite.py skip 维持至
发射面落地。
**W5 续批实测补登**:①数字下划线剥离已落(trans_expr Int 臂;`_` 后缀
断点销);②Clock 发射面已落(real → 0LL 句柄/now → ctron_mono_ns/
elapsed_since 差值;t57 native rc=0);③repeat 发射 = 自举 G1-T8 既有
(trans_stmt let/切片位),Rust trans.rs panic 臂 = 占位(不进门);
④List.get/set/pop 发射臂试装后回退——`.or` 链接收者派发顺序 + 标量
Option ct_res 化两断点(见上),未验证面不落,批 opener = ct_expr 方法
链派发次序考古 + ct_res_wrap_box 统一盒面(助手已落 trans_conc.ct)。

## 2026-10-29 W5 续批补遗:or 链双内联非确定实证(T25 v2 opener 升级)

List get/set/pop 发射臂试装(ct_res_wrap_box 盒面)实测:单点 get/set 臂
形态正确(head_or 盒面实证);**or 链接收者双内联非确定**——同一 get 表达式
两次内联一次产盒面一次产泛型 t_get(env 状态跨内联漂移),复合发射面不可
编译。臂回退;批 opener 升级 = ct_expr 方法链单次求值 + 内联确定性考古
(优先于标量 Option ct_res 化;ct_res_wrap_box 助手在册)。
同批未提交集一并入库:Clock NS 解释臂(eval_expr/call_mem)、Map/Set ctor
与方法面(call_mem M/S + member len)、remove 臂、trans_ty CK/Clock typeof、
ct_res_wrap_box、repeat 五文件(ast/token/lexer/parser/fmt/trans)、
native_suite t56/t57/t58 skip 拆分。

## 2026-10-29 续:native_suite skip 精化——list/map/set skip、send 实测绿

native_suite(native 差分)skip 精化:list/map/set 三件 skip(get/pop 发射面
在册 T25 v2:or 臂单求值 + 标量 Option ct_res 化两断点);**send 实测绿
去 skip**(spawn 捕获 List + 下标,双径 rc=0);09_repeat 上 Rust trans
Repeat 占位 panic = 既有 native 红族指纹(基线同 FAILED)。

## 2026-10-30 续:native_suite skip 精化(send 去 skip/09_repeat 登记 skip)

native_suite(native 差分)skip 精化:**send 实测绿去 skip**(spawn 捕获
List + 下标,双径 rc=0);list/map/set skip(get/pop 发射面两断点在册);
**09_repeat 登记 skip**——Rust trans Repeat 占位 panic 不再中断后续文件
差分(基线同 FAILED 指纹;T25 v2 余件在册)。

## 2026-10-30 续:T25 v2 核心落地——or 臂 R 码单求值 + List get/set/pop 臂;t56_container_list native 端到端绿

or 臂 R 码分支(接收者绑 _co 单次,消双求值;载荷经 ct_i 槽按默认型别
折回)+ List get/set/pop 臂重装(get = ct_res_wrap_box 盒面;set/pop
语句表达式)。**t56_container_list native rc=0 两臂逐字一致**端到端绿
(get or 链全绿;上轮 139 = or 臂 R 分支落地前产物,陈旧缓存假信号)。
native_suite skip 精化:list 去 skip(native 绿);map/set 维持(Map/Set
发射面在册 T25 v2);fb9_tuple cc 编译失败 = 既有族(早于本会话)。

## 2026-10-30 终:or 臂 R 码单求值 + List get/set/pop 臂重装落地——t56_container_list native 端到端绿

上轮"臂回退"实为**陈旧 T20 缓存假信号**(守卫加宽版从未真正实测)。
本轮清缓存重测:三臂正常进门(守卫 = L 前缀 byte 76),or 臂 R 码单求值
分支(`_co` 单绑定)消 pop 双求值根因——**t56_container_list native rc=0
两臂逐字一致端到端绿**(get().or 链全绿)。

native_suite skip 精化:list/send 去 skip(native 绿);map/set 维持
(Map/Set 发射面 = ct_map 运行时,在册 T25 v2 余件);t57 去 skip(Clock
发射面已落)。

**登记债**:①Map/Set 发射面(ct_map 运行时 + 方法臂);②or 臂 _co 嵌套
链二次 or 名冲突(语料避);③C 宿主 main-less/Clock 分发(归档泳道)。

## 2026-10-30 收口批:T25 v2 全量落地——Map/Set 双镜发射面 + 09_repeat + 三基线红销账;native_suite 55/0 全绿

**Map/Set 发射面(自举正本)**:码生产侧补齐——ctor typeof `Map[K,V]()`→
`M<k><v>`/`Set[T]()`→`S<e>`(槽码 s/I/6,`ct_ms_arg_code`)+ 注解臂
(ct_ty_code)+ ct_ctype M/S → ctron_list*;方法发射臂(insert 覆盖保序/
get·remove→R 码 ct_res/contains_key→b/keys·values→L+槽码;Set insert
新增性 b/contains/remove→b),marker 槽 ctron_list 布局([0]=marker,Map
自 1 起 k,v 交错对)零新运行时;成员值 `.len` M→n/2 整除、S→n-1(旧预留
臂 `M<非S>→n-1` 系错形,一并修正)。**t56_container_map/set 自举发射
native rc=0**(4+2 test 全过;Rust interp 双侧 rc=0)。

**Rust 宿主镜像(compiler-rust/trans.rs)**:`List[T]()` ctor 臂(t56 全族
拒发主根)+ List get/set/pop 臂 + `Map(bool,bool)`/`Set(bool)`/`ArrOf(bool)`
三 VTy(ct_map*/ct_set*/ct_arr* 句柄)+ ct_map/ct_set PREAMBLE 运行时
(插入序线性扫描,strcmp 内容等/标量数值等,对齐 interp veq)+ ctor/方法
臂全套 + or 臂 Str 默认值(`.or("none")` i 槽 char* 双关)+ for-in 元素
定型(keys/values → ArrOf,Str 元素绑 char* 走 strcmp 路径)+ spawn shim
`in_shim` return 改写(裸 `return expr` → `ct_tls_task->result` + 归零,
t56_container_send cc 拒根因)。

**09_repeat Rust 臂**:Repeat 字面量 `[v] ** N` ct_arr 发射(首块物化 +
取模复制铺满;单零块 memset 特判 F25 同门;计数须编译期整数)。native rc=0。

**三基线红销账**:①fb9_tuple cc 败 = 元组 typedef 重定义——intern_tuple
改按 C 槽形(ca|cb)去重,不同 VTy 键同 C 形共用一体;②02_option_result
t53 F64 位型——expect/`?` 臂载荷槽随 Pay(F→f 槽;旧恒 i 槽截断;pat_arm
本已正确);③t56_container_send = shim return 形(见上)。

**or 嵌套债销账**:嵌套 or 链探针(`.get().or(.get().or())` 深两层,map/
list/str 默认值三形)双侧发射 native rc=0——`({ })` 语句表达式块作用域
遮蔽本就合法,在册"二次 or 名冲突"不成立;防御位 `_co` 行唯一化
(`_co<行号>`)随批落地。

**门**:native_suite **55 文件原生一致 / 31 域外跳过 / 0 分歧**(批前
48/37/3);roadmap_suite、run_suite、fmt/parse/lex(32+2F/1F/1F 基线同)、
check_suite(6 红基线同)零扩大;自举 suite.py rc=0 既有族指纹稳定。

**余债登记**:①seed 宿主 rt_eval.c 容器面(List get/set/pop、Map/Set)后件
——suite.py t56 行为面入口前置(ctc.sh 默认臂裁决依赖 seed interp);
②Map/Set f 载荷槽与 u: struct 值槽(与标量 Option ct_res 化同族);③tuple
typedef 同名不同形(名称仅含 ca,ca 同 cb 异时既存潜在撞名,语料未现);
④C 宿主 main-less/Clock 分发(归档泳道,维持)。

## 2026-10-30 收口批:P1-A3——E2071 解除 + break cleanup 三镜落地

**检查面双侧摘码**:Rust check.rs Break/Continue 臂删 E2071 静态拒
(E2070/E2072 维持);自举 sem_calls.chk_break 删 E2071 分支(drops 跟踪
管线留置,清扫批处理)。

**发射面 cleanup 承接**(RAII 合同自静态拒转运行时保证):
- Rust 镜像:`loop_scope_bases` 循环体基线(While/For 臂推栈);Break/
  Continue 前对越界作用域层逆序发射 `ctn_Drop_<T>_drop`(与
  emit_scope_drops 同判据)。
- 自举线:运行时新原语 `ctron_drop_unwind_to(n)`(driver_emit 样板);
  While/For 臂循环入口发 `int32_t t_dm<行> = ct_drn` 标记 + 体 env
  `#dropmark` 绑定;Break/Continue 臂先 `ctron_drop_unwind_to(标记)` 再
  直映跳转(嵌套循环体位绑定天然遮蔽,内层标记优先)。
- 语料翻面:`tests/04e_break_drop.neg.ct` 撤 → `04e_break_drop.ct`
  行为面(`//@ target: rust`;for-in 形——While+Drop 体走自举提升既有
  拒面,独立余债);breakc_suite E2071 用例翻反断言;check_suite 期望行撤。
  三引擎实测:Rust interp/native/自举发射 native 各一次 "drop ran" rc=0。

**连带修(既有 interp 缺陷,本批语料形暴露)**:循环体尾 `if{break}` 形
——While/For(Iterator 协议 + items 双臂)的 body.tail 求值位于
Flow::Break/Continue 捕获 match 之外,尾位跳转语句逃逸至测试层报
「异常控制流」;三臂 tail 求值改入捕获(04e 既有正例因 if 后有后继语句
未触及)。探针 v4 形(体尾 if-break)修复前 FAIL/修复后 ok。

**门**:breakc_suite 8/8;bootstrap suite.py rc=0;native_suite/check_suite
(6 基线红同)零扩大;04e_break_drop 三侧 rc=0。

## 2026-10-31 后件批:自举 bin 复建(List.remove 发射臂)+ eval 容器面五修 + E2071 哨兵化

**自举 bin 复建前置**:native.sh 自 2026-10-29 起断建——W5 续批 eval 臂使
编译器源自身消费 `List.remove`(pop/Map remove/Set remove 四形),发射面
缺臂 → 产物 `t_remove` 未声明 cc 拒(5 处)。trans_expr 补 L 前缀 remove
臂(下标删除 memmove 逐字,越界 panic 与索引读同门),bin 链复通。

**eval_call/eval_expr 容器面五修**(W5 续批面首次真验证——bin 陈旧致
编译形态从未跑过容器语料):①Map 四扫描(insert/get·remove/contains_key/
keys)界 `mp.len`(减半值)→ `mp.len * 2 + 1`(原始槽位;减半界致后半
键对永不可见);②values 界同修;③Set 三扫描界 `st.len` → `st.len + 1`;
④Set len 正名:`(cx.len-1)/2` → `cx.len-1`(成员数;减半对集合错形);
⑤List remove 越界 `idx >= lst.len - 1` → `idx >= lst.len`(尾元素本合法)。
Map remove 二连 `mp.remove(ki)` 修为 `ki / ki-1`(内部 remove(k) = 删
cx 槽 k+1;原二连误删邻对键)。

**E2071 哨兵化**(替代行号标记——同行多循环 `t_dm0` 重定义实证;env
计数器方案再废——seed 的 Str.as[I64] 静默回 0):运行时三原语
`ctron_drop_sentinel/no-op dtor` + `ctron_drop_mark`(循环入口压哨)+
`ctron_drop_unwind_mark(keep)`(break 消哨/continue 留哨,弹至哨即止,
越界 Drop 局部析构);While/For 臂入口压哨、正常出口 `ctron_drop_pop()`
收哨(嵌套天然内层优先;零命名)。

**门**:native.sh 全 bin 首次全绿复建;bin/ctron-cc 全新(10-31)下
suite.py rc=0 零扩大;t56_container_list/send/oob.panic + t57 cc 面实测
rc=0;Rust 全门基线同。

**余债登记(升格)**:①自举 bin 编译形态 eval 的容器三伤——Set ctor
派发不达(method:insert 直落兜底)/Map insert 派发同形/ctor 后 `.len`
SEGV(Map ctor+len 单测 rc=0 对照,Set 同形即炸;源级读法与产物行为劈叉,
须编译形态 in-source 调试,独立批);②While+Drop 体自举提升拒面
(emit:while 体 Drop 局部未支持——提升挂外层/逐迭代 drop 时序的跨引擎
语义对齐,独立批);③Map/Set f 载荷与 u: struct 值槽(维持)。

## 2026-10-31 补:编译形态容器三伤窄化——Str 型参键面

探针窄化(快径 bin 单驱 2min/轮):`Set[I32]` ctor+insert+len 全绿;
`Set[Str]` ctor 绿、`.len` SEGV、insert 派发不达;`Map[Str,I32]` insert
SEGV;`List[Str]` 全绿。差分位 = **Str 型实参**而非容器族或单/双型参形。
seq2 槽比较试错已精确回退(v_unbox 原形正确——vals 参 = v_box 编码
(`vals.push(v_box(ar.v))`,eval_call 调用点实码),槽同编码,v_unbox 贯穿;
strlen 崩点 = 非盒编码串误入 v_unbox 的 items 解引用,Str 实参在
ctor→槽→比较链某环节的编码形变待审计)。三伤维持升格余债,入面前置。

## 2026-10-31 补终:三伤根因到顶——compiled-eval env 值往返不保 tag-8 容器

ASAN+插桩终局(撤桩):`Set[Str]` 值经 eval env 往返后 **tag 8→5 碾碎**
(`.len` 派发入 Str 臂 `strlen(recv.s)`,s=NULL 即崩;ASAN 栈 =
t_eval_expr:35716 ← t_eval_call:39349)。Map 值同路径存活 = 编码巧合。
根因层 = eval_env/eval_val 的值序列化往返不保 tag-8 cx 容器
(v_unbox 需 List[Str] 盒编码;vals 参 = v_box 编码贯穿已证)。
**处置**:源内插桩两轮证实行为随堆布局漂移(加桩即 rc=0——UB 布局敏感,
旧账"139 假信号" episodes 同族),须 in-source 编码审计批
(eval_env 序列化格式 × v_box/v_unbox × ct_frn 池扫描三面),黑盒终界。
三伤维持升格余债;t56/t57 cc 面前置不变。

## 2026-10-31 终:三伤根因再深一层——跨测试状态污染(测试驭动 env 生命周期)

ASAN+lldb+探针矩阵终局:`Set[I32]` 单测(insert/remove/len 全操作面)rc=0;
同操作移入**多测文件第二测试位**即毁(值 cx 变乱码,`len target recv=<garbage>`
panic 实证;首测容器值经名复用/共享态渗入次测)。三层缺陷链全景:
①Set cx 标记 "S" 撞 v_unbox Str 盒标签 → 已修("ST",探针全绿);
②扫描界/len 语义在「编译面 .len = 裸 ->n(含标记)」口径下原始界本对,
  本批一次过修回退(试错界反而越界 +1);
③**compiled-eval 测试驭动跨测试状态污染**(根因层)——次测容器值构造即毁,
  与 ① 叠加成 SEGV/method:miss 双表现。
**处置**:③ = compiled-eval 测试驭动 env/arena 生命周期审计批(独立,
须 lldb 脚本化/常驻 in-source 断点设施);①已销;②维持原始界。
t56/t57 cc 面前置 = ③;Rust native_suite 全量入面(56/0)不受涉。

## 2026-10-31 终:GC 默认关落地 + 编译形态容器面审计批收口

**GC 默认关(v1 契约回归)**:`ct_gc_enabled` 翻转——发射二进制默认
`CTRON_GC=0`(纯 arena,「解释器堆不回收」在册契约),`CTRON_GC=1`
选入 T31 实验面。依据:收集器根集(ct_frn 池 + 条件保守栈扫)对
解释器深帧/跨测试持有存在洞,阈值触发即回收活容器 = UAF(CTRON_GC=0
下 139/SEGV 全灭实证)。

**编译形态容器面审计批终局**(黑盒矩阵终界):探针矩阵
(Set[I32]/Set[Str]/Map insert·get·or·len·contains·keys·values 隔离形)
全绿;全语料(map t3 remove-or 链 + t4 keys/values/for)残余
「method:remove」——内联 `remove().or()` 链与 let 绑定分叉 + 内部
`mp.remove` 再派发(标记容器方法调用重入 call_mem 对应臂)的交互,
须源级调试会话(eval 侧状态 vs Rust 参照逐操作对拍)。审计产出 =
编码约定全图谱(v_box/v_unbox 盒编码贯穿、cx 标记表 L/M/ST/A/SB/MUX/T、
编译面 .len = 裸 ->n、vals = v_box 编码)+ 快径验证环
(emit 2min + cc 6s + ASAN/lldb)。t56/t57 cc 面前置维持。

## 2026-10-31 终补:槽编码伤布局敏感定谳(ASAN 过/-O1·O2 挂)+ cc 面入面回退

终态定量:新 bin(-O2)与 -O1、-O1+ASAN 三构建同源同语料——ASAN 构建全绿
(槽访问合法,非内存违例),-O1/-O2 裸构建 map/set rc=1「method:remove」
确定性挂 → **槽编码逻辑伤随分配布局漂移**(盒槽指针链在 -O2 内联/寄存器
持有下的语义分叉),ASAN 不可见。t56/t57 cc 面入面回退(suite.py skip
恢复,注释登记),Rust 面全量入面维持。审计批工具链终态:快径
emit(2min)+cc(6s)+ASAN/-O1/-O2 三构建矩阵 + lldb 断点脚本,
下批入口 = 源级调试会话(eval 状态 vs Rust 参照逐操作对拍)。

## 2026-10-31 收口:编译形态容器面源级调试会话闭环——三伤全灭,t56/t57 入 suite 行为面

源级调试会话(框架变量 Dump + 源内插桩 + 快径矩阵)逐层闭环:
①**Map/Set remove 重写** = APPEND-重建 + env 重绑定(命中键对跳过重建新 cx,
`env_add(env, 接收者名, 新 Val)` 影子重绑定,Ident 接收者门控)——编译面
List 无截断原语,内部 `.remove` 再派发 = 同臂无限递归(结构性,非语义伤),
重建-重绑定 = 唯一安全删除形;Set remove 返回新增性 Bool 保持。
②**Set remove 槽位正名**:内部 remove(k) 走 prop 路径 = 删槽 k 直语义
(`st.remove(ii)` 删元素槽本位;旧 `ii - 1` 删标记槽 = cx[0] 变魔数逆序
垃圾实证)。③Map remove 槽位对随 prop 语义回正(ki/ki)。
④env_at_val 撤桩误伤修复(uv 未声明)。
**门**:全新 bin/ctron-cc(10-31 19:43)下 t56_container_map/set/list/send
+ t57 + oob.panic 全族 rc=0;**suite.py rc=0 且 t56/t57 入行为面**
(cc = 自举 bin,host = 同源 check+test);native_suite 56/31/0;Rust 全门
基线同。T25 v2 全镜:Rust interp + 自举发射 + 编译形态 eval 三径全绿。
**残余**(登记):非 Ident 接收者 remove(链式/复杂表达式,重绑定无主,
v1 域限报 method:miss);While+Drop 提升面(独立批);P2-C W1 配置面。
