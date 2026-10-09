// ctron_crypto —— 原生密码学垫片(lib/crypto;OpenSSL 3)
// 口径:纯计算面——密钥材料以 hex 串/字节视图进出,文件落盘(0600/O_EXCL 纪律)归消费方;
//       密钥生成不触 fs,签验/HMAC/SHA 为无副作用函数。
// ABI 纪律(net/c_src/ctron_net.c 同款):ctron_view_w8u 必须与发射器模板逐字段一致
// ({ uint8_t* d; int64_t n; }),数组实参按值传结构体;Str 实参 = GC NUL 结尾串,
// 长度一律显式传参,不经 strlen;视图 n 是声明容量,逻辑长度由调用方显式传。
// rc 口径:散列族 void(32B 恒出);ed25519 keypair/sign/verify 与 rand 沿 loom_sig.c
// 原语义(1 成 / 0 无效或败 / -1 失败;sign 返写入 hex 字节数 128 / -1 败)。
// 来源:port 自 loom c_src/loom_sha.c + loom_sig.c + loom_raw.c keygen 的 RAND 面
// (loom C 垫片内化战役 CW-F1a,2026-10-09;门 = tests/crypto e2e + loom 40 checks)。
#include <stdint.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

typedef struct { uint8_t* d; int64_t n; } ctron_view_w8u;

// ---- SHA-256 / HMAC-SHA-256(libcrypto;G1 杠杆,纯 Ctron 实现受位运算/宽整型语义债卡) ----

void ctron_crypto_sha256(const char* in, int64_t n, ctron_view_w8u out) {
    SHA256((const uint8_t*)(n > 0 ? in : ""), (size_t)(n > 0 ? n : 0), out.d);
}

void ctron_crypto_sha256_raw(ctron_view_w8u data, int64_t n, ctron_view_w8u out) {
    SHA256(n > 0 ? data.d : (const uint8_t*)"", (size_t)(n > 0 ? n : 0), out.d);
}

void ctron_crypto_hmac_sha256(const char* key, int64_t klen, const char* data, int64_t n, ctron_view_w8u out) {
    unsigned int outlen = 0;
    HMAC(EVP_sha256(), key, (int)klen, (const uint8_t*)data, (size_t)(n > 0 ? n : 0), out.d, &outlen);
}

// ---- hex 助手(loom_sig.c 原样 port;静态私有,不跨 ABI) ----

static int hexval(int c) {
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

static int hexdec(const char* hx, uint8_t* out, int cap) {
    int n = 0;
    while (hx[n * 2] != 0) {
        if (n >= cap) { return -1; }
        int hi = hexval((unsigned char)hx[n * 2]);
        int lo = hexval((unsigned char)hx[n * 2 + 1]);
        if (hi < 0 || lo < 0) { return -1; }
        out[n] = (uint8_t)(hi * 16 + lo);
        n++;
    }
    return n;
}

static void bytes2hex(const uint8_t* in, int n, char* out) {
    static const char* hx = "0123456789abcdef";
    for (int i = 0; i < n; i++) {
        out[i * 2] = hx[in[i] >> 4];
        out[i * 2 + 1] = hx[in[i] & 0x0f];
    }
}

// ---- ed25519(OpenSSL 3 EVP;R2 受信证据面) ----

// 密钥对生成(纯计算):priv/pub 各 32B → 视图各写 64 ASCII hex 字节(无尾随换行;
// 文件格式化归消费方)。容量不足(任一 < 64)= 参数错,返 0。
int32_t ctron_crypto_ed25519_keypair_hex(ctron_view_w8u priv_hex, ctron_view_w8u pub_hex) {
    if (priv_hex.d == NULL || priv_hex.n < 64 || pub_hex.d == NULL || pub_hex.n < 64) { return 0; }
    EVP_PKEY* pk = EVP_PKEY_Q_keygen(NULL, NULL, "ED25519");
    if (!pk) { return 0; }
    uint8_t priv[32], pub[32];
    size_t plen = 32, ulen = 32;
    if (EVP_PKEY_get_raw_private_key(pk, priv, &plen) != 1 ||
        EVP_PKEY_get_raw_public_key(pk, pub, &ulen) != 1) {
        EVP_PKEY_free(pk);
        return 0;
    }
    EVP_PKEY_free(pk);
    bytes2hex(priv, 32, (char*)priv_hex.d);
    bytes2hex(pub, 32, (char*)pub_hex.d);
    return 1;
}

// 签名:privhex(64 hex)+ msg(n 字节)→ 128 ASCII hex 写入 sighbuf;返回 128(-1 失败)
int64_t ctron_crypto_ed25519_sign_hex(const char* privhex, const char* msg, int64_t n, ctron_view_w8u sighbuf) {
    uint8_t priv[32];
    if (hexdec(privhex, priv, 32) != 32) { return -1; }
    EVP_PKEY* pk = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, priv, 32);
    if (!pk) { return -1; }
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    uint8_t sig[64];
    size_t siglen = sizeof sig;
    int ok = 0;
    if (ctx) {
        if (EVP_DigestSignInit(ctx, NULL, NULL, NULL, pk) == 1) {
            if (EVP_DigestSign(ctx, sig, &siglen, (const uint8_t*)msg, (size_t)(n > 0 ? n : 0)) == 1) {
                ok = 1;
            }
        }
        EVP_MD_CTX_free(ctx);
    }
    EVP_PKEY_free(pk);
    if (!ok || siglen != 64) { return -1; }
    if (sighbuf.d == NULL || sighbuf.n < 128) { return -1; }
    bytes2hex(sig, 64, (char*)sighbuf.d);
    return 128;
}

// 验签:pubhex(64 hex)+ msg(n 字节)+ sighex(128 hex)→ 1 有效 / 0 无效(-1 失败)
int32_t ctron_crypto_ed25519_verify_hex(const char* pubhex, const char* msg, int64_t n, const char* sighex) {
    uint8_t pub[32], sig[64];
    if (hexdec(pubhex, pub, 32) != 32) { return -1; }
    if (hexdec(sighex, sig, 64) != 64) { return -1; }
    EVP_PKEY* pk = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pub, 32);
    if (!pk) { return -1; }
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    int r = -1;
    if (ctx) {
        if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pk) == 1) {
            r = EVP_DigestVerify(ctx, sig, sizeof sig, (const uint8_t*)msg, (size_t)(n > 0 ? n : 0));
        }
        EVP_MD_CTX_free(ctx);
    }
    EVP_PKEY_free(pk);
    return r;
}

// ---- 真实熵(RAND_bytes;std/rand 是确定性 PRNG 无熵源——宪章 8,真实熵归原生面) ----

// 填充 out 视图 n 字节;1 成 / 0 败
int32_t ctron_crypto_rand_bytes(ctron_view_w8u out) {
    if (out.d == NULL || out.n <= 0) { return 0; }
    return RAND_bytes(out.d, (int)out.n) == 1 ? 1 : 0;
}
