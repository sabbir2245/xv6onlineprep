# Previous Year Problems and Solutions — xv6

This document compiles all the previous-year online exam problems found in the
`previousyear/` directory and provides a full, worked solution for each one.

**Problems covered:**

| Section | Topic | Type |
| --- | --- | --- |
| A2/B2 | Character frequency counting | System calls |
| B1 | Pseudo-random sampling | System calls |
| C2 | Target-character frequency | System calls |
| C3 | Caesar cipher (shift key) | System calls |
| A1 | Preemptive SJF scheduler | Scheduler |
| C1 | Priority-based scheduler | Scheduler |

> Syscall numbers below start at 22. In a fresh repo `SYS_fork..SYS_uptime` are
> 1..21, and there are no pre-existing `trace`/`history` syscalls, so we use
> consecutive numbers beginning at 22. Adjust if your repo already uses some of them.
> Files are relative to the repo root.

---

## Problem 1 — A2/B2: Character Frequency Counting

### Question

xv6 has no built-in support for counting character frequencies in strings. Add a
system call to count the frequency of each character in a user-provided string and
return the result as an array.

**Add 1 system call:**

- `countFreq(char *str, struct freq_array *a)` — receives a null-terminated string
  from user space, counts the frequency of all characters (ASCII 0–127), and fills up
  the passed `struct freq_array`.

```c
struct freq_array {
  int counts[128]; // frequency counts for ASCII chars 0 to 127
};
```

**Add 1 user command:**

- `freqall someString`

### Sample I/O

```
$ freqall banana
Target string banana
(kernel space)
a: 3
(user space)
b: 1
n: 2
$ freqall HelloWorld
Target string HelloWorld
(kernel space)
H: 1
(user space)
W: 1
d: 1
e: 1
l: 3
o: 2
r: 1
```

You may safely assume that the supplied string will contain only a-z and A-Z.

### Solution

#### 1. `kernel/syscall.h`

```c
#define SYS_countFreq 22
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"
#include "spinlock.h"
#include "string.h"

struct freq_array {
  int counts[128];
};

uint64
sys_countFreq(void)
{
  char str[128];
  struct freq_array fa, *fa_user;
  uint64 str_addr, fa_addr;
  int i;

  if(argaddr(0, &str_addr) < 0 || argaddr(1, &fa_addr) < 0)
    return -1;

  // Copy the string in from user memory.
  if(fetchstr(str_addr, str, sizeof(str)) < 0)
    return -1;

  for(i = 0; i < 128; i++)
    fa.counts[i] = 0;
  for(i = 0; str[i] != '\0'; i++)
    fa.counts[(unsigned char)str[i]]++;

  if(copyout(myproc()->pagetable, fa_addr, (char *)&fa, sizeof(fa)) < 0)
    return -1;
  return 0;
}
```

#### 3. `kernel/syscall.c`

Add extern declaration and array entry:

```c
extern uint64 sys_countFreq(void);
```

```c
[SYS_countFreq] sys_countFreq,
```

#### 4. `user/user.h`

```c
struct freq_array {
  int counts[128];
};

int countFreq(char*, struct freq_array*);
```

#### 5. `user/usys.pl`

```perl
entry("countFreq");
```

#### 6. `user/freqall.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct freq_array fa;
  int i;

  if(argc < 2){
    fprintf(2, "Usage: freqall <string>\n");
    exit(1);
  }

  printf("Target string %s\n", argv[1]);
  printf("(kernel space)\n");

  if(countFreq(argv[1], &fa) < 0){
    fprintf(2, "freqall: countFreq failed\n");
    exit(1);
  }

  printf("(user space)\n");
  for(i = 0; i < 128; i++)
    if(fa.counts[i] > 0)
      printf("%c: %d\n", (char)i, fa.counts[i]);

  exit(0);
}
```

#### 7. `Makefile`

```make
	$U/_freqall\
