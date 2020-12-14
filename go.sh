#!/bin/bash
echo "Compiling..."
if g++ -o main main.cpp -lX11 -lGL -lpthread -lpng -lstdc++fs -std=c++17; then
	echo "Compilation successful"
	echo "Running executable..."
	./main
else
	echo "Compilation failed: Exit code $?"
fi
