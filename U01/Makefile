CFLAGS = -Wall

mult-test: mult-test.c
	$(CC) $(CFLAGS) mult-test.c

test: mult-test tester example-files.txt
	./tester
	
.PHONY: clean
clean:
	rm mult-test
