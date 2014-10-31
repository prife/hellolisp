FILES = $(wildcard *.c) mpc/mpc.c
TARGET := hello

all: $(FILES)
	$(CC) -std=c99 $^ -I. -Impc -lm -lreadline -o $(TARGET)

