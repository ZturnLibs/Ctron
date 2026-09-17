// CTML(.ctml)补全与悬浮提示 —— 扩展侧本地注册表,零依赖。
// 数据源:docs/superpowers/specs/2026-09-16-gui-ctml-design.md
//   §4.2 语法(内建标签集/控制元素/事件绑定)、§4.3 组件模型(props/slot 契约)、
//   §5.1 样式 CSS 子集(红线:无 cascade/grid/动画)、§5.2 令牌与颜色(C1:颜色一律 Str)。
// 组件标签 = 本文档 `view Name` 声明动态扫描(同文件互引用合法,声明顺序无关)。
'use strict';

const vscode = require('vscode');

// ---------- 注册表 ----------

const TAGS = {
    vbox: { snippet: 'vbox class="${1:class}">\n\t$0\n</vbox', doc: '垂直布局容器:子元素纵向排列。' },
    hbox: { snippet: 'hbox class="${1:class}">\n\t$0\n</hbox', doc: '水平布局容器:子元素横向排列。' },
    label: { snippet: 'label class="${1:class}">${2:text}</label', doc: '文本标签;内容支持 `{expr}` 插值(与 Ctron 字符串插值同语法)。' },
    button: { snippet: 'button class="${1:class}" on:click={${2:handler}}>${3:文本}</button', doc: '按钮;`on:click` 绑定 fn 值/闭包,handler 改 model 后统一失效。' },
    input: { snippet: 'input bind={${1:model.draft}} placeholder="${2:…}" on:submit={${3:handler}}/', doc: '受控文本输入:`bind` 双向绑定;IME 组词(composer)一等状态(M3 起)。' },
    checkbox: { snippet: 'checkbox checked={${1:flag}} on:toggle={${2:handler}}/', doc: '复选框:`checked` Bool 绑定,`on:toggle` 事件。' },
    spacer: { snippet: 'spacer/', doc: '弹性占位(吃掉剩余空间)。' },
    img: { snippet: 'img src="${1:path}"/>', doc: '图片;src 相对 `assets/` 目录解析。' },
    each: { snippet: 'each ${1:item} in={${2:list}}}>\n\t$0\n</each', doc: '列表渲染:`<each item in={list} key={k}>`;key 须满足 Eq(重复 = E8180),有 key 重排不串位。' },
    when: { snippet: 'when cond={${1:cond}}}>\n\t$0\n</when', doc: '条件渲染:`<when cond={bool}>`;保持"一切都是标签"的心智模型。' },
    slot: { snippet: 'slot name="${1:name}"/>', doc: '内容分发占位:`<slot/>` 默认;`<slot name="…"/>` 具名;无匹配宿主 = E8160。' },
};

const ATTRS = {
    class: '样式类(空格分隔多类;与动态 `class={expr}` 并存时合并序 = 静态在前、动态在后)。引用未导入样式 = E8150。',
    role: '无障碍语义位(M4 消费,P0 起解析占位);对全部元素可用。',
    label: '无障碍标签(同 role)。',
    slot: '填入宿主 view 的具名 slot(web-components 同款约定);组件标签外使用 = E8160。',
    bind: '双向绑定(input 受控组件;运行时维护光标/选区/IME 合成窗口)。',
    visible: 'Bool 属性绑定;绑定表达式必须纯读(赋值/可变调用 = E8190),类型不符 = E8110。',
    disabled: 'Bool 属性绑定;禁用交互。',
    placeholder: 'input 占位文本。',
    src: 'img 图片路径(相对 assets/ 解析)。',
    checked: 'checkbox 选中绑定(Bool)。',
    key: '<each> 对齐复用键(值语义 Eq;重复 = E8180 运行时契约:dev 断言,release 按首次出现保留)。',
    in: '<each> 数据源表达式(列表长度订阅,push/remove 触发槽失效)。',
    cond: '<when> 条件表达式(Bool)。',
    name: '<slot> 具名。',
};

const EVENTS = {
    click: '点击事件:`on:click={handler}`,值为 fn 值/闭包;是 fn 类型 props 的语法糖,无第二事件系统。',
    toggle: '切换事件(checkbox):`on:toggle={handler}`。',
    submit: '提交事件(input 焦点 + Enter 时由运行时合成):`on:submit={handler}`。',
};

