# Ctron 编译器 C 版 —— Makefile
# 独立于 Rust 版 compiler/,共享 docs/spec 规范与仓库根 tests/ 语料。
CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
INC      = -Isrc
BUILD    = build
BIN      = $(BUILD)/ctronc
CORE     = src/arena.c src/token.c src/lexer.c src/parser.c src/ast_show.c
HDRS     = src/arena.h src/token.h src/lexer.h src/ast.h src/parser.h

all: $(BIN)

$(BIN): src/main.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ src/main.c $(CORE)

$(BUILD)/test_lex: tests/test_lex.c src/arena.c src/token.c src/lexer.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/test_lex.c src/arena.c src/token.c src/lexer.c

$(BUILD)/test_parse: tests/test_parse.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/test_parse.c $(CORE)

$(BUILD)/suite_lex: tests/suite_lex.c src/arena.c src/token.c src/lexer.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/suite_lex.c src/arena.c src/token.c src/lexer.c

$(BUILD)/suite_parse: tests/suite_parse.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/suite_parse.c $(CORE)

# 默认验收:单元(lex+parse)+ 61 文件语料集成(词法 + 解析)
test: $(BUILD)/test_lex $(BUILD)/test_parse $(BUILD)/suite_lex $(BUILD)/suite_parse
	./$(BUILD)/test_lex
	./$(BUILD)/suite_lex ../tests
	./$(BUILD)/test_parse
	./$(BUILD)/suite_parse ../tests

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
