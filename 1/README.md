# Assignment 1
Usage:
```
gcc solution.c libcoro.c -o main
./main --coronums [number of coroutines] --quntum [quantum for yield for one coroutine] [names of files ...]
```

Example:
```
gcc solution.c libcoro.c
python3 generator.py -f test1.txt -c 10000 -m 10000
python3 generator.py -f test2.txt -c 10000 -m 10000
python3 generator.py -f test3.txt -c 10000 -m 10000
python3 generator.py -f test4.txt -c 10000 -m 10000
python3 generator.py -f test5.txt -c 10000 -m 10000
python3 generator.py -f test6.txt -c 100000 -m 10000
./a.out --coronums 3 --quntum 10 test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt
```
