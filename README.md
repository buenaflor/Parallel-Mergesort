# Recursive-Parallel-Mergesort
Recursive Parallel Mergesort in C using Forks, IPC (Pipes), exec

The program takes multiple lines as input and sorts them by using a recursive variant of merge sort
The input is read from stdin and ends when an EOF (End Of File) is encountered.

### Usage

Redirect textfile to stdin

```sh
$ cat 1.txt
Heinrich
Anton
Theodor
Dora
Hugo

$ ./forksort < 1.txt
Anton
Dora
Heinrich
Hugo
Theodor
```
