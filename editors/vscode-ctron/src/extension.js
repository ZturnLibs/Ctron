// Ctron LSP 客户端 —— 手写 JSON-RPC over stdio,零 npm 依赖。
// 协议子集:initialize / textDocument 同步(全量)/ publishDiagnostics /
//          hover / completion / definition / documentSymbol。
'use strict';

const vscode = require('vscode');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const ctclSupport = require('./ctcl');
const ctmlSupport = require('./ctml');

let client = null;
let outputChannel = null;
let lspDiagnostics = null;      // 词法诊断(LSP 服务器推送)
let checkDiagnostics = null;    // 编译器语义诊断(ctronc check --format=json)
let checkRunner = null;

function log(msg) {
    const cfg = vscode.workspace.getConfiguration('ctron');
    if (cfg.get('server.debug') && outputChannel) {
        outputChannel.appendLine(msg);
    }
}

function resolveServerCommand(context) {
    const cfg = vscode.workspace.getConfiguration('ctron');
    const custom = cfg.get('server.command');
    if (custom && custom.trim()) { return custom.trim(); }
    return path.join(context.extensionPath, 'server', 'ctron-lsp.sh');
}

// ---------- JSON-RPC 帧编解码 ----------

function encodeMessage(obj) {
    const body = Buffer.from(JSON.stringify(obj), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body
    ]);
}

const decoderState = Symbol('decoder');

function createFrameParser(onMessage) {
    let buf = Buffer.alloc(0);
    return (chunk) => {
        buf = Buffer.concat([buf, chunk]);
        for (;;) {
            const sep = buf.indexOf('\r\n\r\n');
            if (sep < 0) { return; }
            const header = buf.slice(0, sep).toString('ascii');
            const m = /Content-Length:\s*(\d+)/i.exec(header);
            if (!m) { buf = Buffer.alloc(0); return; }
            const len = parseInt(m[1], 10);
            if (buf.length < sep + 4 + len) { return; }
            const body = buf.slice(sep + 4, sep + 4 + len).toString('utf8');
            buf = buf.slice(sep + 4 + len);
            try {
                onMessage(JSON.parse(body));
            } catch (e) {
                log(`帧解析失败: ${e.message}`);
            }
        }
    };
}

// ---------- LSP 客户端 ----------

class CtronLspClient {
    constructor(context) {
        this.context = context;
        this.child = null;
        this.nextId = 1;
        this.pending = new Map();      // id -> {resolve, reject, timer}
        this.ready = false;
        this.serverCaps = {};
        this.opened = new Set();
        this.restartCount = 0;
    }

    start() {
        const cmd = resolveServerCommand(this.context);
        if (!fs.existsSync(cmd)) {
            vscode.window.showWarningMessage(`Ctron 语言服务器脚本不存在: ${cmd}`);
            return;
        }
        log(`启动 ${cmd}`);
        this.child = spawn(cmd, [], {
            shell: cmd.endsWith('.sh'),
            env: { ...process.env }
        });
        this.child.stdout.setEncoding(null);
        const parse = createFrameParser((msg) => this.onMessage(msg));
        this.child.stdout.on('data', parse);
        this.child.stderr.on('data', (d) => log(`[server] ${d.toString().trim()}`));
        this.child.on('exit', (code, signal) => {
            log(`服务器退出 code=${code} signal=${signal}`);
            this.ready = false;
            this.rejectAllPending('服务器已退出');
            if (!signal && code !== 0 && this.restartCount < 3) {
                this.restartCount += 1;
                setTimeout(() => this.start(), 500 * this.restartCount);
            } else if (!signal && code !== 0 && this.restartCount >= 3) {
                vscode.window.showWarningMessage(
                    'Ctron 语言服务器启动失败:找不到 ctronc 或运行出错。诊断/补全/跳转不可用(语法高亮不受影响)。',
                    '查看安装说明'
                ).then((pick) => {
                    if (pick === '查看安装说明') {
                        const readme = vscode.Uri.file(path.join(this.context.extensionPath, 'README.md'));
                        vscode.commands.executeCommand('markdown.showPreview', readme);
                    }
                });
            }
        });
        this.initialize();
    }

