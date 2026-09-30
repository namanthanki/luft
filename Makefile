CC = gcc
CFLAGS ?= -std=c11 -O3 -Wall -Wextra -pedantic -Wno-unused-function -flto
LDFLAGS ?= -flto -lm

SRCS = src/main.c \
       src/bitboard.c \
       src/attacks.c \
       src/zobrist.c \
       src/position.c \
       src/makemove.c \
       src/movegen.c \
       src/perft.c \
       src/eval.c \
       src/search.c \
       src/tt.c \
       src/uci.c \
       src/datagen.c \
       src/tuner.c

OBJS = $(SRCS:.c=.o)
TARGET = luft.exe

.PHONY: all clean debug native

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

native: CFLAGS += -march=native
native: clean $(TARGET)

bmi2: CFLAGS += -mbmi2
bmi2: clean $(TARGET)

debug: CFLAGS = -std=c11 -g -O0 -Wall -Wextra -pedantic -DDEBUG
debug: clean $(TARGET)

ifeq ($(OS),Windows_NT)
clean:
	-del /f /q $(subst /,\,$(OBJS)) $(TARGET) 2>nul || (exit 0)
else
clean:
	rm -f $(OBJS) $(TARGET)
endif
