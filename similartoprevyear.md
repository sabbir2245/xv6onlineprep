# Practice Problems and Solutions — xv6 (Similar Difficulty)

This document contains 5 practice problems designed to match the difficulty of the
previous-year online exams. Each one mirrors the same style and workload: 1–2 system
calls (or a scheduler change), a user program, and full kernel registration. Use them
to rehearse the complete flow before the real exam.

**Problems covered:**

| # | Topic | Type |
| --- | --- | --- |
| 1 | Vowel stripper | System calls |
| 2 | Running average | System calls |
| 3 | Character replacer | System calls |
| 4 | Longest job first scheduler | Scheduler |
| 5 | Ageing round-robin scheduler | Scheduler |

> Syscall numbers below start at 22 (after the standard 1..21). Files are relative to
> the repo root.

---

## Problem 1 — Vowel Stripper

### Question

xv6 has no built-in support for removing vowels from strings. Add a mechanism to strip
all vowels from a user-supplied string and return the cleaned string.

**Add 2 system calls:**

1. `setStripMode(int on)` — sets an internal kernel flag: `1` = strip vowels,
   `0` = leave the string unchanged.
2. `stripVowels(struct str_buf *buf)` — removes all vowels (`a e i o u`, both cases)
   from `buf->data`, compacts the string, and updates `buf->len`.

```c
struct str_buf {
  int  len;        // length of string
  char data[32];   // string buffer
};
```

**Add 2 user commands:**

1. `strip on`
2. `strip run text`

### Sample I/O

```
$ strip on
Strip mode on
$ strip run hello
Stripped : hll
$ strip run xv6
Stripped : xv6
$ strip off
Strip mode off
$ strip run hello
Stripped : hello
```

### Solution

#### 1. `kernel/syscall.h`

```c
#define SYS_setStripMode 22
#define SYS_stripVowels  23
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"
#include "string.h"

static int strip_mode = 0;

struct str_buf {
  int  len;
  char data[32];
};

static int
is_vowel(char c)
{
  return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
         c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U';
}

uint64
sys_setStripMode(void)
{
  int on;
  if(argint(0, &on) < 0)
    return -1;
  if(on != 0 && on != 1)
    return -1;
  strip_mode = on;
  return 0;
}

uint64
sys_stripVowels(void)
{
  struct str_buf ubuf, *buf;
  uint64 addr;
  int i, w = 0;

  if(argaddr(0, &addr) < 0)
    return -1;
  if(copyin(myproc()->pagetable, (char *)&ubuf, addr, sizeof(ubuf)) < 0)
    return -1;
  if(ubuf.len > (int)sizeof(ubuf.data))
    ubuf.len = sizeof(ubuf.data);

  if(strip_mode){
    for(i = 0; i < ubuf.len; i++)
      if(!is_vowel(ubuf.data[i]))
        ubuf.data[w++] = ubuf.data[i];
    ubuf.data[w] = '\0';
    ubuf.len = w;
  }

  if(copyout(myproc()->pagetable, addr, (char *)&ubuf, sizeof(ubuf)) < 0)
    return -1;
  return 0;
}
```

#### 3. `kernel/syscall.c`

```c
extern uint64 sys_setStripMode(void);
extern uint64 sys_stripVowels(void);
```

```c
[SYS_setStripMode] sys_setStripMode,
[SYS_stripVowels]  sys_stripVowels,
```

#### 4. `user/user.h`

```c
struct str_buf {
  int  len;
  char data[32];
};

int setStripMode(int);
int stripVowels(struct str_buf*);
```

#### 5. `user/usys.pl`

```perl
entry("setStripMode");
entry("stripVowels");
```

#### 6. `user/strip.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct str_buf buf;

  if(argc < 3){
    fprintf(2, "Usage: strip <on|off|run> <arg>\n");
    exit(1);
  }

  if(strcmp(argv[1], "on") == 0){
    if(setStripMode(1) < 0){
      fprintf(2, "strip: invalid mode\n");
      exit(1);
    }
    printf("Strip mode on\n");
  } else if(strcmp(argv[1], "off") == 0){
    if(setStripMode(0) < 0){
      fprintf(2, "strip: invalid mode\n");
      exit(1);
    }
    printf("Strip mode off\n");
  } else if(strcmp(argv[1], "run") == 0){
    memset(buf.data, 0, sizeof(buf.data));
    buf.len = strlen(argv[2]);
    if(buf.len > (int)sizeof(buf.data))
      buf.len = sizeof(buf.data);
    safestrcpy(buf.data, argv[2], sizeof(buf.data));
    if(stripVowels(&buf) < 0){
      fprintf(2, "strip: strip failed\n");
      exit(1);
    }
    printf("Stripped : %s\n", buf.data);
  } else {
    fprintf(2, "Usage: strip <on|off|run> <arg>\n");
    exit(1);
  }
  exit(0);
}
```

#### 7. `Makefile`

```make
	$U/_strip\
