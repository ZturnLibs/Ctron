<div align="center">

[English](README.md) | [简体中文](README.zh-CN.md) | [Deutsch](README.de.md) | [Français](README.fr.md) | [Español](README.es.md) | **日本語**

</div>

# Ctron

**AI ネイティブのシステムプログラミング言語** — 「AI が書き、人間がレビューする」ために設計されています。エラーはコンパイル時に捕捉され、コードは書かれた通りに正確に意味し、魔法は一切ありません。

```bash
ctron run main.ct      # 解釈実行 — 外部依存ゼロ
ctron check main.ct    # 静的検査 — --format=json で構造化診断を出力
ctron build main.ct    # 等価な C を出力 → ローカル cc → ネイティブ実行ファイル
```

- リポジトリ: <https://github.com/ZturnLibs/Ctron>
- ドキュメントサイト: <https://zturnlibs.github.io/Ctron/> (English / 简体中文)

---

> **プロジェクトの状況:** Ctron は開発中であり、**本番環境での使用はまだ推奨されません** — 言語とツールチェーンは今後も変化します。興味のある開発者との共同開発を歓迎します。詳しくは[コントリビューション](#コントリビューション)をご覧ください。

## なぜ Ctron なのか

AI が生成するコードには安定した失敗パターンがあります。**構造は正しいが、細部が間違っている** — 識別子の綴りミス、型の不一致、エッジケースの見落としです。動的言語ではこれらは実行時に表面化し、C/C++ では未定義動作に発展することもあります。Ctron の答えは「より賢い AI」ではなく、より厳格な言語設計です:

- **すべての診断に安定したエラーコードが付与されます**(E2010、E2030、E3040…)。コードさえ分かれば AI は的確に修正できる — 長文の説明から意図を推測する必要はありません。
- **構造化された診断**(`ctron check --format=json`)をエディタや CI パイプラインが直接取り込めます。
- **曖昧さのない文法と明示的な型推論規則** — 同じコードはどこでも同じ判定になります。

**魔法はありません。** AI が言語に恐れるのは複雑な構文ではなく、言語が陰で行うことです:

| Ctron にないもの | 代わりに使うもの |
|---|---|
| null なし | `Option[T]` |
| 暗黙の数値変換なし | 明示的な `as[T]()` |
| 演算子オーバーロードなし | `+` は加算を意味する |
| マクロなし | テキスト置換なし — ソースこそが真実 |
| 未定義動作なし | 仕様はすべての合法な挙動を列挙。書かれていないものは存在しない |

**すべてがソースです。** 標準ライブラリはコンパイル済みバイナリではなく Ctron のソースとして同梱され、`use std.*` はコンパイル時にソースを読み込んで、プログラムと単一の AST にマージします。AI は標準ライブラリのソースを読んで API の挙動を理解できます — ドキュメントは不要です。

## クイックスタート

```bash
# macOS / Linux ワンラインインストール(SHA256 検証付き)
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
export PATH="$HOME/.ctron/bin:$PATH"
```

```bash
$ ctron new hello && cd hello     # Ctron.toml + Ctron.ctcl + src/main.ct をスキャフォールド

$ ctron run src/main.ct           # 構文解析、検査、評価 — ひとつのパイプライン
hello, ctron

$ ctron check src/main.ct --format=json
{"diagnostics":[]}

$ ctron build src/main.ct         # 読める自己完結 C を出力し、ローカル cc を呼び出す
$ ./src/main
hello, ctron
```

`run` / `check` / `new` に C ツールチェーンは不要で、ローカル cc が必要なのは `build` だけです(`CC` 環境変数で上書き可)。Windows では `run` / `check` は追加設定なしで動作し、`build` には mingw-w64 が必要です。終了コード:`0` 成功 / `1` プログラムの診断に失敗 / `2` 環境または使用方法のエラー。

## 言語をひと目で

null なし、`Result` 伝播、エラーチェーン、言語内テスト:

```ctron
@derive(Error)
enum MathErr {
    DivByZero
}

fn safe_div(a: I32, b: I32) -> Result[I32, MathErr] {
    if b == 0 { return Err(DivByZero) }
    return Ok(a / b)
}

fn ratio(a: I32, b: I32, c: I32) -> Result[I32, MathErr] {
    let x = safe_div(a, b)?                   // `?` が伝播し、文脈を付与する
    return safe_div(x, c)
}

test "result propagation with ?" {
    assert_eq(ratio(100, 5, 4).or(-1), 5)
    match ratio(1, 0, 2) {
        Err(DivByZero) => assert(true)
        Ok(_)          => assert(false)
    }
}

test "error context chain" {
    let res = ratio(1, 0, 1).context("computing ratio")
    match res {
        Err(e) => {
            assert_eq(e.message, "computing ratio")
            assert(e.cause.is_some())          // cause チェーンが根本原因を保持する
        }
        Ok(_) => assert(false)
    }
}
```

実際の、完全なツール(`examples/ctwc`、wc 風ユーティリティ):

```ctron
use std.str.{words, count_ch}

fn main() -> I32 {
    var path = ctron_entry()
    if path == "" {
        println("ctwc: usage: ctwc run <file>")
        return 1
    }
    match read_file(path) {
        Some(s) => {
            var nw = words(s).len
            var nl = count_ch(s, 10)
            println(nl.to_string() + " " + nw.to_string() + " " + s.len.to_string() + " " + path)
        }
        None => {
            println("ctwc: cannot open " + path)
            return 1
        }
    }
    return 0
}
```

さらに先へ:

- **構造化並行性** — タスクは `scope` / `spawn` / `join` の下で生存し、その寿命はブロックスコープに束縛されます。キャンセルはスコープに沿って伝播し、パニックしたタスクは構造化キャンセルブロードキャストを引き起こします。3 つの `Send` チェックポイントはすべてコンパイル時に実行されます。
- **2 層のメモリ管理** — GC の上に、`own` ブロック(スコープ内で GC メモリを排除したサブセット)と `Arena` が用意されています。`bare` プロファイルは GC アロケーションを完全に拒否し、ベアメタルのマイクロコントローラをターゲットにします。
- **コンパイル時実行** — `const` / `comptime fn` はコンパイル時に評価されます。`@derive(Show, Eq)` はよく使われる impl を合成し、`///` ドキュメントコメント内のコードブロックはコンパイル・実行されます(doc-test)。
- **明示的なケイパビリティ** — I/O 権限はパッケージマニフェストで宣言された明示的な `Cap` オブジェクトです。権限の越境はコンパイル時に失敗します。
- **第一級の FFI** — `extern "c"` 宣言に加えて、`c_src/*.c` が自動的にリンクされます。

## 2 つの実行パス

```
ctron run main.ct      interpret: zero external dependencies, fastest dev loop
ctron build main.ct    emit C → local cc: native executable, same language end to end
```

出力される成果物は**読める、自己完結した C**(ランタイムは完全にインライン化、システムヘッダのみ)で、監査のためにソースの隣に置かれます。ひとつのソースコードベースで開発から配置まで賄えます。

## 標準ライブラリとドメインパッケージ

標準ライブラリはツールチェーンとともに**ソースとして**同梱されます。ドメインパッケージはトップレベルの名前空間に接続します:`use std.*` / `use net.*` / `use gui.*`。

| パッケージ | 内容 |
|---|---|
| `std` | 26 モジュール:json / csv / uuid / crypto(SHA-256、HMAC、PBKDF2)/ fmap / heap / iter / sort / unicode / strconv / rand / path / time… |
| `net` | TCP トランスポートファサード:listen / connect / accept / read / write |
| `http` | HTTP/1.1 パーサ + クライアント + WebSocket + SSE + **Web フレームワークミドルウェアファミリー**(router / JWT auth / cors / csrf / openapi / metrics / timeout / static…) |
| `tls` | TLS ファサード |
| `db` | 純 Ctron のワイヤプロトコルドライバ:**PostgreSQL** / RESP2(Redis)+ コネクションプール + 行マッピング |
| `ffi` | C 境界でのエラーラッピング(errno を `Result` に畳み込み) |

## ツールチェーンをひと目で

`ctron` はユーザーにとっての唯一のエントリポイントです:

| コマンド | 目的 |
|---|---|
| `run` / `check` / `build` | 解釈実行 / 静的検査(JSON 診断)/ 出力してビルド |
| `test` | 言語内 `test` ブロックの実行 |
| `fmt` | 正準フォーマッタ |
| `lint` | 診断サマリー(`--strict` で警告をエラーに昇格) |
| `doc` | インターフェース射影:公開シンボルテーブル + 契約コメント |
| `bench` | ベンチマークファミリー:lang / gc / http / net / ffi |
| `new` | プロジェクトのスキャフォールディング |
| `add` / `publish` / `lock` | パッケージのインストール / 公開 / ロックファイル |

## エディタサポート

- **言語サーバ**(`lsp/`)— 診断、documentSymbol、ホバー、補完、定義/参照、リネーム、シグネチャヘルプ、インレイヒント、フォーマット、フォールディング。
- **VS Code 拡張**(`editors/vscode-ctron`)— シンタックスハイライト、スニペット、保存時のセマンティック診断とクイックフィックス、セマンティックトークン、パラメータ名インレイヒント。CTML(GUI マークアップ)と CTCL(設定言語)にも対応しています。

## 現状と限界

- 最初のリリース **v0.0.1** は公開済みです。言語仕様は凍結ドラフト(v0.8)です。ダウンロード経路が利用できない場合は、ソースからビルドしてください — [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md) を参照。
- 向いている用途:CLI ツール、ネットワークサービス、コンパイラ、パーサ。
- 向いていない用途:GPU を多用する計算。ハードリアルタイムシステム(GC の一時停止は制御できません — `bare` プロファイルで緩和はできるものの、排除はできません)。
- `build` と Windows サポートは全体として β です。

## コントリビューション

issue、サンプル、ドキュメント、コード、いずれも歓迎します — ほとんどのコントリビューションにコンパイラ内部の知識は必要ありません。

**バグ報告** — 最小限の `.ct` 再現コードと `ctron check <file> --format=json` の出力を添えて issue を起票してください。診断には安定したエラーコードが付与されているため、修正は的確に行えます。

**エコシステムを育てる** — 標準ライブラリのモジュール、サンプル、エディタツール、ドキュメント、ウェブサイト:PR を歓迎します。

**言語の進化** — 仕様は凍結ドラフトです。まず issue で設計ディスカッションを始めてください。承認された変更は、仕様リビジョンのエントリと `tests/` へのコーパス固定として取り込まれます。言語の挙動は、適合スイートを仲裁者として、3 つの実装すべて(C シード / Rust リファレンス / セルフホストコンパイラ)で一致を保たなければなりません。新しい診断には、仕様のエラーコードレジストリに登録された安定したエラーコードが必要です。

**開発ワークフロー**

```bash
git clone https://github.com/ZturnLibs/Ctron && cd Ctron
bash ci.sh    # ひとつのコマンドで:ツールチェーンをブートストラップし、受け入れゲートをすべて実行(ctron gate の 21 ステップ;ctron.ctcl を参照)
```

PR は `ci.sh` がグリーンの状態で取り込んでください。`ctron`(sh)と `ctron.ps1` / `ctron.cmd` の挙動は同一に保ってください。コミットメッセージは conventional commits(`feat:` / `fix:` / `docs:`)に従います。

## ドキュメントマップ

| ドキュメント | 内容 |
|---|---|
| [docs/spec/](docs/spec/README.md) | 言語仕様 v0.8(12 章) |
| [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md) | ツールチェーンのソースからのビルド |
| [examples/](examples/) | 完全なサンプルアプリケーション |
| [website/](https://zturnlibs.github.io/Ctron/) | はじめに、標準ライブラリリファレンス、サンプル、ダウンロード |

## ライセンス

MIT — [LICENSE](LICENSE) を参照。
