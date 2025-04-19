CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lcrypto -lbluetooth
SOURCES = miniShell.c sendfile.c \
          cat.c clear.c cp.c finddupes.c \
          killlag.c ln.c ls.c mkdir.c \
          mv.c rm.c rmdir.c connect.c
EXEC = minsh

all: $(EXEC) connect

$(EXEC): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

connect: connect.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(EXEC) connect 