```

### Tips

- Use a write index `w` to compact the string while skipping vowels.
- Null-terminate `ubuf.data` and update `ubuf.len` after compacting.
- Guard `ubuf.len` so you never overrun the 32-byte array.

---

## Problem 2 — Running Average

### Question

xv6 has no built-in support for aggregating numbers across calls. Add a mechanism to
accumulate integers in kernel memory and return the running average.

**Add 2 system calls:**

1. `setAccum(int reset)` — if `reset` is nonzero, clears the stored accumulator
   (sum and count) to zero.
2. `addAccum(int x, int *avg)` — adds `x` to the stored sum, increments the count,
   and writes the new running average (sum / count) to `*avg`.

**Add 2 user commands:**

1. `avg reset`
2. `avg add x`

### Sample I/O

```
$ avg reset
Accumulator reset
$ avg add 10
Average : 10
$ avg add 20
Average : 15
$ avg add 30
Average : 20
$ avg reset
Accumulator reset
$ avg add 7
Average : 7
```

### Solution

#### 1. `kernel/syscall.h`

```c
#define SYS_setAccum 24
#define SYS_addAccum 25
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"

static int accum_sum = 0;
static int accum_count = 0;

uint64
sys_setAccum(void)
{
  int reset;
  if(argint(0, &reset) < 0)
    return -1;
  if(reset != 0){
    accum_sum = 0;
    accum_count = 0;
  }
  return 0;
}

uint64
sys_addAccum(void)
{
  int x, avg;
  uint64 avg_addr;

  if(argint(0, &x) < 0)
    return -1;
  if(argaddr(1, &avg_addr) < 0)
    return -1;

  accum_sum += x;
  accum_count++;
  avg = accum_sum / accum_count;

  if(copyout(myproc()->pagetable, avg_addr, (char *)&avg, sizeof(avg)) < 0)
    return -1;
  return 0;
}
```

#### 3. `kernel/syscall.c`

```c
extern uint64 sys_setAccum(void);
extern uint64 sys_addAccum(void);
```

```c
[SYS_setAccum] sys_setAccum,
[SYS_addAccum] sys_addAccum,
```

#### 4. `user/user.h`

```c
int setAccum(int);
int addAccum(int, int*);
```

#### 5. `user/usys.pl`

```perl
entry("setAccum");
entry("addAccum");
```

#### 6. `user/avg.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int avg;

  if(argc < 3){
    fprintf(2, "Usage: avg <reset|add> <arg>\n");
    exit(1);
  }

  if(strcmp(argv[1], "reset") == 0){
    setAccum(1);
    printf("Accumulator reset\n");
  } else if(strcmp(argv[1], "add") == 0){
    if(addAccum(atoi(argv[2]), &avg) < 0){
      fprintf(2, "avg: add failed\n");
      exit(1);
    }
    printf("Average : %d\n", avg);
  } else {
    fprintf(2, "Usage: avg <reset|add> <arg>\n");
    exit(1);
  }
  exit(0);
}
```

#### 7. `Makefile`

```make
	$U/_avg\
```

### Tips

- Keep `sum` and `count` as kernel-globals so the average persists across calls.
- Integer division truncates, which matches the expected output (e.g. `(10+20)/2 = 15`).
- Write the out-parameter `avg` back with `copyout`.

---

## Problem 3 — Character Replacer

### Question

xv6 has no built-in support for replacing characters in strings. Add a mechanism to set
an old and a new character, then replace all occurrences of the old character with the
new one in a user-supplied string.

**Add 2 system calls:**

1. `setReplacePair(int oldc, int newc)` — stores the pair of characters to replace.
2. `replaceChar(char *str, struct rep_result *r)` — copies `str`, replaces every
   occurrence of the old char with the new char, and reports how many replacements were
   made.

```c
struct rep_result {
  char data[32];  // the replaced string
  int  count;     // number of replacements made
};
```

**Add 2 user commands:**

1. `rep set a b`
2. `rep run text`

### Sample I/O

```
$ rep set a o
Replace set: a -> o
$ rep run banana
Replaced : bonono (3 replacements)
$ rep set x y
Replace set: x -> y
$ rep run xv6
Replaced : yv6 (1 replacements)
```

### Solution

#### 1. `kernel/syscall.h`

```c
#define SYS_setReplacePair 26
#define SYS_replaceChar    27
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"
#include "string.h"