```

### Tips

- Use `fetchstr` to safely copy the user string into the kernel.
- Zero the whole 128-entry array before counting.
- The output `(kernel space)` / `(user space)` lines are just printed by the user
  program; they don't reflect where the counting actually happens.

---

## Problem 2 — B1: Pseudo-Random Sampling

### Question

xv6 has no built-in pseudo-random number generator. Add a mechanism for generating
random numbers and returning a k-length array of elements chosen from an n-length
array.

**Add 2 system calls:**

1. `setSeed(int z)` — sets the seed for a pseudo-random number generator to `z`.
2. `sample(struct array *arr, int k)` — returns the `k` randomly selected numbers from
   the input array and updates the internal state.

```c
struct array{
  int len;        // total length of array
  int array[15];  // array elements (input)
  int selected[15]; // randomly selected array elements
};
```

**Policy:** calling `sample` iteratively first increases the seed by 1, and the
selected index is `seed % array->len`. This is done `k` times inside `sample`. The
return value is a pointer to a `struct array` object.

**Add 2 user commands:**

1. `seed n`
2. `sample k len [the_array elements]`

### Sample I/O

```
$ seed 2
The seed has been set to 2
$ sample 2 3 1 2 3
Sampled elements are [1, 2]
$ sample 3 5 10 20 30 40 50
Sampled elements are [50, 10, 20]
$ seed 12
The seed has been set to 12
$ sample 2 3 6 7 8
Sampled elements are [7, 8]
$ sample 3 1 5
Sampled elements are [5, 5, 5]
```

You can safely assume that the length of the array will be at most 15.

### Solution

#### 1. `kernel/syscall.h`

```c
#define SYS_setSeed 22
#define SYS_sample  23
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"
#include "spinlock.h"
#include "string.h"

static int seed_value = 0;

struct array {
  int len;
  int array[15];
  int selected[15];
};

uint64
sys_setSeed(void)
{
  int z;
  if(argint(0, &z) < 0)
    return -1;
  seed_value = z;
  return 0;
}

uint64
sys_sample(void)
{
  struct array uarr, *arr;
  uint64 arr_addr;
  int k, i;

  if(argaddr(0, &arr_addr) < 0 || argint(1, &k) < 0)
    return -1;
  if(copyin(myproc()->pagetable, (char *)&uarr, arr_addr, sizeof(uarr)) < 0)
    return -1;
  if(k < 0 || k > 15)
    return -1;

  for(i = 0; i < k; i++){
    seed_value++;                                  // increase seed by 1
    uarr.selected[i] = uarr.array[seed_value % uarr.len]; // selected index
  }
  uarr.len = k;                                    // result holds k selected items

  if(copyout(myproc()->pagetable, arr_addr, (char *)&uarr, sizeof(uarr)) < 0)
    return -1;
  return 0;
}
```

#### 3. `kernel/syscall.c`

```c
extern uint64 sys_setSeed(void);
extern uint64 sys_sample(void);
```

```c
[SYS_setSeed] sys_setSeed,
[SYS_sample]  sys_sample,
```

#### 4. `user/user.h`

```c
struct array {
  int len;
  int array[15];
  int selected[15];
};

int setSeed(int);
int sample(struct array*, int);
```

#### 5. `user/usys.pl`

```perl
entry("setSeed");
entry("sample");
```

#### 6. `user/rand.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct array arr;
  int k, i, j;

  if(strcmp(argv[1], "seed") == 0){
    setSeed(atoi(argv[2]));
    printf("The seed has been set to %d\n", atoi(argv[2]));
    exit(0);
  }

  // sample k len [elements...]
  k = atoi(argv[2]);
  arr.len = atoi(argv[3]);
  for(j = 0; j < arr.len; j++)
    arr.array[j] = atoi(argv[4 + j]);

  sample(&arr, k);

  printf("Sampled elements are [");
  for(i = 0; i < k; i++){
    if(i > 0) printf(", ");
    printf("%d", arr.selected[i]);
  }
  printf("]\n");
  exit(0);
}
```

#### 7. `Makefile`

```make
	$U/_rand\
```

### Tips

- The seed is a kernel-global; it persists across calls, which is why `sample 3 1 5`
  yields `[5, 5, 5]` (seed keeps incrementing but `% 1` is always 0).
- Read the whole struct in with `copyin`, fill `selected[]`, set `len = k`, and write
  it back with `copyout`.
- `seed % arr->len` when the seed is positive gives a valid index; the policy
  increments the seed before each selection.

---

## Problem 3 — C2: Target-Character Frequency

### Question

xv6 has no built-in functionality to count the frequency of a specific character in a
string. Add a mechanism to set a target character and then count how many times this
character appears in any user-supplied string.

**Add 2 system calls:**

1. `setTargetChar(char)` — sets the target character whose frequency will be counted
   in subsequent queries.
2. `countTargetFreq(char *str)` — returns the number of occurrences of the previously
   set target character in the input string `str`.

The target character is stored in kernel space. `countTargetFreq` receives a
null-terminated string from user space, counts the occurrences, and returns the count.

**Add 2 user commands:**

1. `target c`
2. `freq str`

### Sample I/O

```
$ target a
Target character set to 'a'
$ freq banana
Found 3 occurrences of target
$ target n
Target character set to 'n'
$ freq banana
Found 2 occurrences of target
$ freq apple
Found 0 occurrences of target
```

### Solution

#### 1. `kernel/syscall.h`

```c
#define SYS_setTargetChar    22
#define SYS_countTargetFreq  23
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"
#include "spinlock.h"
#include "string.h"

