// ctron_tls.c —— mbedTLS 3.x 包装(TLS 门面 P3-C;M3C2 批2 补全 2026-10-09)
// ABI:bind.ct extern 面;句柄 = 堆分配 TlsCtx*(I64 直存,Ctron 不解引用)
// 缓冲:var U8[] 视图(ctron_view_w8u,net CW1a 同型——bind.ct 旧 &I64[] 注记同步修)
// BIO:mbedTLS 回调 recv/send 桥接 socket fd(net 层 accept 的裸 fd)
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/error.h>

typedef struct { uint8_t* d; int64_t n; } ctron_view_w8u;

// TLS 上下文(句柄表.slot = 此结构指针)
typedef struct {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config cfg;
    mbedtls_x509_crt cert;     // 服务端证书链(或客户端自身证书)
    mbedtls_pk_context key;    // 服务端私钥
    mbedtls_x509_crt ca;       // 客户端:CA 包;服务端:可选客户端 CA
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    int is_server;
    int handshake_done;
    int64_t last_errno;        // 最近系统 errno(供 ctron_tls_last_errno)
    char errbuf[256];          // strerror 缓冲
} TlsCtx;

static __thread TlsCtx* g_last_ctx = NULL;  // strerror 上下文

// ---- BIO 桥:socket fd ↔ mbedTLS ----

static int tls_bio_send(void* ctx, const unsigned char* buf, size_t len) {
    int64_t fd = *(int64_t*)ctx;
    ssize_t w = send((int)fd, buf, len, 0);
    if (w < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_WRITE;
        if (errno == EINTR) return MBEDTLS_ERR_SSL_WANT_WRITE;
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    return (int)w;
}

static int tls_bio_recv(void* ctx, unsigned char* buf, size_t len) {
    int64_t fd = *(int64_t*)ctx;
    ssize_t r = recv((int)fd, buf, len, 0);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_READ;
        if (errno == EINTR) return MBEDTLS_ERR_SSL_WANT_READ;
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    if (r == 0) return MBEDTLS_ERR_SSL_CONN_EOF;  // 对端 close
    return (int)r;
}

// ---- extern 面(bind.ct 逐函数) ----

int64_t ctron_tls_ctx_new(int64_t is_server) {
    TlsCtx* c = calloc(1, sizeof(TlsCtx));
    if (!c) return 0;
    c->is_server = (int)is_server;
    mbedtls_ssl_init(&c->ssl);
    mbedtls_ssl_config_init(&c->cfg);
    mbedtls_x509_crt_init(&c->cert);
    mbedtls_pk_init(&c->key);
    mbedtls_x509_crt_init(&c->ca);
    mbedtls_entropy_init(&c->entropy);
    mbedtls_ctr_drbg_init(&c->ctr_drbg);
    const char* pers = "ctron-tls";
    if (mbedtls_ctr_drbg_seed(&c->ctr_drbg, mbedtls_entropy_func, &c->entropy,
                              (const unsigned char*)pers, strlen(pers)) != 0) {
        free(c);
        return 0;
    }
    int endpoint = is_server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT;
    int transport = MBEDTLS_SSL_TRANSPORT_STREAM;
    int preset = MBEDTLS_SSL_PRESET_DEFAULT;
    if (mbedtls_ssl_config_defaults(&c->cfg, endpoint, transport, preset) != 0) {
        free(c);
        return 0;
    }
    mbedtls_ssl_conf_rng(&c->cfg, mbedtls_ctr_drbg_random, &c->ctr_drbg);
    mbedtls_ssl_conf_authmode(&c->cfg, MBEDTLS_SSL_VERIFY_NONE);  // v0:不验证(列后续)
    mbedtls_ssl_set_bio(&c->ssl, NULL, NULL, NULL, NULL);  // fd 在 handshake 时设
    g_last_ctx = c;
    return (int64_t)c;
}

int64_t ctron_tls_use_cert(int64_t h, const char* cert_pem, const char* key_pem) {
    TlsCtx* c = (TlsCtx*)h;
    if (!c || !cert_pem || !key_pem) return -1;
    if (mbedtls_x509_crt_parse(&c->cert, (const unsigned char*)cert_pem,
                               strlen(cert_pem) + 1) != 0) return -2;
    if (mbedtls_pk_parse_key(&c->key, (const unsigned char*)key_pem,
                             strlen(key_pem) + 1, NULL, 0,
                             mbedtls_ctr_drbg_random, &c->ctr_drbg) != 0) return -3;
    mbedtls_ssl_conf_own_cert(&c->cfg, &c->cert, &c->key);
    return 0;
}

int64_t ctron_tls_use_ca_bundle(int64_t h, const char* ca_path) {
    TlsCtx* c = (TlsCtx*)h;
    if (!c || !ca_path) return -1;
    if (mbedtls_x509_crt_parse_file(&c->ca, ca_path) != 0) return -2;
    mbedtls_ssl_conf_ca_chain(&c->cfg, &c->ca, NULL);
    return 0;
}

int64_t ctron_tls_set_alpn(int64_t h, const char* protos) {
    TlsCtx* c = (TlsCtx*)h;
    if (!c || !protos) return -1;
    // 协议串逗号分隔:"h2,http/1.1" → mbedtls 需要长度前缀列表
    // v0:单协议直设
    const char* alpn_list[2] = { protos, NULL };
    if (mbedtls_ssl_conf_alpn_protocols(&c->cfg, alpn_list) != 0) return -2;
    return 0;
}

int64_t ctron_tls_set_hostname(int64_t h, const char* host) {
    TlsCtx* c = (TlsCtx*)h;
    if (!c || !host) return -1;
    if (mbedtls_ssl_set_hostname(&c->ssl, host) != 0) return -2;
    return 0;
}

// fd_ctx 存 fd 让 BIO 回调取(句柄内嵌,生命周期同 TlsCtx)
static int64_t g_bio_fd = -1;  // v0 单连接(服务端骨架同口径)

int64_t ctron_tls_handshake(int64_t h, int64_t fd) {
    TlsCtx* c = (TlsCtx*)h;
    if (!c || fd < 0) return -1;
    // 设 SSL 上下文配置
    if (mbedtls_ssl_setup(&c->ssl, &c->cfg) != 0) return -2;
    // BIO 桥
    g_bio_fd = fd;
    mbedtls_ssl_set_bio(&c->ssl, &g_bio_fd, tls_bio_send, tls_bio_recv, NULL);
    // 握手(阻塞;mbedTLS 内部循环)
    int rc;
    while ((rc = mbedtls_ssl_handshake(&c->ssl)) != 0) {
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        c->last_errno = rc;
        return -3;
    }
    c->handshake_done = 1;
    return 0;
}

// 读(解密后明文 → U8 视图)
int64_t ctron_tls_read(int64_t h, ctron_view_w8u buf) {
    TlsCtx* c = (TlsCtx*)h;
    if (!c || !c->handshake_done) return -1;
    if (!buf.d || buf.n <= 0) return -1;
    int rc = mbedtls_ssl_read(&c->ssl, buf.d, (size_t)buf.n);
    if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;  // EOF
    if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) return -2;
    if (rc < 0) {
        c->last_errno = rc;
        return -1;
    }
    return rc;
}

