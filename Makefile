
CC = g++
INCLUDE += -Icore -Ihttp -Ilib/llhttp
LDFLAGS += -Llib/llhttp -lllhttp $(shell pcre2-config --libs8)
CXXHEADERS := $(shell find $(SOURCEDIR) -name '*.hpp')

.PHONY: all
all: rpx

rpx: rpx.cpp $(CXXHEADERS)
	$(CC) $< -o $@ -pthread -O2 $(INCLUDE) $(LDFLAGS) --std=c++17 -g -fsanitize=thread
	-rm -f libllhttp.so
	ln -s lib/llhttp/libllhttp.so libllhttp.so

.PHONY: clean
clean:
	-rm -f rpx
	-rm -f libllhttp.so
