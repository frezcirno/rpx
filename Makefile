
CC = g++
INCLUDE += -Icore
LDFLAGS += -Llib/llhttp -lllhttp

.PHONY: all
all: rpx

rpx: rpx.cpp
	$(CC) $< -o $@ -pthread -O2 $(INCLUDE) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f rpx
