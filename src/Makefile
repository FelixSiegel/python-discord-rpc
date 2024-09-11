CC=/usr/bin/gcc

OBJ=\
connection_unix.o

%.o: %.c
	$(CC) -c $< -o $@

connection_unix: $(OBJ)
	$(CC) $(OBJ) -o bin/$@

clean:
	rm $(OBJ) bin/connection_unix