all:
	gcc -g -O0 -Wall -Wextra -Wshadow -Wpedantic -fprofile-arcs -ftest-coverage  -DPS_SYNC_LINUX -DPS_QUEUE_LL tests.c ../src/*.c -I../src -lpthread -o tests.out && ./tests.out

clean:
	rm *.out
	rm *.gc*

coverage: all
	@echo '--------------------------------'
	./tests.out
	@echo '--------------------------------'
	gcov *.gcda

example:
	gcc -g -Wall -DPS_SYNC_LINUX -DPS_QUEUE_LL example.c ../src/*.c -I../src -lpthread -o example.out
	
benchmark:
	gcc -g -Wall -O0 -DPS_SYNC_LINUX -DPS_QUEUE_LL benchmark.c ../src/*.c -I../src -lpthread -lm -o benchmark.out && ./benchmark.out -i
