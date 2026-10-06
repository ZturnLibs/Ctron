/* rv32.ld —— riscv32imac-unknown-none 链接脚本(qemu -M virt:RAM 128M @ 0x80000000)。
 * 布局:单 RAM 区 = [text/rodata][data][bss][guard 1K][stack 8K→顶];
 * data 镜像驻 RAM(_sidata=LOADADDR,搬运为空转),物理写入面一次性驻留。 */
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

STACK_SIZE = 8K;
GUARD_SIZE = 1K;
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.text.start))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > RAM

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM
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
