all:
	gcc -Wall -fprofile-arcs -ftest-coverage  tests.c ../pubsub.c -I../ -lpthread -o tests.out
clean:
	rm *.gc*
	rm tests.out
coverage: all
	@echo '--------------------------------'
	./tests.out
	@echo '--------------------------------'
	gcov tests.c
	@echo '--------------------------------'
	gcov pubsub.c
	
