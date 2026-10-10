# 构建驱动器升格 W3(std 前置件三件)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 补齐 W4(Ctron 自写原生驱动器)的三个 std 硬前置:①`env_set` 内建(用户态写 OS 环境变量,子进程注入必需)②`fs.walk` 递归遍历(测试发现/缓存枚举必需,字节序确定性)③lib/proc 增 rc 型 argv 执行面 + argv_to_blob 门面 + argv 上限 12→32。

**Architecture:** env_set 走内建四锚点注册(解释臂/发射臂/形表/prelude 白名单)+ 发射模板 C 助手,与 env_get 全程同构;walk 落 lib/std/fs.ct,read_dir 双口径分歧用 `read_or` 既有两宿通吃惯用法,每层字节序插入排序保证确定性;proc 面绕开 `&ProcBox64` 普通形参不可发射的在册限制(net Task 6),加一枚无 out_len 的新 extern `ctron_proc_run_argv_rc`(capture=0 rc 型 = W4 run_step 的精确所需),捕获型仍走 bind 直用(loom gitx 先例不变)。

**Tech Stack:** Ctron(lib/std、lib/proc)+ 编译器四锚点(compiler/src)+ 发射模板(driver_emit.ct)+ C(lib/proc/c_src/ctron_proc.c)。

**Spec:** `docs/superpowers/specs/2026-10-10-build-driver-design.md` §7 前置件三件 + §8 W3 行(验收门「各自模块门 + smoke 全绿(标准泳道口径)」)。**smoke 口径修正(依现状)**:smoke --full 当前有对端在飞的环境红(基线浮动),验收 = **smoke 红数与基线零差**(开工前记录基线,收工复跑对比),模块门全绿。

## Global Constraints

- 双宿纪律:每个新面必须在解释臂(ctron-cc run)与发射臂(ctron-emit+cc)下行为一致;`read_dir`/`read_file` 在 seed 解释下返回 Option、发射原生返回裸 Str——lib/std 新代码**必须**抄 `lib/std/fs.ct:24-34 read_or` 的两宿通吃 match 形惯用法,不得自创。
- 新内建(本计划仅 env_set)四处锚点一个不漏:①`compiler/src/eval_call.ct` 解释臂(env_get 臂 :492 旁)②`compiler/src/trans_expr.ct` 发射臂(:1369 旁)③`compiler/src/trans_ty.ct` 形表(:1640 旁)④`compiler/src/sem_type.ct:34` prelude_ok 白名单。**parse_pkg.ct:49-60 与 sem_calls.ct prel 表不加**(env_get 亦不在,保持同构)。发射模板 C 助手加在 `driver_emit.ct:543` 的 env_get 助手旁。
- 已知名撞车:`env_set` 在 selfhosted/cc.ct:4759、selfhosted/ev2.ct:2355 已有同名 fn(语义 = CVM 解释器变量换绑,非 OS env)——前奏新增后这两处将出 W8040 shadow 警告,属预期(lint 默认警告不红),不改 selfhosted,报告中登记。
- env_set 不进 comptime 禁表(sem_comptime.ct:10)与插件沙箱禁表(plugin.ct:16-22)(与 env_get 同构);不进 caps 注册表(env_get/read_dir 均不受 E4010 门控,前奏内建不经 use 通道)。
- 对端并发:开工前 `git status`(对端在飞 trans_conc.ct/trans_expr.ct/driver_emit.ct——**T1 要动 trans_expr.ct 与 driver_emit.ct,若发现对端改动与本任务 hunks 交叠,STOP 升级**);pathspec 限定,禁 -A。
- smoke 基线纪律:T1/T2 开工前先跑 `sh compiler/test/smoke.sh --full` 记基线红数,收工复跑对比零新增;T3 不触编译器与 lib/std,模块门 = tests/proc/run.sh,免 smoke(报告中说明理由)。
- 报告文件命名:`.superpowers/sdd/task-w3-{1,2,3}-report.md`(勿覆写他泳道同名件)。

---

### Task 1: `env_set` 内建(写 OS 环境变量)

**Files:**
- Modify: `compiler/src/eval_call.ct`、`compiler/src/trans_expr.ct`、`compiler/src/trans_ty.ct`、`compiler/src/sem_type.ct`、`compiler/src/driver_emit.ct`、`lib/std/process.ct`

