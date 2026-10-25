# Basic Makefile for a C program

# Compiler
CC = gcc

SCAN = 0
ASAN = 0

# Compiler flags
CFLAGS = -fno-strict-aliasing -Wall -Wextra -std=gnu99 -Og -g -Wno-unused-parameter -Wno-unused-result

ifeq ($(SCAN), 0)
CFLAGS += -fanalyzer
endif

ifeq ($(ASAN), 1)
CFLAGS += -fsanitize=address -fsanitize=undefined
endif

# Preprocessor definitions
CDEFS = -DFX4_COMPILER

C_INCLUDES = -I. 

# Target executable name
TARGET = fx4

# Source files
SOURCES = main.c lexer.c hash32.c unity.c array.c bytecode.c parser.c ir.c syslib.c pixelarray.c debug_printf.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Default target
all: $(TARGET) $(TARGET).elf

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET)

$(TARGET).elf: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET).elf

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) $(CDEFS) $(C_INCLUDES) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET)

# Phony targets
.PHONY: all clean
