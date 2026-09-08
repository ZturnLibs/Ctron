#!/usr/bin/env python3
"""genmod.py —— Ctron 自举工具链: 生成换靶后的 cc 编译器实例。

用法:
  python3 tools/genmod.py <输入程序.ct> <输出模块.ct> [--count]

功能:
  读取 selfhosted/cc.ct(Ctron 写的编译器单文件快照),把其中 read_file 的
  输入锚(../selfhosted/input_cc.ct)替换为指定输入,写出可被宿主 seed
  (compiler-c/build/ctronc)解释执行的模块。

  --count: 额外改造为 decl 计数模式 —— parse 后打印全部 decl 清单并止,
  不运行程序(用于自编译检查阶梯的 decl 计数对照)。
"""
import sys

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    inp, out = sys.argv[1], sys.argv[2]
    count = '--count' in sys.argv[3:]
    trans = '--trans' in sys.argv[3:]

    anchor = '../selfhosted/input_cc.ct'
    import os
    cc_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir, 'cc.ct')
    cc = open(cc_path).read()
    if anchor not in cc:
        print('genmod: cc.ct 缺少输入锚', file=sys.stderr)
        sys.exit(2)
    mod = cc.replace(anchor, inp)

    if trans:
        import os as _os
        part_path = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), 'trans_part.ct')
        cut = mod.index('fn main() -> I32 {')
        part = open(part_path).read()
        mod = mod[:cut] + part
        mod = mod.replace('ANCHORINPUT', inp)

    if count:
        sems_line = '            var sems = sem_walk2(file)'
        fd_line = '            var d = find_decl(file, "main")'
        ln = mod.index(sems_line)
        fd = mod.index(fd_line)
        dump = (
            '            var dbgs = ""\n'
            '            var di: I32 = 1\n'
            '            while di < file.len {\n'
            '                var d = file[di]\n'
            '                dbgs = dbgs + di.to_string() + " " + d[0] + " " + d[1] + " NL "\n'
            '                di += 1\n'
            '            }\n'
            '            print(dbgs)\n'
            '            println("selfcheck: parse+sem OK decls=" + (file.len - 1).to_string())\n'
            '            return 0\n'
        )
        mod = mod[:ln] + dump + mod[mod.index('\n', fd) + 1:]

    open(out, 'w').write(mod)

if __name__ == '__main__':
    main()
