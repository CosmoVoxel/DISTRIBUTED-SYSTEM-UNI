CC=mpicxx
CFLAGS=-Wall -O2
TARGET=main.out
SOURCE=main.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	@echo "Building target: $(TARGET)"
	@echo "Compiler: $(CC)"
	@echo "Compiler flags: $(CFLAGS)"
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)
	@echo "Build complete."

run: $(TARGET)
	mpirun -np 4 ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
