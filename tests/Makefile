all:
	gcc -g -O0 -Wall -Wextra -Wshadow -Wpedantic -fprofile-arcs -ftest-coverage  -DPS_SYNC_LINUX tests.c ../src/*.c -I../src -lpthread -o tests.out

test: clean all
	./tests.out

clean:
	rm *.out
	rm *.gc*

coverage: clean all
	@echo '--------------------------------'
	./tests.out
	@echo '--------------------------------'
	gcov *.gcda

coverage-show: coverage
	grep "###" *.gcov

example:
	gcc -g -Wall -DPS_SYNC_LINUX example.c ../src/*.c -I../src -lpthread -o example.out
	
benchmark:
	gcc -g -Wall -O0 -DPS_SYNC_LINUX benchmark.c ../src/*.c -I../src -lpthread -lm -o benchmark.out && ./benchmark.out

benchmark-all:
	gcc -g -Wall -O0 -DPS_SYNC_LINUX -DPS_QUEUE_CUSTOM -DPS_QUEUE_BUCKET benchmark.c ../src/*.c -I../src -lpthread -lm -o benchmark.out && ./benchmark.out
	@echo
	gcc -g -Wall -O0 -DPS_SYNC_LINUX -DPS_QUEUE_CUSTOM -DPS_QUEUE_LL benchmark.c ../src/*.c -I../src -lpthread -lm -o benchmark.out && ./benchmark.out
	@echo
	gcc -g -Wall -O3 -DPS_SYNC_LINUX -DPS_QUEUE_CUSTOM -DPS_QUEUE_BUCKET benchmark.c ../src/*.c -I../src -lpthread -lm -o benchmark.out && ./benchmark.out
	@echo
	gcc -g -Wall -O3 -DPS_SYNC_LINUX -DPS_QUEUE_CUSTOM -DPS_QUEUE_LL benchmark.c ../src/*.c -I../src -lpthread -lm -o benchmark.out && ./benchmark.out
