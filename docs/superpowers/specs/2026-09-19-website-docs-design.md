# Ctron GitHub Pages 主页与文档系统设计(v1 双语分批)

> 日期:2026-09-19
> 状态:设计已批准,待实施
> 范围:Ctron 对外主页 + 文档系统(v1 五件套),MkDocs Material,同仓 Actions 部署到 GitHub Pages。
> 不含:自定义域名、博客/changelog、playground(wasm)、文档版本化、doc comment 生成器(均挂账,§7)。

## 0. 决策记录

| 决策 | 结论 | 落选项 |
|---|---|---|
| 语言 | **双语分批**:英文为主,中文全量先行,英文首批只上首页/入门/下载,骨架留好 | 落选:中文单语(受众受限)、英文单语(内容成本最高) |
| 内容范围 | **五件套**:首页 / 入门 / 语言规范 / std 参考 / 下载页 | 落选:spec 直搬最小面(观感糙)、全家桶(运营成本高) |
| 技术栈 | **MkDocs Material**(+ mkdocs-static-i18n 插件) | 落选:手写极简站(导航/搜索/i18n 全自造)、mdBook(i18n 弱) |
| 部署 | **同仓 + 专用 Actions workflow** → deploy-pages | 落选:gh-pages 分支(噪音)、独立仓库(内容与代码割裂) |
| 站点版本 | 单版本起步;文档版本化挂账 | |

## 1. 硬前提与执行次序(排一切之前)

1. 仓库当前**无远端**——GitHub Pages 要求仓库在 GitHub 上且 **public**(免费版约束;
   install.sh 下载与 Releases 资产直链同样要求 public)。
2. 次序:**建仓 → push main → 发 v0.0.1 tag(分发线)→ 网站上线(本设计)**。
   下载页与 install.sh 指向 GitHub Releases,Releases 未发则下载页优雅降级(§4)。
3. 这两线在"建仓 push"汇合;URL 形态 `<user>.github.io/Ctron`,自定义域挂账。

## 2. 架构与目录

```
website/                        # mkdocs 项目根(与语言代码彻底分离)
├── mkdocs.yml                  # Material 主题 + i18n 插件 + 导航 + 搜索
├── docs/
│   ├── index.{en,zh}.md        # 首页
│   ├── getting-started.{en,zh}.md
│   ├── spec/*.md               # 语言规范十章(同步件,§3)
│   ├── std/*.md                # std 参考(20+ 模块页,§2.1)
│   ├── examples.{en,zh}.md     # ctgrep/ctwc/ctwf 注解版
│   └── download.{en,zh}.md     # 下载/安装
└── .github/workflows/pages.yml # push main 且 website/ 或 docs/spec 变更 → build → deploy-pages
```

构建依赖只在 Actions(本地 `mkdocs serve` 预览为可选)。文件形态:默认语言(en)
无后缀,中文加 `.zh` 后缀;英文未上线页面的无后缀文件以中文占位 + "英文待翻"注记。
切换器用 Material 内建语言选择。

## 3. 内容面

### 3.1 首页

一句话定位 + install 一行(复制按钮,指向 `releases/latest/download/install.sh`)
+ 三特性卡(自举工具链 / 编译到 C 的可移植后端 / 结构化并发与错误链)+ 规范与
GitHub 入口。英文为主。

### 3.2 入门

安装 → hello, ctron → `ctc run` → `ctc build` 出可执行 → `ctc new` 起项目;
15 分钟体量;口径与分发线一致(run/check 零依赖、build 需 cc、Windows 需 mingw)。

### 3.3 语言规范十章(同步件,防漂移)

`docs/spec/*.md` 是内部规范(实现者语风,含规范锚),上站需语风适配——但规范是活的,
fork 必漂移。机制:

- **单向同步脚本 `tools/sync_site_spec.sh`**:拷 `docs/spec/*.md` →
  `website/docs/spec/` + 注入导航 front matter + 站内语风补丁层(最小适配,不改内容);
- **CI 漂移检查**:pages workflow 内 diff `docs/spec` 与同步基线,不一致即红,逼同步;
- v1 手动跑同步;规范页改写以 front matter + 最小语风适配为限。

### 3.4 std 参考

每模块一页:用途一句话 + `pub fn` 签名表 + 单行说明。v1 用 ~30 行签名提取脚本
从 `std/*.ct` 扫 `pub fn` 半自动生成骨架,人补说明。doc comment + 生成器挂账。

### 3.5 下载页

五平台产物矩阵(链接 Release 资产,命名与分发线一致
`ctron-<ver>-<os>-<arch>.tar.gz` / windows zip)+ install.sh 三种方式 +
cc 依赖口径。Releases 未发时显示"即将发布"。

### 3.6 示例页

三个示例(ctgrep/ctwc/ctwf)注解版:源码 + 逐段讲解 + `ctc run`/`build` 复现命令。

## 4. 错误处理与降级

| 场景 | 行为 |
|---|---|
| 英文页缺失 | **文件形态规则**:默认语言文件无后缀(=en),翻译文件 `.zh` 后缀;英文未上线的页面,无后缀文件暂以中文内容占位 + front matter 注记"英文待翻"——en 树永远完整,无回落分支 |
| Releases 未发 | 下载页降级"即将发布";install.sh 按钮隐藏 |
| Pages 构建失败 | Actions 红,站点保持上一版(deploy-pages 原子切换) |
| spec 漂移 | workflow 漂移检查红,合入被阻 |

## 5. 验收

1. Pages workflow 绿,站点可达,语言切换可用,全站搜索可用;
2. 五件套每页可达;spec 十章全在且漂移检查绿;
3. 下载页与 v0.0.1 Release 资产真实互联(资产名对齐分发线);
4. en 树完整(每页可达,无 .en.md 缺失死链);英文待翻页面的中文占位页均有 front matter 注记;
5. 本地 `mkdocs serve` 预览路径可用(可选依赖,文档写明)。

## 6. 明确不做(v1)

自定义域名、博客/changelog、playground(wasm 解释器)、mkdocs 文档版本化、
语言级 doc comment 文档生成器、SEO/统计。

## 7. 挂账(v1 之后)

按优先级:playground(等解释器面更稳)→ 英文全量 → 文档版本化 → doc comment
生成器 → 自定义域。
