# web 包 —— Ctron 应用框架(registry 包)

应用框架层:Req/Resp/Router/中间件/视图微 builder,坐于
[http](../http/README.md)(net 之上协议半层)与 [net](../net/README.md)
(TCP 门面)两域包之上;应用代码(handler + views + store)零 HTTP 底座。
设计全案 = 仓库 `docs/superpowers/specs/2026-09-27-web-framework-design.md`
(API 定稿面 §4,语义条款 §7,范围边界 §8);包内速览与 API 表以仓库
`pkgs/web/README.md` 为单一真源。

**消费形态 = `use web.<子模块>.{...}`**(包清单 `pkgs/web/Ctron.ctcl`)。

## 门面契约(spec §4.4 流式演进条款)

应用与中间件只经构造器与 `.with()` 族触 Resp,禁直构 `Resp` 字面量、不依赖
字段布局;Resp 序列化咽喉收敛于 serve.ct `render` 一处(全仓唯一状态行/头区/
CL 拼接点)。

## 参考页(签名面自动生成)

| 页 | 内容 |
|---|---|
| [core.md](core.md) | Req/Resp 核心形 |
| [router.md](router.md) | 路由(纯函数,零 socket 确定性) |
| [mw.md](mw.md) | 中间件链 |
| [json.md](json.md) | JSON 面 |
| [serve.md](serve.md) | 服务装配(render 咽喉) |
| view.ct(参考页暂缺) | 视图微 builder |
| [static.md](static.md) / [openapi.md](openapi.md) / [compress.md](compress.md) / [guard.md](guard.md) | 静态/OpenAPI/压缩/守卫 |

\* view.md 暂缺:doc 驱动 use 合并 E5030 与真链分歧(登记债务),API 面以
仓库 README §4 定稿表为准。

## 测试口径

引导链双臂:语义门 `ctc.sh check` 十文件全 OK + 行为裸跑每文件 rc=0;含
crypto 的 mw/guard 走 canonical 编译器快道(`CTRON_STDPATH=$PWD/std
compiler/bin/ctron-cc run web/<f>`,C seed 宿主 crypto 闭包面挂死在册)。
压缩原语 extern lane 帧不可 interp——压缩往返 e2e 属 emit 臂。
