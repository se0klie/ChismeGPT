DEPS = common.h
CFLAGS = -Wall -c -g
DFLAGS =  # Default to no flags

.PHONY: all debug sanitize clean

# Main targets to build the programs
all: core client multi_client

core: core.o common.o $(DEPS)
	gcc -o $@ core.o common.o $(DFLAGS) -lpthread

client: client.o common.o $(DEPS)
	gcc -o $@ client.o common.o $(DFLAGS) -lpthread

multi_client: multi_client.o
	gcc -o $@ multi_client.o -lpthread

# Compile any .c file into .o file
%.o: %.c $(DEPS)
	gcc $(CFLAGS) $< $(DFLAGS)

# Debug target to compile with debugging symbols
debug: DFLAGS = -g
debug: clean all

# Sanitize target to enable AddressSanitizer and Undefined Behavior sanitizer
sanitize: DFLAGS = -fsanitize=address,undefined
sanitize: clean all

# Clean up build artifacts
clean:
	rm -f *.o core client multi_client
