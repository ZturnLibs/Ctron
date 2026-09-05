# Ctron 编译器 C 版 —— Makefile
# 独立于 Rust 版 compiler/,共享 docs/spec 规范与仓库根 tests/ 语料。
CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
INC      = -Isrc
BUILD    = build
BIN      = $(BUILD)/ctronc
CORE     = src/arena.c src/token.c src/lexer.c
HDRS     = src/arena.h src/token.h src/lexer.h

all: $(BIN)

$(BIN): src/main.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ src/main.c $(CORE)

$(BUILD)/test_lex: tests/test_lex.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/test_lex.c $(CORE)

$(BUILD)/suite_lex: tests/suite_lex.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/suite_lex.c $(CORE)

# 默认验收:单元 + 61 文件语料集成
test: $(BUILD)/test_lex $(BUILD)/suite_lex
	./$(BUILD)/test_lex
	./$(BUILD)/suite_lex ../tests

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
