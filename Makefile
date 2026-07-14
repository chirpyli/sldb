CC       = gcc
CFLAGS   = -Wall -Wextra -g -D_GNU_SOURCE      # -D_GNU_SOURCE 启用 fmemopen/getline/strndup
INCLUDES = -Isrc/include -Isrc/parser

PARSER_DIR = src/parser
MEM_DIR     = src/mem
NODES_DIR   = src/nodes

GEN_SRC  = $(PARSER_DIR)/scan.c $(PARSER_DIR)/gram.c
GEN_HDR  = $(PARSER_DIR)/gram.h

# 后续模块目录（Phase 0 占位；Phase 1-6 逐步填充实现）
MODULE_DIRS = src/storage src/access src/executor src/catalog src/txn src/planner

# 统一错误码模块（Phase 0 新增）+ 各模块占位 .c（通过 MODULE_DIRS 通配纳入）
SRCS = src/main.c \
       src/error.c \
       $(MEM_DIR)/arena.c \
       $(NODES_DIR)/node.c \
       $(NODES_DIR)/list.c \
       $(PARSER_DIR)/scansup.c \
       $(GEN_SRC) \
       $(foreach d,$(MODULE_DIRS),$(wildcard $(d)/*.c))

OBJS = $(SRCS:.c=.o)

TARGET = sldb

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# flex: .l -> .c
$(PARSER_DIR)/scan.c: $(PARSER_DIR)/scan.l $(GEN_HDR)
	flex -o $@ $<

# bison: .y -> .c + .h
$(PARSER_DIR)/gram.c $(PARSER_DIR)/gram.h: $(PARSER_DIR)/gram.y
	bison -d -o $(PARSER_DIR)/gram.c $<

%.o: %.c $(GEN_HDR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(OBJS) $(GEN_SRC) $(GEN_HDR) $(TARGET)

test: $(TARGET)
	@for f in test/sql/*.sql; do \
		echo "=== $$f ==="; \
		./$(TARGET) $$f; \
		echo; \
	done