static char target_char = 0;   // stored in kernel space

uint64
sys_setTargetChar(void)
{
  int c;
  if(argint(0, &c) < 0)
    return -1;
  if(c < 0 || c > 255)
    return -1;
  target_char = (char)c;
  return 0;
}

uint64
sys_countTargetFreq(void)
{
  char str[128];
  uint64 str_addr;
  int i, count = 0;

  if(argaddr(0, &str_addr) < 0)
    return -1;
  if(fetchstr(str_addr, str, sizeof(str)) < 0)
    return -1;

  for(i = 0; str[i] != '\0'; i++)
    if(str[i] == target_char)
      count++;

  return count;
}
```

#### 3. `kernel/syscall.c`

```c
extern uint64 sys_setTargetChar(void);
extern uint64 sys_countTargetFreq(void);
```

```c
[SYS_setTargetChar]    sys_setTargetChar,
[SYS_countTargetFreq]  sys_countTargetFreq,
```

#### 4. `user/user.h`

```c
int setTargetChar(int);
int countTargetFreq(char*);
```

#### 5. `user/usys.pl`

```perl
entry("setTargetChar");
entry("countTargetFreq");
```

#### 6. `user/freq.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int count;

  if(argc < 3){
    fprintf(2, "Usage: target <c> | freq <str>\n");
    exit(1);
  }

  if(strcmp(argv[1], "target") == 0){
    setTargetChar((int)argv[2][0]);
    printf("Target character set to '%c'\n", argv[2][0]);
  } else if(strcmp(argv[1], "freq") == 0){
    count = countTargetFreq(argv[2]);
    printf("Found %d occurrences of target\n", count);
  } else {
    fprintf(2, "Usage: target <c> | freq <str>\n");
    exit(1);
  }
  exit(0);
}
```

#### 7. `Makefile`

```make
	$U/_freq\
```

### Tips

- The target char lives in kernel memory so it survives across separate `freq`
  invocations.
- `countTargetFreq` returns the count directly (in `a0`), unlike Problem 1 which used
  an out-struct.
- Reset the target to `0` and check it if you want to guard against an unset target.

---

## Problem 4 — A1: Preemptive SJF Scheduler

### Question

Implement the preemptive SJF (Shortest Job First) scheduling algorithm in xv6. The
scheduler should run the process with the shortest remaining job length, and it will
preempt a currently running process if a newly runnable process has a shorter remaining
time.

You are provided a user program `testloop.c`. The job length of this program is its
iteration count provided as its argument. For all other jobs, the default length is 10.
You will add fields in the proc structure for keeping track of job lengths.

**Sample Input:**

```
testloop 120 &;
testloop 110 &;
testloop 100 &;
ls
```

**Sample Output:**

```
Process 5: Starting 120 iterations at time 35
Process 8: Starting 110 iterations at time 36
Process 11: Starting 100 iterations at time 37
<output of ls, omitted for brevity>
Process 11: Finished at time 167
Process 8: Finished at time 216
Process 5: Finished at time 265
```

**Hints:**

- Set `CPUS := 1` in the Makefile.
- Observe the execution order: when a new job becomes runnable, the scheduler compares
  its remaining time with that of the currently running process. If the new job has a
  shorter remaining time, it preempts the current process. Since `ls` is the shortest
  job (default 10), it immediately starts running and completes. After that, the next
  shortest job completes, and so on.
- The remaining job length must be updated from within the kernel, not from user space.
  Modify `usertrap()` in `trap.c` to decrement the process's remaining time on each
  timer interrupt. If the remaining time reaches 0, don't wait for the process to
  finish — terminate it by calling `exit(0)`. Don't update the remaining time for
  processes with PID 1 and 2.

### Solution

#### 1. `Makefile`

```make
CPUS := 1
```

#### 2. `kernel/proc.h`

Add job-length fields to `struct proc`:

```c
  int job_length;     // total job length (remaining)
  int elapsed;        // ticks consumed so far (optional)
