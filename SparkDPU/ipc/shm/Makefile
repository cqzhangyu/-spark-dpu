APP     := shminit 
CC      := gcc
CFLAGS  := -O3
LDFLAGS := -lrt

all:main

main: main.c 
	$(CC) -o $(APP) main.c $(LDFLAGS)

.PHONY: clean
clean:
	rm $(APP)
