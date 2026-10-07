# ctron.ps1 —— Ctron 工具链用户驱动(v0.0.1;Windows 版,命令面基准 = sh 版 ctron,spec §7.9 驱动 conformance)
# rc 约定:0 成功 / 1 程序诊断失败 / 2 ctron 环境或用法错误(exit code 同 sh)
# 注意:本文件必须保持 UTF-8 with BOM —— Windows PowerShell 5.1 对无 BOM 脚本按 ANSI 解码,中文帮助文本会乱码
# 重定向/管道下 powershell.exe 的 stdout 默认按 OEM CP 编码,中文帮助/诊断会变 '?',子进程输出解码同样失真;显式收口为 UTF8
try { [Console]::OutputEncoding = [Text.Encoding]::UTF8 } catch {}
$ErrorActionPreference = 'Stop'
$Bin = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Split-Path -Parent $Bin
$DevRoot = $Bin
if (-not (Test-Path (Join-Path $Bin 'ctron-cc.exe')) -and (Test-Path (Join-Path $Bin 'compiler/bin/ctron-cc.exe'))) {
	$Bin = Join-Path $Bin 'compiler/bin'
}
if ($env:CC) { $Cc = $env:CC } else { $Cc = 'gcc' }
$global:LASTEXITCODE = 0

