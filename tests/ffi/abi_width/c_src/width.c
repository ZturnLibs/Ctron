/* tests/ffi/abi_width —— C 侧:逐宽度原样回环(声明宽度即真)。 */
#include <stdint.h>

int8_t w_i8(int8_t v) { return v; }
uint8_t w_u8(uint8_t v) { return v; }
int16_t w_i16(int16_t v) { return v; }
uint16_t w_u16(uint16_t v) { return v; }
int64_t w_i64(int64_t v) { return v; }
uint64_t w_u64(uint64_t v) { return v; }
double w_f64(double v) { return v; }
int w_bool(int v) { return v; }
const char* w_str(const char* s) { return s; }
