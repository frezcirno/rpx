
CC = g++
INCLUDE += -Icore -Ihttp -Ilib/llhttp
LDFLAGS += -Llib/llhttp -lllhttp
CXXHEADERS := $(shell find $(SOURCEDIR) -name '*.hpp')

.PHONY: all
all: rpx

rpx: rpx.cpp $(CXXHEADERS)
	$(CC) $< -o $@ -pthread -O2 $(INCLUDE) $(LDFLAGS)
	-rm -f libllhttp.so
	ln -s lib/llhttp/libllhttp.so libllhttp.so

.PHONY: clean
clean:
	-rm -f rpx
	-rm -f libllhttp.so
