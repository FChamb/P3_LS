# CS3104 Practical 3 LS - 220010065 #

# Overview #
This project implements the `ls` command to stacsos which enables
viewing files in a provided directory. The command also excepts
the `-l` flag which prints additional details. The bottom of this
README has a section covering which parts of stacsos were changed
to enable this implementation.

# Setup #
Run make clean and make run to start up stacsos
```console
make clean
```
```console
make clean
```

# Running #
To run the ls implementation, use the following command:
```console
/usr/ls [-l] <path/to/directory>
```

# Example #
Below are some commands to run to test `ls`
```console
/usr/ls -l /tree
```
```console
/usr/ls /usr
```

# Changed Files #
The following files have been changed in order to enable the `ls`
implementation. The lines changed are provided and all the code
is commented in the respective sections.

1. `stacsos/user/Makefile` - {lines: 3-4} \
Added `ls` to the end of `apps := init` command to make the `ls`
directory


2. `stacsos/user/ls/src/main.cpp` - {entire file/working directory} \
Added an entry point for `ls` to process command line arguments and
print the output of the requested directory


3. `stacsos/user/ulib/inc/stacsos/user-syscall.h` - {lines: 90-91} \
Added a call to `ls` in syscall.cpp and enables how to access where
the compute logic for `ls` resides


4. `stacsos/kernel/inc/stacsos/kernel/fs/tar-filesystem.h` - {lines: 57-70} \
Added three additional helper functions which return a list of children
for a given node, the number of children, or the size of a node


5. `stacsos/lib/inc/stacsos/syscalls.h` - {lines: 32-33} \
Added an additional syscall_number for the `ls` command


6. `stacsos/kernel/src/syscall.cpp` - {lines: 187-299} \
All the logic for `ls` is added in an additional case: 
`case syscall_numbers::ls`