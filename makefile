CC = gcc
CFLAGS = -g
SRCS = core.c common.c linkedlist.c
HEADER = common.h linkedlist.h
OBJS = $(SRCS:.c=.o)
TARGET = core
CLIENT_TARGET = client
MULTI_CLIENT_TARGET = multi_client

all: $(TARGET) $(CLIENT_TARGET) $(MULTI_CLIENT_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(CLIENT_TARGET): client.o common.o linkedlist.o
	$(CC) $(CFLAGS) -o $(CLIENT_TARGET) client.o common.o linkedlist.o -lm

$(MULTI_CLIENT_TARGET): multi_client.o common.o linkedlist.o
	$(CC) $(CFLAGS) -o $(MULTI_CLIENT_TARGET) multi_client.o common.o linkedlist.o -lm

%.o: %.c $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET) $(CLIENT_TARGET) $(MULTI_CLIENT_TARGET)