    initialize() {
        const roots = vscode.workspace.workspaceFolders || [];
        this.request('initialize', {
            processId: process.pid,
            rootUri: roots.length ? String(roots[0].uri) : null,
            capabilities: {}
        }, 8000).then((result) => {
            this.serverCaps = (result && result.capabilities) || {};
            this.ready = true;
            this.notify('initialized', {});
            log('initialize 完成');
            // 补发已在编辑器中打开的文档
            for (const ed of vscode.window.visibleTextEditors) {
                this.handleOpen(ed.document);
            }
        }).catch((e) => {
            log(`initialize 失败: ${e.message}`);
        });
    }

    onMessage(msg) {
        log(`< ${JSON.stringify(msg).slice(0, 400)}`);
        if (msg.id !== undefined && (msg.result !== undefined || msg.error !== undefined)) {
            const p = this.pending.get(msg.id);
            if (p) {
                clearTimeout(p.timer);
                this.pending.delete(msg.id);
                if (msg.error) { p.reject(new Error(msg.error.message || 'server error')); }
                else { p.resolve(msg.result); }
            }
            return;
        }
        if (msg.method === 'textDocument/publishDiagnostics') {
            this.onDiagnostics(msg.params);
        }
    }

    onDiagnostics(params) {
        const diags = [];
        for (const d of params.diagnostics || []) {
            const range = new vscode.Range(
                d.range.start.line, d.range.start.character,
                d.range.end.line, d.range.end.character
            );
            const sev = d.severity === 2 ? vscode.DiagnosticSeverity.Warning
                : d.severity === 3 ? vscode.DiagnosticSeverity.Information
                : d.severity === 4 ? vscode.DiagnosticSeverity.Hint
                : vscode.DiagnosticSeverity.Error;
            const diag = new vscode.Diagnostic(range, d.message || '', sev);
            diag.code = d.code;
            diag.source = 'ctron-lsp';
            diags.push(diag);
        }
        lspDiagnostics.set(vscode.Uri.parse(params.uri), diags);
    }

    sendRaw(obj) {
        if (!this.child) { return false; }
        log(`> ${JSON.stringify(obj).slice(0, 400)}`);
        this.child.stdin.write(encodeMessage(obj));
        return true;
    }