**Interfaces:**
- Produces: 内建 `env_set(name: Str, value: Str) -> Bool`(setenv overwrite=1 语义;true=成功);发射 C 助手 `ctron_env_set`;process.ct 增 roundtrip 内嵌 test。W4 驱动器据其在 spawn 前注入 CTRON_STDPATH。

- [ ] **Step 1: 失败测试先行**

`lib/std/process.ct` 由纯注释门面改为含真实 test(**test 块语法勿臆造——镜像 `lib/std/fs.ct:36-47`「read_or hits and defaults」的既有块形与断言风格**)。断言序列(roundtrip 两段):

1. `env_set("CTRON_W3_ENV_PROBE", "hello")` 为真 → `env_get` 读回 "hello";
2. `env_set(同键, "world")` 覆写为真 → `env_get` 读回 "world"。

(探针键唯一化,跑完即弃;文件头注释补一行 env_set 运行底座说明。跑法:`CTRON_STDPATH=$PWD/lib/std compiler/bin/ctron-cc run lib/std/process.ct` 解释臂先看红——env_set 未注册报 E2020 未解析;发射臂 `compiler/bin/ctron-emit run lib/std/process.ct > /tmp/p.c && cc -O1 -w /tmp/p.c -o /tmp/p && /tmp/p`,同样红。)

- [ ] **Step 2: 四锚点 + C 助手实现**

① `eval_call.ct` env_get 臂(:492-501)后加 env_set 臂:**先读 fs_write 或 fs_exists 的两参臂**,镜像其取参/tag 约定(两个 Str 参;返回 Bool 值构造——镜像 fs_exists 臂的 Bool 返回形);setenv 失败(非 0)返回 false。参数个数不对 → panic("env_set args") 同款。

② `trans_expr.ct` env_get 发射臂(:1369)后:`if callee == "env_set" { return "ctron_env_set(" + 两参发射拼接 + ")"; }`(拼接写法镜像 env_get 臂)。

③ `trans_ty.ct` 形表(:1640 env_get → "s" 行旁):`env_set → "b"`(镜像 fs_exists 的 Bool 行写法)。

④ `sem_type.ct:34` prelude_ok 白名单:env_get 同行追加 env_set。

⑤ `driver_emit.ct:543` env_get 助手旁发射:

```c
static int ctron_env_set(const char* n, const char* v) { return setenv(n, v, 1) == 0; }
```

- [ ] **Step 3: 验证**

```bash
CTRON_STDPATH=$PWD/lib/std compiler/bin/ctron-cc run lib/std/process.ct && echo interp-ok
compiler/bin/ctron-emit run lib/std/process.ct > /tmp/w3a.c && cc -O1 -w /tmp/w3a.c -o /tmp/w3a && /tmp/w3a && echo emit-ok
sh compiler/test/smoke.sh --full 2>&1 | tail -3   # 对比开工前基线:零新增红
sh tests/fmt/parity.sh                            # fmt 门
```

Expected: 双臂 ok;smoke 红数 = 基线;selfhosted 两个 W8040(env_set shadow)为唯一预期新增警告(lint 非严格不红——如 smoke 对警告计数敏感,以基线对比口径记录)。

- [ ] **Step 4: Commit**

```bash
git add compiler/src/eval_call.ct compiler/src/trans_expr.ct compiler/src/trans_ty.ct compiler/src/sem_type.ct compiler/src/driver_emit.ct lib/std/process.ct
git commit -m "feat(std): env_set 内建(写 OS 环境变量;W3 前置件①)——四锚点注册与 env_get 全程同构(解释臂/trans_expr 发射臂/trans_ty 形表 b/sem_type prelude_ok)+发射模板 ctron_env_set(setenv overwrite=1)+process.ct 增 roundtrip+覆写双断言 test(两宿通吃);selfhosted cc.ct/ev2.ct 两处同名 fn 出 W8040 shadow 属预期(语义为 CVM 换绑非 OS env,不改);门:双臂 roundtrip 绿+smoke 零新增红+fmt 净;W4 驱动器 spawn 前注入 CTRON_STDPATH 的底座"
```

---

### Task 2: `fs.walk` 递归遍历(字节序确定性)

