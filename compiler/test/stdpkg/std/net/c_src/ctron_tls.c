/* ctron_tls.c - Ctron std/tls shim:mbedTLS 3.6.7 包装(BIO-over-hybrid)
 * §P3-C:BIO 回调转调 ctron_net_read_t / ctron_net_write —— TLS 停车不设新
 * 停车点,完整继承 net 垫片的 P2-C 混合化(协程 → MSG_DONTWAIT 直试 + wait_fd
 * 停车;裸线程 → P1 poll/阻塞原路径)与 P3-A 探针消除。本文件【不引用】任何
 * ctron_rt_* 符号(停车发生在 net 垫片内部),net+tls 双链即混合化成立。
 *
 * BIO 契约(mbedtls_ssl_set_bio,取「blocking I/O」标准形态:只有
 * f_recv_timeout,无 f_recv —— ssl.h:2298 注"f_recv == NULL,
 * f_recv_timeout != NULL";net_sockets.c 参考实现逐条对齐):
 *   - send:     发完为止(ctron_net_write 是 send-all 语义;协程上 EAGAIN →
 *               wait_fd 停车重试,裸线程阻塞发完),恒返回全量 n,永不半发、
 *               永不返回 WANT_WRITE(阻塞即无该面)→ mbedtls_ssl_handshake
 *               /ssl_write 的 WANT_WRITE 分支天然不可达;
 *   - recv_timeout: 阻塞至数据/超时(契约原文 ssl.h:844 "must block until
 *               data is received, or the timeout delay expires"),超时返回
 *               MBEDTLS_ERR_SSL_TIMEOUT(ssl.h:856 契约)——映射自
 *               ctron_net_read_t 的 <0 + errno==ETIMEDOUT;EOF 映射 0
 *               (fetch_input 收 0 转 MBEDTLS_ERR_SSL_CONN_EOF,ssl_msg.c:2255
 *               /2324)。因回调阻塞,h WANT_READ 亦不可达 → 握手无热循环
 *               (net_sockets.c 专设 recv_timeout 正为防 WANT_READ 空转);
 *               PARTIAL 读契约:STREAM 分支 fetch_input while(in_left<nb_want)
 *               逐笔累积(ssl_msg.c:2295-2340),read_t 每笔至多 CT_CHUNK 合法。
 *   - STREAM(TCP)下 fetch_input 传给 recv_timeout 的 timeout = conf 的
 *     read_timeout(ssl_msg.c:2311),握手中亦然 —— 故 ctron_tls_handshake 入口
 *     置 read_timeout=0(永久阻塞,握手的阻塞语义),ctron_tls_read 每调用按
 *     timeout_ms 重置(conf 为每连接独享,非共享,改写安全)。
 * STREAM 部分读/写契约:mbedtls_ssl_write 经 flush_output 循环发完 out_left
 * (ssl_msg.c:2373),send-all BIO 下单调用即全量。
 *
 * 错误面(镜像 ctron_net.c 口径):
 *   - thread-local errno 槽 + noinline 访问器(同一 TLV 迁移约束:darwin/arm64
 *     clang 缓存 TLV 槽位解析于 callee-saved 寄存器,协程跨 worker 迁移后直读/
 *     直写会命中别的线程的块 —— ctron_net.c:68-73 实证与对策,同款照抄);
 *   - 槽存 mbedTLS rc(负值,如 -0x2700)或 BIO 层 errno(正值);mbedtls 转
 *     文本经 ctron_tls_strerror(thread-local 静态,facade str_from_c 深拷);
 *   - BIO 层失败的底层 errno 细节留在【net】槽(ctron_net_last_errno 可读,
 *     BIO 回调与调用方同线程);TLS 槽存 MBEDTLS_ERR_NET_x / MBEDTLS_ERR_SSL_x。
 *
 * 缓冲口径(登记,任务书 char* 形式的发射面落地):
 *   - ctron_tls_read 取 ct_view6 视图 lane(每字节一条 int64 lane)——与
 *     ctron_net_read_t 同形(发射器 &I64[] 唯一缓冲形;布局同型即 ABI 兼容,
 *     tests/ffi/repr_c 先例),内部经暂存逐条写 lane,单调用搬运上限 CT_CHUNK
 *     (与 net 口径⑤一致);
 *   - ctron_tls_write 取 const char*(Str 零拷贝直通,同 ctron_net_write_str);
 *   - 读暂存 = ctx 堆上 CT_CHUNK(协程栈冻结 64KB,net 垫片单帧 ≤4KB 预算,
 *     栈上再开 32KB lane 数组即破预算 —— 故读路径入 ctx 堆,写路径 lane 分块
 *     512×8B=4KB/帧,同 net 单帧预算)。
 *
 * ALPN 生存期(任务自查项):mbedtls_ssl_conf_alpn_protocols 保存指向协议串的
 * 指针(ssl_tls.c:9658 直接 strlen/memcmp)→ 协议串深拷归 ctx 所有,close 才
 * 释放;selected 指针指回本表,get_alpn 即时拷出。服务端按【服务端】偏好序选
 * (ssl_tls.c:9663 "Use our order of preference")。
 *
 * fd 所有权:本文件【绝不】关 fd —— fd 归 net 层(TcpStream Drop);Drop 序
 * (任务自查项):TlsConn Drop → close_notify 尽力发(fd 已关亦容错)→ ssl_free
 * → conf_free → 证书/密钥/drbg/entropy/alpn/暂存。TlsConn 与 TcpStream 的 Drop
 * 顺序无关紧要(close_notify 失败被吞)。
 *
 * 主机名口径(登记):冻结面无 host 参 → client 默认 set_hostname(NULL) 显式
 * opt-out(mbedTLS 3.6 契约:REQUIRED 链验证保留,仅名字匹配跳过;从不清
 * set_hostname + REQUIRED = 握手期 WITHOUT_HOSTNAME 错)。名字匹配待后继
 * 随 host 参任务开启(增补面 ctron_tls_set_hostname 已备)。
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
#define CT_TLS __declspec(thread)
#else
#define CT_TLS _Thread_local
#endif

#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>   /* 仅取 MBEDTLS_ERR_NET_* 常量,不链 MBEDTLS_NET_C */
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/error.h>