    request(method, params, timeoutMs = 4000) {
        if (!this.child) { return Promise.reject(new Error('服务器未启动')); }
        const id = this.nextId++;
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pending.delete(id);
                reject(new Error(`${method} 超时`));
            }, timeoutMs);
            this.pending.set(id, { resolve, reject, timer });
            this.sendRaw({ jsonrpc: '2.0', id, method, params });
        });
    }

    notify(method, params) {
        this.sendRaw({ jsonrpc: '2.0', method, params });
    }

    rejectAllPending(reason) {
        for (const [, p] of this.pending) {
            clearTimeout(p.timer);
            p.reject(new Error(reason));
        }
        this.pending.clear();
    }

    // ---------- 文档事件 ----------

    handleOpen(doc) {
        if (doc.languageId !== 'ctron' || this.opened.has(doc.uri.toString())) { return; }
        this.opened.add(doc.uri.toString());
        this.notify('textDocument/didOpen', {
            textDocument: {
                uri: doc.uri.toString(),
                languageId: doc.languageId,
                version: doc.version,
                text: doc.getText()
            }
        });
    }

    handleChange(doc) {
        if (doc.languageId !== 'ctron' || !this.opened.has(doc.uri.toString())) { return; }
        this.notify('textDocument/didChange', {
            textDocument: { uri: doc.uri.toString(), version: doc.version },
            contentChanges: [{ text: doc.getText() }]
        });
    }

    handleClose(doc) {
        const key = doc.uri.toString();
        if (!this.opened.has(key)) { return; }
        this.opened.delete(key);
        lspDiagnostics.delete(doc.uri);
        this.notify('textDocument/didClose', {
            textDocument: { uri: key }
        });
    }

    // ---------- 语言特性 ----------

    positionOf(document, position) {
        return { line: position.line, character: position.character };
    }

    textDocId(document) {
        return { uri: document.uri.toString() };
    }

    async safeRequest(method, params) {
        if (!this.ready) { return null; }
        try { return await this.request(method, params, 3000); }
        catch { return null; }
    }

    hover(document, position) {
        return this.safeRequest('textDocument/hover', {
            textDocument: this.textDocId(document),
            position: this.positionOf(document, position)
        }).then((result) => {
            if (!result || !result.contents) { return null; }
            const c = result.contents;
            const md = new vscode.MarkdownString(typeof c === 'string' ? c : (c.value || ''));
            return new vscode.Hover(md, (typeof c === 'object' && c.range) ? asRange(c.range) : undefined);
        });
    }

    completion(document, position) {
        return this.safeRequest('textDocument/completion', {
            textDocument: this.textDocId(document),
            position: this.positionOf(document, position)
        }).then((result) => {
            if (!result) { return null; }
            const items = Array.isArray(result) ? result : (result.items || []);
            return items.map((it) => {
                const item = new vscode.CompletionItem(it.label, kindOf(it.kind));
                if (it.detail) { item.detail = it.detail; }
                if (it.documentation) { item.documentation = new vscode.MarkdownString(it.documentation); }
                return item;
            });
        });
    }

    definition(document, position) {
        return this.safeRequest('textDocument/definition', {
            textDocument: this.textDocId(document),
            position: this.positionOf(document, position)
        }).then((result) => {
            if (!result) { return null; }
            const defs = Array.isArray(result) ? result : [result];
            return defs.map((d) => new vscode.Location(
                vscode.Uri.parse(d.uri), asRange(d.range)
            ));
        });
    }

    documentSymbol(document) {
        return this.safeRequest('textDocument/documentSymbol', {
            textDocument: this.textDocId(document)
        }).then((result) => {
            if (!Array.isArray(result)) { return null; }
            return result.map((s) => ({
                name: s.name,
                kind: s.kind,
                range: asRange(s.range),
                selectionRange: asRange(s.selectionRange || s.range)
            }));
        });
    }

    // ---------- 新增:rename / references / highlight / signatureHelp ----------

    prepareRename(document, position) {
        return this.safeRequest('textDocument/prepareRename', {
            textDocument: this.textDocId(document),
            position: this.positionOf(document, position)
        }).then((result) => {
            if (!result || !result.range) { return null; }
            return { range: asRange(result.range), placeholder: result.placeholder };
        });
    }

    rename(document, position, newName) {
        return this.safeRequest('textDocument/rename', {
            textDocument: this.textDocId(document),
            position: this.positionOf(document, position),
            newName
        }).then((result) => {
            if (!result || !result.changes) { return null; }
            const edit = new vscode.WorkspaceEdit();
            for (const [uri, edits] of Object.entries(result.changes)) {
                for (const e of edits || []) {
                    edit.replace(vscode.Uri.parse(uri), asRange(e.range), e.newText);
                }
            }
            return edit;
        });
    }

    references(document, position) {
        return this.safeRequest('textDocument/references', {
            textDocument: this.textDocId(document),
            position: this.positionOf(document, position),
            context: { includeDeclaration: true }
        }).then((result) => {
            if (!Array.isArray(result)) { return null; }
            return result.map((d) => new vscode.Location(vscode.Uri.parse(d.uri), asRange(d.range)));
        });
    }

    documentHighlight(document, position) {
        return this.safeRequest('textDocument/documentHighlight', {
            textDocument: this.textDocId(document),
            position: this.positionOf(document, position)
        }).then((result) => {
            if (!Array.isArray(result)) { return null; }
            return result.map((h) => new vscode.DocumentHighlight(asRange(h.range)));
        });
    }

    signatureHelp(document, position) {
        return this.safeRequest('textDocument/signatureHelp', {
            textDocument: this.textDocId(document),
            position: this.positionOf(document, position)
        }).then((result) => {
            if (!result || !Array.isArray(result.signatures)) { return null; }
            return {
                activeSignature: result.activeSignature || 0,
                activeParameter: result.activeParameter || 0,
                signatures: result.signatures.map((s) => {
                    const info = new vscode.SignatureInformation(s.label);
                    info.parameters = (s.parameters || []).map((p) => {
                        const pi = new vscode.ParameterInformation(
                            Array.isArray(p.label) ? new vscode.Range(0, p.label[0], 0, p.label[1]) : p.label
                        );
                        return pi;
                    });
                    return info;
                })
            };
        });
    }

    // ---------- P2:inlay hints / 格式化 / 折叠 ----------

    inlayHints(document, range) {
        return this.safeRequest('textDocument/inlayHint', {
            textDocument: this.textDocId(document),
            range: { start: { line: 0, character: 0 }, end: { line: 1e9, character: 0 } }
        }).then((result) => {
            if (!Array.isArray(result)) { return null; }
            return result.map((h) => {
                const pos = new vscode.Position(h.position.line, h.position.character);
                return new vscode.InlayHint(pos, h.label || '',
                    h.kind === 2 ? vscode.InlayHintKind.Type : vscode.InlayHintKind.Parameter);
            });
        });
    }

    formatting(document, _options) {
        return this.safeRequest('textDocument/formatting', {
            textDocument: this.textDocId(document),
            options: { tabSize: 4, insertSpaces: true }
        }).then((result) => {
            if (!Array.isArray(result)) { return null; }
            return result.map((e) => new vscode.TextEdit(asRange(e.range), e.newText));
        });
    }

    foldingRanges(document) {
        return this.safeRequest('textDocument/foldingRange', {
            textDocument: this.textDocId(document)
        }).then((result) => {
            if (!Array.isArray(result)) { return null; }
            return result.map((f) => new vscode.FoldingRange(f.startLine, f.endLine, f.kind || 'region'));
        });
    }

    semanticTokens(document) {
        return this.safeRequest('textDocument/semanticTokens/full', {
            textDocument: this.textDocId(document)
        }).then((result) => {
            if (!result || !Array.isArray(result.data)) { return null; }
            return new vscode.SemanticTokens(new Uint32Array(result.data), undefined);
        });
    }

    dispose() {
        try { this.notify('exit', {}); } catch { /* 忽略 */ }
        if (this.child) {
            setTimeout(() => { try { this.child.kill(); } catch { /* 忽略 */ } }, 300);
        }
        this.rejectAllPending('客户端关闭');
        this.ready = false;
    }
}