static char rep_old = 0;
static char rep_new = 0;

struct rep_result {
  char data[32];
  int  count;
};

uint64
sys_setReplacePair(void)
{
  int oldc, newc;
  if(argint(0, &oldc) < 0 || argint(1, &newc) < 0)
    return -1;
  if(oldc < 0 || oldc > 255 || newc < 0 || newc > 255)
    return -1;
  rep_old = (char)oldc;
  rep_new = (char)newc;
  return 0;
}

uint64
sys_replaceChar(void)
{
  char str[32];
  struct rep_result res, *r;
  uint64 str_addr, res_addr;
  int i, cnt = 0;

  if(argaddr(0, &str_addr) < 0 || argaddr(1, &res_addr) < 0)
    return -1;
  if(fetchstr(str_addr, str, sizeof(str)) < 0)
    return -1;

  for(i = 0; str[i] != '\0'; i++){
    if(str[i] == rep_old){
      res.data[i] = rep_new;
      cnt++;
    } else {
      res.data[i] = str[i];
    }
  }
  res.data[i] = '\0';
  res.count = cnt;

  if(copyout(myproc()->pagetable, res_addr, (char *)&res, sizeof(res)) < 0)
    return -1;
  return 0;
}
```

#### 3. `kernel/syscall.c`

```c
extern uint64 sys_setReplacePair(void);
extern uint64 sys_replaceChar(void);
```

```c
[SYS_setReplacePair] sys_setReplacePair,
[SYS_replaceChar]    sys_replaceChar,
```

#### 4. `user/user.h`

```c
struct rep_result {
  char data[32];
  int  count;
};

int setReplacePair(int, int);
int replaceChar(char*, struct rep_result*);
```

#### 5. `user/usys.pl`

```perl
entry("setReplacePair");
entry("replaceChar");
```

#### 6. `user/rep.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct rep_result res;

  if(strcmp(argv[1], "set") == 0){
    setReplacePair((int)argv[2][0], (int)argv[3][0]);
    printf("Replace set: %c -> %c\n", argv[2][0], argv[3][0]);
    exit(0);
  }

  if(strcmp(argv[1], "run") == 0){
    if(replaceChar(argv[2], &res) < 0){
      fprintf(2, "rep: replace failed\n");
      exit(1);
    }
    printf("Replaced : %s (%d replacements)\n", res.data, res.count);
  } else {
    fprintf(2, "Usage: rep <set old new | run text>\n");
    exit(1);
  }
  exit(0);
}
```

#### 7. `Makefile`

```make
	$U/_rep\
```

### Tips

- The old/new pair lives in kernel memory, so it persists across `run` calls.
- `fetchstr` bounds the copy; make sure the result buffer is null-terminated.
- The `count` is an out-field written back through the struct.

---

## Problem 4 — Longest Job First Scheduler

### Question

Implement a Longest Job First (LJF) scheduling algorithm in xv6. The scheduler should
run the runnable process with the **largest** remaining job length to completion. If a
newly runnable process has a longer remaining time than the running process, it should
preempt it.

You are provided `testloop.c`, whose job length is its iteration count argument. For all
other jobs the default length is 10. Add fields to `struct proc` for job lengths.

**Sample Input:**

```
testloop 100 &;
testloop 50 &;
testloop 20 &;
ls
```

**Sample Output:**

```
Process 5: Starting 100 iterations at time 35
Process 8: Starting 50 iterations at time 36
Process 11: Starting 20 iterations at time 37
<output of ls, omitted for brevity>
Process 5: Finished at time 265
Process 8: Finished at time 216
Process 11: Finished at time 167
```

**Hints:**

- Set `CPUS := 1` in the Makefile.
- The remaining job length must be decremented from within the kernel on each timer
  interrupt (`usertrap()` in `trap.c`). When it reaches 0, terminate the process via
  `setkilled(p)`. Don't update PIDs 1 and 2.
- Since the longest job runs first, job 5 (100) finishes before jobs 8 and 11.

### Solution

#### 1. `Makefile`

```make
CPUS := 1
```

#### 2. `kernel/proc.h`

```c
  int job_length;     // remaining job length (ticks)
  int elapsed;        // ticks consumed so far
