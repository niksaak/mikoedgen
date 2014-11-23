CXX=clang++

CFLAGS+=-std=c++11 -c -Wall -Wpedantic -g
ifeq ($(CXX), clang++)
CFLAGS+=-stdlib=libc++
endif

LDFLAGS+=
ifeq ($(CXX), clang++)
LDFLAGS+=-lc++ -lc++abi -lm -lc -lgcc_s -lgcc
endif

.PHONY: all
all: mikoedgen

.PHONY: valgrind
valgrind:
	valgrind ./mikoedgen

mikoedgen: main.o
	$(CXX) -o $@ $+ $(LDFLAGS)

%.o: %.cxx
	$(CXX) -o $@ $(CFLAGS) $<

clean:
	rm -rf *.o mikoedgen

# vim: tabstop=8 softtabstop=8 shiftwidth=8 noexpandtab
