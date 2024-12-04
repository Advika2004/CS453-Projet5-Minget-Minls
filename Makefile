# Compiler and Flags
CC = gcc
CFLAGS = -Wall -g

# Targets
all: minget minls

# Executables
minget: minget.o helper.o print.o
	$(CC) $(CFLAGS) -o minget minget.o helper.o print.o

minls: minls.o helper.o print.o
	$(CC) $(CFLAGS) -o minls minls.o helper.o print.o

# Object Files
minget.o: minget.c helper.h print.h minfunc.h
	$(CC) $(CFLAGS) -c minget.c

minls.o: minls.c helper.h print.h minfunc.h
	$(CC) $(CFLAGS) -c minls.c

helper.o: helper.c helper.h minfunc.h
	$(CC) $(CFLAGS) -c helper.c

print.o: print.c print.h minfunc.h
	$(CC) $(CFLAGS) -c print.c

# Clean Up
clean:
	rm -f minget minls minget.o minls.o helper.o print.o

# Test
test: minls minget
	@echo Testing minls...
	@./minls TestImage
	@echo Testing minget...
	@./minget TestImage src_path dst_path
	@echo done.