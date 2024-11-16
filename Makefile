CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -g

# OpenSSL libraries
LIBS = -lssl -lcrypto

SRCS = ArgumentParser.cpp Program.cpp IMAPClient.cpp EmailMessage.cpp Helpers.cpp
OBJS = $(SRCS:.cpp=.o)

TARGET = imapcl

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

pack:
	tar -cvf xjakub41.tar $(SRCS) *.h LICENSE Makefile README.md
