/* start_riscv.c —— rv32imac 启动面(qemu -M virt -bios none:0x80000000 直启)。
 * _start 置 sp(静态栈顶)+ 挂 machine trap 向量 + .bss 清零 + 调 main。
 * 静态栈与 guard 页布局见 link/rv32.ld。 */
extern unsigned long _estack;
extern unsigned long _sbss, _ebss;

int main(int argc, char** argv);
void exit(int code) __attribute__((noreturn));
void _ct_trap(void);

/* C 世界入口:sp 已就位后才有资格带 prologue(mtvec 挂 trap 向量 + 调 main) */
void _ct_start_c(void) __attribute__((noreturn, used));
void _ct_start_c(void) {
    /* .bss 清零(同 start_arm:裸机无 crt0,qemu ELF 装载的清零面不承承诺;
     * T41 实证:跨文件 .bss 邻接布局下 GC 池游标含残值 → 首块分配即败 */
    unsigned long* dst = &_sbss;
    while (dst < &_ebss) { *dst++ = 0; }
    __asm__ volatile("csrw mtvec, %0" : : "r"(_ct_trap));
    /* argc/argv 显式清零(同 start_arm:残渣闯 argv 扫描面) */
    (void)main(0, 0);
    exit(0);
}

/* naked 入口:RISC-V 复位 sp 残值(0)——任何 prologue push 都会 store fault
 * (实证:tval=0xfffffffc@sw ra;ARM 面硬件从向量表装 SP 无此坑) */
void _start(void) __attribute__((naked, noreturn, section(".text.start"), used));
void _start(void) {
    __asm__ volatile(".option push\n"
                     ".option norelax\n"
                     "la sp, _estack\n"
                     ".option pop\n"
                     "call _ct_start_c");
}

/* mtvec 低 2 位 = mode 位:靶址须 4 字节对齐,否则 csrw 本身非法指令
 * (实证:非对齐 → 陷阱环 epc=0 fetch fault;ARM 面无此约束) */
__attribute__((aligned(4), used))
void _ct_trap(void) {
    for (;;) { __asm__ volatile("wfi"); }
}
