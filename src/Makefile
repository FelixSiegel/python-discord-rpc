CC=/usr/bin/gcc

SRC=\
cJSON.c \
connection_unix.c \
serialization.c \
rpc_connection.c

TARGET=bin/rpc_connection

OBJS=$(SRC:.c=.o)
OBJS := $(addprefix bin/, $(OBJS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

bin/%.o: %.c
	$(CC) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: all
	./$(TARGET)
