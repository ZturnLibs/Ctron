// Ctron LSP 客户端 —— 手写 JSON-RPC over stdio,零 npm 依赖。
// 协议子集:initialize / textDocument 同步(全量)/ publishDiagnostics /
//          hover / completion / definition / documentSymbol。
'use strict';

const vscode = require('vscode');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

let client = null;
let outputChannel = null;
let diagnosticCollection = null;

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
            diag.source = d.source || 'ctron-lsp';
            diags.push(diag);
        }
        diagnosticCollection.set(vscode.Uri.parse(params.uri), diags);
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
        diagnosticCollection.delete(doc.uri);
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

// ---------- 扩展入口 ----------

function activate(context) {
    outputChannel = vscode.window.createOutputChannel('ctron-lsp');
    diagnosticCollection = vscode.languages.createDiagnosticCollection('ctron');
    context.subscriptions.push(diagnosticCollection, outputChannel);

    function launch() {
        if (client) { client.dispose(); }
        client = new CtronLspClient(context);
        client.start();
    }

    context.subscriptions.push(vscode.commands.registerCommand('ctron.restartServer', launch));

    context.subscriptions.push(vscode.workspace.onDidOpenTextDocument((doc) => {
        if (client) { client.handleOpen(doc); }
    }));
    context.subscriptions.push(vscode.workspace.onDidChangeTextDocument((e) => {
        if (client) { client.handleChange(e.document); }
    }));
    context.subscriptions.push(vscode.workspace.onDidCloseTextDocument((doc) => {
        if (client) { client.handleClose(doc); }
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
        })
    );

    // 已打开的 .ct 文档在激活时补发
    for (const ed of vscode.window.visibleTextEditors) {
        if (client) { client.handleOpen(ed.document); }
    }
}

function deactivate() {
    if (client) { client.dispose(); client = null; }
}

module.exports = { activate, deactivate };