function asRange(r) {
    return new vscode.Range(r.start.line, r.start.character, r.end.line, r.end.character);
}

const LSP_KIND_MAP = {
    2: vscode.CompletionItemKind.Method,
    3: vscode.CompletionItemKind.Function,
    4: vscode.CompletionItemKind.Constructor,
    5: vscode.CompletionItemKind.Field,
    6: vscode.CompletionItemKind.Variable,
    7: vscode.CompletionItemKind.Class,
    9: vscode.CompletionItemKind.Module,
    10: vscode.CompletionItemKind.Property,
    12: vscode.CompletionItemKind.Unit,
    14: vscode.CompletionItemKind.Keyword,
    22: vscode.CompletionItemKind.Struct
};

function kindOf(k) {
    return LSP_KIND_MAP[k] || vscode.CompletionItemKind.Text;
}

// ---------- ctronc 定位与编译器检查包装(§10.2 JSON 契约) ----------

function resolveCtronc(context) {
    const cfg = vscode.workspace.getConfiguration('ctron');
    const candidates = [];
    const custom = cfg.get('ctroncPath');
    if (custom && custom.trim()) { candidates.push(custom.trim()); }
    if (process.env.CTRONC) { candidates.push(process.env.CTRONC); }
    candidates.push('/opt/homebrew/bin/ctronc', '/usr/local/bin/ctronc');
    if (process.env.HOME) {
        candidates.push(path.join(process.env.HOME, '.local/bin/ctronc'));
        candidates.push(path.join(process.env.HOME, 'bin/ctronc'));
    }
    // 仓库开发布局(扩展仍在仓库内时)
    candidates.push(path.resolve(context.extensionPath, '..', '..', 'compiler-c', 'build', 'ctronc'));
    for (const c of candidates) {
        try { fs.accessSync(c, fs.constants.X_OK); return c; } catch { /* 下一个 */ }
    }
    for (const dir of (process.env.PATH || '').split(path.delimiter)) {
        if (!dir) { continue; }
        const c = path.join(dir, 'ctronc');
        try { fs.accessSync(c, fs.constants.X_OK); return c; } catch { /* 下一个 */ }
    }
    return null;
}

