#!/bin/bash
echo "Compiling..."
if g++ -o main main.cpp -lX11 -lGL -lpthread -lpng -lstdc++fs -std=c++17; then
	echo "Compilation successful"
  MAPS=./maps
  if [ ! -d "$MAPS" ]; then
    echo "Creating maps folder..."
    mkdir $MAPS
  fi
	echo "Running executable..."
	./main
else
	echo "Compilation failed: Exit code $?"
fi
