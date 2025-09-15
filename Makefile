LLVM_FLAGS=`llvm-config-16 --cxxflags --ldflags --system-libs --libs all`

all: build_compiler

build_compiler:
	mkdir -p build
	bison -d -o build/emoji.tab.c src/emoji.y
	flex -o build/emoji.yy.c src/emoji.l
	clang++ -I src -x c++ -O0 -ggdb $(LLVM_FLAGS) src/main.cpp build/emoji.tab.c build/emoji.yy.c -o build/emoji

compile_program:
ifndef FILE
	$(error You need to provide the file to compile: make compile_program FILE=./program.emj)
endif

	./build/emoji $(FILE) build/program.o
	gcc -no-pie src/stdc.c build/program.o -o build/program

clean:
	rm -rf build

