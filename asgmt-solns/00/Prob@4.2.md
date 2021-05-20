This problem is solved by following two steps:

# Adding New command `dth` into the `cmdtable[]`

To add a new command to the kernel's operations 
sub-menu, we add a new entry of the form:

    { "cmd",    corresponding-function }

into the `struct cmdtable[]` at (roughly) `line 538` 
of `$OS161TOP/os161-1.99/kern/startup/menu.c`. In this 
case, a new line of

	{ "dth",    cmd_debugthreads },

was added at `line 548` at `menu.c`. In this way, when 
kernel recived command `dth` from command prompt, the 
corresponding function `cmd_debugthreads` will be 
called. Note that we havn't implement 
`cmd_debugthreads` yet, which will be introduced in 
next part.

# Implement `cmd_debugthreads`

## Prelimitary

As the question at `a0.pdf: subsection 4.2` asked:

> The new command, which must be called `dth`, should 
> enable the output of debugging messages of type
> `DB_THREADS`.

We can achieve this by manipulating the flag variable
`dbflags` defined at (roughly) `line 93` of 
`$OS161TOP/os161-1.99/kern/include/lib.h`:

    93 extern uint32_t dbflags;

Note that this variable is defined using keyword 
`extern`, which means we can modify it in *program*
scope. In this case, we will make use of this property
by updating its value at `menu.c`. (This value is 
*defined* at `line 43` of `$OS161TOP/os161-1.99/kern/
lib/kprintf.c`, see [this article on `extern`](https://www.geeksforgeeks.org/understanding-extern-keyword-in-c/)
for more)

## Set the Bit Flag

To **enable** the output of debugging messages 
(instead of print single message), we must
set the flag variable `dbflags` to include the value of
`DB_THREADS`. Note how the general macro on printing
debug messages implemented (`lib.h: line 111`):

    111 #define DEBUG(d, ...) ((dbflags & (d)) ? kprintf(__VA_ARGS__) : 0)

`dbflags` was bitwise-ANDed with predefined constant
bit flags (`DB_LOCORE`, `DB_SYSCALL`, etc from 
`lib.h: line 79 - 91`) so that multiple flags are 
supported for solving the problem from piazza post: 
[printing debug information of different categories](https://piazza.com/class/knqf0bfi5pq3q2?cid=27).

Thus, we should bitwise-OR all the desired constant bit
flag into the general debugging flag variable 
`dbflags`, see 
[this StackExahcange post](https://softwareengineering.stackexchange.com/questions/23852/bitwise-or-vs-adding-flags)
for more information. Hence the line

    531     dbflags | DB_THREADS;

was added into the implementation of function 
`cmd_debugthreads`.
