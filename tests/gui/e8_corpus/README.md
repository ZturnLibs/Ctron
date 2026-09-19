# e8_corpus —— GUI 诊断负例语料(W3 接入前 gated)

规范 §4.4 预留码的负例语料。W3(E8xxx 检查面进 ctc check)落地前,
这些文件在 check 口径**全数通过**(gui 块直通)——不是 bug,是排期。
W3 落地验收:每文件标注的 `gui expect:` 码逐一出现(run.sh 届时启用)。

| 文件 | 期待码 | 场景 |
|---|---|---|
| e8100_unknown_tag.neg.ct | E8100 | 未声明标签 |
| e8110_bind_type.neg.ct | E8110 | 绑定表达式类型不匹配 |
| e8130_color.neg.ct | E8130 | 颜色串非 #RRGGBB[AA] |
| e8150_cls_missing.neg.ct | E8150 | class 引用未导入样式 |
