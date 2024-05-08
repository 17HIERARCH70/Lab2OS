CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -Iinclude

# Define the main program and dynamic library names
EXECUTABLE = lab1psiN3245
LIBRARY1 = libavgN3245.so
LIBRARY2 = libagkN3246.so

# Source files for executable
EXE_SOURCES = $(wildcard src/*.c)
EXE_OBJECTS = $(EXE_SOURCES:.c=.o)

# Source files for libraries
PLUGIN_SOURCES = $(wildcard plugin/*.c)
PLUGIN_OBJECTS = $(PLUGIN_SOURCES:.c=.o)
PLUGIN_LIBRARIES = $(patsubst %.c, %.so, $(PLUGIN_SOURCES))

# Compile dynamic libraries with position-independent code
PIC_FLAGS = -fPIC

# Default target
all: $(EXECUTABLE) $(PLUGIN_LIBRARIES)

$(EXECUTABLE): $(EXE_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(LIBRARY1): $(PLUGIN_OBJECTS)
	$(CC) $(CFLAGS) $(PIC_FLAGS) -shared -o $@ $^

$(LIBRARY2): $(PLUGIN_OBJECTS)
	$(CC) $(CFLAGS) $(PIC_FLAGS) -shared -o $@ $^

# Generic rule for compiling object files
%.o: %.c
	$(CC) $(CFLAGS) $(PIC_FLAGS) -c $< -o $@

# Generic rule for compiling plugin libraries
%.so: %.o
	$(CC) $(CFLAGS) $(PIC_FLAGS) -shared -o $@ $<

clean:
	rm -f $(EXECUTABLE) $(PLUGIN_LIBRARIES) $(EXE_OBJECTS) $(PLUGIN_OBJECTS)

.PHONY: all clean