**Files:**
- Modify: `lib/std/fs.ct`

**Interfaces:**
- Consumes: read_dir 内建(双口径:解释 Option/发射裸 Str——**必须抄 read_or :24-34 的 match 惯用法**);fs_mkdir/fs_write/fs_delete(测试用)。
- Produces: `pub fn walk(root: Str) -> List[Str]`——DFS 前序,每层条目字节序插入排序(镜像 pkg_dep.ct:896 的排序惯例),路径 = root 前缀原样拼接;root 缺失/不可读 = 空列表;目录含自身入列;深度上限 64(防符号链接环,无 stat 内建,v0 文档化)。W4 驱动器测试发现/缓存枚举消费。

- [ ] **Step 1: 设计钉(先读三处再动笔)**

动手前必读:①`lib/std/fs.ct` 全文(read_or 的 read_dir…… 准确说 read_or 用的是 read_file——**read_dir 的两宿 match 形在全仓找既有消费范例**,pkg_dep.ct:896 区是发射面直用,3j2 解释面范例在 smoke 覆盖的 std 文件里找;若全仓无「lib/std 内 match read_dir」先例,则按 read_or 对 read_file 的同构写法落地,报告中登记这是该惯用法的首次 read_dir 应用)②`compiler/src/pkg_dep.ct:896-930` 的插入排序惯例 ③lib/std 里换行切分的现成助手(str/config 域;不引新依赖,fs.ct 现零 use 自洽纪律——**fs.ct 不得新增 use**)。

- [ ] **Step 2: 实现 + 失败测试转绿**

fs.ct 尾部增(test 之上):

```ctron
// walk:递归遍历(root 前序;每层字节序;root 缺失=空表;深度上限 64 防符号链接环)
pub fn walk(root: Str) -> List[Str] {
    var out = List[Str]()
    walk_into(root, out, 0)
    return out
}

fn walk_into(dir: Str, var out: List[Str], depth: I32) {
    ...(match read_dir 双宿形;条目按字节序插入排序;逐项 full = dir + "/" + 名;
        full 入列;若 full 可再列且 depth < 64 则递归)...
}
```

(私有助手不入 pub 面;空目录入列即为叶;排序/切分/递归的具名助手按 fs.ct 现有风格 `walk_*` 前缀。)

内嵌 test(hermetic,唯一目录名用内建单调量避免碰撞):

```ctron
test "walk deterministic depth-first byte order" {
    ...(在 /tmp 下建唯一根:fs_mkdir 根/a、根/a/b、根/x;fs_write 三文件 根/f1、根/a/f2、根/a/b/f3;
        exact 断言 walk(根) == [根, 根/a, 根/a/b, 根/a/b/f3, 根/a/f2, 根/f1, 根/x](字节序推导后写死);
        walk(根/缺失) == [];清理 fs_delete 三文件;目录残留登记)...
}
```

- [ ] **Step 3: 验证**

```bash
CTRON_STDPATH=$PWD/lib/std compiler/bin/ctron-cc run lib/std/fs.ct && echo interp-ok
compiler/bin/ctron-emit run lib/std/fs.ct > /tmp/w3b.c && cc -O1 -w /tmp/w3b.c -o /tmp/w3b && /tmp/w3b && echo emit-ok
sh compiler/test/smoke.sh --full 2>&1 | tail -3   # 零新增红(对 T1 后基线)
sh tests/fmt/parity.sh
```

- [ ] **Step 4: Commit**

```bash
git add lib/std/fs.ct
git commit -m "feat(std): fs.walk 递归遍历(W3 前置件②)——DFS 前序+每层字节序插入排序(pkg_dep 惯例)=全序确定性;root 缺失空表;深度上限 64 防符号链接环(无 stat,文档化);read_dir 双口径用 read_or 同构 match 形;内嵌 hermetic test 精确表断言(两宿通吃);门:双臂绿+smoke 零新增红+fmt 净;W4 驱动器测试发现/缓存枚举底座"
```

---

### Task 3: lib/proc rc 型 argv 面 + argv_to_blob + 上限 12→32

**Files:**
- Modify: `lib/proc/c_src/ctron_proc.c`、`lib/proc/bind.ct`、`lib/proc/proc.ct`、`tests/proc/src/main.ct`

