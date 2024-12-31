all: output/record-analyzer.o | output
	@echo "Linking..."
	@gcc -Wall -o output/record-analyzer output/record-analyzer.o output/sqlite.o -lpthread -ldl
	@echo "Done."

output/sqlite.o: sqlite/sqlite3.c | output
	@echo "Compiling sqlite files..."
	@gcc -Wall -g -c sqlite/sqlite3.c -o output/sqlite.o

output/record-analyzer.o: record-analyzer.c output/sqlite.o | output
	@echo "Compiling record-analyzer files..."
	@gcc -Wall -g -c record-analyzer.c -o output/record-analyzer.o

output:
	@mkdir output

clean: 
	@echo "Cleaning"
	@rm output/*
	@echo "Done"
