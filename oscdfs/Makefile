# oscdfs/Makefile
CC = gcc
CFLAGS = -g -Wall -Wextra -I./include
LDFLAGS = -lpthread
SRC_DIR = src
OBJ_DIR = obj
BIN = oscdfs

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/builtin.c $(SRC_DIR)/exec.c \
       $(SRC_DIR)/disk.c $(SRC_DIR)/bitmap.c $(SRC_DIR)/super.c \
       $(SRC_DIR)/inode.c $(SRC_DIR)/dir.c $(SRC_DIR)/mkfs.c \
       $(SRC_DIR)/state.c $(SRC_DIR)/file.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN)

.PHONY: all clean