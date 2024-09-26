CC=/usr/bin/gcc

SRC=\
cJSON.c \
connection_unix.c \
serialization.c \
rpc_connection.c

TARGET=rpc_connection

OBJS=$(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) bin/*