/* 视图 lane 镜像:&I64[] 发射 ctron_view6 { d, n } 按值(§9.6;ctron_net.c
 * ct_view6 独立 TU 布局同型即 ABI 兼容) */
typedef struct { int64_t* d; int64_t n; } ct_view6;

void ctron_tls_close(int64_t h);

/* net 垫片面(强符号,net+tls 恒双链;签名以 ctron_net.c 为准) */
int64_t ctron_net_read_t(int64_t fd, ct_view6 buf, int64_t cap, int64_t timeout_ms);
int64_t ctron_net_write(int64_t fd, ct_view6 buf, int64_t n);
int64_t ctron_net_last_errno(void);

/* 单调用搬运上限(与 ctron_net.c CT_CHUNK 同值同口径⑤;两处常量,改须同步) */
#define CT_CHUNK 4096
/* BIO 回调单帧 lane 暂存条数(512×8B=4KB/帧,协程栈预算内,同 net 垫片单帧) */
#define CT_LANES 512

/* ---- 错误槽(TLV 约束与对策:注释见文件头;逐字镜像 ctron_net.c:57-81) ---- */
static CT_TLS int64_t ct_tls_errno_v = 0;

__attribute__((noinline)) static int64_t* ct_tls_err_slot(void) { return &ct_tls_errno_v; }

int64_t ctron_tls_last_errno(void) { return *ct_tls_err_slot(); }

/* mbedTLS rc → 文本(thread-local 静态,Ctron 侧 str_from_c 深拷,§9.6 三约定) */
const char* ctron_tls_strerror(int64_t e) {
    static CT_TLS char ct_tls_strerr_buf[256];
    mbedtls_strerror((int)e, ct_tls_strerr_buf, sizeof(ct_tls_strerr_buf));
    return ct_tls_strerr_buf;
}

/* 任务书冻结形(出参通道;C 消费面 —— Ctron 侧无可变缓冲类型,门面走
 * last_errno + strerror 路径,见 bind.ct 头注) */
void ctron_tls_last_error(char* out, int64_t cap) {
    if (out == NULL || cap <= 0) return;
    mbedtls_strerror((int)*ct_tls_err_slot(), out, (size_t)cap);
}

/* ---- ctx(句柄 = 堆胞指针作 I64;<0 = 错) ---- */

