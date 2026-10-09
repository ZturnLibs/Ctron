# API 文档补齐计划(2026-10-09)

## 背景审计(答案先行)

**Ctron 有 API 文档的骨架与工具链,没有成形的 API 文档。** 现状四面:

| 面 | 现状 | 缺口 |
|---|---|---|
| 语言规范 | `docs/spec/` 12 章齐;网站挂 10 章 | 11-net/12-db 未挂 nav |
| std 参考页 | `website/docs/std/` 20 页(55251cec);`tools/std_doc.py` v1 正则生成 | 7 模块缺页(crypto/iter/log/ndjson/pb/process/uuid);已有页说明栏全 TODO;v1 漏 impl/struct 面、不读源码注释 |
| 域包参考 | gui/web 有 README(面级);`server-guide.md` 用法级 | net/http/tls/db/ffi/rt 无 README 无参考页;pkgs s3/pkg 零文档;无逐 API 页 |
| 机器通道 | `ctc.sh doc --format=json`(driver_doc.ct,T50 S0 iface 投影)已落:pub 符号表+trait/impl 面+**doc 注释捕获**,确定性输出 | 未接入网站生成;无「agent 怎么用」的说明;CI 无同步门 |

关键实证:`ctc.sh doc lib/std/<mod>.ct` 已能一键产出**带源码注释**的 API 面
(26/26 模块验证通过,单次 0.25s;JSON 面 kind=fn/struct/enum/trait/impl 全捕获)。
补齐的主工作不是「从零写文档」,而是**把已有源码注释面接进人类页与门禁**。

## 裁决(自主默认,用户可改)

1. **单一真源 = 源码 doc 注释**(模块头注 + 每个 pub decl 前注释)。人类页
   (website)与 agent 面(JSON)同源生成,不养第二份手写签名表。
2. **生成器后端 = ctron-doc JSON**(`ctc.sh doc <file> --format=json`);`std_doc.py`
   降为渲染薄壳(Python 只做排版/nav/门禁,不再自己正则解析源码)。v1 双轨退役。
3. **页形**:标题 + tier/since 元行 + 手写区(模块简介,注释界定,再生保留)+
   生成区(pub struct/enum 字段表 → trait → impl 方法 → pub fn 签名表;
   说明列 = 源码注释,缺注 = `—`)。
4. **导航全挂**:std 26 分页 + spec 11/12 章入 mkdocs nav。
5. **漂移门**:`std_doc.py --check` 入 ci.yml(镜像 pages.yml 的
   `sync_site_spec --check` 先例);pages.yml 触发路径 `std/**` 修为 `lib/std/**`
   (0930 库根布局后旧路径永不命中 = std 源改不触发网站构建,存量 bug)。
6. **AI 面**:`ctc.sh doc <file> --format=json` 即 agent 消费通道,用法写入
   std/README 与网站(一次说明,双侧可见)。

## 波次

- **W1(已落库 187ea6aa)** std 26 模块页全量:std_doc.py v2 + 重生成 + nav 挂接
  + ci 门 + pages 触发路径修 + mkdocs strict 绿。
- **W1b(已落库 5e19effe)** 说明列回填:20 模块 66 处 pub decl 回源码补注释
  (fn/struct 面;trait/impl 面驱动器 S0 设计恒传空串、页面该二节本无说明列,
  豁免);stdpkg 字节级副本同步;说明列空缺清零。
- **W2(已落库 4455a002)** 域包参考:net/http/tls/db/ffi 33 张页 + 六域概览
  README;生成器域模式;rt 指针页(无 .ct 门面)。
- **W3(已落库 b2f49ad7)** pkgs 参考:八包 21 张页 + 概览 README
  (gui.ct / web/view.ct 因驱动 E5030 分歧跳过,登记)。
- **W4(本批)** agent 消费指南页(agent.md:通道速查/JSON schema/五坑/
  再生成与门禁)+ nav;双语页(i18n `.zh.md`)维持缓发——生成内容本就以
  中文注释为主体,翻译 Pass 待专批。

## W1 执行细节

- 生成器:`tools/std_doc.py` v2。手写区契约 `<!-- hand:desc -->…<!-- /hand:desc -->`;
  新页手写区 = 模块头注全文(剥 tier/since 行入元行);`--check` 全页逐字节对拍;
  ctron-doc 瞬时失败(驱动重拼撞中间态)重试一次。
- 页内签名重建:fn `name[T](a: T, b: var U) -> V`(mutable → `var ` 前缀);
  vaargs 槽 `...`;struct/enum 字段表;trait/impl 签名代码块。
- 不触 `lib/std/**` 源(W1b 才回源码)、不触 `compiler/**`、不触 `tests/**`
  (机刷泳道在飞,tests/gui 有对端活动;提交 pathspec 限定)。

## 坑位登记

- `ctc.sh doc` 每次调 `build.sh` 重拼驱动;并发机刷改 `compiler/src` 的中间态
  会使个别调用 E1001 假红(实测 pb/str/set/process 四连失败、复跑全绿)——
  生成器必须重试一次并以复跑为准。
- ctron-doc JSON 走 stdout、seed 宿主杂音走 stderr,**调用必须分离流**,
  否则 `json.load` 假红(v1 时代同坑)。
- `bin/ctron-doc`(native)无 CLI 输入通道锚(ANCHORINPUT 字面量待 sed 换靶),
  脚本化调用一律走 `ctc.sh doc`,勿走 native 二进制。
- **ctc.sh doc 未注入库根**(W2 实证):`use <域>.<mod>` 在包子目录
  (http/frm/* 等)解析 W8902 未命中——emit 的 S4-⑤ STDPATH 注入同型缺口;
  生成器以 `CTRON_SITEPATH=<root>/lib` 显式指路,ctc.sh 侧补注随编译器泳道。
- **doc 驱动 E5030 与真链分歧**(W3 实证):gui.ct、web/view.ct 在 doc 驱动
  use 合并报同名 decl 拦截,真链(check/消费面)放行——PKG_SKIP_PAGES 登记
  跳过,语义以真链为准;修复归编译器泳道。
- **源注释假链接**(W2 实证):泛型形 `[I32](x` 撞 markdown 链接语法
  (mkdocs strict 红)——生成器 split_meta 统一转义 `](`。