```

#### 3. `kernel/proc.c`

Initialize the fields in `allocproc()`:

```c
  p->job_length = 10;   // default length for all jobs
  p->elapsed = 0;
```

Add a `setlength` system call handler so `testloop` can set its own job length (see
`kernel/sysproc.c` below).

Modify the scheduler to pick the runnable process with the smallest remaining job
length:

```c
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    // enable interrupts on this processor
    intr_on();

    // SJF: find the runnable process with the smallest remaining job length.
    int min = 1 << 30;
    struct proc *chosen = 0;
    for(p = proc; p < &proc[NPROC]; p++){
      acquire(&p->lock);
      if(p->state == RUNNABLE && p->job_length < min){
        if(chosen != 0)
          release(&chosen->lock);
        chosen = p;
        min = p->job_length;
        continue;
      }
      release(&p->lock);
    }

    if(chosen){
      acquire(&chosen->lock);
      chosen->state = RUNNING;
      c->proc = chosen;
      swtch(&c->context, &chosen->context);
      c->proc = 0;
      release(&chosen->lock);
    }
  }
}
```

#### 4. `kernel/trap.c`

In `usertrap()`, inside the timer-interrupt block, decrement the running process's
remaining job length each tick, and terminate the process when it reaches 0:

```c
  if(which_dev == 2){
    // timer interrupt
    if(p->pid > 2){                       // don't update PIDs 1 and 2
      p->elapsed++;
      if(p->elapsed >= p->job_length){
        // job finished; terminate without waiting
        setkilled(p);
      }
    }
    yield();
  }
```

> Using `setkilled(p)` and letting the normal exit path run is simpler than calling
> `exit(0)` directly. If you prefer to call `exit(0)` directly from the trap, ensure
> you release the proper locks first.

#### 5. `kernel/sysproc.c`

Implement `sys_setlength` so `testloop` can record its job length:

```c
uint64
sys_setlength(void)
{
  int len;
  if(argint(0, &len) < 0 || len <= 0)
    return -1;
  myproc()->job_length = len;
  return 0;
}
```

#### 6. Registration

- `kernel/syscall.h`: `#define SYS_setlength 22`
- `kernel/syscall.c`: `extern uint64 sys_setlength(void);` and
  `[SYS_setlength] sys_setlength,`
- `user/user.h`: `int setlength(int);`
- `user/usys.pl`: `entry("setlength");`

`testloop.c` is provided and calls `setlength(iters)`.

### Tips

- Set `CPUS := 1` so the SJF behavior is deterministic.
- Reset `elapsed` (or keep it in `job_length` directly) — be careful to distinguish
  "initial job length" from "remaining".
- The scheduler must pick the min remaining-length runnable process on every scan so
  that a newly-arrived shorter job preempts the running one.

---

## Problem 5 — C1: Priority-Based Scheduler

### Question

Implement a simple priority-based scheduler. You are given `testloop.c`, which
simulates a long-running job by iterating a loop a given number of times. This program
takes two arguments: iteration count and priority. You will implement the system call
to set priority for each process.

The process with the highest priority will keep running till completion, unless another
process with higher priority arrives. In that case, the higher priority process will
run till completion.

- All processes have a default priority of 300.
- If a process consumes 10 clock ticks (timer interrupts) consecutively, decrement its
  priority by 1 and reset its consecutive clock ticks count to 0. (Check `usertrap()`.)
- If the priority reaches 0, no need to decrement it further.
- Avoid decrementing priority for PIDs 1 and 2.

You will also implement the `setpriority` system call used by the `testloop` program.

**Sample I/O:** See the provided `sampleio.txt` (priority is the second argument).

**Note:**

- Set `CPUS := 1` in the Makefile.
- Notice how the priorities change the execution order. At first, the process with the
  highest priority keeps running until it finishes. Then a process with priority 10
  runs for a while. As its priority gets decremented and becomes equal to the last
  remaining process, they both chase each other and run in RR order.

### Solution

#### 1. `Makefile`

```make
CPUS := 1
```

#### 2. `kernel/proc.h`

Add priority fields to `struct proc`:

```c
  int priority;       // higher value = higher priority, default 300
  int tick_count;     // consecutive timer ticks consumed
```

