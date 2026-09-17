// CTCL(.ctcl / Ctron.lock)补全与悬浮提示 —— 扩展侧本地注册表,零依赖。
// 数据源:docs/superpowers/specs/2026-09-16-config-language-v1.md
//   §5 注册表一(包清单)、§3 键控块、附录 D.3(锁文件,示意)、§8 能力注册表;
//   能力集与 compiler-rust/src/sem.rs(cap_key_by_def)及自举线 parse_pkg.ct 一致。
// 上下文识别:CTCL 深度恒 1,行扫描块头/块尾即可(注释行跳过)。
'use strict';

const vscode = require('vscode');

// ---------- 注册表 ----------

const CAPABILITIES = ['fs', 'time', 'net', 'log'];

const BLOCKS = {
    pkg: {
        keyed: false,
        doc: '包清单根块(必填,≤1)。重复 = E5045;第一个键规范形态为 `manifest_version`(W5051)。',
    },
    comptime: {
        keyed: false,
        doc: 'comptime 求值预算(可选)。`budget_ms` 超时 = E6010。',
    },
    dep: {
        keyed: true,
        doc: '依赖声明(键控块,块名 = 包名;重复名 = E5045)。来源三形互斥(E5049):`path` | `git`+`rev` | `version`。',
    },
    lock: {
        keyed: false,
        doc: '锁文件根块(Ctron.lock;机器独写,规范渲染,diff 即依赖变更审计)。',
    },
    resolve: {
        keyed: true,
        doc: '锁文件解析项(键控块,块名 = 包名;示意——注册表随 v0.6 依赖解析立表)。',
    },
    workspace: { keyed: false, doc: '工作区(预留,Cargo 式;成员用键控块 member "名" { path })。' },
    profile: { keyed: true, doc: '档位/目标覆写(预留;单层条件,覆写键写在块内)。' },
    member: { keyed: true, doc: '工作区成员(预留;path = 成员目录)。' },
    cap: { keyed: true, doc: '带参数的能力(示意,§C.2;立表须随消费实现)。' },
    menu: { keyed: true, doc: '树形数据示例块(路径实参键控,D11;示意)。' },
    fmt: { keyed: false, doc: '工具配置根块示例:根块 = 工具名,表由消费者立(§3 框架)。' },
};

const KEYS = {
    pkg: {
        manifest_version: { type: 'I64', values: ['1'], doc: '语言版本键:必须存在且 = 1(E5050);规范形态居首(W5051)。' },
        name: { type: 'Str', doc: '包名;模式 `[a-z][a-z0-9_-]*`。' },
        version: { type: 'Str', doc: '完整 semver(含 prerelease/build,R1);清单与 lockfile 同形。' },
        caps: { type: 'List[Str]', doc: '能力声明,集合语义(规范形态排序)。成员 ∈ fs/time/net/log;未知能力 = E5043(安全边界,fail-closed)。' },
    },
    comptime: {
        budget_ms: { type: 'I64', doc: 'comptime 预算(毫秒,≥ 1);超时 = E6010。数值一律定点整数(无浮点,G1)。' },
    },
    dep: {
        path: { type: 'Str', doc: '本地路径来源(值可含 `//`——注释识别感知字符串)。与 git/version 互斥(E5049)。' },
        git: { type: 'Str', doc: 'git 仓库 URL;必须伴随 `rev`(E5049)。URL 原样保留。' },
        rev: { type: 'Str', doc: 'git 形的修订钉死(标签/哈希);升级须改此行并过测试。' },
        version: { type: 'Str', doc: 'semver 依赖来源;实际版本由 lockfile 固定。与 path/git 互斥(E5049)。' },
    },
    lock: {
        lock_version: { type: 'I64', values: ['1'], doc: '锁文件语言版本键(示意,随 v0.6 立表)。' },
    },
    resolve: {
        git: { type: 'Str', doc: '实际拉取的仓库 URL(示意,随 v0.6 立表)。' },
        rev: { type: 'Str', doc: '实际钉死的修订(示意)。' },
        version: { type: 'Str', doc: '实际使用的 semver(示意)。' },
        content: { type: 'Str', doc: '拉取物内容哈希(`sha256:…`);清单说"要什么",锁文件说"实际用了什么"。' },
    },
    workspace: { path: { type: 'Str', doc: '成员目录(示意)。' } },
    member: { path: { type: 'Str', doc: '成员目录(示意)。' } },
    profile: { budget_ms: { type: 'I64', doc: '覆写 comptime 预算(示意)。' } },
    cap: { mode: { type: 'Str', doc: '能力参数(示意,§C.2)。' } },
    menu: { label: { type: 'Str', doc: '显示文本(示意;非 ASCII 原样存储)。' }, key: { type: 'Str', doc: '快捷键(示意)。' }, order: { type: 'I64', doc: '显式顺序键:顺序即数据(路径段是名字,不是下标,B.4)。' } },
    fmt: {
        config_version: { type: 'I64', values: ['1'], doc: '工具配置版本键(示意)。' },
        use_color: { type: 'Bool', values: ['true', 'false'], doc: '布尔唯一拼写 true/false(不设 yes/no/on/off,C.7)。' },
        max_width: { type: 'I64', doc: '定点整数(示意)。' },
    },
};

