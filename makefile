CC = gcc
CFLAGS = -g
TARGET = oss
TARGET2 = process
OBJS = oss.o
OBJS2 = process.o

.SUFFIXES: .c .o

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

$(TARGET2): $(OBJS2)
	$(CC) -o $@ $(OBJS2)

oss.o: oss.c functs.h
	$(CC) $(CFLAGS) -c oss.c

process.o: process.c functs.h
	$(CC) $(CFLAGS) -c process.c

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	-rm *.o log.txt $(TARGET) $(TARGET2)