#### 3. `kernel/proc.c`

Initialize in `allocproc()`:

```c
  p->priority = 300;   // default priority
  p->tick_count = 0;
```

Modify the scheduler to always run the runnable process with the highest priority
(preemptive priority scheduling):

```c
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    intr_on();

    // Priority scheduling: pick the runnable process with the highest priority.
    int max = -1;
    struct proc *chosen = 0;
    for(p = proc; p < &proc[NPROC]; p++){
      acquire(&p->lock);
      if(p->state == RUNNABLE && p->priority > max){
        if(chosen != 0)
          release(&chosen->lock);
        chosen = p;
        max = p->priority;
        continue;
      }
      release(&p->lock);
    }

    if(chosen){
      acquire(&chosen->lock);
      chosen->state = RUNNING;
      c->proc = chosen;
      swtch(&c->context, &chosen->context);
      c->proc = 0;
      release(&chosen->lock);
    }
  }
}
```

> This scheduler runs the highest-priority runnable process exclusively. Because
> `yield()` is called on every timer interrupt, the scheduler re-scans and naturally
> preempts in favor of a newly arrived higher-priority process.

#### 4. `kernel/trap.c`

In `usertrap()`, inside the timer-interrupt block, track consecutive ticks and
decrement priority every 10 ticks:

```c
  if(which_dev == 2){
    if(p->pid > 2){                      // don't adjust PIDs 1 and 2
      p->tick_count++;
      if(p->tick_count >= 10){
        if(p->priority > 0)              // don't go below 0
          p->priority--;
        p->tick_count = 0;
      }
    }
    yield();
  }
```

#### 5. `kernel/sysproc.c`

Implement `sys_setpriority`:

```c
uint64
sys_setpriority(void)
{
  int pr;
  if(argint(0, &pr) < 0)
    return -1;
  if(pr < 0 || pr > 300)
    return -1;
  myproc()->priority = pr;
  return 0;
}
```

#### 6. Registration

- `kernel/syscall.h`: `#define SYS_setpriority 22`
- `kernel/syscall.c`: `extern uint64 sys_setpriority(void);` and
  `[SYS_setpriority] sys_setpriority,`
- `user/user.h`: `int setpriority(int);`
- `user/usys.pl`: `entry("setpriority");`

`testloop.c` (with the colored output) calls `setpriority(priority)`.

### Tips

- Set `CPUS := 1`.
- The "chase each other in RR order" behavior falls out naturally: once two runnable
  processes have equal priority, the scan simply alternates between them.
- Only decrement priority inside `usertrap` for timer interrupts, and skip PIDs 1 and 2.

---

## Problem 6 — C3: Caesar Cipher (Shift Key)

### Question

Add two system calls to store a single-byte shift key in kernel memory and apply a
standard Caesar cipher transformation to a string payload provided by a user
application via a struct pointer.

**System call 1 — `setCipherKey(int key)`**
Sets the internal kernel shift key (a value between 1 and 25).

**System call 2 — `transformBuffer(struct msg_buffer *buf)`**
Takes a pointer to a `msg_buffer` struct and shifts every alphabetic character in
`buf->data` forward by `key` positions (wrapping `z` to `a` and `Z` to `A`).
Non-alphabetic characters remain unchanged. Updates `buf->data` in place.

The structure is:

```c
struct msg_buffer {
  int  len;        // length of string
  char data[32];   // string buffer
};
```

**Two user commands:**

- `cipher key k`  → prints `Cipher key set to k`
- `cipher run text` → prints `Transformed : <text>`

### Sample I/O

```
$ cipher_key 3
Cipher key set to 3
$ cipher_run hello
Transformed : khoor
$ cipher_key 1
Cipher key set to 1
$ cipher_run Zebra
Transformed : Afbsb
$ cipher_run xv6
Transformed : yw6
```

### Solution

> Note: In the original exam repo, syscall numbers 22/23 were already used
> (`SYS_trace`, `SYS_history`), so this solution uses **24** and **25**. If your repo
> is fresh (22+ free), adjust the numbers accordingly. Files are relative to the repo
> root.

#### 1. `kernel/syscall.h`

