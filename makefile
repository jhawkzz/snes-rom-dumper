# Compiler
CC = g++

# Compiler flags
CFLAGS = -std=c++11 -Wall

# Include directories
INCLUDES = -I/path/to/include

# Libraries
# LIBS = -L/path/to/lib -lmylibrary
LIBS = -lgpiodcxx -lgpiod

# Source files
SRCS = main.cpp port.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = copyrom

# Make rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(OBJS) $(LIBS)

.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)