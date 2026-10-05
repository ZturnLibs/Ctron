/* minilibc setjmp.h —— bare 档单线程无任务面:_ct_setjmp 恒 0(首次返回)、
 * longjmp = 响亮 exit。任务/panic-longjmp 全貌列 full 档(bare panic 走 exit(1))。 */
#ifndef CT_MLC_SETJMP_H
#define CT_MLC_SETJMP_H

typedef long jmp_buf[32];

int _ct_setjmp(jmp_buf env);
void _ct_longjmp(jmp_buf env, int v) __attribute__((noreturn));

#define setjmp(env) _ct_setjmp(env)
#define _setjmp(env) _ct_setjmp(env)
#define longjmp(env, v) _ct_longjmp((env), (v))

#endif
