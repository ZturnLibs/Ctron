/* ct_smoke_env.c —— tls_smoke 夹具参数注入垫:run.sh 生成自签证书后把路径
 * 经环境变量传入(std 无 getenv 面)。返回 C 静态串,Ctron 侧 Str 收取
 * (发射面 Str 形参/返回 = char*,深拷语义见 net/bind.ct 头注);缺值返 ""。 */
#include <stdlib.h>

const char* ctron_smoke_env(const char* k) {
    if (k == NULL) return "";
    const char* v = getenv(k);
    return v != NULL ? v : "";
}