function Usage {
@'
ctron —— Ctron 工具链驱动
用法:
  ctron run <file.ct>          解释执行(不需要 C 编译器)
  ctron check <file.ct> [--format=json] [--profile=bare]
                             静态检查
  ctron doc <file.ct> [--format=json]
                             iface 投影:pub 符号表 + trait/impl 面 + 契约注释
  ctron build <file.ct>        发射 C → 本机 cc → 可执行 <stem>(C 侧 <stem>.c)
  ctron build                  项目模式:读 Ctron.toml(入口 src/main.ct,链接 c_src/*.c)
  ctron test <file.ct>         test 块执行(无 main 走解释;含 main 文件的 test 执行挂账)
  ctron fmt <file|pkg目录> [-w|--check]
                             规范格式化(docs/fmt-spec.md):默认打印;-w 原位写回;
                             --check 列出待格式化文件后非零退出
  ctron lint <file|pkg目录> [--trusted] [--strict]
                             lint 汇总:错误红(rc=1),警告默认只汇总;
                             --trusted 信任审计枚举(§9.6);--strict 警告也红
  ctron bench [族]            基准族入口(lang/gc/http/net/ffi;dev 布局)
  ctron add <名>[@<版本约束>] 装包:写 dep 块 + pkgs/ 安装 + 刷新 Ctron.lock(T49 本地 registry)
  ctron publish               发布当前目录单文件包到本地 registry(T49;CTRON_REGPATH,缺省 ~/.ctron/registry)
  ctron lock                  解析依赖并生成/校验 Ctron.lock(内容寻址)
  ctron pkg verify <artifact-dir>
                             密封工件(.ctart)摘要自洽校验(D8-2 L1):SHA256SUMS 逐成员重算
                             + self_digest 重算;fail-closed,不过即 rc=1;--deep 走
                             in-language 发射验证器(ctron-verify,C 速,L3 载体)
  ctron pkg trace record <src.ct> --case <fn>=<args 字面量> [--case ...]
                             黄金轨迹录制(S3-α 纯函数):值=to_string 规范形,落
                             traces/<stem>.ctrt;含 main 源不支持
  ctron pkg trace replay <src.ct>
                             轨迹复放:逐字比对,不符即 rc=1 点名双值(S3-α)
  ctron new <dir>              脚手架:hello + Ctron.toml + Ctron.ctcl
  ctron --version              版本
  ctron --help | help [cmd]    帮助(亦可 ctron <cmd> --help)
环境变量:CC(默认 gcc)、CTRON_STDPATH(覆盖标准库位置)
rc 约定:0 成功 / 1 程序诊断失败 / 2 ctron 环境或用法错误
'@
}

function Help-Cmd($c) {
	if ($c -eq 'build') {
@'
ctron build —— 发射 C 并编译为可执行
  ctron build <file.ct>   产物 <stem>.exe 与 <stem>.c(与源同目录)
  ctron build             项目模式:入口 src/main.ct,产物 build/<name>.exe;
                        Ctron.toml 声明的 c_src/*.c 一并链接
前置:mingw-w64(MSYS2 或 w64devkit);缺失时 C 照常发射,并给出两条出路
'@
	} elseif ($c -eq 'check') {
		Write-Output 'ctron check <file.ct> [--format=json] [--profile=bare] —— 静态检查,不改任何文件'
	} elseif ($c -eq 'doc') {
		Write-Output 'ctron doc <file.ct> [--format=json] —— iface 投影:pub 符号表/trait/impl 面 + 模块头与 per-fn 契约注释(§5.3),不改任何文件'
	} elseif ($c -eq 'run') {
		Write-Output 'ctron run <file.ct> —— 解释执行;不需要 C 编译器'
	} elseif ($c -eq 'test') {
		Write-Output 'ctron test <file.ct> —— 执行 test 块(无 main 文件);含 main 文件的 test 执行暂走宿主口径挂账'
	} elseif ($c -eq 'fmt') {
		Write-Output 'ctron fmt <file|pkg目录> [-w|--check] —— 规范格式化(docs/fmt-spec.md);默认打印,-w 原位写回,--check 列出待格式化并不零退出'
	} elseif ($c -eq 'lint') {
		Write-Output 'ctron lint <file.ct|pkg目录> [--trusted] [--strict] —— check 命令化+汇总(§9.7):错误 rc=1;警告默认只汇总不红,--strict 才红;--trusted 信任边界审计枚举 #[trusted] extern(§9.6)'
	} elseif ($c -eq 'bench') {
		Write-Output "ctron bench [族] —— tests/ 基准族统一入口(T43;dev 布局)`n  lang   own/热点算术核 vs C 同构对照(T44;门 ≤1.05,WARN 档)`n  gc     GC 档 vs bump 档 churn(T32;门 ≤1.15)`n  http   http_parse_head vs picohttpparser(CTRON_HTTP_BENCH=1 启用)`n  net    吞吐/协程切换三门禁(CTRON_NET_BENCH=1 启用)`n  ffi    FFI 边界微基准四场景(报告制)`n无族名列出注册表;未注册族名清晰诊断(fail-closed)。"
	} elseif ($c -eq 'add') {
		Write-Output 'ctron add <name>[@<版本约束>] —— 本地 registry 装包(T49):写 dep 块 + pkgs/<名>/ 安装(module+manifest)+ ctron-dep 刷新 Ctron.lock;registry = CTRON_REGPATH 或 ~/.ctron/registry;零网络'
	} elseif ($c -eq 'publish') {
		Write-Output 'ctron publish —— 当前目录单文件包发布到本地 registry(T49):manifest+module+sha256 三件落 <reg>/<名>/<版>/;版本不可覆盖;零网络(对外发布面启用仍须经用户确认)'
	} elseif ($c -eq 'lock') {
		Write-Output 'ctron lock —— 依赖解析并生成/校验 Ctron.lock(T49 内容寻址;= ctron-dep run Ctron.ctcl)'
	} elseif ($c -eq 'pkg') {
		Write-Output 'ctron pkg verify <artifact-dir> [--deep] —— 密封工件摘要自洽校验(D8-2 L1):SHA256SUMS 逐成员重算 + self_digest=sha256(SHA256SUMS 字节) 重算比对;缺 meta/SHA256SUMS/impl 或任一成员不符即 fail-closed rc=1;--deep = ctron-verify 发射验证器(in-language 重算,L3 载体);trace 复放腿归 S3'
	} elseif ($c -eq 'new') {
		Write-Output 'ctron new <dir> —— 生成 <dir>/Ctron.toml + Ctron.ctcl + src/main.ct(hello)'
	} else { Usage }
}

function Have-Cc {
	if (-not (Get-Command $Cc -ErrorAction SilentlyContinue)) { return $false }
	$probe = Join-Path ([System.IO.Path]::GetTempPath()) ("ctronp" + [guid]::NewGuid().ToString('N'))
	Set-Content -Path "$probe.c" -Value 'int main(void){return 0;}' -NoNewline
	try { & $Cc -O0 -w -o "$probe.exe" "$probe.c" 2>$null | Out-Null } catch { }
	$ok = ($LASTEXITCODE -eq 0) -and (Test-Path "$probe.exe")
	if (Test-Path "$probe.exe") { Remove-Item "$probe.exe" }
	if (Test-Path "$probe.c") { Remove-Item "$probe.c" }
	return $ok
}

function Cc-Missing($cfile) {
	[Console]::Error.WriteLine("ctron: 未找到可用的 C 编译器('$Cc' 冒烟失败) - ctron build 需要。")
	[Console]::Error.WriteLine("  ① 安装后重跑:MSYS2 'pacman -S mingw-w64-x86_64-gcc' 或 w64devkit")
	[Console]::Error.WriteLine("  ② C 已发射到 $cfile - 可手动编译,或拿到任何有 gcc 的机器上")
}

function Build-File($f) {
	$full = (Resolve-Path $f).Path
	$dir = Split-Path -Parent $full
	$leaf = Split-Path -Leaf $full
	if ($leaf.EndsWith('.ct')) { $stem = $leaf.Substring(0, $leaf.Length - 3) } else { $stem = $leaf }
	# 产物不经管道字符串化,直接以无 BOM UTF8 写行(PS5.1 的 Set-Content -Encoding Ascii 会把非 ASCII 串打成 '?')
	$tmpOut = & (Join-Path $Bin 'ctron-emit.exe') run $full
	[IO.File]::WriteAllText((Join-Path $dir "$stem.c"), ($tmpOut -join "`n") + "`n", (New-Object Text.UTF8Encoding($false)))
	if ($LASTEXITCODE -ne 0) { exit 1 }
	if (-not (Have-Cc)) { Cc-Missing (Join-Path $dir "$stem.c"); exit 2 }
	Push-Location $dir
	& $Cc -O2 -w -pthread "-Wl,--stack,8388608" "$stem.c" -o "$stem.exe"
	$rc = $LASTEXITCODE
	Pop-Location
	if ($rc -ne 0) { exit 2 }
	Write-Output "ctron: 已构建 $(Join-Path $dir $stem).exe"
}

function Build-Proj {
	if (-not (Test-Path 'Ctron.toml')) { [Console]::Error.WriteLine('ctron: 项目模式需 Ctron.toml'); exit 2 }
	$m = Select-String -Path Ctron.toml -Pattern '^name\s*=\s*"(.*)"' -CaseSensitive | Select-Object -First 1
	$name = $null
	if ($m) { $name = $m.Matches[0].Groups[1].Value }
	if (-not $name) { $name = Split-Path -Leaf (Get-Location).Path }
	if (-not (Test-Path 'src/main.ct')) { [Console]::Error.WriteLine('ctron: 缺入口 src/main.ct'); exit 2 }
	New-Item -ItemType Directory -Force -Path build | Out-Null
	$tmpOut = & (Join-Path $Bin 'ctron-emit.exe') run (Resolve-Path 'src/main.ct').Path
	[IO.File]::WriteAllText((Join-Path (Get-Location).Path "build/$name.c"), ($tmpOut -join "`n") + "`n", (New-Object Text.UTF8Encoding($false)))
	if ($LASTEXITCODE -ne 0) { exit 1 }
	if (-not (Have-Cc)) { Cc-Missing "build/$name.c"; exit 2 }
	$srcs = @(Get-ChildItem -Path 'c_src/*.c' -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
	& $Cc -O2 -w -pthread "-Wl,--stack,8388608" "build/$name.c" @srcs -o "build/$name.exe"
	if ($LASTEXITCODE -ne 0) { exit 2 }
	Write-Output "ctron: 已构建 build/$name.exe"
}

# T43 lint(§9.7):check 命令化+汇总。口径与 sh 版同文:错误(E####)红 rc=1;
# 警告(W####)默认只汇总不红;--trusted 透传信任审计;--strict 警告也红。
function Lint-Target($t, $trusted, $strict) {
	$files = @()
	if (Test-Path $t -PathType Container) {
		$dir = $t
		if (Test-Path (Join-Path $t 'src') -PathType Container) { $dir = Join-Path $t 'src' }
		$files = @(Get-ChildItem -Path (Join-Path $dir '*.ct') -File | Sort-Object -Property Name | ForEach-Object { $_.FullName })
		if ($files.Count -eq 0) { [Console]::Error.WriteLine("ctron: lint 目录无 .ct 文件: $t"); exit 2 }
	} elseif (Test-Path $t) {
		$files = @((Resolve-Path $t).Path)
	} else {
		[Console]::Error.WriteLine("ctron: lint 目标不存在: $t"); exit 2
	}
	$tmp = [System.IO.Path]::GetTempFileName()
	$totE = 0; $totW = 0; $nf = 0; $hard = 0
	foreach ($f in $files) {
		$nf++
		if ($nf -gt 1) { Write-Output "-- $f" }
		$chkArgs = @('run', $f)
		if ($trusted) { $chkArgs += '--trusted' }
		& (Join-Path $Bin 'ctron-chk.exe') @chkArgs > $tmp 2>&1
		$chkRc = $LASTEXITCODE
		$lines = @(Get-Content $tmp)
		$es = @($lines | Where-Object { $_ -match '^E\d{4}' })
		$ws = @($lines | Where-Object { $_ -match '^W\d{4}' })
		$lines | Where-Object { $_ -notmatch '^check OK' } | Write-Output
		if (($chkRc -ge 2) -and ($es.Count -eq 0)) { $hard++ }
		$totE += $es.Count; $totW += $ws.Count
	}
	Remove-Item $tmp -ErrorAction SilentlyContinue
	Write-Output "lint: $nf 文件,$($totE + $hard) 个错误 / $totW 警告"
	if (($totE + $hard) -gt 0) { exit 1 }
	if ($strict -and ($totW -gt 0)) { exit 1 }
	exit 0
}

# T43 bench(§9.7):tests/ 基准族统一入口。注册表 fail-closed(同 sh 版;同 ctc.sh targets 哲学)。
function Bench-Cmd($fam) {
	if (-not $fam) {
		Write-Output 'ctron bench —— 基准族注册表(§9.4 性能口径;脚本内含门禁与归因)'
		Write-Output '  lang   own/热点算术核 vs C 同构对照(T44;门 ≤1.05,WARN 档)'
		Write-Output '  gc     GC 档 vs bump 档 churn(T32;门 ≤1.15)'
		Write-Output '  http   http_parse_head vs picohttpparser(CTRON_HTTP_BENCH=1 启用)'
		Write-Output '  net    吞吐/协程切换三门禁(CTRON_NET_BENCH=1 启用)'
		Write-Output '  ffi    FFI 边界微基准四场景(报告制)'
		Write-Output '用法: ctron bench <族>'
		exit 0
	}
	$map = @{ 'lang' = 'tests/lang/bench/bench.sh'; 'gc' = 'tests/gc/bench.sh'; 'http' = 'tests/http/bench/bench.sh'; 'net' = 'tests/net/bench/bench.sh'; 'ffi' = 'compiler/test/bench_ffi.sh' }
	if (-not $map.ContainsKey($fam)) {
		[Console]::Error.WriteLine("ctron: 未注册基准族: $fam(注册表: lang gc http net ffi;ctron bench 看详)"); exit 2
	}
	# 路径基 = DevRoot(回落前的脚本目录):dev=仓库根;Root 是装机语义 exe/../
	$s = Join-Path $DevRoot ($map[$fam] -replace '/', [IO.Path]::DirectorySeparatorChar)
	if (-not (Test-Path $s)) {
		[Console]::Error.WriteLine("ctron: 基准族脚本缺席: $s(基准族仅 dev 布局随发;装机形态未含)"); exit 2
	}
	& sh $s
	exit $LASTEXITCODE
}

# T43 add/publish(§9.7):骨架。registry 未接(T49 前置未到)——只做参数校验+
# 出路指引,零写入零网络(fail-closed;对外发布面启用须经用户确认)。
function Add-Validate($spec) {
	$name = $spec.Split('@')[0]
	$req = ''
	if ($spec.Contains('@')) { $req = $spec.Substring($spec.IndexOf('@') + 1) }
	if ($name -notmatch '^[A-Za-z_][A-Za-z0-9_-]*$') {
		[Console]::Error.WriteLine("ctron: add: 依赖名非法: '$name'(形: <name>[@<版本约束>],name = [A-Za-z_][A-Za-z0-9_-]*)"); exit 2
	}
	if (($req -ne '') -and ($req -notmatch '^[A-Za-z0-9.^~<>=,*_-]+$')) {
		[Console]::Error.WriteLine("ctron: add: 版本约束非法: '$req'(semver 约束形,T49 实装)"); exit 2
	}
}

# T50/S3-α:pkg trace record/replay(与 sh 版同文;值=to_string 规范形,轨迹=
# traces/<stem>.ctrt CTCL 键控块;宿主 ctron-cc;效果函数/零参与加载期拒载归 S3-β)
function Cmd-PkgTrace($trest) {
	$cc = Join-Path $Bin 'ctron-cc.exe'
	if (-not (Test-Path $cc)) { $cc = Join-Path $Bin 'ctron-cc' }
	if (-not (Test-Path $cc)) { [Console]::Error.WriteLine('ctron: pkg trace: 缺 ctron-cc(重装工具链)'); exit 2 }
	if ($trest.Count -lt 1) { [Console]::Error.WriteLine('ctron: pkg trace 需要子命令(用法: pkg trace record <src.ct> --case <fn>=<args> [--case ...] / pkg trace replay <src.ct>)'); exit 2 }
	$tsub = $trest[0]
	if ($tsub -ne 'record' -and $tsub -ne 'replay') { [Console]::Error.WriteLine("ctron: pkg trace: 未知子命令 '$tsub'(现支持: record replay)"); exit 2 }
	$tsrc = ''; $tcases = @()
	$ti = 1
	while ($ti -lt $trest.Count) {
		$ta = $trest[$ti]
		if ($ta -eq '--case') { $ti++; if ($ti -ge $trest.Count) { [Console]::Error.WriteLine('ctron: pkg trace: --case 缺值'); exit 2 }; $tcases += $trest[$ti] }
		elseif ($ta.StartsWith('--case=')) { $tcases += $ta.Substring(7) }
		elseif ($tsrc -eq '') { $tsrc = $ta }
		$ti++
	}
	if ($tsrc -eq '') { [Console]::Error.WriteLine("ctron: pkg trace $tsub 需要源文件"); exit 2 }
	if (-not (Test-Path $tsrc -PathType Leaf)) { [Console]::Error.WriteLine("ctron: pkg trace: 源文件不存在: $tsrc"); exit 2 }
	$tsrcText = [IO.File]::ReadAllText((Resolve-Path $tsrc).Path)
	if ($tsrcText -match 'fn main') { [Console]::Error.WriteLine('ctron: pkg trace: 源含 main(轨迹面向无 main 的 provider 纯函数源)'); exit 2 }
	$tdir = Split-Path -Parent (Resolve-Path $tsrc).Path
	$tstem = [IO.Path]::GetFileNameWithoutExtension($tsrc)
	$ttgt = Join-Path $tdir "traces/$tstem.ctrt"
	$enc = New-Object Text.UTF8Encoding($false)
	if ($tsub -eq 'record') {
		if ($tcases.Count -lt 1) { [Console]::Error.WriteLine('ctron: pkg trace record 需要 --case <fn>=<args>(可多例)'); exit 2 }
		New-Item -ItemType Directory -Force -Path (Join-Path $tdir 'traces') | Out-Null
		$tblocks = @()
		foreach ($te in $tcases) {
			$tfn = $te.Split('=')[0]
			$targs = $te.Substring($tfn.Length + 1)
			if ($tfn -notmatch '^[A-Za-z_][A-Za-z0-9_]*$') { [Console]::Error.WriteLine("ctron: pkg trace record: fn 名非法: '$tfn'([A-Za-z_][A-Za-z0-9_]*)"); exit 2 }
			$tdrv = Join-Path ([IO.Path]::GetTempPath()) ('ctron_tr_' + [guid]::NewGuid().ToString('N') + '.ct')
			[IO.File]::WriteAllText($tdrv, $tsrcText + "`nfn main() {`n    println($tfn($targs).to_string())`n}`n", $enc)
			$tout = & $cc run $tdrv 2> ($tdrv + '.err')
			$trc = $LASTEXITCODE
			if ($trc -ne 0) { Get-Content ($tdrv + '.err') | [Console]::Error.WriteLine; Remove-Item $tdrv, "$tdrv.err" -ErrorAction SilentlyContinue; [Console]::Error.WriteLine("ctron: pkg trace record: 用例 $tfn($targs) 运行失败(rc=$trc)"); exit 2 }
			if (@($tout).Count -gt 1) { Remove-Item $tdrv, "$tdrv.err" -ErrorAction SilentlyContinue; [Console]::Error.WriteLine("ctron: pkg trace record: 用例 $tfn($targs) 输出多行(α 轨迹面向单值纯函数)"); exit 2 }
			$texp = (@($tout)[0] -replace '"', '\"')
			$targsE = $targs -replace '"', '\"'
			$tblocks += "trace `"$tfn`" {`n  args = `"$targsE`"`n  expect = `"$texp`"`n}"
			Remove-Item $tdrv, "$tdrv.err" -ErrorAction SilentlyContinue
		}
		[IO.File]::WriteAllText($ttgt, ($tblocks -join "`n") + "`n", $enc)
		Write-Output "ctron: 轨迹已录制 $($tcases.Count) 用例 → $ttgt"
		exit 0
	}
	if (-not (Test-Path $ttgt -PathType Leaf)) { [Console]::Error.WriteLine("ctron: pkg trace replay: 无轨迹文件: $ttgt"); exit 2 }
	$ttext = [IO.File]::ReadAllText($ttgt)
	$tms = [regex]::Matches($ttext, 'trace "([^"]*)" \{\r?\n  args = "((?:[^"\\]|\\.)*)"\r?\n  expect = "((?:[^"\\]|\\.)*)"\r?\n\}')
	if ($tms.Count -lt 1) { [Console]::Error.WriteLine("ctron: pkg trace replay: 轨迹文件无块: $ttgt"); exit 2 }
	$tbad = 0; $tn = 0
	foreach ($tm in $tms) {
		$tfn = $tm.Groups[1].Value
		$targs = $tm.Groups[2].Value -replace '\\"', '"'
		$texp = $tm.Groups[3].Value -replace '\\"', '"'
		$tdrv = Join-Path ([IO.Path]::GetTempPath()) ('ctron_tr_' + [guid]::NewGuid().ToString('N') + '.ct')
		[IO.File]::WriteAllText($tdrv, $tsrcText + "`nfn main() {`n    println($tfn($targs).to_string())`n}`n", $enc)
		$tout = & $cc run $tdrv 2> ($tdrv + '.err')
		$trc = $LASTEXITCODE
		if ($trc -ne 0) { Get-Content ($tdrv + '.err') | [Console]::Error.WriteLine; Remove-Item $tdrv, "$tdrv.err" -ErrorAction SilentlyContinue; [Console]::Error.WriteLine("ctron: pkg trace replay: 用例 $tfn($targs) 运行失败(rc=$trc)"); exit 2 }
		$tact = @($tout)[0]
		if ($tact -ne $texp) { [Console]::Error.WriteLine("trace mismatch: $tfn($targs) 记录 $texp 实际 $tact"); $tbad++ }
		Remove-Item $tdrv, "$tdrv.err" -ErrorAction SilentlyContinue
		$tn++
	}
	if ($tbad -gt 0) { [Console]::Error.WriteLine("pkg trace replay: $tn 用例中 $tbad 不符"); exit 1 }
	Write-Output "trace replay OK: $tn 用例"
	exit 0
}

if ($args.Count -lt 1) { Usage; exit 2 }
$cmd = $args[0]; $rest = @($args | Select-Object -Skip 1)
# <cmd> --help / <cmd> -h:子命令详助入口(usage 宣传的第四帮助入口),先于各分派臂拦截(同 sh 版)
if ($cmd -in 'run','check','build','test','new','fmt','doc','lint','bench','add','publish','lock','pkg' -and $rest.Count -ge 1 -and $rest[0] -in '--help','-h') {
	Help-Cmd $cmd; exit 0
}
switch ($cmd) {
	{ $_ -in '--help','-h','help' } { if ($rest.Count -ge 1) { Help-Cmd $rest[0] } else { Usage }; exit 0 }
	{ $_ -in '--version','-V' } {
		$vfile = Join-Path $Root 'VERSION'
		if (Test-Path $vfile) { $v = (Get-Content $vfile -Raw).Trim(); Write-Output "ctron $v" } else { Write-Output 'ctron dev' }
		exit 0 }
	'run' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctron: run 需要输入文件'); exit 2 }
		& (Join-Path $Bin 'ctron-cc.exe') run $rest[0]; exit $LASTEXITCODE }
	'check' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctron: check 需要输入文件'); exit 2 }
		$full = (Resolve-Path $rest[0]).Path
		& (Join-Path $Bin 'ctron-chk.exe') run $full @($rest | Select-Object -Skip 1); exit $LASTEXITCODE }
	'doc' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctron: doc 需要输入文件'); exit 2 }
		# §5.3:std.<module> 形原样透传(无路径分隔),由 ctron-doc 四级 std 根解析(同 sh 版)
		$full = if ($rest[0] -notmatch '[/\\]') { $rest[0] } else { (Resolve-Path $rest[0]).Path }
		& (Join-Path $Bin 'ctron-doc.exe') run $full @($rest | Select-Object -Skip 1); exit $LASTEXITCODE }
	'test' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctron: test 需要输入文件'); exit 2 }
		& (Join-Path $Bin 'ctron-cc.exe') run $rest[0]; exit $LASTEXITCODE }
	'fmt' {
		# R-P2d:ctron fmt <file|pkg目录> [-w|--check](语义对齐 compiler-rust main.rs fmt 分支)
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctron: fmt 需要输入文件或 pkg 目录'); exit 2 }
		$target = $rest[0]
		$w = $false; $check = $false
		foreach ($a in ($rest | Select-Object -Skip 1)) {
			if ($a -in '-w','--write') { $w = $true } elseif ($a -eq '--check') { $check = $true }
		}
		if (Test-Path $target -PathType Container) {
			$dir = $target
			if (Test-Path (Join-Path $target 'src') -PathType Container) { $dir = Join-Path $target 'src' }
			$files = @(Get-ChildItem -Path (Join-Path $dir '*.ct') -File | Sort-Object -Property Name | ForEach-Object { $_.FullName })
		} else {
			$files = @($target)
		}
		# 字节级比较(规范化输出必须逐字节一致才算已格式化)
		function Same-File($a, $b) {
			$ba = [IO.File]::ReadAllBytes($a); $bb = [IO.File]::ReadAllBytes($b)
			if ($ba.Length -ne $bb.Length) { return $false }
			for ($i = 0; $i -lt $ba.Length; $i++) { if ($ba[$i] -ne $bb[$i]) { return $false } }
			return $true
		}
		$errors = 0; $unf = 0
		$tmp = [System.IO.Path]::GetTempFileName()
		foreach ($f in $files) {
			if (-not $check -and -not $w) {
				& (Join-Path $Bin 'ctron-fmt.exe') run $f
				if ($LASTEXITCODE -ne 0) { $errors++ }
				continue
			}
			& (Join-Path $Bin 'ctron-fmt.exe') run $f > $tmp
			if ($LASTEXITCODE -ne 0) { Get-Content $tmp | Write-Output; $errors++; continue }
			if ($check) {
				if (-not (Same-File $tmp $f)) { Write-Output $f; $unf++ }
			} else {
				if (-not (Same-File $tmp $f)) { Copy-Item $tmp $f -Force }
				Write-Output $f
			}
		}
		Remove-Item $tmp -ErrorAction SilentlyContinue
		if ($errors -gt 0) { exit 1 }
		if ($check -and $unf -gt 0) { [Console]::Error.WriteLine("$unf 个文件待格式化"); exit 1 }
		exit 0 }
	'build' { if ($rest.Count -ge 1) { Build-File $rest[0] } else { Build-Proj } }
	'new' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctron: new 需要目录名'); exit 2 }
		New-Item -ItemType Directory -Force -Path "$($rest[0])/src" | Out-Null
		Set-Content -Path "$($rest[0])/Ctron.toml" -Value "name = `"$(Split-Path -Leaf $rest[0])`""
		Set-Content -Path "$($rest[0])/Ctron.ctcl" -Value "pkg {`n    manifest_version = 1`n    name = `"$(Split-Path -Leaf $rest[0])`"`n    version = `"0.1.0`"`n}"
		Set-Content -Path "$($rest[0])/src/main.ct" -Value "fn main() {`n    println(`"hello, ctron`")`n}"
		Write-Output "ctron: 已生成 $($rest[0])/(ctron run $($rest[0])/src/main.ct 试跑)"
		exit 0 }
	'lint' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctron: lint 需要输入文件或 pkg 目录'); exit 2 }
		$trusted = $false; $strict = $false
		foreach ($a in ($rest | Select-Object -Skip 1)) {
			if ($a -eq '--trusted') { $trusted = $true } elseif ($a -eq '--strict') { $strict = $true }
			else { [Console]::Error.WriteLine("ctron: lint 未知旗标: $a(支持: --trusted --strict)"); exit 2 }
		}
		Lint-Target $rest[0] $trusted $strict }
	'bench' { Bench-Cmd $(if ($rest.Count -ge 1) { $rest[0] } else { $null }) }
	'add' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctron: add 需要依赖名(形: <name>[@<版本约束>])'); exit 2 }
		Add-Validate $rest[0]
		if (-not (Test-Path 'Ctron.ctcl')) { [Console]::Error.WriteLine('ctron: add: 当前目录缺 Ctron.ctcl(项目模式)'); exit 2 }
		$reg = $env:CTRON_REGPATH; if (-not $reg) { $reg = Join-Path $HOME '.ctron/registry' }
		$name = $rest[0].Split('@')[0]
		$req = ''; if ($rest[0].Contains('@')) { $req = $rest[0].Substring($rest[0].IndexOf('@') + 1) }
		if (-not (Test-Path (Join-Path $reg $name))) { [Console]::Error.WriteLine("ctron: add: registry 无包 $name(registry: $reg;试: ctron publish)"); exit 2 }
		$cands = @(Get-ChildItem (Join-Path $reg $name) -Name | Where-Object { if ($req -eq '') { $true } else { ($_ -eq $req) -or ($_.StartsWith("$req.")) } })
		if ($cands.Count -lt 1) { [Console]::Error.WriteLine("ctron: add: $name 无满足约束 $req 的版本"); exit 2 }
		$ver = $cands | Sort-Object { $_ -split '\.' | ForEach-Object { [int]($_ -replace '[^0-9].*$','') } } | Select-Object -Last 1
		$mod = Join-Path $reg "$name/$ver/module"
		if (-not (Test-Path $mod)) { [Console]::Error.WriteLine("ctron: add: registry 损坏:缺 module($name@$ver)"); exit 2 }
		$mf = Select-String -Path Ctron.ctcl -Pattern ('^dep\s*"' + $name + '"') | Select-Object -First 1
		if ($mf) {
			[Console]::Error.WriteLine("ctron: dep \"$name\" 已在清单(不重复追加)")
		} else {
			Add-Content -Path Ctron.ctcl -Value ("`ndep `"$name`" {`n    version = `"$ver`"`n}")
		}
		New-Item -ItemType Directory -Force -Path (Join-Path (Get-Location).Path "pkgs/$name") | Out-Null
		Copy-Item $mod (Join-Path (Get-Location).Path "pkgs/$name/$name.ct") -Force
		Copy-Item (Join-Path $reg "$name/$ver/manifest") (Join-Path (Get-Location).Path "pkgs/$name/Ctron.ctcl") -Force
		Write-Output "ctron: 已安装 pkgs/$name/($name@$ver)"
		$dep = Join-Path $Bin 'ctron-dep.exe'
		if (-not (Test-Path $dep)) { $dep = Join-Path $Bin 'ctron-dep' }
		if (-not (Test-Path $dep)) { [Console]::Error.WriteLine('ctron: add: 缺 ctron-dep(重装工具链)'); exit 2 }
		& $dep run (Join-Path (Get-Location).Path 'Ctron.ctcl')
		exit $LASTEXITCODE }
	'publish' {
		if (-not (Test-Path 'Ctron.ctcl')) { [Console]::Error.WriteLine('ctron: publish 需项目清单 Ctron.ctcl(当前目录)'); exit 2 }
		$m = Select-String -Path Ctron.ctcl -Pattern '^\s*name\s*=\s*"(.*)"' | Select-Object -First 1
		$name = if ($m) { $m.Matches[0].Groups[1].Value } else { Split-Path -Leaf (Get-Location).Path }
		$v = Select-String -Path Ctron.ctcl -Pattern '^\s*version\s*=\s*"(.*)"' | Select-Object -First 1
		$ver = if ($v) { $v.Matches[0].Groups[1].Value } else { '' }
		if ($ver -eq '') { [Console]::Error.WriteLine('ctron: publish: 清单缺 version'); exit 2 }
		if ($ver -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z.\-]+)?(\+[0-9A-Za-z.\-]+)?$') { [Console]::Error.WriteLine("ctron: publish: version '$ver' 不符严格 semver"); exit 2 }
		if (-not (Test-Path 'src/main.ct')) { [Console]::Error.WriteLine('ctron: publish: v0 registry 单文件包,需 src/main.ct'); exit 2 }
		$pn = @(Get-ChildItem src -Filter '*.ct' -Recurse -Name).Count
		if ($pn -ne 1) { [Console]::Error.WriteLine("ctron: publish: v0 registry 单文件包(src/ 下有 $pn 个 .ct);多文件 tar 志向"); exit 2 }
		$reg = $env:CTRON_REGPATH; if (-not $reg) { $reg = Join-Path $HOME '.ctron/registry' }
		$tgt = Join-Path $reg "$name/$ver"
		if (Test-Path $tgt) { [Console]::Error.WriteLine("ctron: publish: $name@$ver 已发布(版本不可覆盖)"); exit 2 }
		New-Item -ItemType Directory -Force -Path $tgt | Out-Null
		Copy-Item Ctron.ctcl (Join-Path $tgt 'manifest')
		Copy-Item src/main.ct (Join-Path $tgt 'module')
		(Get-FileHash -Path (Join-Path $tgt 'module') -Algorithm SHA256).Hash.ToLower() | Set-Content -Path (Join-Path $tgt 'sha256') -NoNewline
		Write-Output "ctron: 已发布 $name@$ver → $tgt(sha256:$(Get-Content (Join-Path $tgt 'sha256')))"
		exit 0 }
	'lock' {
		if (-not (Test-Path 'Ctron.ctcl')) { [Console]::Error.WriteLine('ctron: lock: 当前目录缺 Ctron.ctcl'); exit 2 }
		$dep = Join-Path $Bin 'ctron-dep.exe'
		if (-not (Test-Path $dep)) { $dep = Join-Path $Bin 'ctron-dep' }
		if (-not (Test-Path $dep)) { [Console]::Error.WriteLine('ctron: lock: 缺 ctron-dep(重装工具链)'); exit 2 }
		& $dep run (Join-Path (Get-Location).Path 'Ctron.ctcl')
		exit $LASTEXITCODE }
	'pkg' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctron: pkg 需要子命令(现支持: pkg verify [--deep] <工件目录> / pkg trace record|replay)'); exit 2 }
		if ($rest[0] -eq 'trace') { Cmd-PkgTrace @($rest | Select-Object -Skip 1) }
		if ($rest[0] -ne 'verify') { [Console]::Error.WriteLine("ctron: pkg: 未知子命令 '$($rest[0])'(现支持: verify trace)"); exit 2 }
		$pvDeep = $false; $pvDir = ''
		foreach ($pvA in ($rest | Select-Object -Skip 1)) {
			if ($pvA -eq '--deep') { $pvDeep = $true } elseif ($pvDir -eq '') { $pvDir = $pvA }
		}
		if ($pvDir -eq '') { [Console]::Error.WriteLine('ctron: pkg verify 需要工件目录(.ctart)'); exit 2 }
		if (-not (Test-Path $pvDir -PathType Container)) { [Console]::Error.WriteLine("ctron: pkg verify: 工件目录不存在: $pvDir"); exit 2 }
		if ($pvDeep) {
			$vv = Join-Path $Bin 'ctron-verify.exe'
			if (-not (Test-Path $vv)) { $vv = Join-Path $Bin 'ctron-verify' }
			if (-not (Test-Path $vv)) { [Console]::Error.WriteLine('ctron: pkg verify --deep: 缺 ctron-verify(重装工具链)'); exit 2 }
			$pvFull = (Resolve-Path $pvDir).Path
			& $vv run $pvFull
			if ($LASTEXITCODE -ne 0) { exit 1 }
			# S3-β 复放腿(§7.2;E5056):traces/ 在场即逐案复放(D3:工件行为 vs 录制期望)
			$pvRn = 0; $pvRb = 0
			$trDir = Join-Path $pvFull 'traces'
			if (Test-Path $trDir -PathType Container) {
				$cc = Join-Path $Bin 'ctron-cc.exe'
				if (-not (Test-Path $cc)) { $cc = Join-Path $Bin 'ctron-cc' }
				if (-not (Test-Path $cc)) { [Console]::Error.WriteLine('ctron pkg verify: traces 在场但缺 ctron-cc(复放腿不可跳过;重装工具链)'); exit 1 }
				$pvName = (Select-String -Path (Join-Path $pvFull 'meta.ctcl') -Pattern '^artifact "([^"]*)"').Matches[0].Groups[1].Value
				if (-not $pvName) { [Console]::Error.WriteLine('ctron pkg verify: meta artifact 块缺失(复放腿无包名)'); exit 1 }
				$enc2 = New-Object Text.UTF8Encoding($false)
				foreach ($trF in @(Get-ChildItem -Path (Join-Path $trDir '*.ctrt') -File)) {
					$trMod = [IO.Path]::GetFileNameWithoutExtension($trF.Name)
					if (-not (Test-Path (Join-Path $pvFull "impl/$trMod.ast"))) { [Console]::Error.WriteLine("ctron pkg verify: 陈旧信任证据: traces/$trMod.ctrt 的模块 $trMod 不在工件 impl/(清理或重录)"); exit 1 }
					$ttext = [IO.File]::ReadAllText($trF.FullName)
					$tms2 = [regex]::Matches($ttext, 'trace "([^"]*)" \{\r?\n  args = "((?:[^"\\]|\\.)*)"\r?\n  expect = "((?:[^"\\]|\\.)*)"\r?\n\}')
					$fns = (@($tms2 | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique) -join ', ')
					$trp = Join-Path ([IO.Path]::GetTempPath()) ('ctron_trp_' + [guid]::NewGuid().ToString('N'))
					New-Item -ItemType Directory -Force -Path (Join-Path $trp "deps/$pvName.ctart") | Out-Null
					Copy-Item (Join-Path $pvFull '*') (Join-Path $trp "deps/$pvName.ctart") -Recurse -Force
					$enc2 = New-Object Text.UTF8Encoding($false)
					foreach ($tm2 in $tms2) {
						$tfn = $tm2.Groups[1].Value
						$targs = $tm2.Groups[2].Value -replace '\\"', '"'
						$texp = $tm2.Groups[3].Value -replace '\\"', '"'
						[IO.File]::WriteAllText((Join-Path $trp 'main.ct'), "use $pvName.$trMod.{ $fns }`nfn main() {`n    println($tfn($targs).to_string())`n}`n", $enc2)
						$errF = Join-Path $trp 'err.txt'
						Push-Location $trp
						$tout = & $cc run main.ct 2> $errF
						$trc = $LASTEXITCODE
						Pop-Location
						if ($trc -ne 0) { Get-Content $errF | [Console]::Error.WriteLine; Remove-Item $trp -Recurse -Force; [Console]::Error.WriteLine("ctron pkg verify: 复放用例 $tfn($targs) 运行失败(rc=$trc)"); exit 1 }
						$tact = @($tout)[0]
						if ($tact -ne $texp) { [Console]::Error.WriteLine("E5056 trace mismatch: $tfn($targs) 记录 $texp 实际 $tact"); $pvRb++ }
						$pvRn++
					}
					Remove-Item $trp -Recurse -Force
				}
				if ($pvRb -gt 0) { [Console]::Error.WriteLine("ctron pkg verify: 复放 $pvRn 用例中 $pvRb 不符(E5056)"); exit 1 }
			}
			Write-Output "ctron pkg verify OK: $pvFull(deep: 摘要自洽;复放 $pvRn 用例)"
			exit 0 }
		$pvBad = $false; $pvN = 0
		# ① 形态面(§3.5 后缀即契约:自称 .ctart 而缺成员 = 损坏/伪造,fail-closed)
		foreach ($pvF in @('meta.ctcl','SHA256SUMS')) {
			if (-not (Test-Path (Join-Path $pvDir $pvF) -PathType Leaf)) { [Console]::Error.WriteLine("ctron pkg verify: 缺 $pvF(fail-closed 拒载)"); $pvBad = $true }
		}
		if (-not (Test-Path (Join-Path $pvDir 'impl') -PathType Container)) { [Console]::Error.WriteLine('ctron pkg verify: 缺 impl/(密封实现目录;fail-closed 拒载)'); $pvBad = $true }
		if ($pvBad) { exit 1 }
		# ② SHA256SUMS 逐成员重算(路径字节序由 seal 保证,此处只对账;SHA256SUMS 是不可信输入)
		foreach ($pvLine in @(Get-Content (Join-Path $pvDir 'SHA256SUMS'))) {
			if ($pvLine -eq '') { continue }
			$pvIdx = $pvLine.IndexOf('  ')
			$pvHex = if ($pvIdx -ge 0) { $pvLine.Substring(0, $pvIdx) } else { $pvLine }
			if ($pvIdx -lt 0 -or $pvHex -notmatch '^[0-9a-f]{64}$') { [Console]::Error.WriteLine("ctron pkg verify: SHA256SUMS 行畸形: $pvLine"); $pvBad = $true; continue }
			$pvRel = $pvLine.Substring($pvIdx + 2)
			if ($pvRel -match '\.\.' -or $pvRel.StartsWith('/') -or $pvRel -eq '') { [Console]::Error.WriteLine("ctron pkg verify: 成员路径越界: $pvRel"); $pvBad = $true; continue }
			$pvMember = Join-Path $pvDir ($pvRel -replace '/', [IO.Path]::DirectorySeparatorChar)
			if (-not (Test-Path $pvMember -PathType Leaf)) { [Console]::Error.WriteLine("ctron pkg verify: 成员缺失: $pvRel"); $pvBad = $true; continue }
			$pvAct = (Get-FileHash -Path $pvMember -Algorithm SHA256).Hash.ToLower()
			if ($pvAct -ne $pvHex) { [Console]::Error.WriteLine("ctron pkg verify: 成员摘要不符: $pvRel 记录 $pvHex 实际 $pvAct"); $pvBad = $true }
			$pvN++
		}
		# ③ self_digest 重算 = sha256(SHA256SUMS 字节)(D8-2 冻结定义;与成员腿相互独立)
		$pvSd = Select-String -Path (Join-Path $pvDir 'meta.ctcl') -Pattern 'self_digest = "(.*)"'
		$pvSdv = if ($pvSd) { $pvSd[0].Matches[0].Groups[1].Value } else { '' }
		$pvSum = (Get-FileHash -Path (Join-Path $pvDir 'SHA256SUMS') -Algorithm SHA256).Hash.ToLower()
		if ($pvSdv -eq '') {
			[Console]::Error.WriteLine('ctron pkg verify: meta 缺 self_digest(fail-closed;工件须经 ctc.sh ast --ast=seal 摘要步骤)'); $pvBad = $true
		} elseif ($pvSdv -ne "sha256:$pvSum") {
			[Console]::Error.WriteLine("ctron pkg verify: self_digest 不符: 记录 $pvSdv 实际 sha256:$pvSum"); $pvBad = $true
		}
		# ④ 判决
		if (-not $pvBad) { Write-Output "ctron pkg verify OK: $pvDir($pvN 成员,self_digest sha256:$pvSum)"; exit 0 }
		exit 1 }
	default { [Console]::Error.WriteLine("ctron: 未知子命令 '$cmd'(详见 ctron --help)"); exit 2 }
}
exit 0
