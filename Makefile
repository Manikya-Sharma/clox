CC = gcc
CFLAGS = -Wall -g

SRCDIR = src
OBJDIR = obj
BINDIR = bin

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))
EXECUTABLE = $(BINDIR)/clox

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/%.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/main.o: $(SRCDIR)/main.c $(SRCDIR)/common.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJDIR)/*.o $(EXECUTABLE)

run-binary:
	make clean && make && ./bin/clox
