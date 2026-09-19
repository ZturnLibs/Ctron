# ctc.ps1 —— Ctron 工具链用户驱动(v0.0.1;Windows 版,命令面基准 = sh 版 ctc,spec §7.9 驱动 conformance)
# rc 约定:0 成功 / 1 程序诊断失败 / 2 ctc 环境或用法错误(exit code 同 sh)
# 注意:本文件必须保持 UTF-8 with BOM —— Windows PowerShell 5.1 对无 BOM 脚本按 ANSI 解码,中文帮助文本会乱码
# 重定向/管道下 powershell.exe 的 stdout 默认按 OEM CP 编码,中文帮助/诊断会变 '?',子进程输出解码同样失真;显式收口为 UTF8
try { [Console]::OutputEncoding = [Text.Encoding]::UTF8 } catch {}
$ErrorActionPreference = 'Stop'
$Bin = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Split-Path -Parent $Bin
if ($env:CC) { $Cc = $env:CC } else { $Cc = 'gcc' }
$global:LASTEXITCODE = 0

function Usage {
@'
ctc —— Ctron 工具链驱动
用法:
  ctc run <file.ct>          解释执行(不需要 C 编译器)
  ctc check <file.ct> [--format=json] [--profile=bare]
                             静态检查
  ctc build <file.ct>        发射 C → 本机 cc → 可执行 <stem>(C 侧 <stem>.c)
  ctc build                  项目模式:读 Ctron.toml(入口 src/main.ct,链接 c_src/*.c)
  ctc test <file.ct>         test 块执行(无 main 走解释;含 main 文件的 test 执行挂账)
  ctc new <dir>              脚手架:hello + Ctron.toml + Ctron.ctcl
  ctc --version              版本
  ctc --help | help [cmd]    帮助(亦可 ctc <cmd> --help)
环境变量:CC(默认 gcc)、CTRON_STDPATH(覆盖标准库位置)
rc 约定:0 成功 / 1 程序诊断失败 / 2 ctc 环境或用法错误
'@
}

function Help-Cmd($c) {
	if ($c -eq 'build') {
@'
ctc build —— 发射 C 并编译为可执行
  ctc build <file.ct>   产物 <stem>.exe 与 <stem>.c(与源同目录)
  ctc build             项目模式:入口 src/main.ct,产物 build/<name>.exe;
                        Ctron.toml 声明的 c_src/*.c 一并链接
前置:mingw-w64(MSYS2 或 w64devkit);缺失时 C 照常发射,并给出两条出路
'@
	} elseif ($c -eq 'check') {
		Write-Output 'ctc check <file.ct> [--format=json] [--profile=bare] —— 静态检查,不改任何文件'
	} elseif ($c -eq 'run') {
		Write-Output 'ctc run <file.ct> —— 解释执行;不需要 C 编译器'
	} elseif ($c -eq 'test') {
		Write-Output 'ctc test <file.ct> —— 执行 test 块(无 main 文件);含 main 文件的 test 执行暂走宿主口径挂账'
	} elseif ($c -eq 'new') {
		Write-Output 'ctc new <dir> —— 生成 <dir>/Ctron.toml + Ctron.ctcl + src/main.ct(hello)'
	} else { Usage }
}

function Have-Cc {
	if (-not (Get-Command $Cc -ErrorAction SilentlyContinue)) { return $false }
	$probe = Join-Path ([System.IO.Path]::GetTempPath()) ("ctcp" + [guid]::NewGuid().ToString('N'))
	Set-Content -Path "$probe.c" -Value 'int main(void){return 0;}' -NoNewline
	try { & $Cc -O0 -w -o "$probe.exe" "$probe.c" 2>$null | Out-Null } catch { }
	$ok = ($LASTEXITCODE -eq 0) -and (Test-Path "$probe.exe")
	if (Test-Path "$probe.exe") { Remove-Item "$probe.exe" }
	if (Test-Path "$probe.c") { Remove-Item "$probe.c" }
	return $ok
}

function Cc-Missing($cfile) {
	[Console]::Error.WriteLine("ctc: 未找到可用的 C 编译器('$Cc' 冒烟失败) - ctc build 需要。")
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
	Write-Output "ctc: 已构建 $(Join-Path $dir $stem).exe"
}

function Build-Proj {
	if (-not (Test-Path 'Ctron.toml')) { [Console]::Error.WriteLine('ctc: 项目模式需 Ctron.toml'); exit 2 }
	$m = Select-String -Path Ctron.toml -Pattern '^name\s*=\s*"(.*)"' -CaseSensitive | Select-Object -First 1
	$name = $null
	if ($m) { $name = $m.Matches[0].Groups[1].Value }
	if (-not $name) { $name = Split-Path -Leaf (Get-Location).Path }
	if (-not (Test-Path 'src/main.ct')) { [Console]::Error.WriteLine('ctc: 缺入口 src/main.ct'); exit 2 }
	New-Item -ItemType Directory -Force -Path build | Out-Null
	$tmpOut = & (Join-Path $Bin 'ctron-emit.exe') run (Resolve-Path 'src/main.ct').Path
	[IO.File]::WriteAllText((Join-Path (Get-Location).Path "build/$name.c"), ($tmpOut -join "`n") + "`n", (New-Object Text.UTF8Encoding($false)))
	if ($LASTEXITCODE -ne 0) { exit 1 }
	if (-not (Have-Cc)) { Cc-Missing "build/$name.c"; exit 2 }
	$srcs = @(Get-ChildItem -Path 'c_src/*.c' -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
	& $Cc -O2 -w -pthread "-Wl,--stack,8388608" "build/$name.c" @srcs -o "build/$name.exe"
	if ($LASTEXITCODE -ne 0) { exit 2 }
	Write-Output "ctc: 已构建 build/$name.exe"
}

if ($args.Count -lt 1) { Usage; exit 2 }
$cmd = $args[0]; $rest = @($args | Select-Object -Skip 1)
# <cmd> --help / <cmd> -h:子命令详助入口(usage 宣传的第四帮助入口),先于各分派臂拦截(同 sh 版)
if ($cmd -in 'run','check','build','test','new' -and $rest.Count -ge 1 -and $rest[0] -in '--help','-h') {
	Help-Cmd $cmd; exit 0
}
switch ($cmd) {
	{ $_ -in '--help','-h','help' } { if ($rest.Count -ge 1) { Help-Cmd $rest[0] } else { Usage }; exit 0 }
	{ $_ -in '--version','-V' } {
		$vfile = Join-Path $Root 'VERSION'
		if (Test-Path $vfile) { $v = (Get-Content $vfile -Raw).Trim(); Write-Output "ctron $v" } else { Write-Output 'ctron dev' }
		exit 0 }
	'run' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctc: run 需要输入文件'); exit 2 }
		& (Join-Path $Bin 'ctron-cc.exe') run $rest[0]; exit $LASTEXITCODE }
	'check' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctc: check 需要输入文件'); exit 2 }
		$full = (Resolve-Path $rest[0]).Path
		& (Join-Path $Bin 'ctron-chk.exe') run $full @($rest | Select-Object -Skip 1); exit $LASTEXITCODE }
	'test' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctc: test 需要输入文件'); exit 2 }
		& (Join-Path $Bin 'ctron-cc.exe') run $rest[0]; exit $LASTEXITCODE }
	'build' { if ($rest.Count -ge 1) { Build-File $rest[0] } else { Build-Proj } }
	'new' {
		if ($rest.Count -lt 1) { [Console]::Error.WriteLine('ctc: new 需要目录名'); exit 2 }
		New-Item -ItemType Directory -Force -Path "$($rest[0])/src" | Out-Null
		Set-Content -Path "$($rest[0])/Ctron.toml" -Value "name = `"$(Split-Path -Leaf $rest[0])`""
		Set-Content -Path "$($rest[0])/Ctron.ctcl" -Value "pkg {`n    manifest_version = 1`n    name = `"$(Split-Path -Leaf $rest[0])`"`n    version = `"0.1.0`"`n}"
		Set-Content -Path "$($rest[0])/src/main.ct" -Value "fn main() {`n    println(`"hello, ctron`")`n}"
		Write-Output "ctc: 已生成 $($rest[0])/(ctc run $($rest[0])/src/main.ct 试跑)"
		exit 0 }
	default { [Console]::Error.WriteLine("ctc: 未知子命令 '$cmd'(详见 ctc --help)"); exit 2 }
}
exit 0