// 样式属性 = §5.1 CSS 子集(冻结;红线:cascade/specificity/元素选择器/!important/grid/动画均不进第一版)
const STYLE_PROPS = {
    padding: 'box model;全部长度为逻辑像素(物理 = 逻辑 × DPI scale)。多值:`padding: 8 16`。',
    margin: 'box model 外边距。',
    width: '宽度(逻辑像素)。',
    height: '高度(逻辑像素)。',
    direction: 'flex 方向:`column | row`。',
    gap: '子元素间距(逻辑像素)。',
    align: '交叉轴对齐(flex 子集)。',
    justify: '主轴对齐(flex 子集)。',
    wrap: 'flex 换行。',
    bg: '背景色;颜色一律 Str 字面量 `#RRGGBB[AA]`(C1 裁决),格式非法 = E8130。',
    color: '前景/文本色;`#RRGGBB[AA]`。',
    border: '边框。',
    radius: '圆角(逻辑像素)。',
    opacity: '不透明度。',
    'font-size': '字号(逻辑像素)。',
    'font-weight': '字重(如 600)。',
    'font-family': '字体族。',
};

// 元素 × 属性 相关性(只列元素特有;class/role/label 对全部元素可用)
const TAG_ATTRS = {
    each: ['in', 'key'],
    when: ['cond'],
    slot: ['name'],
    input: ['bind', 'placeholder', 'disabled', 'visible'],
    checkbox: ['checked', 'disabled', 'visible'],
    button: ['disabled', 'visible'],
    img: ['src'],
    label: ['visible'],
    vbox: ['visible'],
    hbox: ['visible'],
    spacer: [],
};
const TAG_EVENTS = {
    button: ['click'],
    input: ['submit'],
    checkbox: ['toggle'],
};
const COMMON_ATTRS = ['class', 'role', 'label', 'slot', 'visible'];

// ---------- 上下文识别 ----------

const RE_STYLE_HEAD = /^\s*(?:pub\s+)?style\s+[a-z][a-z0-9_-]*.*\{/;
const RE_VIEW_HEAD = /^\s*(?:pub\s+)?view\s+[A-Z]/;
const RE_BLOCK_END = /^\s*\}/;
const RE_VIEW_DECL = /^\s*(?:pub\s+)?view\s+([A-Z][A-Za-z0-9_]*)/;

function inStyleBlock(document, line) {
    const stack = [];
    for (let i = 0; i < line; i++) {
        const t = (document.lineAt(i).text || '').trim();
        if (!t || t.startsWith('//')) { continue; }
        if (RE_BLOCK_END.test(t)) { stack.pop(); continue; }
        if (RE_STYLE_HEAD.test(t)) { stack.push('style'); continue; }
        if (RE_VIEW_HEAD.test(t)) { stack.push('view'); }
    }
    return stack[stack.length - 1] === 'style';
}

// 单行形态 `style name { ... }`:光标在本行 style 体上也算(规范示例常见单行样式)
function inStyleBody(document, position, before) {
    if (inStyleBlock(document, position.line)) { return true; }
    const m = /(?:pub\s+)?style\s+[a-z][a-z0-9_-]*(?:\s+extends\s+[a-z][a-z0-9_-]*)?\s*\{/.exec(before);
    return !!m && !before.slice(m.index).includes('}');
}

function documentViews(document) {
    const names = new Set();
    const text = document.getText();
    let m;
    const re = new RegExp(RE_VIEW_DECL.source, 'gm');
    while ((m = re.exec(text)) !== null) { names.add(m[1]); }
    return [...names];
}

// 光标是否处于标签内;返回 { tag, phase: 'name'|'attr' } 或 null
function tagContext(before) {
    const lt = before.lastIndexOf('<');
    if (lt < 0) { return null; }
    const gt = before.lastIndexOf('>');
    if (gt > lt) { return null; }
    const inner = before.slice(lt + 1);
    const sp = inner.search(/\s/);
    if (sp < 0) {
        return { tag: inner.replace(/[^A-Za-z0-9_-].*$/, ''), phase: 'name' };
    }
    return { tag: inner.slice(0, sp), phase: inner.slice(sp + 1).includes('=') ? 'value' : 'attr' };
}

function linePrefix(document, position) {
    return document.lineAt(position.line).text.slice(0, position.character);
}

// ---------- 补全 ----------

function attrItem(name) {
    const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Property);
    item.detail = 'CTML 属性';
    item.documentation = new vscode.MarkdownString(ATTRS[name] || '');
    item.insertText = `${name} = `;
    return item;
}