// 写(明文 → 加密发送;Str = NUL 终结 char*)
int64_t ctron_tls_write(int64_t h, const char* data, int64_t n) {
    TlsCtx* c = (TlsCtx*)h;
    if (!c || !c->handshake_done) return -1;
    if (!data || n <= 0) return -1;
    int rc = mbedtls_ssl_write(&c->ssl, (const unsigned char*)data, (size_t)n);
    if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) return -2;
    if (rc < 0) {
        c->last_errno = rc;
        return -1;
    }
    return rc;
}

const char* ctron_tls_strerror(int64_t e) {
    TlsCtx* c = g_last_ctx;
    if (c && e < 0) {
        mbedtls_strerror((int)e, c->errbuf, sizeof(c->errbuf));
        return c->errbuf;
    }
    if (e == 0) return "ok";
    return "unknown";
}

int64_t ctron_tls_last_errno(void) {
    TlsCtx* c = g_last_ctx;
    return c ? c->last_errno : 0;
}

int64_t ctron_tls_close(int64_t h) {
    TlsCtx* c = (TlsCtx*)h;
    if (!c) return 0;
    if (c->handshake_done) {
        mbedtls_ssl_close_notify(&c->ssl);
    }
    mbedtls_ssl_free(&c->ssl);
    mbedtls_ssl_config_free(&c->cfg);
    mbedtls_x509_crt_free(&c->cert);
    mbedtls_pk_free(&c->key);
    mbedtls_x509_crt_free(&c->ca);
    mbedtls_ctr_drbg_free(&c->ctr_drbg);
    mbedtls_entropy_free(&c->entropy);
    free(c);
    if (g_last_ctx == c) g_last_ctx = NULL;
    return 0;
}
