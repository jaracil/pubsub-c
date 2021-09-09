build:
	gcc -g -O2 -Wall -Wextra -Wshadow -Wpedantic -fprofile-arcs -ftest-coverage -DPS_DEPRECATE_NO_PREFIX tests.c ../src/*.c -I../src -lpthread -o tests.out

test: clean build
	./tests.out

clean:
	rm -f *.out*
	rm -f *.gc*
	rm -f perf.data*

coverage: clean build
	@echo '--------------------------------'
	./tests.out
	@echo '--------------------------------'
	gcov *.gcda

coverage-show: coverage
	grep "###" *.gcov

example:
	gcc -g -Wall example.c ../src/*.c -I../src -lpthread -o example.out
	
benchmark:
	gcc -g -Wall -O0 benchmark.c ../src/*.c -I../src -lpthread -lm -o benchmark.out && ./benchmark.out

benchmark-all:
	gcc -g -Wall -O0 -DPS_QUEUE_CUSTOM -DPS_QUEUE_BUCKET benchmark.c ../src/*.c -I../src -lpthread -lm -o benchmark.out && ./benchmark.out
	@echo
	gcc -g -Wall -O0 -DPS_QUEUE_CUSTOM -DPS_QUEUE_LL benchmark.c ../src/*.c -I../src -lpthread -lm -o benchmark.out && ./benchmark.out
	@echo
	gcc -g -Wall -O3 -DPS_QUEUE_CUSTOM -DPS_QUEUE_BUCKET benchmark.c ../src/*.c -I../src -lpthread -lm -o benchmark.out && ./benchmark.out
	@echo
	gcc -g -Wall -O3 -DPS_QUEUE_CUSTOM -DPS_QUEUE_LL benchmark.c ../src/*.c -I../src -lpthread -lm -o benchmark.out && ./benchmark.out

all: coverage