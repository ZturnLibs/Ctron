/* ct_unix_path.c —— unix_sock 夹具参数注入垫:AF_UNIX socket 路径生成
 * (std 无 getenv/mktemp 面;tls_smoke ct_smoke_env.c 同款垫法)。
 * 路径 = TMPDIR(getenv,空缺省 /tmp)+ pid 组合:进程唯一(并发 run.sh 互
 * 不踩),陈旧残留由垫片 unlink-before-bind 自愈。组合后 >= 104(sun_path
 * 上限 darwin 104 / linux 108 取 min,含 NUL)返回 ""(夹具 panic 响亮失败,
 * 不静默截断)。返回 C 静态串,Ctron 侧 Str 收取(bind.ct 头注深拷语义)。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char* ctron_unix_sockpath(void) {
    static char path[256];
    const char* tmp = getenv("TMPDIR");
    if (tmp == NULL || *tmp == '\0') tmp = "/tmp";
    snprintf(path, sizeof path, "%s/ctron_us_%ld.sock", tmp, (long)getpid());
    if (strlen(path) >= 104) return "";
    return path;
}