**Interfaces:**
- Produces: extern `ctron_proc_run_argv_rc(cwd: Str, var blob: U8[], n: I64, stdin_path: Str) -> I32`(capture=0,stdout/stderr/stdin 继承语义与 argv_p 同源;stdin_path 空串=继承);门面 `proc.run_argv_rc` 与 `proc.argv_to_blob(argv: List[Str], var blob: U8[]) -> I32`(NUL 分隔打包,返总字节,溢出/空 argv = -1);argv 上限 12→32。W4 驱动器 run_step 的精确进程面;捕获型继续 bind 直用(loom gitx 先例,proc.ct:36-38 注释维持)。

- [ ] **Step 1: C 侧**

`ctron_proc.c` 读全文后重构:argv_p 的执行核(argv 解析+chdir+stdin dup2+execvp+wait)抽 `static int proc_exec_core(...)`;`ctron_proc_run_argv_p` 改薄壳保持 ABI 不变;新增:

```c
int ctron_proc_run_argv_rc(const char* cwd, const void* blob, int64_t n, const char* stdin_path) {
    ...(同核,capture 恒 0、out NULL、out_len NULL;头注口径:rc 型工具面,stdout/stderr 继承)...
}
```

argv 上限 `argv[12]` → `argv[32]`(含守卫检查与溢出消息);头部注释口径同步。**编译验证**:`cc -O1 -w -I lib/proc/c_src -c lib/proc/c_src/ctron_proc.c` 零警告。

- [ ] **Step 2: 门面 + bind 声明**

bind.ct 墶 `pub extern "c" fn ctron_proc_run_argv_rc(...)`(与 argv_p 同区);proc.ct use 行补名,增两门面(:12 run 的同构薄壳)与 argv_to_blob(镜像 words_to_blob :54-86 的打包循环,入参 List[Str],逐元素 byte 级拷贝 + NUL 分隔);头注更新:rc 型有门面,捕获型仍 bind 直用(理由注释保留)。

- [ ] **Step 3: 测试(tests/proc/src/main.ct 增臂,镜像既有 :54-75 argv_p 例)**

①run_argv_rc rc 面:`echo` 带 3 参(继承 stdout——测试主进程不捕获,断言 rc==0);②run_argv_rc rc 透传:`false` → rc==1;③run_argv_rc stdin_path 空=继承 + 缺程序 rc==127;④**上限实证**:argv_p(capture=1)带 14 段 argv(`/bin/echo` + 13 参),断言 plen.v == 期望字节数(12 旧限下此臂必败,新限过);⑤argv_to_blob:words 场景对照(words_to_blob 与 argv_to_blob 同输入产同 blob)+ 空表 -1。

- [ ] **Step 4: 验证 + Commit**

```bash
sh tests/proc/run.sh    # 模块门全绿(既有臂零回归+新臂全过)
sh tests/fmt/parity.sh
```

(免 smoke 理由写进报告:未触 compiler/src、driver_emit、lib/std;lib/proc 唯一消费门即 tests/proc。)

```bash
git add lib/proc/c_src/ctron_proc.c lib/proc/bind.ct lib/proc/proc.ct tests/proc/src/main.ct
git commit -m "feat(proc): rc 型 argv 执行面+argv_to_blob 门面+上限 12→32(W3 前置件③)——新 extern ctron_proc_run_argv_rc(capture=0 无 out_len,绕开 &ProcBox64 普通形参不可发射在册限制[net Task 6];stdin 空串继承/缺程序 127 与 argv_p 同源),执行核抽 proc_exec_core 共用,argv_p ABI 不变;门面 run_argv_rc+argv_to_blob(List[Str] NUL 打包);上限实证臂 14 段 argv;捕获型仍 bind 直用(loom gitx 先例);门:tests/proc 全绿+fmt 净;W4 驱动器 run_step 进程面"
```

---

## 收尾(控制器自做)

1. 计划档回写执行记录;ledger;memory。
2. 推送。W3 幅度小于 W2,不另派全分支终审(三任务各自评审 + smoke 零差口径代之);T1 若与对端在飞件交叠升级。
3. W3 齐后 W4 立项(std/config L1 通用面 → compiler/driver/ 骨架 → 逐族对拍 → 垫片翻转,另立计划)。

## 执行记录

(执行时逐任务回写。)
