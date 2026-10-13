# G5 下载路径 实施计划(sha256 流式 / https 集成 / Range 206 / 流式体读 / lib/down)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** P1-8 落地——模型拉取主路径:sha256 流式(大文件哈希破 Str 域+512MB 上限)、http client https 集成(J6;今天 https 直接拒=隐含前置)、Range/206 客户端面(断点续传)、流式体读三件(边收边落盘,1MiB 上限只辖非流式面)、整请求 deadline、lib/down 下载器域包(续传循环+流式哈希校验+重试退避)。

**Architecture:** W1 纯 lib/crypto(C EVP 流式三件+bind 门面+对拍);W2-W3 lib/http(client scheme 分支接 tls 域握手;use 树 http→tls→net 链形验 E5020/E5030)+lib/crypto 协作;W4 新 lib/down 域包(编排层,坐 http+crypto)。验收=本地 serve e2e(Range 续传断点对比、流式哈希对拍 one-shot、https 自签夹具)。

**Tech Stack:** OpenSSL 3 EVP 流式(EVP_MD_CTX new/init/update/final/free,句柄 I64 面);lib/tls client 全件(tls_client/tls_handshake/tls_read/tls_write_str,tls.ct:66-100);lib/http client_request(client.ct:676,keep-alive 多槽,重定向 5 跳)。

## Global Constraints

- 三宿主纪律(interp/emit/C 宿主);能力显形惯例(门面函数首参值 struct/capability token)。
- use 图严格互斥树(E5020/E5030):lib/down→http+crypto;http→tls→net 链形(禁菱形)。
- pathspec 限定提交;`.cache/emit` 清缓存真跑;每 Wave 独立绿独立落库。

---

### Wave 1: sha256 流式(lib/crypto)

- [ ] C 侧三件(ctron_crypto.c:EVP_MD_CTX 句柄面——new 初始化/update 块入/final 出 32B+free;句柄跨调用=malloc 生命周期,final 即释放)
- [ ] bind.ct 门面(sha256_new→I64/sha256_update(h, U8[])/sha256_final(h)→U8[];镜像既有 StdCrypto 面)+eval/C 宿主镜像(extern 桥 dlsym,interp 可用性随垫片链接)
- [ ] 对拍测试:大缓冲分块 update vs one-shot 摘要逐字一致(100B/1MiB 两档)+ final 后句柄失效负例
- [ ] Commit

### Wave 2: https 集成 + 整请求 deadline(lib/http)

- [ ] client_request scheme 探测(https → tls 握手接线,fd 注入 BIO;use 树补 tls)
- [ ] 内部读写双路(secure 旗 → tls_read/tls_write_str;明文路径零扰动回归)
- [ ] 整请求 deadline_ms 参(client.ct:39-40 销账;内部换算每读截止)
- [ ] 夹具:本地 https server(自签证书)往返;Commit

### Wave 3: Range/206 + 流式体读(lib/http)

- [ ] 206 单范围识别(Content-Range 偏移校验;err 面)+ extra 头 Range 文档化
- [ ] 流式体读三件(response_open/read_chunk/close,与 sse_step 同形;边收边写回调/落盘;max_body 只辖非流式)
- [ ] 夹具:Range 请求 206 判词+分块大响应流式消费;Commit

### Wave 4: lib/down 下载器域包

- [ ] down.ct 门面:download_to_file(url, path, 哈希期望)——Range 续传循环(已有 .part 按文件大小续)+流式哈希校验(W1 件)+重试退避(3 次)
- [ ] e2e:本地 serve 断点续传(中断重启对比完整性)+ 哈希错拒;Commit + COVERAGE 批终

## 明确不做

- 多段 Range;TLS 服务端证书管理面(测试自签即止);HTTP/2;下载并发分片(单流 v0)。
