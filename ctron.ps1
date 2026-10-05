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
  ctron add <名>[@<版本约束>] 骨架:registry 未接(T49),仅参数校验+出路指引
  ctron publish               骨架:registry 未接(T49),只读清单零网络零写入
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
		Write-Output 'ctron add <name>[@<版本约束>] —— 骨架:registry 未接(T49),仅参数校验+出路指引(手动编辑 Ctron.ctcl dependencies);零写入零网络'
	} elseif ($c -eq 'publish') {
		Write-Output 'ctron publish —— 骨架:registry 未接(T49),只读本地清单报告将发布面;对外发布面启用须经用户确认'
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

if ($args.Count -lt 1) { Usage; exit 2 }
$cmd = $args[0]; $rest = @($args | Select-Object -Skip 1)
# <cmd> --help / <cmd> -h:子命令详助入口(usage 宣传的第四帮助入口),先于各分派臂拦截(同 sh 版)
if ($cmd -in 'run','check','build','test','new','fmt','doc','lint','bench','add','publish' -and $rest.Count -ge 1 -and $rest[0] -in '--help','-h') {
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
		[Console]::Error.WriteLine('ctron: add 骨架——registry 未接(T49 前置未到):依赖解析/lockfile/本地 registry 协议未实装,未做任何写入。')
		[Console]::Error.WriteLine('  出路:手动编辑 Ctron.ctcl dependencies 块(三互斥形,docs/superpowers/specs/2026-09-16-config-language-v1.md);')
		[Console]::Error.WriteLine('        本地路径依赖与 deps/<pkg>.ctart 工件回落已在解析链生效。')
		exit 2 }
	'publish' {
		if (-not (Test-Path 'Ctron.ctcl')) { [Console]::Error.WriteLine('ctron: publish 需项目清单 Ctron.ctcl(当前目录)'); exit 2 }
		$m = Select-String -Path Ctron.ctcl -Pattern '^\s*name\s*=\s*"(.*)"' | Select-Object -First 1
		$name = if ($m) { $m.Matches[0].Groups[1].Value } else { Split-Path -Leaf (Get-Location).Path }
		$v = Select-String -Path Ctron.ctcl -Pattern '^\s*version\s*=\s*"(.*)"' | Select-Object -First 1
		$ver = if ($v) { $v.Matches[0].Groups[1].Value } else { '0.0.0' }
		[Console]::Error.WriteLine("ctron: publish 骨架——将发布 $name@$ver,但 registry 未接(T49)。")
		[Console]::Error.WriteLine('  对外发布面启用须经用户确认;骨架只读本地清单,零网络零写入。')
		[Console]::Error.WriteLine('  本地替代:deps/ 路径依赖与 deps/<pkg>.ctart 闭源工件消费已可用(工件密封随 T50 触发线)。')
		exit 2 }
	default { [Console]::Error.WriteLine("ctron: 未知子命令 '$cmd'(详见 ctron --help)"); exit 2 }
}
exit 0
