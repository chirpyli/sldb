CC       = gcc
CFLAGS   = -Wall -Wextra -g
INCLUDES = -Isrc/include -Isrc/parser

PARSER_DIR = src/parser
MEM_DIR     = src/mem
NODES_DIR   = src/nodes

GEN_SRC  = $(PARSER_DIR)/scan.c $(PARSER_DIR)/gram.c
GEN_HDR  = $(PARSER_DIR)/gram.h

SRCS = src/main.c \
       $(MEM_DIR)/arena.c \
       $(NODES_DIR)/node.c \
       $(NODES_DIR)/list.c \
       $(PARSER_DIR)/scansup.c \
       $(GEN_SRC)

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
