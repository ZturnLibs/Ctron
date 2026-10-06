/* minilibc pthread.h —— 线程族仅声明。bare 档单线程(sem_spawn 档门拦截);
 * 聚合类型须可接受 PTHREAD_MUTEX_INITIALIZER 的 {0} 平铺初始化。 */
#ifndef CT_MLC_PTHREAD_H
#define CT_MLC_PTHREAD_H

typedef unsigned long pthread_t;
typedef struct { long long x[8]; } pthread_mutex_t;
typedef struct { long long x[8]; } pthread_cond_t;
typedef struct { long long x[8]; } pthread_attr_t;
typedef struct { long long x[8]; } pthread_mutexattr_t;

#define PTHREAD_MUTEX_INITIALIZER {0}
#define PTHREAD_COND_INITIALIZER {0}

int pthread_create(pthread_t* t, const pthread_attr_t* a, void* (*fn)(void*), void* arg);
int pthread_join(pthread_t t, void** ret);
pthread_t pthread_self(void);
int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* a);
int pthread_mutex_lock(pthread_mutex_t* m);
int pthread_mutex_unlock(pthread_mutex_t* m);
int pthread_cond_init(pthread_cond_t* c, const void* a);
int pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m);
int pthread_cond_broadcast(pthread_cond_t* c);

#endif
