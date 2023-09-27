# Assignment 1
Usage:
```
gcc solution.c libcoro.c -o main
./main --coronums [number of coroutines] --quntum [quantum for yield for one coroutine] [names of files ...]
```
Please don't set argument --corunums, it is not available yet
Example:
```
gcc solution.c libcoro.c -o main
./main test1.txt test2.txt test3.txt --quntum 10000

```
