// ct_wasm_run.js —— wasm32 产物 node 胶水运行器(T37)
// 用法: node ct_wasm_run.js <app.wasm> [args...]
// import 面(lib/rt/wasm32/src/wasm_libc.c 契约):
//   env.ct_print(ptr, len) —— stdout 单通道(字节直写,UTF-8 原样)
//   env.ct_exit(code)      —— exit 面(panic=1)
// export 面: main(argc=0, argv=0) → 进程退出码;memory(线性内存,--export-memory)
'use strict';
const fs = require('fs');

const wasmPath = process.argv[2];
if (!wasmPath) {
    console.error('用法: node ct_wasm_run.js <app.wasm>');
    process.exit(2);
}

(async () => {
    const bytes = fs.readFileSync(wasmPath);
    const mod = await WebAssembly.compile(bytes);
    let inst = null;
    const imports = { env: {
        ct_print: (ptr, len) => {
            // 每次现取 memory:memory.grow 后旧 buffer 会 detach
            const mem = inst.exports.memory;
            process.stdout.write(Buffer.from(new Uint8Array(mem.buffer, ptr, len)));
        },
        ct_exit: (code) => process.exit(code),
    }};
    inst = await WebAssembly.instantiate(mod, imports);
    const code = inst.exports.main(0, 0);
    process.exit(code);
})().catch((e) => {
    console.error('ct_wasm_run:', e && e.message ? e.message : e);
    process.exit(1);
});
