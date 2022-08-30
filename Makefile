
CC = g++
INCLUDE += -Icore -Ihttp -Ilib/llhttp
LDFLAGS += -Llib/llhttp -lllhttp $(shell pcre-config --libs) -lzlog
CXXHEADERS := $(shell find $(SOURCEDIR) -name '*.hpp')

.PHONY: all
all: rpx

rpx: rpx.cpp $(CXXHEADERS)
	$(CC) $< -o $@ -O2 $(INCLUDE) $(LDFLAGS) --std=c++17 -g -fsanitize=thread

.PHONY: clean
clean:
	-rm -f rpx
