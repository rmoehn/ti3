CFLAGS = -Wall

mult-test: mult-test.c
	$(CC) $(CFLAGS) -o mult-test mult-test.c

test: mult-test tester example-correct1.dat example-incorrect1.dat
	echo 'correct? '   && ./mult-test example-correct1.dat
	echo 'correct? '   && ./mult-test example-correct2.dat
	echo 'incorrect? ' && ./mult-test example-incorrect1.dat
	
.PHONY: clean
clean:
	rm mult-test
