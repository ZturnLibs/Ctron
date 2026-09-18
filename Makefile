# Makefile —— 源码线:预发射 C → 工具链(cc-only;seed 引导走 git 仓库,不进 tarball)
CC ?= cc
CFLAGS ?= -O2 -w -pthread
PREFIX ?= $(HOME)/.ctron
DEST = $(PREFIX)/ctron

.PHONY: all install clean
all: bin/ctron-cc bin/ctron-chk bin/ctron-emit

bin:
	mkdir -p bin
# 词干映射:bin/ctron-cc ← prebuilt/ctron-cc.c(%=cc/chk/emit,前置再补 ctron-)
bin/ctron-%: prebuilt/ctron-%.c | bin
	$(CC) $(CFLAGS) -o $@ $<

install: all
	install -d $(DEST)/bin $(DEST)/lib/ctron/std $(DEST)/share/doc
	install bin/ctron-cc bin/ctron-chk bin/ctron-emit ctc $(DEST)/bin/
	cp -R std/. $(DEST)/lib/ctron/std/
	[ ! -f VERSION ] || install VERSION $(DEST)/
	install README.md docs/superpowers/specs/2026-09-17-toolchain-distribution-design.md $(DEST)/share/doc/ 2>/dev/null || true

clean:
	rm -rf bin
