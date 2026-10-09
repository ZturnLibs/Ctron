# Agent 指南 —— AI/机器消费 Ctron 文档的通道

面向 AI 代理与工具链的文档消费面。原则:**单一真源 = 源码 doc 注释**,
人类页与机器面同源生成、逐字节门禁防漂移。

## 通道速查

| 任务 | 通道 |
|---|---|
| 查某 std 模块的 API(符号+说明) | `compiler/ctc.sh doc lib/std/<mod>.ct --format=json` |
| 查域包/registry 包的 API | 同上,路径 `lib/<域>/<file>.ct` / `pkgs/<包>/<file>.ct` |
| 人读参考页(同源渲染) | [Std Reference](std/README.md) · [Domain Packages](net/README.md) · [Registry Packages](gui/README.md) |
| 语言语义/文法/诊断码 | [Language Spec](spec/README.md)(docs/spec 12 章) |
| std 组织与分层说明 | 仓库 `lib/std/README.md` |
| 发射面缺口清单 | 仓库 `tests/COVERAGE.md` |

## ctron-doc JSON 面

```bash
compiler/ctc.sh doc lib/std/opt.ct --format=json
# {"entry":"...","decls":15,"doc":"<模块头注全文>","iface":[...]}
```

`iface[]` 条目按 kind:

| kind | 字段 |
|---|---|
| `fn` | `name` `generics[]` `params[{name,type,mutable,vaargs?}]` `ret` `doc` |
| `struct` | `name` `generics[]` `fields[{name,type,mutable}]` `doc` |
| `enum` | `name` `variants[{name,tuple[]?,fields?}]` `doc` |
| `trait` | `name` `sigs[]`(方法签名串) |
| `impl` | `sig`(impl 行) `sigs[]`(方法签名串) |
| `extern` | bind 窄桥符号(use 合并混入;消费方忽略) |

`doc` = 源码中该 pub 声明紧前的 `//` 注释(捕获自源文件,确定性输出);
trait/impl 的 doc 恒空串(驱动 S0 设计)。

### 调用坑位(踩过六次以上的都在这)

1. **流分离**:JSON 走 stdout,宿主杂音走 stderr——捕获必须分开,混流即
   `json.load` 假红。
2. **瞬时假红重试**:`ctc.sh doc` 每次调用重拼驱动;并行机刷改 `compiler/src`
   的中间态会让个别调用 E1001 假红(行号指向驱动自身)——**重试一次为准**。
3. **域包子目录需 SITEPATH**:`ctc.sh doc` 未注入库根(与 emit 的 S4-⑤
   同型缺口),`use <域>.<mod>` 解析未命中 W8902——加
   `CTRON_SITEPATH=<仓库根>/lib`。
4. **native 二进制不可脚本化**:`compiler/bin/ctron-doc` 无输入通道锚
   (ANCHORINPUT 字面量待 sed 换靶),一律走 `ctc.sh doc`。
5. **E5030 分歧**:个别文件(gui.ct、web/view.ct)doc 驱动 use 合并报
   E5030 而真链放行(登记债务)——以真链 check 为语义准,iface 面暂缺。

## 参考页的再生成与门禁

参考页由 `tools/std_doc.py` 从源码注释生成(ctron-doc JSON 后端),页内
`<!-- hand:desc -->` 界定的手写简介区再生成时保留:

```bash
python3 tools/std_doc.py                                   # std 全部模块页 + index
python3 tools/std_doc.py --domain net --domain http ...    # 域包页
python3 tools/std_doc.py --pkg gui --pkg web ...           # registry 包页
python3 tools/std_doc.py --check                           # 门禁:全部生成页+index 逐字节对拍(CI 红=漂移)
```

**改了 std/域包源码注释 ⇒ 复跑生成器随批提交**;改 `lib/std/*.ct` 还须同步
`compiler/test/stdpkg/std/` 字节级副本(smoke 漂移断言)。网站以
`mkdocs build --strict` 构建断链/警告即红。
