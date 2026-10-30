# 构建报告:native.sh 自宿链于 HEAD 红(t_remove 未定义)

- 日期:2026-10-30(loom 侧相邻仓自动化演练发现)
- 复现:`git worktree add /tmp/wt b42e21cc --detach && cd /tmp/wt/compiler-c && make && cd ../compiler && sh native.sh`
- 症状:cc 链接臂 5 error——`call to undeclared function 't_remove'`(ctron_cc.c 内 4 处调用点,List.remove 发射体无定义)
- 范围:d96da3ba(容器臂重装)起,含 b42e21cc(T25 v2 收口——用户面收口 ≠ 编译器自身代码的 List.remove 自宿发射)
- 推断:编译器自身源使用了容器运行时(List.remove),T25 v2 收的是用户代码路径;自宿闭环(native.sh)不在测试矩阵 = 缺口
- 影响:自宿链断 = bin/ 无法从源重建(存量 bin 仍可用);下游 loom 钉 = 停在最后一个可自宿点(bfe5c621)
- 建议:补「native.sh 全链」入测试矩阵(commit message 已有 self-host 语义,缺执行面);T25 v2 或后续提交补 t_remove 发射定义
- 备注:loom 侧 gate-all(64 检 + e2e 六连)= 事实上比自宿更严的下游验证面,可作回归参照
- (2026-10-29 的 bin 重建告知:原落 docs/ledger/ 已随目录重组消失,重述要点 = compiler/bin 三件曾被 loom 侧 worktree 构建覆盖,已按 609c080f 态 native.sh 重建,幂等可复现)