```c
#define SYS_setCipherKey     24
#define SYS_transformBuffer  25
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"

// Kernel-wide Caesar shift key (a single byte in kernel memory).
static int cipher_key = 0;

struct msg_buffer {
  int  len;
  char data[32];
};

// System call 1: set the internal shift key.
uint64
sys_setCipherKey(void)
{
  int key;
  if(argint(0, &key) < 0)
    return -1;
  if(key < 1 || key > 25)      // key must be between 1 and 25
    return -1;
  cipher_key = key;
  return 0;
}

// System call 2: transform buf->data in place using a Caesar cipher.
uint64
sys_transformBuffer(void)
{
  struct msg_buffer ubuf, *buf;
  int i;
  char c;

  if(argaddr(0, (uint64 *)&buf) < 0)
    return -1;
  // Copy the struct in from user memory.
  if(copyin(myproc()->pagetable, (char *)&ubuf, (uint64)buf,
            sizeof(ubuf)) < 0)
    return -1;
  if(ubuf.len > sizeof(ubuf.data))
    ubuf.len = sizeof(ubuf.data);

  for(i = 0; i < ubuf.len; i++){
    c = ubuf.data[i];
    if(c >= 'a' && c <= 'z')
      c = 'a' + (c - 'a' + cipher_key) % 26;   // wrap 'z' -> 'a'
    else if(c >= 'A' && c <= 'Z')
      c = 'A' + (c - 'A' + cipher_key) % 26;   // wrap 'Z' -> 'A'
    ubuf.data[i] = c;                           // non-alpha unchanged
  }

  // Write the transformed struct back to user memory.
  if(copyout(myproc()->pagetable, (uint64)buf, (char *)&ubuf,
             sizeof(ubuf)) < 0)
    return -1;
  return 0;
}
```

#### 3. `kernel/syscall.c`

```c
extern uint64 sys_setCipherKey(void);
extern uint64 sys_transformBuffer(void);
```

```c
[SYS_setCipherKey]     sys_setCipherKey,
[SYS_transformBuffer]  sys_transformBuffer,
```

#### 4. `user/user.h`

```c
struct msg_buffer {
  int  len;
  char data[32];
};

int setCipherKey(int);
int transformBuffer(struct msg_buffer*);
```

#### 5. `user/usys.pl`

```perl
entry("setCipherKey");
entry("transformBuffer");
```

#### 6. `user/cipher.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct msg_buffer buf;
  int key, i;

  if(argc < 3){
    fprintf(2, "Usage: cipher <key|run> <arg>\n");
    exit(1);
  }

  if(strcmp(argv[1], "key") == 0){
    key = atoi(argv[2]);
    if(setCipherKey(key) < 0){
      fprintf(2, "cipher: invalid key\n");
      exit(1);
    }
    printf("Cipher key set to %d\n", key);
  } else if(strcmp(argv[1], "run") == 0){
    memset(buf.data, 0, sizeof(buf.data));
    buf.len = strlen(argv[2]);
    if(buf.len > (int)sizeof(buf.data))
      buf.len = sizeof(buf.data);
    safestrcpy(buf.data, argv[2], sizeof(buf.data));
    if(transformBuffer(&buf) < 0){
      fprintf(2, "cipher: transform failed\n");
      exit(1);
    }
    printf("Transformed : %s\n", buf.data);
  } else {
    fprintf(2, "Usage: cipher <key|run> <arg>\n");
    exit(1);
  }
  exit(0);
}
```

> `safestrcpy` is available in `kernel/string.c`; if you prefer, replace it with a
> manual copy loop.

#### 7. `Makefile`

Inside the `UPROGS=` list:

```make
	$U/_cipher\
```

### Verification (expected output)

```bash
$ cipher key 3
Cipher key set to 3
$ cipher run hello
Transformed : khoor
$ cipher key 1
Cipher key set to 1
$ cipher run Zebra
Transformed : Afbsb
$ cipher run xv6
Transformed : yw6
```

### Tips / common pitfalls

- **Key range check:** return an error if `key` is outside 1..25.
- **Struct copied with `copyin`/`copyout`:** you cannot dereference a user pointer in
  the kernel; copy the whole struct in, modify it, and copy it back so `data` is
  updated in place.
- **Modulo arithmetic:** `(c - 'a' + key) % 26` handles the `z -> a` wrap for both
  upper and lower case.
- **Non-alpha skip:** leave digits, spaces and punctuation untouched (e.g. `xv6 -> yw6`).

---

## General Submission (all problems)

```bash
git add --all
git diff HEAD > ../{studentID}.patch
```

Remember to run `make clean` before rebuilding, and verify against the sample I/O.