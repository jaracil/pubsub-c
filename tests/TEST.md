# Testing

For running the tests it is necessary to run `make` witch will generate a binary `tests.out`. 
By running this binary it will run the tests cases and generate `pubsub.c.gcov ` witch contains the trace of the execution to show the test coverage. 

Executing `gcov pubsub.c` will show a summary of the execution state. As an alternative it is possible to execute `make coverage` witch will to all this steps automatically.