class CheckRunner {
    constructor(context) {
        this.context = context;
        this.ctronc = resolveCtronc(context);
        this.collection = vscode.languages.createDiagnosticCollection('ctron-check');
        this.raw = new Map();          // uri -> [{diag, fixes}]
        this.running = false;
        this.queued = null;
    }

    runNow(uri) {
        if (!this.ctronc) {
            vscode.window.showWarningMessage(
                'ctronc 不可用,语义检查跳过。可设置 "ctron.ctroncPath" 或构建 compiler-c。',
                '知道了'
            );
            return;
        }
        if (uri.scheme !== 'file') { return; }
        const fsPath = uri.fsPath;
        if (this.running) { this.queued = uri; return; }
        this.running = true;
        log(`[check] ${fsPath}`);
        const child = spawn(this.ctronc, ['check', '--format=json', fsPath], {});
        let out = '';
        child.stdout.on('data', (d) => { out += d.toString('utf8'); });
        child.on('exit', () => {
            this.running = false;
            try { this.apply(uri, out); } catch (e) { log(`check 解析失败: ${e.message}`); }
            if (this.queued) {
                const q = this.queued;
                this.queued = null;
                this.runNow(q);
            }
        });
    }

    apply(uri, jsonText) {
        const list = [];
        let parsed = null;
        try { parsed = JSON.parse(jsonText); } catch { parsed = null; }
        const ds = (parsed && parsed.diagnostics) || [];
        let text = '';
        try { text = fs.readFileSync(uri.fsPath, 'utf8'); } catch { text = ''; }
        const lineStarts = computeLineStarts(text);
        for (const d of ds) {
            const ls = Math.max(0, (d.span.line_start || 1) - 1);
            const cs = Math.max(0, (d.span.col_start || 1) - 1);
            const le = Math.max(0, (d.span.line_end || 1) - 1);
            const ce = Math.max(0, (d.span.col_end || 1) - 1);
            let range = new vscode.Range(ls, cs, le, ce);
            // 退化 span(0,0 或零宽)→ 扩到整行,便于阅读
            if (range.isEmpty && range.start.character === 0) {
                const lineEnd = lineEndOf(lineStarts, text, ls);
                range = new vscode.Range(ls, 0, ls, lineEnd);
            }
            const diag = new vscode.Diagnostic(
                range,
                (d.message || '') + ((d.notes || []).length ? '\n' + d.notes.join('\n') : ''),
                d.severity === 'warning' ? vscode.DiagnosticSeverity.Warning : vscode.DiagnosticSeverity.Error
            );
            diag.code = d.code;
            diag.source = 'ctronc';
            const entry = { diag, fixes: d.fixes || [] };
            list.push(entry);
        }
        this.raw.set(uri.toString(), list);
        this.collection.set(uri, list.map((e) => e.diag));
    }

    clear(uri) {
        this.raw.delete(uri.toString());
        this.collection.delete(uri);
    }

