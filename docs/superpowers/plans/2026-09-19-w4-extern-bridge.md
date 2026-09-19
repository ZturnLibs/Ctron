# W4 解释口径前置:解释器 extern 直调桥——实施计划

> 状态:**E1+E2+E3 已交付**(2026-09-19 当日;E3 为 demo 级——gui_counter 进程内
> 原址替换 + headless 探针;规范级 retained 树/绑定槽失效随 M1 语义面)。
> 上游:gui 规范 §11.5(解释口径热重载,C4 裁决)、MVP 阶梯 W4 余项登记。
> 现状实证(2026-09-19):解释器对 extern 调用明确拒绝(eval_call.ct:282,
> §9.6 口径"经 ctron-emit + cc 链接后运行");原生口径热重载已由
> examples/gui_counter 快照切片交付。

## 0. 目标

被解释程序可调用 vendored 库 extern(标量 + Str 子集先行),使 GUI 程序在
**不经过 cc 链接用户程序**的解释口径下运行——热重载"文件变更 → gui_parse 重跑 →
骨架原址替换,状态树保留"的宿主前提。

## 0′. E1+E2 交付记录(2026-09-19;与原计划的偏差与实证坑)

- **实现形态与计划差异**:白名单=程序内 `#[trusted] extern` 声明表 ✓;但胶水不走
  "发射期逐声明 case",而是**通用 12 参帧编码运行时派发**(`ctron_ext_dispatch`:
  帧 `"i:<int>"`/`"s:<str>"`,空帧终止;通用 `long(*)(long,…)` 原型直调——SysV/
  ARM64/Win64 整参与指针同 GP 寄存器传递)。理由:Ctron 无变参/列表 FFI,
  定长帧绕开;签名不符在编译期不存在(解释口径无编译期)。
- **符号源**:`bin/ctron-cc` 由 native.sh 链接域库 shim(ctron_gui.o)+ whole-archive
  raylib + rdynamic/force_load;vendored 库缺席时降级纯解释器(运行期"符号未找到")。
  C 宿主 seed 不扩(维持计划);`ctc.sh run` 的 extern 程序报错形态从 §9.6 指引
  变为宿主错误(登记)。
- **实证坑(价值最高的记录)**:
  1. **`\}` 是非法转义**(E1001 非法转义)——Ctron 字符串转义表只有 `\{`/`\"`/`\\`/
     `\n`;闭括号一律裸 `}`(惰性)。boilerplate 三处内联 `\}` 曾致宿主解析静默死亡
     (rc=1 零输出——宿主解析错被 ctc.sh 的 >/dev/null 吞掉,取证靠崩溃残页);
  2. **发射口径字符串内裸 `{` 触发插值扫描吞引号**(W1 旧账重踩, `\{` 强制);
  3. **C 宿主解析器不支持跨行参数表**——自举源首个 extern 声明(13 参)折行即
     E1001"参数缺少 :",必须单行;
  4. macOS 新版 ld 移除 `-noall_load`,whole-archive 用 `-Wl,-force_load,<archive>`;
  5. 多泳道并发跑 build.sh 会撞 `build/*.ct.tmp`(竞态失败间歇出现;smoke 曾因此
     假红 120/2 → 117/5 波动,静默窗口复跑 122/0)。
- **验收**:E2 夹具 tests/gui/w4_interp(ctron-cc 解释执行 Clay 桥一帧,命令数 +
  label 逐字节断言)全绿;固定点 smoke --full 122/0;suite rc=0;阶梯 11/11;
  w1/w2 黄金、双示例(含快照往返)全绿。

## 1. 宿主口径决策(先钉死,防做歪)

| 宿主 | 决策 | 理由 |
|---|---|---|
| C 宿主 seed(compiler-c/build/ctronc) | **不扩** | 仅引导期使用;引导期无 GUI 需求;C 宿主改动属另一条线 |
| 原生解释器 bin/ctron-cc(发射 C + cc) | **目标宿主** | 编译产物可含 dl 胶水,自身链 vendored raylib + -ldl |

即:`ctc.sh run` 语义不变(仍拒绝 extern);新增/扩展口径是 **bin/ctron-cc**。
用户可见形态:`ctron-cc run src/main.ct` 在链了 vendored 库的解释器上直接跑 GUI 程序。

## 2. 白名单与胶水形态

- **白名单 = 程序内 `#[trusted] extern "c"` 声明表**(不新增配置面)——发射期已能
  枚举全部 extern 声明(driver_emit 阶段),逐声明生成签名特化 dispatch:
  ```c
  static ct_i ctron_ext_call(const char* nm, int n, ct_i* a, const char** s) {
      if (!strcmp(nm, "gui_open") && n == 0) return gui_open();
      if (!strcmp(nm, "gui_text") && n == 6) return gui_text(s[0], (int)a[0], ...);
      ...  /* 每声明一 case;签名不符 = 编译期生成,不存在运行期错配 */
      ctron_panic("extern 未在解释口径白名单");
  }
  ```
- eval 侧:Call 到 extern 声明时改走 `ctron_ext_call`(现 panic 点分流);
  C 宿主解释路径**维持 panic**(无该运行时符号,宏守卫或字符串探测)。
- Str 实参:`const char*` 直传(编组已证);F32 参数维持 I32 边界口径(与 shim 同,
  待裸 F32 缺口修复统一解)。

## 3. 切片

1. **E1 胶水生成 + 路由**(emit + eval 各一小刀):emit 期扫 extern 声明 → 生成
   `ctron_ext_call`;eval 的 extern Call 分流到该符号。硬门:自编译固定点 +
   smoke --full + suite 全绿(改核心后必跑)。
2. **E2 解释口径跑通现有面**:gui_counter 的 s6 形态(注入脚本断言)经
   `ctron-cc run` 全绿;新增夹具 tests/gui/w3x_interp_ext/(黄金=命令数+文本字节,
   同 s6 口径)。
3. **E3 热重载环**:ctron-cc 常驻 + mtime watch → 重跑 parse/gui_lower → 骨架原址
   替换(状态树不重建);验收 = §6.3 口径(改样式/结构,输入态/焦点不丢)。
   repr(c) struct 实参(Vector2 等)随 M3 input/textarea 需求再评,E1–E2 不含。

## 4. 风险

- 自举固定点是最大风险面(E1 动 eval/emit 两个核心文件)——每切片当日
  固定点全绿才落库;
- 与在途泳道的文件协调:eval_call.ct/driver_emit.ct 动工前按 peer_status 协议登记;
- 解释器性能:解释口径千元素 60fps 门槛(§11.6 F1)不因 dispatch 恶化
  (dispatch 仅 extern 调用路径,布局重放次数不变)。
