# Assignment 1
Usage:
```
gcc solution.c libcoro.c -o main
./main --coronums [number of coroutines] --quntum [quantum for yield for one coroutine] [names of files ...]
```
Please don't set argument --corunums, it is not available yet. 

Example:
```
gcc solution.c libcoro.c -o main
python3 generator.py -f test1.txt -c 40000 -m 10000
python3 generator.py -f test2.txt -c 40000 -m 10000
python3 generator.py -f test3.txt -c 40000 -m 10000
./main test1.txt test2.txt test3.txt --quntum 10000
```
