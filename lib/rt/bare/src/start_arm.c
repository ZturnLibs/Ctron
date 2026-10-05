/* start_arm.c —— cortex-M 启动面(T40 §9.3:零 OS 依赖运行时)。
 * 向量表(初始 SP=静态栈顶 + Reset_Handler)+ 数据搬运/bss 清零。
 * 静态栈与 guard 页布局见 link/cortex-m4.ld(RAM: [bss][guard][stack↔顶])。 */
extern unsigned long _estack;
extern unsigned long _sidata, _sdata, _edata, _sbss, _ebss;

int main(int argc, char** argv);
void exit(int code) __attribute__((noreturn));

void Default_Handler(void) {
    for (;;) { __asm__ volatile("" ::: "memory"); }
}

void Reset_Handler(void) {
    unsigned long* src = &_sidata;
    unsigned long* dst = &_sdata;
    while (dst < &_edata) { *dst++ = *src++; }
    dst = &_sbss;
    while (dst < &_ebss) { *dst++ = 0; }
    /* argc/argv 显式清零:bare 无宿主传参面,残渣≥3 会闯 argv 扫描(HardFault 实证) */
    (void)main(0, 0);
    exit(0);
}

__attribute__((section(".isr_vector"), used))
void (*const ct_vectors[])(void) = {
    (void (*)(void))(&_estack), /* 0: 初始 MSP = 静态栈顶 */
    Reset_Handler,              /* 1: Reset */
    Default_Handler,            /* 2: NMI */
    Default_Handler,            /* 3: HardFault */
    Default_Handler,            /* 4: MemManage */
    Default_Handler,            /* 5: BusFault */
    Default_Handler,            /* 6: UsageFault */
    0, 0, 0, 0,                 /* 7-10: 保留 */
    Default_Handler,            /* 11: SVCall */
    Default_Handler,            /* 12: DebugMon */
    0,                          /* 13: 保留 */
    Default_Handler,            /* 14: PendSV */
    Default_Handler,            /* 15: SysTick */
    /* 16..127: 外设 IRQ 缺省(超出本表的中断向量由靶机手册对齐;ISR 面归 T42) */
    [16 ... 127] = Default_Handler,
};
