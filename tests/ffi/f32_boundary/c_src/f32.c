/* tests/ffi/f32_boundary —— C 实现:F32 边界保真(§9.6 v0.9 精度约定) */
#include <stdint.h>

float f_add(float a, float b) { return a + b; }
float f_half(float x) { return x / 2.0f; }

/* 边界保真证人:F32 形参按 binary32 舍入(0.1 → 0.100000001490116...),
   加宽回 double 后 != 0.1(double)。若边界误为 double 通道(旧折叠口径),
   0.1 全程双精度,加宽判等会反转。 */
double f_widen(float x) { return (double)x; }