    // CodeActions:fixes[] 映射 + 词法确定性修复
    codeActions(document, contextDiagnostics) {
        const actions = [];
        const entries = this.raw.get(document.uri.toString()) || [];
        for (const d of contextDiagnostics) {
            // 1) 编译器 fixes[](§10.2)
            if (d.source === 'ctronc') {
                const entry = entries.find((e) => e.diag.message === d.message && e.diag.code === d.code && e.diag.range.isEqual(d.range));
                for (const fix of (entry && entry.fixes) || []) {
                    const we = new vscode.WorkspaceEdit();
                    for (const ed of fix.edits || []) {
                        const r = new vscode.Range(
                            Math.max(0, (ed.span.line_start || 1) - 1), Math.max(0, (ed.span.col_start || 1) - 1),
                            Math.max(0, (ed.span.line_end || 1) - 1), Math.max(0, (ed.span.col_end || 1) - 1)
                        );
                        if (ed.kind === 'insert') { we.insert(document.uri, r.start, ed.text || ''); }
                        else if (ed.kind === 'delete') { we.delete(document.uri, r); }
                        else { we.replace(document.uri, r, ed.text || ''); }
                    }
                    const action = new vscode.CodeAction(fix.title || '快速修复', vscode.CodeActionKind.QuickFix);
                    action.edit = we;
                    action.diagnostics = [d];
                    actions.push(action);
                }
            }
            // 2) 词法 E1001 确定性修复
            if (d.source === 'ctron-lsp' && d.code === 'E1001') {
                const msg = d.message || '';
                if (msg.includes('禁用的标点 ;')) {
                    const we = new vscode.WorkspaceEdit();
                    we.delete(document.uri, d.range);
                    const a = new vscode.CodeAction('删除多余的 ;', vscode.CodeActionKind.QuickFix);
                    a.edit = we;
                    a.diagnostics = [d];
                    actions.push(a);
                } else if (msg.includes('禁用的标点 ::')) {
                    const we = new vscode.WorkspaceEdit();
                    we.replace(document.uri, d.range, '.');
                    const a = new vscode.CodeAction(':: 改为 .(Ctron 路径分隔符是 .)', vscode.CodeActionKind.QuickFix);
                    a.edit = we;
                    a.diagnostics = [d];
                    actions.push(a);
                }
            }
        }
        return actions;
    }
}

function computeLineStarts(text) {
    const starts = [0];
    for (let i = 0; i < text.length; i++) {
        if (text[i] === '\n') { starts.push(i + 1); }
    }
    return starts;
}

function lineEndOf(lineStarts, text, line) {
    if (line + 1 < lineStarts.length) { return lineStarts[line + 1] - 1; }
    return text.length;
}

// ---------- 扩展入口 ----------

