all:
	gcc -Wall -fprofile-arcs -ftest-coverage  tests.c ../src/pubsub.c -I../src -lpthread -o tests.out

clean:
	rm *.out
	rm *.gc*

coverage: all
	@echo '--------------------------------'
	./tests.out
	@echo '--------------------------------'
	gcov tests.c
	@echo '--------------------------------'
	gcov pubsub.c

example:
	gcc -Wall example.c ../src/pubsub.c -I../src -lpthread -o example.out
	
benchmark:
	gcc -O0 benchmark.c ../src/pubsub.c -I../src -lpthread -lm -o benchmark.out
