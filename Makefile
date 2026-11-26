CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
BISON ?= bison
FLEX ?= flex
PYTHON ?= python3

.PHONY: all test clean
all: build/config15

build:
	mkdir -p build

build/parser.cpp: src/parser.y | build
	$(BISON) -d -o build/parser.cpp src/parser.y

build/parser.hpp: build/parser.cpp
	test -f $@ || $(BISON) -d -o build/parser.cpp src/parser.y

build/lexer.cpp: src/lexer.l build/parser.hpp
	$(FLEX) -o $@ $<

build/config15: src/main.cpp src/config.cpp src/config.hpp build/parser.cpp \
                build/lexer.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Isrc -Ibuild src/main.cpp src/config.cpp \
		build/parser.cpp build/lexer.cpp $(LDFLAGS) -o $@

test: build/config15
	$(PYTHON) -m unittest discover -s tests -v

clean:
	rm -rf build
