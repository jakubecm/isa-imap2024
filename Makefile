CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -g

SRCS = ArgumentParser.cpp Program.cpp
OBJS = $(SRCS:.cpp=.o)

TARGET = imapcl

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)