function eventItem(ev) {
    const name = `on:${ev}`;
    const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Event);
    item.detail = 'CTML 事件绑定';
    item.documentation = new vscode.MarkdownString(EVENTS[ev] || '事件绑定:值为 fn 值/闭包(fn 类型 props 的语法糖)。');
    item.insertText = `on:${ev}={\${1:handler}}`;
    item.insertTextRules = vscode.CompletionItemInsertTextRule.InsertAsSnippet;
    return item;
}

function complete(document, position) {
    const before = linePrefix(document, position);
    const word = (before.match(/[a-zA-Z_-]+$/) || [''])[0];
    const items = [];

    // 样式块:属性补全 + direction 值
    if (inStyleBody(document, position, before)) {
        const valPos = /:\s*([a-z]*)$/.exec(before);
        if (valPos && /direction\s*$/.test(before.slice(0, valPos.index))) {
            for (const v of ['column', 'row']) {
                const item = new vscode.CompletionItem(v, vscode.CompletionItemKind.Value);
                item.detail = 'direction 值';
                items.push(item);
            }
            return items;
        }
        for (const [name, doc] of Object.entries(STYLE_PROPS)) {
            const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Property);
            item.detail = '样式属性(CSS 子集)';
            item.documentation = new vscode.MarkdownString(doc);
            item.insertText = `${name}: `;
            items.push(item);
        }
        return items;
    }

    const ctx = tagContext(before);

    // 标签名位:内建元素 + 本文档 view 组件
    if (ctx && ctx.phase === 'name') {
        for (const [name, def] of Object.entries(TAGS)) {
            const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Keyword);
            item.detail = 'CTML 内建元素';
            item.documentation = new vscode.MarkdownString(def.doc);
            item.insertText = def.snippet;
            item.insertTextRules = vscode.CompletionItemInsertTextRule.InsertAsSnippet;
            items.push(item);
        }
        for (const name of documentViews(document)) {
            const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Class);
            item.detail = 'view 组件(本文档)';
            item.documentation = new vscode.MarkdownString('PascalCase 标签 = view 组件;同文件直接引用(声明顺序无关),自引用合法(树形递归)。');
            item.insertText = `${name} $0/>`;
            item.insertTextRules = vscode.CompletionItemInsertTextRule.InsertAsSnippet;
            items.push(item);
        }
        return items;
    }

    // 属性位:元素特有 + 通用 + 事件
    if (ctx && ctx.phase === 'attr') {
        const own = TAG_ATTRS[ctx.tag] || [];
        for (const name of [...own, ...COMMON_ATTRS]) {
            if (!items.some((i) => i.label === name)) { items.push(attrItem(name)); }
        }
        for (const ev of TAG_EVENTS[ctx.tag] || ['click']) {
            items.push(eventItem(ev));
        }
        return items;
    }

    // 兜底:无标签上下文时给属性全集(宽容启发式)
    if (!ctx && word) {
        for (const name of Object.keys(ATTRS)) { items.push(attrItem(name)); }
    }
    return items;
}

// ---------- 悬浮提示 ----------

function hover(document, position) {
    const range = document.getWordRangeAtPosition(position, /[a-zA-Z_][a-zA-Z0-9_-]*/);
    if (!range) { return null; }
    const word = document.getText(range);

    let doc = null;
    let title = null;
    if (TAGS[word]) {
        title = `\`${word}\` — CTML 内建元素`;
        doc = TAGS[word].doc;
    } else if (ATTRS[word]) {
        title = `\`${word}\` — CTML 属性`;
        doc = ATTRS[word];
    } else if (EVENTS[word]) {
        title = `\`on:${word}\` — CTML 事件绑定`;
        doc = EVENTS[word];
    } else if (STYLE_PROPS[word]) {
        title = `\`${word}\` — 样式属性(CSS 子集)`;
        doc = STYLE_PROPS[word];
    } else {
        const re = new RegExp(`^(?:pub\\s+)?view\\s+${word}\\b`, 'm');
        if (re.test(document.getText())) {
            title = `\`${word}\` — view 组件(本文档)`;
            doc = '编译为静态骨架 + 绑定槽表 + 事件表;props 全必填(缺失/多余 = E8100),子组件禁止回写父状态(E8170)。';
        }
    }
    if (!doc) { return null; }

    const md = new vscode.MarkdownString(`${title}\n\n${doc}`);
    md.appendMarkdown('\n\n---\n*出处:CTML 设计规范(`docs/superpowers/specs/2026-09-16-gui-ctml-design.md`)*');
    return new vscode.Hover(md, range);
}

module.exports = { complete, hover };
