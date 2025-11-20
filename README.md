# Operating-Systems-PA2

This project implements a simple concurrent hash table driven by commands in `commands.txt`. Build with `make` to produce the `chash` executable. When run, the program:

1. Reads each command from `commands.txt` and spawns a thread for it. Lines beginning with `threads,<count>,<start_priority>` adjust the starting priority without launching a worker.
2. Orders thread starts with a condition variable based on the provided priority, but signals the next thread immediately to allow overlap during hash computation. Every thread logs when it begins waiting and when it is awakened.
3. Protects the shared linked list with a reader-writer lock to allow safe concurrent searches and mutations.
4. Writes diagnostic timing information to `hash.log` while printing command results to stdout.

A final print of the table is emitted even if `commands.txt` does not end with a `print` command.
