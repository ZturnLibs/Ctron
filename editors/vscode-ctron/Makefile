# Ctron 扩展打包:同步 LSP 源码副本 → vsce 打包
# 用法:make package   (产出 ctron-lang-<版本>.vsix)

VERSION := $(shell python3 -c "import json;print(json.load(open('package.json'))['version'])")

.PHONY: package sync clean

package: sync
	npx --yes @vscode/vsce package --allow-missing-repository

# LSP 服务器本体在 ../../lsp/src/main.ct,打包前同步进扩展(保持单一事实源)
sync:
	cp -f ../../lsp/src/main.ct server/main.ct
	@diff -q ../../lsp/src/main.ct server/main.ct >/dev/null && echo "sync: server/main.ct 与 lsp 源一致"

clean:
	rm -f *.vsix
