# Understanding `2205040.patch`

## What a patch file is

A patch (`.patch`) file is a text diff produced by `git diff` (or `diff -u`) that
describes exactly what was changed between two versions of code. Each section:

- `diff --git a/... b/...` — which file changed.
- `--- a/...` / `+++ b/...` — the "old" vs "new" file.
- `@@ -start,count +start,count @@` — where in the file the change is located.
- Lines starting with `-` are removed; lines with `+` are added; plain lines are
  context (unchanged, shown for orientation).

Applying it (`git apply 2205040.patch`) turns the old code into the new code. This
patch adds a **system-call tracing + history feature** to an xv6 kernel.

## Files changed and what each does

### `Makefile`
- Adds `-march=rv64g` to `CFLAGS` and `ASFLAGS` (a build/architecture flag).
- Adds `_trace` and `_history` to the `UPROGS` list so the two new user programs are
  compiled and placed on the disk image `fs.img`.

### `kernel/syscall.h`
Adds two new syscall numbers:

```c
#define SYS_trace   22
#define SYS_history 23
```

### `kernel/syscall.c` — the core of the feature
- Declares and registers `sys_trace` and `sys_history` in the `syscalls[]` dispatch table.
- Adds a `syscall_names[]` array mapping every syscall number to a readable name (used
  for pretty printing).
- Adds a kernel-side statistics table `syscall_stats[]`, one entry per syscall, each
  guarded by its own `spinlock`, storing a `count` and accumulated time.
- `syscall_stats_init()` — initializes those locks (called from `main()`).
- `syscall_ticks()` — reads the global `ticks` counter under the `tickslock`.
- `syscall_stat_record()` / `syscall_stat_get()` — increment a counter and accumulate
  time, or read them back out (used by the `history` syscall).
- `trace_string()` / `trace_arguments()` — format a syscall's arguments for printing,
  handling each syscall's argument layout (strings vs pointers vs ints).
- The `syscall()` function itself is modified to:
  1. Pre-fetch all 6 arguments into `args[]` (and pre-save the `exec` path, because
     `exec` destroys the page table).
  2. Record the start tick, call the syscall, then record the elapsed time.
  3. If the process is being traced (`p->trace_sys_num == num`), print a line like:
     `pid: 3, syscall: write, args: (1, 0x.., 14), return: 14`.
  4. Handles `SYS_exit` specially (it never returns, so it records/prints before
     dispatching).

### `kernel/defs.h`
Declares the two helper functions `syscall_stats_init` and `syscall_stat_get` so other
kernel files can call them.

### `kernel/main.c`
Calls `syscall_stats_init()` during kernel boot.

### `kernel/proc.h`
Adds a field to the process struct:

```c
int trace_sys_num;  // System call number traced by this process.
```

### `kernel/proc.c`
- Initializes `trace_sys_num = 0` on process alloc and free.
- In `fork()`, the child **inherits** the parent's tracing state
  (`np->trace_sys_num = p->trace_sys_num`), so tracing survives `exec`/`fork`.

### `kernel/sysproc.c`
Implements the two new syscalls:
- `sys_trace()` — reads the syscall number argument, validates it (1..SYS_history),
  and sets `myproc()->trace_sys_num`.
- `sys_history()` — reads a syscall number and a user pointer; fills a
  `struct syscall_stat` (name/count/accum_time) from the kernel stats table and
  copies it back to user space via `copyout`.

### `user/trace.c` (new program)
A `trace <num> <command>` utility: it calls `trace(num)`, then `exec`s the command so
the traced process's syscalls get printed.

### `user/history.c` (new program)
A `history [num]` utility: prints per-syscall statistics (name, count, accumulated
time). With no argument it prints stats for all syscalls.

### `user/usys.pl`
Adds `entry("trace")` and `entry("history")` so the compiler generates the low-level
assembly stubs (`.global trace`, `li a7, SYS_trace; ecall; ret`).

### `user/user.h`
Declares the `struct syscall_stat` type and the user-space prototypes
`int trace(int)` and `int history(int, struct syscall_stat*)`.

### `user/ulib.c`
Improves `atoi()` to handle whitespace and a leading `+`/`-` sign (so negative /
signed numbers parse correctly).

### `user/usertests.c`
Small fix: changes the signature `rwsbrk()` → `rwsbrk(char *s)` to match how it is
called by the test framework.

## Summary — what the patch adds

1. Two new syscalls: `trace` (22) and `history` (23).
2. Per-syscall kernel statistics: call counts + accumulated time, protected by
   spinlocks, initialized at boot.
3. A syscall tracing mechanism where a process can ask the kernel to print every
   invocation of a given syscall with its arguments and return value.
4. Two user programs (`trace`, `history`) plus the build glue (Makefile, usys.pl,
   user.h).
5. Minor quality fixes to `atoi` and `usertests.c`.

It is a classic xv6-lab style change: add syscalls end-to-end (headers → handlers →
stubs → user programs) while introducing kernel concurrency (spinlocks) and user/kernel
copying (`copyin`/`copyout`).