function activate(context) {
    outputChannel = vscode.window.createOutputChannel('ctron-lsp');
    lspDiagnostics = vscode.languages.createDiagnosticCollection('ctron-lsp');
    checkRunner = new CheckRunner(context);
    context.subscriptions.push(lspDiagnostics, checkRunner.collection, outputChannel, checkRunner);

    function launch() {
        if (client) { client.dispose(); }
        client = new CtronLspClient(context);
        client.start();
    }

    context.subscriptions.push(vscode.commands.registerCommand('ctron.restartServer', launch));
    context.subscriptions.push(vscode.commands.registerCommand('ctron.checkNow', () => {
        const ed = vscode.window.activeTextEditor;
        if (ed && ed.document.languageId === 'ctron') { checkRunner.runNow(ed.document.uri); }
    }));

    context.subscriptions.push(vscode.workspace.onDidOpenTextDocument((doc) => {
        if (client) { client.handleOpen(doc); }
    }));
    context.subscriptions.push(vscode.workspace.onDidChangeTextDocument((e) => {
        if (client) { client.handleChange(e.document); }
    }));
    context.subscriptions.push(vscode.workspace.onDidSaveTextDocument((doc) => {
        const cfg = vscode.workspace.getConfiguration('ctron');
        if (doc.languageId === 'ctron' && cfg.get('checkOnSave', true)) {
            checkRunner.runNow(doc.uri);
        }
    }));
    context.subscriptions.push(vscode.workspace.onDidCloseTextDocument((doc) => {
        if (client) { client.handleClose(doc); }
        checkRunner.clear(doc.uri);
    }));

    const selector = { language: 'ctron' };
    context.subscriptions.push(
        vscode.languages.registerHoverProvider(selector, {
            provideHover: (doc, pos) => client ? client.hover(doc, pos) : null
        }),
        vscode.languages.registerCompletionItemProvider(selector, {
            provideCompletionItems: (doc, pos) => client ? client.completion(doc, pos) : null
        }, '.'),
        vscode.languages.registerDefinitionProvider(selector, {
            provideDefinition: (doc, pos) => client ? client.definition(doc, pos) : null
        }),
        vscode.languages.registerDocumentSymbolProvider(selector, {
            provideDocumentSymbols: (doc) => client ? client.documentSymbol(doc) : null
        }),
        vscode.languages.registerRenameProvider(selector, {
            prepareRename: (doc, pos) => client ? client.prepareRename(doc, pos) : null,
            provideRenameEdits: (doc, pos, newName) => client ? client.rename(doc, pos, newName) : null
        }),
        vscode.languages.registerReferenceProvider(selector, {
            provideReferences: (doc, pos) => client ? client.references(doc, pos) : null
        }),
        vscode.languages.registerDocumentHighlightProvider(selector, {
            provideDocumentHighlights: (doc, pos) => client ? client.documentHighlight(doc, pos) : null
        }),
        vscode.languages.registerSignatureHelpProvider(selector, {
            provideSignatureHelp: (doc, pos) => client ? client.signatureHelp(doc, pos) : null
        }, '(', ','),
        vscode.languages.registerInlayHintsProvider(selector, {
            provideInlayHints: (doc, range) => client ? client.inlayHints(doc, range) : null
        }),
        vscode.languages.registerDocumentFormattingEditProvider(selector, {
            provideDocumentFormattingEdits: (doc, opts) => client ? client.formatting(doc, opts) : null
        }),
        vscode.languages.registerFoldingRangeProvider(selector, {
            provideFoldingRanges: (doc) => client ? client.foldingRanges(doc) : null
        }),
        vscode.languages.registerDocumentSemanticTokensProvider(selector, {
            provideDocumentSemanticTokens: (doc) => client ? client.semanticTokens(doc) : null
        }, new vscode.SemanticTokensLegend(
            ['function', 'method', 'property', 'variable', 'class', 'type', 'enumMember', 'macro'],
            ['declaration']
        )),
        vscode.languages.registerCodeActionsProvider(selector, {
            provideCodeActions: (doc, range, ctx) => checkRunner ? checkRunner.codeActions(doc, ctx.diagnostics) : []
        })
    );

    // CTML/CTCL:扩展侧本地补全与悬浮(注册表驱动;LSP 服务器只服务 ctron)
    for (const [lang, mod] of [['ctml', ctmlSupport], ['ctcl', ctclSupport]]) {
        const sel = { language: lang };
        context.subscriptions.push(
            vscode.languages.registerCompletionItemProvider(sel, {
                provideCompletionItems: (doc, pos) => {
                    try { return mod.complete(doc, pos); } catch (e) { log(`[${lang}] 补全失败: ${e.message}`); return null; }
                }
            }),
            vscode.languages.registerHoverProvider(sel, {
                provideHover: (doc, pos) => {
                    try { return mod.hover(doc, pos); } catch (e) { log(`[${lang}] hover 失败: ${e.message}`); return null; }
                }
            })
        );
    }

    // 已打开的 .ct 文档在激活时补发
    for (const ed of vscode.window.visibleTextEditors) {
        if (client) { client.handleOpen(ed.document); }
    }
}

function deactivate() {
    if (client) { client.dispose(); client = null; }
}

module.exports = { activate, deactivate };
