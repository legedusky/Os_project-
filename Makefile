CC = gcc
CFLAGS = -Wall -Wextra -I. -I./cmds
LDFLAGS = -lbluetooth -lcrypto
CMD_SRCS = $(wildcard cmds/*.c)
EXEC = minsh

all: $(EXEC)

$(EXEC): miniShell.o $(CMD_SRCS:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(EXEC) *.o cmds/*.o

.PHONY: all clean