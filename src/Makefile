CC=/usr/bin/gcc

SRC=\
connection_unix.c \
serialization.c \
test_serialization.c \

OBJ=$(SRC:.c=.o)

%.o: %.c
	$(CC) -c $< -o $@

connection_unix: $(OBJ)
	$(CC) $(OBJ) -o bin/$@

test_serialization: test_serialization.c serialization.o
	$(CC) test_serialization.c serialization.o -o bin/test_serialization

clean:
	rm -f $(OBJ) bin/*