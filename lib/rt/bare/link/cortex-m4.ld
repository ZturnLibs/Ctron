/* cortex-m4.ld —— thumbv7em-none-eabi 链接脚本(T40 §9.3)。
 * 布局:FLASH 驻代码/rodata/data 镜像;RAM = [data][bss][guard 1K][stack 8K→顶]。
 * guard 页 = 静态栈下溢缓冲带(链接期 ASSERT 兜底;运行期硬隔离 = MPU 升级路径,
 * 列 ISR 面 T42 后的志向)。 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

STACK_SIZE = 8K;
GUARD_SIZE = 1K;
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text.start)
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH
    _sidata = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    .guard (NOLOAD) :
    {
        . = ALIGN(4);
        _eguard = .;
        . += GUARD_SIZE;
    } > RAM
}

ASSERT(_eguard >= _ebss, "ctron bare: RAM 溢出 —— data/bss 侵入 guard 页(减静态或降消耗)")
ASSERT(_eguard + GUARD_SIZE <= _estack - STACK_SIZE, "ctron bare: guard/栈区间重叠")