typedef struct {
    mbedtls_ssl_config conf;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt own;      /* 本端证书链(server 面) */
    mbedtls_pk_context pkey;   /* 本端私钥 */
    mbedtls_x509_crt ca;       /* 对端验证信任锚(client 面) */
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context entropy;
    int64_t fd;                /* BIO p_bio 指向此槽;handshake 时置入 */
    int is_server;             /* 端点(handshake 的 hostname 语义分叉用) */
    int hostname_set;          /* 显式 set_hostname 过(含 NULL opt-out) */
    char** alpn_strs;          /* ALPN 深拷串(生存期:close,见文件头) */
    const char** alpn_list;    /* NULL 结尾,交予 conf(指针归 ctx) */
    int alpn_n;
    unsigned char* rd_stage;   /* 读暂存(ctx 堆;栈预算理由见文件头) */
} ct_tls_ctx;

/* ---- BIO(mbedTLS 回调 → net 垫片;契约对齐 net_sockets.c,见文件头) ---- */

static int ct_bio_send(void* p, const unsigned char* buf, size_t len) {
    int64_t fd = *(int64_t*)p;
    int64_t lanes[CT_LANES];
    size_t off = 0;
    while (off < len) {
        size_t chunk = len - off;
        if (chunk > (size_t)CT_LANES) chunk = (size_t)CT_LANES;
        for (size_t i = 0; i < chunk; i++) lanes[i] = (int64_t)buf[off + i];
        ct_view6 v = { lanes, (int64_t)chunk };
        if (ctron_net_write(fd, v, (int64_t)chunk) < 0) {
            return MBEDTLS_ERR_NET_SEND_FAILED;   /* 底层 errno 在 net 槽 */
        }
        off += chunk;
    }
    return (int)len;                              /* send-all:恒全量 */
}