// ---------- 上下文识别 ----------

const RE_BLOCK_HEAD = /^\s*([a-z_][a-z0-9_]*)(\s+"(?:\\.|[^"\\])*")?\s*\{/;
const RE_BLOCK_END = /^\s*\}/;
const RE_FIELD_PREFIX = /^\s*([a-z_][a-z0-9_]*)?$/;          // 行首裸词(块头/键位)
const RE_VALUE_POS = /^\s*([a-z_][a-z0-9_]*)\s*=\s*(\S*)$/;  // `key = <partial>`
const RE_LIST_POS = /^\s*([a-z_][a-z0-9_]*)\s*=\s*\[[^\]]*$/; // 列表内

function currentBlock(document, line) {
    const stack = [];
    for (let i = 0; i < line; i++) {
        const t = (document.lineAt(i).text || '').trim();
        if (!t || t.startsWith('//')) { continue; }
        if (RE_BLOCK_END.test(t)) { stack.pop(); continue; }
        const m = RE_BLOCK_HEAD.exec(t);
        if (m) { stack.push(m[1]); }
    }
    return stack.length ? stack[stack.length - 1] : null;
}

function linePrefix(document, position) {
    return document.lineAt(position.line).text.slice(0, position.character);
}

// ---------- 补全 ----------

function keyItem(name, def) {
    const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Field);
    item.detail = `${def.type} — CTCL 注册表键`;
    item.documentation = new vscode.MarkdownString(def.doc);
    item.insertText = `${name} = `;
    return item;
}

function blockItem(name, def, indent) {
    const pad = indent || '';
    const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Keyword);
    item.detail = 'CTCL 块';
    item.documentation = new vscode.MarkdownString(def.doc);
    if (def.keyed) {
        item.insertText = `${name} "\${1:name}" {\n${pad}\t\${2:key} = \${3:value}\n${pad}}`;
    } else if (name === 'pkg') {
        item.insertText = `${name} {\n${pad}\tmanifest_version = 1\n${pad}\tname = "\${1:name}"\n${pad}\tversion = "\${2:0.1.0}"\n${pad}\tcaps = [\${3:"fs"}]\n${pad}}`;
    } else {
        item.insertText = `${name} {\n${pad}\t\${1:key} = \${2:value}\n${pad}}`;
    }
    item.insertTextRules = vscode.CompletionItemInsertTextRule.InsertAsSnippet;
    return item;
}

function capItem(name) {
    const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.EnumMember);
    item.detail = '能力(Cap)';
    item.documentation = new vscode.MarkdownString('能力对象权限;未在 pkg.caps 声明而使用 = E4010(编译期,非运行期)。');
    item.insertText = `"${name}"`;
    return item;
}

function complete(document, position) {
    const before = linePrefix(document, position);
    const block = currentBlock(document, position.line);
    const items = [];

    // 列表内:caps 能力值
    const listKey = (RE_LIST_POS.exec(before) || [])[1];
    if (listKey === 'caps') {
        for (const c of CAPABILITIES) { items.push(capItem(c)); }
        return items;
    }

    // 值位:按键给字面量
    const val = RE_VALUE_POS.exec(before);
    if (val && block && KEYS[block] && KEYS[block][val[1]] && KEYS[block][val[1]].values) {
        for (const v of KEYS[block][val[1]].values) {
            const item = new vscode.CompletionItem(v, vscode.CompletionItemKind.Value);
            item.detail = `${val[1]} 的合法值`;
            item.insertText = v;
            items.push(item);
        }
        return items;
    }

    // 键位:当前块的注册表键
    if (block && RE_FIELD_PREFIX.test(before) && KEYS[block]) {
        for (const [k, def] of Object.entries(KEYS[block])) { items.push(keyItem(k, def)); }
        return items;
    }

    // 块位:注册表块头
    if (!block && RE_FIELD_PREFIX.test(before)) {
        for (const [name, def] of Object.entries(BLOCKS)) { items.push(blockItem(name, def)); }
    }
    return items;
}

// ---------- 悬浮提示 ----------

function hover(document, position) {
    const range = document.getWordRangeAtPosition(position, /[a-zA-Z_][a-zA-Z0-9_]*/);
    if (!range) { return null; }
    const word = document.getText(range);
    const block = currentBlock(document, position.line);

    const lines = [];
    if (block && KEYS[block] && KEYS[block][word]) {
        const def = KEYS[block][word];
        lines.push(`\`${word}\`: **${def.type}**(${block} 块)`, '', def.doc);
    } else if (BLOCKS[word]) {
        lines.push(`\`${word}\` — CTCL 块`, '', BLOCKS[word].doc);
    } else if (CAPABILITIES.includes(word)) {
        lines.push(`\`${word}\` — 能力(Cap)`, '', `能力对象权限;须经 pkg.caps 声明,未声明而使用 = E4010。能力是安全边界,未知能力 = E5043。`);
    } else {
        return null;
    }
    const md = new vscode.MarkdownString(lines.join('\n'));
    md.appendMarkdown('\n\n---\n*出处:CTCL 规范 v1(`docs/superpowers/specs/2026-09-16-config-language-v1.md`)*');
    return new vscode.Hover(md, range);
}

module.exports = { complete, hover };
