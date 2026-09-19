# 示例

`examples/` 里有三个完整、可跑的 CLI 工具——它们同时是发布验收的考题。前置只有一个:可用的 `ctc`(安装见[入门](getting-started.md));三者构建形态相同,以 ctgrep 为例:`ctc build examples/ctgrep/src/main.ct`。它们共用同一入口模型:`ctron_entry()` 返回 CLI 单串,约定以字面 `run` 触发(`<二进制> run "<参数>"`),构建产物与解释执行行为一致。

| 示例 | 对标 | 语义口径 |
|---|---|---|
| ctgrep | grep | 固定串匹配(`grep -F -n`),行号 1 起,输出 `行号:内容`,退出码 0/1/2 = 命中/无命中/用法错误 |
| ctwc | wc | `行数 词数 字节数 路径`,行数与 `wc -l` 同口径 |
| ctwf | 词频统计 | 逐行 `词 次数`(插入序,确定性)+ 汇总行 + Top-3 |

## ctgrep —— 子串搜索

```bash
$ examples/ctgrep/src/main run "alpha sample.txt"
1:alpha beta
2:gamma alpha
```

首个空格分隔模式与路径——路径可含空格,模式不可;嵌套 `if` 定位首空格是刻意的(避开 `&&` 不短路的运行时已知坑)。核心循环(源码 26-48 行,缩进前移一级):

```ctron
match read_file(path) {
    Some(s) => {
        var ls = lines(s)
        var n: I32 = 0
        var hits: I32 = 0
        while n < ls.len {
            if contains(ls[n], pat) {
                var no = n + 1
                println(no.to_string() + ":" + ls[n])
                hits += 1
            }
            n += 1
        }
        if hits > 0 {
            return 0
        }
        return 1
    }
    None => {
        println("ctgrep: cannot open " + path)
        return 2
    }
}
```

## ctwc —— 行词字节计数

```bash
$ examples/ctwc/src/main run sample.txt
3 5 28 sample.txt
```

行数按换行字节计(`count_ch(s, 10)`),词按空白分隔,字节取 `s.len`——`main` 的有效主体只有三行(源码 14-16 行,缩进前移):

```ctron
var nw = words(s).len
var nl = count_ch(s, 10)
println(nl.to_string() + " " + nw.to_string() + " " + s.len.to_string() + " " + path)
```

## ctwf —— 词频统计

```bash
$ examples/ctwf/src/main run sample.txt
alpha 2
beta 1
gamma 1
nope 1
distinct=4|total=5|sample.txt
top:alpha=2,beta=1,gamma=1
```

词频行按首次插入序,输出确定;汇总行经 `std.str.join` 组装(锚定 join 的发射面);Top-3 用 `std.sort.sorted_by_desc` 对索引排序,计数比较经独立函数 `cnt_of` 完成——闭包形参走 32 位 ABI,直接捕获词表/值表两个 List 句柄即可(源码 56 行):

```ctron
var byidx = sorted_by_desc[I32](idxs, |a, b| cnt_of(ks, vs, vocab[a]) < cnt_of(ks, vs, vocab[b]))
```

三个示例都只消费 `std.str` / `std.fmap` / `std.sort` 的公开入口,源码各不到 80 行——当语言导读读,比读规范快。