static int ct_bio_recv_timeout(void* p, unsigned char* buf, size_t len, uint32_t timeout_ms) {
    int64_t fd = *(int64_t*)p;
    if (len == 0) return 0;
    int64_t lanes[CT_LANES];
    int64_t want = (int64_t)len < (int64_t)CT_LANES ? (int64_t)len : (int64_t)CT_LANES;
    int64_t n = ctron_net_read_t(fd, (ct_view6){ lanes, want }, want, (int64_t)timeout_ms);
    if (n == 0) return 0;                          /* EOF → mbedTLS 转 CONN_EOF */
    if (n < 0) {
        /* 超时 → SSL_TIMEOUT(契约);其余 → NET_RECV_FAILED(细节在 net 槽) */
        if (ctron_net_last_errno() == (int64_t)ETIMEDOUT) return MBEDTLS_ERR_SSL_TIMEOUT;
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    for (int64_t i = 0; i < n; i++) buf[i] = (unsigned char)lanes[i];
    return (int)n;
}

/* ---- 生命周期 ---- */

/* 系统 CA 探测(ca_path 空串时):darwin / 主流 linux 布局;命中即用。
 * _WIN32 无探针面(显式路径),登记。 */
static const char* ct_ca_probe(void) {
    static const char* cands[] = {
        "/etc/ssl/cert.pem",                        /* macOS */
        "/etc/ssl/certs/ca-certificates.crt",       /* Debian/Ubuntu */
        "/etc/pki/tls/certs/ca-bundle.crt",         /* RHEL/Fedora */
        "/usr/local/etc/ssl/cert.pem",              /* FreeBSD / brew */
        "/etc/ssl/ca-bundle.pem",                   /* OpenSUSE */
    };
    for (int i = 0; i < (int)(sizeof(cands) / sizeof(cands[0])); i++) {
        FILE* f = fopen(cands[i], "rb");
        if (f != NULL) { fclose(f); return cands[i]; }
    }
    return NULL;
}

int64_t ctron_tls_ctx_new(int is_server) {
    ct_tls_ctx* c = (ct_tls_ctx*)calloc(1, sizeof(ct_tls_ctx));
    if (c == NULL) { *ct_tls_err_slot() = ENOMEM; return -1; }
    mbedtls_ssl_config_init(&c->conf);
    mbedtls_ssl_init(&c->ssl);
    mbedtls_x509_crt_init(&c->own);
    mbedtls_pk_init(&c->pkey);
    mbedtls_x509_crt_init(&c->ca);
    mbedtls_ctr_drbg_init(&c->drbg);
    mbedtls_entropy_init(&c->entropy);
    c->fd = -1;
    c->is_server = is_server ? 1 : 0;
    c->hostname_set = 0;
    c->rd_stage = (unsigned char*)malloc(CT_CHUNK);
    if (c->rd_stage == NULL) { ctron_tls_close((int64_t)(intptr_t)c); *ct_tls_err_slot() = ENOMEM; return -1; }

    const unsigned char pers[] = "ctron-tls";
    if (mbedtls_ctr_drbg_seed(&c->drbg, mbedtls_entropy_func, &c->entropy,
                              pers, sizeof(pers) - 1) != 0) {
        ctron_tls_close((int64_t)(intptr_t)c);
        *ct_tls_err_slot() = EIO;
        return -1;
    }
    if (mbedtls_ssl_config_defaults(&c->conf,
                                    is_server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        ctron_tls_close((int64_t)(intptr_t)c);
        *ct_tls_err_slot() = EIO;
        return -1;
    }
    mbedtls_ssl_conf_rng(&c->conf, mbedtls_ctr_drbg_random, &c->drbg);
    /* server = 不索客户端证书(VERIFY_NONE);client = 验服务端链(REQUIRED,
     * CA 经 use_ca_bundle 装入 —— 未装而握手的用法错误在握手期响亮报) */
    mbedtls_ssl_conf_authmode(&c->conf,
                              is_server ? MBEDTLS_SSL_VERIFY_NONE : MBEDTLS_SSL_VERIFY_REQUIRED);
    if (mbedtls_ssl_setup(&c->ssl, &c->conf) != 0) {
        ctron_tls_close((int64_t)(intptr_t)c);
        *ct_tls_err_slot() = EIO;
        return -1;
    }
    return (int64_t)(intptr_t)c;
}

int64_t ctron_tls_use_cert(int64_t h, const char* cert_pem, const char* key_pem) {
    ct_tls_ctx* c = (ct_tls_ctx*)(intptr_t)h;
    if (c == NULL || cert_pem == NULL || key_pem == NULL) { *ct_tls_err_slot() = EINVAL; return -1; }
    int rc = mbedtls_x509_crt_parse_file(&c->own, cert_pem);
    if (rc != 0) { *ct_tls_err_slot() = (int64_t)rc; return -1; }
    rc = mbedtls_pk_parse_keyfile(&c->pkey, key_pem, NULL,
                                  mbedtls_ctr_drbg_random, &c->drbg);
    if (rc != 0) { *ct_tls_err_slot() = (int64_t)rc; return -1; }
    rc = mbedtls_ssl_conf_own_cert(&c->conf, &c->own, &c->pkey);
    if (rc != 0) { *ct_tls_err_slot() = (int64_t)rc; return -1; }
    return 0;
}

int64_t ctron_tls_use_ca_bundle(int64_t h, const char* ca_path) {
    ct_tls_ctx* c = (ct_tls_ctx*)(intptr_t)h;
    if (c == NULL) { *ct_tls_err_slot() = EINVAL; return -1; }
    const char* path = (ca_path != NULL && ca_path[0] != '\0') ? ca_path : ct_ca_probe();
    if (path == NULL) { *ct_tls_err_slot() = ENOENT; return -1; }
    int rc = mbedtls_x509_crt_parse_file(&c->ca, path);
    if (rc != 0) { *ct_tls_err_slot() = (int64_t)rc; return -1; }
    mbedtls_ssl_conf_ca_chain(&c->conf, &c->ca, NULL);
    return 0;
}

int64_t ctron_tls_set_alpn(int64_t h, const char* protos) {
    ct_tls_ctx* c = (ct_tls_ctx*)(intptr_t)h;
    if (c == NULL || protos == NULL) { *ct_tls_err_slot() = EINVAL; return -1; }
    /* 重复设置:先释旧表(生存期纪律见文件头) */
    if (c->alpn_strs != NULL) {
        for (int i = 0; i < c->alpn_n; i++) free(c->alpn_strs[i]);
        free(c->alpn_strs);
        free(c->alpn_list);
        c->alpn_strs = NULL; c->alpn_list = NULL; c->alpn_n = 0;
    }
    if (protos[0] == '\0') { mbedtls_ssl_conf_alpn_protocols(&c->conf, NULL); return 0; }
    int n = 1;
    for (const char* p = protos; *p != '\0'; p++) if (*p == ',') n++;
    c->alpn_strs = (char**)calloc((size_t)n, sizeof(char*));
    c->alpn_list = (const char**)calloc((size_t)n + 1, sizeof(char*));
    if (c->alpn_strs == NULL || c->alpn_list == NULL) { *ct_tls_err_slot() = ENOMEM; return -1; }
    const char* p = protos;
    while (*p != '\0') {
        const char* q = p;
        while (*q != '\0' && *q != ',') q++;
        size_t len = (size_t)(q - p);
        if (len > 0) {
            char* s = (char*)malloc(len + 1);
            if (s == NULL) { *ct_tls_err_slot() = ENOMEM; return -1; }
            memcpy(s, p, len); s[len] = '\0';
            c->alpn_strs[c->alpn_n] = s;
            c->alpn_list[c->alpn_n] = s;
            c->alpn_n++;
        }
        p = (*q == ',') ? q + 1 : q;
    }
    if (c->alpn_n == 0) { mbedtls_ssl_conf_alpn_protocols(&c->conf, NULL); return 0; }
    int rc = mbedtls_ssl_conf_alpn_protocols(&c->conf, c->alpn_list);
    if (rc != 0) { *ct_tls_err_slot() = (int64_t)rc; return -1; }
    return 0;
}

/* 主机名(超出冻结面的增补面,同 alpn_selected_s):host NULL = 显式 opt-out
 * (mbedTLS 3.6 语义:set_hostname(NULL) 留"已调用"标记 —— 链验证仍 REQUIRED,
 * 仅跳过名字匹配;从未调用 + REQUIRED 则握手期
 * MBEDTLS_ERR_SSL_CERTIFICATE_VERIFICATION_WITHOUT_HOSTNAME 响亮拒绝,
 * ssl_tls.c:9899)。冻结 API 无 host 参 → 门面默认 opt-out(见 handshake),
 * 名字匹配面待后继任务随 host 参一起开。 */
int64_t ctron_tls_set_hostname(int64_t h, const char* host) {
    ct_tls_ctx* c = (ct_tls_ctx*)(intptr_t)h;
    if (c == NULL) { *ct_tls_err_slot() = EINVAL; return -1; }
    /* 空串 = NULL opt-out(Ctron Str 无 null 通道;"" 映射到同一语义) */
    if (host != NULL && host[0] == '\0') host = NULL;
    int rc = mbedtls_ssl_set_hostname(&c->ssl, host);
    if (rc != 0) { *ct_tls_err_slot() = (int64_t)rc; return -1; }
    c->hostname_set = 1;
    return 0;
}

int64_t ctron_tls_handshake(int64_t h, int64_t fd) {
    ct_tls_ctx* c = (ct_tls_ctx*)(intptr_t)h;
    if (c == NULL || fd < 0) { *ct_tls_err_slot() = EINVAL; return -1; }
    c->fd = fd;
    /* client 且未显式设名:set_hostname(NULL) 显式 opt-out(语义见
     * ctron_tls_set_hostname 注 —— 链 REQUIRED 验证保留,名字匹配默认关)。
     * server 无此检查面。 */
    if (!c->is_server && !c->hostname_set) {
        (void)mbedtls_ssl_set_hostname(&c->ssl, NULL);
    }
    /* blocking 形 BIO:只给 recv_timeout(契约见文件头);STREAM 下握手中的
     * 每笔读 timeout 即 conf.read_timeout(ssl_msg.c:2311)→ 钉 0 = 阻塞握手 */
    mbedtls_ssl_set_bio(&c->ssl, &c->fd, ct_bio_send, NULL, ct_bio_recv_timeout);
    mbedtls_ssl_conf_read_timeout(&c->conf, 0);
    int ret;
    do {
        ret = mbedtls_ssl_handshake(&c->ssl);
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    if (ret != 0) { *ct_tls_err_slot() = (int64_t)ret; return -1; }
    return 0;
}

/* 读:>0 n / 0 eof(CONN_EOF)/ <0 err(槽 = mbedTLS rc;超时 = SSL_TIMEOUT)。
 * 单调用搬运 ≤ CT_CHUNK(net 口径⑤同款);timeout_ms ≤ 0 = 永久阻塞。 */
int64_t ctron_tls_read(int64_t h, ct_view6 buf, int64_t cap, int64_t timeout_ms) {
    ct_tls_ctx* c = (ct_tls_ctx*)(intptr_t)h;
    if (c == NULL) { *ct_tls_err_slot() = EINVAL; return -1; }
    if (cap > buf.n) cap = buf.n;
    if (cap <= 0) return 0;
    if (cap > (int64_t)CT_CHUNK) cap = (int64_t)CT_CHUNK;
    /* conf 为每连接独享 → 按调用重置读超时安全(见文件头);0 = 无超时 */
    mbedtls_ssl_conf_read_timeout(&c->conf, timeout_ms > 0 ? (uint32_t)timeout_ms : 0u);
    int ret;
    do {
        ret = mbedtls_ssl_read(&c->ssl, c->rd_stage, (size_t)cap);
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    /* EOF 双形(mbedTLS 3.x 实测):CONN_EOF = BIO 层 0(fd FIN,fetch_input
     * 转换,ssl_msg.c:2324);PEER_CLOSE_NOTIFY = 对端 TLS close_notify 记录
     * (记录层语义,不复用 CONN_EOF 常量)—— 两者同映 0(eof 口径) */
    if (ret == MBEDTLS_ERR_SSL_CONN_EOF || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
    if (ret < 0) { *ct_tls_err_slot() = (int64_t)ret; return -1; }
    for (int i = 0; i < ret; i++) buf.d[i] = (int64_t)c->rd_stage[i];
    return (int64_t)ret;
}

/* 写:≥0 n(send-all 语义;mbedtls_ssl_write 经 flush_output 分块发完)/ <0 err */
int64_t ctron_tls_write(int64_t h, const char* buf, int64_t n) {
    ct_tls_ctx* c = (ct_tls_ctx*)(intptr_t)h;
    if (c == NULL || (buf == NULL && n > 0)) { *ct_tls_err_slot() = EINVAL; return -1; }
    if (n <= 0) return 0;
    int ret;
    do {
        ret = mbedtls_ssl_write(&c->ssl, (const unsigned char*)buf, (size_t)n);
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    if (ret < 0) { *ct_tls_err_slot() = (int64_t)ret; return -1; }
    return (int64_t)ret;
}

/* 选定协议(握手后);无 ALPN/未协商 → ""。指针指回 ctx 自有 alpn 表(文件头),
 * 调用方 str_from_c 深拷即用。门面专用形(冻结 out/cap 形见 ctron_tls_alpn_selected
 * 之外的 C 消费面注记 —— 实现于下,双形并存)。 */
const char* ctron_tls_alpn_selected_s(int64_t h) {
    ct_tls_ctx* c = (ct_tls_ctx*)(intptr_t)h;
    if (c == NULL) return "";
    const char* p = mbedtls_ssl_get_alpn_protocol(&c->ssl);
    return p != NULL ? p : "";
}

/* 冻结形(任务书):拷入调用方缓冲;返回 0 = 有,−1 = 无/参数坏(不置槽) */
int64_t ctron_tls_alpn_selected(int64_t h, char* out, int64_t cap) {
    ct_tls_ctx* c = (ct_tls_ctx*)(intptr_t)h;
    if (c == NULL || out == NULL || cap <= 0) return -1;
    const char* p = mbedtls_ssl_get_alpn_protocol(&c->ssl);
    if (p == NULL) return -1;
    int64_t n = (int64_t)strlen(p);
    if (n >= cap) n = cap - 1;                     /* 截断守卫 */
    memcpy(out, p, (size_t)n);
    out[n] = '\0';
    return 0;
}

/* 释放次序(任务自查项):close_notify 尽力发(fd 可能已关,吞错)→ ssl_free
 * → conf_free(conf 仍被 ssl 引用,先 ssl 后 conf)→ alpn 深拷 → 证书/密钥/
 * drbg/entropy → 暂存 → ctx。【不关 fd】—— fd 归 net 层。 */
void ctron_tls_close(int64_t h) {
    ct_tls_ctx* c = (ct_tls_ctx*)(intptr_t)h;
    if (c == NULL) return;
    (void)mbedtls_ssl_close_notify(&c->ssl);       /* 尽力:fd 已关/未握手均吞 */
    mbedtls_ssl_free(&c->ssl);
    mbedtls_ssl_config_free(&c->conf);
    if (c->alpn_strs != NULL) {
        for (int i = 0; i < c->alpn_n; i++) free(c->alpn_strs[i]);
        free(c->alpn_strs);
        free(c->alpn_list);
    }
    mbedtls_x509_crt_free(&c->own);
    mbedtls_pk_free(&c->pkey);
    mbedtls_x509_crt_free(&c->ca);
    mbedtls_ctr_drbg_free(&c->drbg);
    mbedtls_entropy_free(&c->entropy);
    free(c->rd_stage);
    free(c);
}
