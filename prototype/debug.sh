#!/bin/bash
echo "Compiling..."
if g++ -o main main.cpp -lX11 -lGL -lpthread -lpng -lstdc++fs -std=c++17 -g; then
	echo "Compilation successful"
	echo "Running executable..."
	gdb ./main
else
	echo "Compilation failed: Exit code $?"
fi
