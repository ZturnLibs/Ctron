// ct_wasm_run.js —— wasm32 产物 node 胶水运行器(T37;T39 扩 dom 桥 shim)
// 用法: node ct_wasm_run.js <app.wasm> [args...]
// import 面(lib/rt/wasm32/src/wasm_libc.c + 发射 dom 块契约,§9.2):
//   env.ct_print(ptr, len)       —— stdout 单通道(字节直写,UTF-8 原样)
//   env.ct_exit(code)            —— exit 面(panic=1)
//   env.ct_dom_set_title(ptr,len)→i32 —— T39 桥:node=host shim 存值;0=ok/1=异常边界捕获
//   env.ct_dom_set_body(ptr,len)→i32  —— 同上
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
    // T39 dom 桥 host shim:node 无真 DOM,以同构语义承接(浏览器页=真 document)
    const hostDom = { title: '', body: '' };
    const readStr = (ptr, len) => {
        // 每次现取 memory:memory.grow 后旧 buffer 会 detach
        return Buffer.from(new Uint8Array(inst.exports.memory.buffer, ptr, len)).toString('utf8');
    };
    const imports = { env: {
        ct_print: (ptr, len) => {
            process.stdout.write(Buffer.from(new Uint8Array(inst.exports.memory.buffer, ptr, len)));
        },
        ct_exit: (code) => process.exit(code),
        ct_dom_set_title: (ptr, len) => {
            try { hostDom.title = readStr(ptr, len); return 0; } catch (e) { return 1; }
        },
        ct_dom_set_body: (ptr, len) => {
            try { hostDom.body = readStr(ptr, len); return 0; } catch (e) { return 1; }
        },
    }};
    inst = await WebAssembly.instantiate(mod, imports);
    const code = inst.exports.main(0, 0);
    if (hostDom.title) { console.log('dom-title: ' + hostDom.title); }
    if (hostDom.body) { console.log('dom-body: ' + hostDom.body); }
    process.exit(code);
})().catch((e) => {
    console.error('ct_wasm_run:', e && e.message ? e.message : e);
    process.exit(1);
});