```

#### 3. `kernel/proc.c`

Initialize in `allocproc()`:

```c
  p->job_length = 10;   // default length
  p->elapsed = 0;
```

Modify the scheduler to pick the runnable process with the **largest** remaining job
length:

```c
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    intr_on();

    int max = -1;
    struct proc *chosen = 0;
    for(p = proc; p < &proc[NPROC]; p++){
      acquire(&p->lock);
      if(p->state == RUNNABLE && p->job_length > max){
        if(chosen != 0)
          release(&chosen->lock);
        chosen = p;
        max = p->job_length;
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

```c
  if(which_dev == 2){
    if(p->pid > 2){
      p->elapsed++;
      if(p->elapsed >= p->job_length)
        setkilled(p);
    }
    yield();
  }
```

#### 5. `kernel/sysproc.c`

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

- `kernel/syscall.h`: `#define SYS_setlength 28`
- `kernel/syscall.c`: `extern uint64 sys_setlength(void);` and
  `[SYS_setlength] sys_setlength,`
- `user/user.h`: `int setlength(int);`
- `user/usys.pl`: `entry("setlength");`

### Tips

- Pick the **max** remaining length (unlike SJF, which picks the min).
- With `CPUS := 1` the LJF order is deterministic.
- Skip PIDs 1 and 2 when updating job lengths.

---

## Problem 5 — Ageing Round-Robin Scheduler

### Question

Implement a round-robin scheduler with priority ageing in xv6. Each process has a
priority; the scheduler always runs the runnable process with the highest priority. To
avoid starvation, a process's priority is increased by 1 each time it is skipped by the
scheduler.

- All processes have a default priority of 0.
- When the scheduler selects a process, it must be the runnable one with the highest
  priority.
- Each time the scheduler scans the process table, every runnable process that was
  **not** selected has its priority increased by 1.
- When a process runs for 10 consecutive timer ticks, reset its consecutive-tick count
  to 0 and **yield** so another process gets a chance. Don't adjust PIDs 1 and 2.

You will implement the `setpriority` system call used by the `testloop` program.

**Sample I/O:** See the provided `sampleio.txt` (priority is the second argument).

### Solution

#### 1. `Makefile`

```make
CPUS := 1
```

#### 2. `kernel/proc.h`

```c
  int priority;       // higher value = higher priority, default 0
  int tick_count;     // consecutive timer ticks consumed
```

#### 3. `kernel/proc.c`

Initialize in `allocproc()`:

```c
  p->priority = 0;     // default priority
  p->tick_count = 0;
```

Modify the scheduler to run the highest-priority runnable process and age the others:

```c
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    intr_on();

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
      // Age every runnable process that is not selected.
      if(p->state == RUNNABLE && p != chosen)
        p->priority++;
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

> Because `yield()` is called on every timer interrupt (and every 10 ticks forces a
> yield), the scheduler re-scans frequently. Processes that keep getting skipped rise
> in priority until they are selected, preventing starvation.

#### 4. `kernel/trap.c`

```c
  if(which_dev == 2){
    if(p->pid > 2){
      p->tick_count++;
      if(p->tick_count >= 10){
        p->tick_count = 0;
        yield();          // give others a chance
      }
    }
    yield();
  }
```

#### 5. `kernel/sysproc.c`

```c
uint64
sys_setpriority(void)
{
  int pr;
  if(argint(0, &pr) < 0)
    return -1;
  myproc()->priority = pr;
  return 0;
}
```

#### 6. Registration

- `kernel/syscall.h`: `#define SYS_setpriority 29`
- `kernel/syscall.c`: `extern uint64 sys_setpriority(void);` and
  `[SYS_setpriority] sys_setpriority,`
- `user/user.h`: `int setpriority(int);`
- `user/usys.pl`: `entry("setpriority");`

`testloop.c` calls `setpriority(priority)`.

### Tips

- Ageing happens inside the scheduler scan: every runnable process not chosen gets
  `priority++`.
- Force a yield every 10 ticks so a long-running high-priority process doesn't hog the
  CPU forever.
- Skip PIDs 1 and 2 when tracking ticks.

---

## General Submission (all problems)

```bash
git add --all
git diff HEAD > ../{studentID}.patch
```

Remember to run `make clean` before rebuilding, and verify against the sample I/O.