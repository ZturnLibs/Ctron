// Ctron 扩展打包脚本(零 npm 依赖,ESM;替代原 Makefile)。
// 用法:
//   npm run package          或   node scripts/package.mjs                  # 同步 LSP 源 + 打包到 dist/
//   npm run sync             或   node scripts/package.mjs sync             # 仅同步 server/main.ct
//   npm run clean            或   node scripts/package.mjs clean            # 清 dist/*.vsix
//   npm run install:vsix     或   node scripts/package.mjs install [版本]    # 安装进 VSCode;缺省装最新
//     指定版本:npm run install:vsix -- 0.4.0(可简写 0.4;精确匹配 → 前缀匹配)
// vsce / code CLI 经 npx / PATH 调用,不进 dependencies。
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const LSP_SRC = path.join(ROOT, '..', '..', 'lsp', 'src', 'main.ct');
const SERVER_COPY = path.join(ROOT, 'server', 'main.ct');
const DIST = path.join(ROOT, 'dist');

function version() {
    return JSON.parse(fs.readFileSync(path.join(ROOT, 'package.json'), 'utf8')).version;
}

function sync() {
    if (!fs.existsSync(LSP_SRC)) {
        console.error(`sync: 找不到 LSP 源 ${LSP_SRC}`);
        process.exit(1);
    }
    fs.copyFileSync(LSP_SRC, SERVER_COPY);
    const same = fs.readFileSync(LSP_SRC).equals(fs.readFileSync(SERVER_COPY));
    console.log(same ? 'sync: server/main.ct 与 lsp 源一致' : 'sync: server/main.ct 已更新');
}

function clean() {
    if (!fs.existsSync(DIST)) { return; }
    for (const f of fs.readdirSync(DIST)) {
        if (f.endsWith('.vsix')) {
            fs.unlinkSync(path.join(DIST, f));
            console.log(`clean: 移除 dist/${f}`);
        }
    }
}

function pack() {
    sync();
    fs.mkdirSync(DIST, { recursive: true });
    const out = `dist/ctron-lang-${version()}.vsix`;
    const npx = process.platform === 'win32' ? 'npx.cmd' : 'npx';
    const r = spawnSync(npx, ['--yes', '@vscode/vsce', 'package', '--allow-missing-repository', '-o', out], {
        cwd: ROOT, stdio: 'inherit'
    });
    if (r.status !== 0) {
        console.error(`package: vsce 失败(退出码 ${r.status});需要 Node/npm 环境与网络(首次拉取 @vscode/vsce)`);
        process.exit(r.status || 1);
    }
    const stat = fs.statSync(path.join(ROOT, out));
    console.log(`package: ${out} (${(stat.size / 1024).toFixed(2)} KB)`);
}

// ---------- install:把 dist 里的 vsix 装进 VSCode ----------

function semverKey(v) {
    const m = /^(\d+)\.(\d+)\.(\d+)/.exec(v);
    return m ? [Number(m[1]), Number(m[2]), Number(m[3])] : [-1, -1, -1];
}

function cmpKey(a, b) {
    for (let i = 0; i < 3; i++) { if (a[i] !== b[i]) { return a[i] - b[i]; } }
    return 0;
}

function availableVersions() {
    if (!fs.existsSync(DIST)) { return []; }
    return fs.readdirSync(DIST)
        .filter((f) => /^ctron-lang-.+\.vsix$/.test(f))
        .map((f) => /^ctron-lang-(.+)\.vsix$/.exec(f)[1])
        .sort((a, b) => cmpKey(semverKey(b), semverKey(a)));
}

function pickVersion(requested) {
    const all = availableVersions();
    if (!all.length) {
        console.error('install: dist/ 下没有 .vsix;先运行 npm run package');
        process.exit(1);
    }
    if (!requested) { return all[0]; }
    if (all.includes(requested)) { return requested; }
    const prefixed = all.filter((v) => v.startsWith(requested));
    if (prefixed.length) { return prefixed[0]; }
    console.error(`install: 没有版本 ${requested};可用:${all.join(', ')}`);
    process.exit(1);
}

function install(requested) {
    const ver = pickVersion(requested);
    const file = `dist/ctron-lang-${ver}.vsix`;
    const code = process.platform === 'win32' ? 'code.cmd' : 'code';
    console.log(`install: code --install-extension ${file}(版本 ${ver}${requested ? '' : ',最新'})`);
    const r = spawnSync(code, ['--install-extension', file], { cwd: ROOT, stdio: 'inherit' });
    if (r.status !== 0) {
        console.error('install: code CLI 调用失败。VSCode 命令行需在 PATH 中'
            + '(macOS:VSCode 命令面板执行 "Shell Command: Install \'code\' command in PATH");'
            + '也可手动:code --install-extension ' + file);
        process.exit(r.status || 1);
    }
    console.log(`install: ctron-lang ${ver} 安装完成`);
}

const cmd = process.argv[2] || 'package';
if (cmd === 'package') { pack(); }
else if (cmd === 'sync') { sync(); }
else if (cmd === 'clean') { clean(); }
else if (cmd === 'install') { install(process.argv[3]); }
else {
    console.error('用法:node scripts/package.mjs [package|sync|clean|install [版本]](缺省 package)');
    process.exit(1);
}
