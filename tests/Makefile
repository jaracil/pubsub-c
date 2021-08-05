all:
	gcc -g -O0 -Wall -Wextra -Wshadow -Wpedantic -fprofile-arcs -ftest-coverage  tests.c ../src/*.c -DPS_SYNC_LINUX -I../src -lpthread -o tests.out

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
	gcc -g -Wall -O0 benchmark.c ../src/pubsub.c ../src/sync_linux.c -I../src -lpthread -lm -o benchmark.out
