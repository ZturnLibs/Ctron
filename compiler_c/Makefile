# Ctron 编译器 C 版 —— Makefile
# 独立于 Rust 版 compiler/,共享 docs/spec 规范与仓库根 tests/ 语料。
CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
INC      = -Isrc
BUILD    = build
BIN      = $(BUILD)/ctronc
CORE     = src/arena.c src/token.c src/lexer.c \
           src/parser_core.c src/parser_decl.c src/parser_expr.c src/parser.c src/ast_show.c src/sem.c src/pkg.c \
           src/rt_core.c src/rt_eval.c src/rt_stmt.c \
           src/trans_core.c src/trans_expr.c src/trans_conc.c src/trans_stmt.c src/trans_rt.c src/trans.c
HDRS     = src/arena.h src/token.h src/lexer.h src/ast.h src/parser.h src/trans.h src/trans_internal.h

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

$(BUILD)/suite_sem: tests/suite_sem.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/suite_sem.c $(CORE)

$(BUILD)/suite_pkg: tests/suite_pkg.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/suite_pkg.c $(CORE)

$(BUILD)/suite_rt: tests/suite_rt.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/suite_rt.c $(CORE)

$(BUILD)/suite_run: tests/suite_run.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/suite_run.c $(CORE)

$(BUILD)/suite_diff: tests/suite_diff.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/suite_diff.c $(CORE)

$(BUILD)/suite_trans: tests/suite_trans.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/suite_trans.c $(CORE)

$(BUILD)/suite_corpus_trans: tests/suite_corpus_trans.c $(CORE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ tests/suite_corpus_trans.c $(CORE)

# 默认验收:单元 + 语料集成(词法+解析+语义+模块级)+ 执行层(C4-a 子集)

test: $(BUILD)/test_lex $(BUILD)/test_parse $(BUILD)/suite_lex $(BUILD)/suite_parse $(BUILD)/suite_sem $(BUILD)/suite_pkg $(BUILD)/suite_rt $(BUILD)/suite_run $(BUILD)/suite_diff $(BUILD)/suite_trans $(BUILD)/suite_corpus_trans
	./$(BUILD)/test_lex
	./$(BUILD)/suite_lex ../tests
	./$(BUILD)/test_parse
	./$(BUILD)/suite_parse ../tests
	./$(BUILD)/suite_sem ../tests
	./$(BUILD)/suite_pkg ../tests/modules
	./$(BUILD)/suite_rt ../tests
	./$(BUILD)/suite_run ../selfhosted
	./$(BUILD)/suite_diff ../selfhosted
	./$(BUILD)/suite_trans tests/trans_fixtures
	./$(BUILD)/suite_corpus_trans ../tests

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
