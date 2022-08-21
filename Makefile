
CC = g++
INCLUDE += -Icore -Ihttp -Ilib/llhttp
LDFLAGS += -Llib/llhttp -lllhttp $(shell pcre-config --libs)
CXXHEADERS := $(shell find $(SOURCEDIR) -name '*.hpp')

.PHONY: all
all: rpx

rpx: rpx.cpp $(CXXHEADERS)
	$(CC) $< -o $@ -O2 $(INCLUDE) $(LDFLAGS) --std=c++17 -g -fsanitize=thread
	-rm -f libllhttp.so
	ln -s lib/llhttp/libllhttp.so libllhttp.so

.PHONY: clean
clean:
	-rm -f rpx
	-rm -f libllhttp.so
