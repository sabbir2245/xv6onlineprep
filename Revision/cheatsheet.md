# xv6 System Call Cheatsheet (Exam)

> Schedulers (SJF / Priority / LJF / Ageing RR) are **out of syllabus** — ignore them.
> This cheatsheet covers the **system-call** flow used by every exam problem.

---

## 1. The 5 Files You Must Touch (per new syscall)

| File | What you add |
|------|--------------|
| `kernel/syscall.h` | `#define SYS_name N` (number) |
| `kernel/syscall.c` | `extern` prototype + `[SYS_name] sys_name,` entry |
| `kernel/sysproc.c` | `uint64 sys_name(void) { ... }` implementation |
| `user/user.h` | `int name(args);` prototype (+ any structs) |
| `user/usys.pl` | `entry("name");` |

Plus: the user program `.c` and its `$U/_prog\` line in the Makefile `UPROGS=` list.

---

## 2. Syscall Numbering

- Standard syscalls `SYS_fork..SYS_uptime` = **1..21**.
- Your new calls start at **22**, then 23, 24, … in order.
- If a repo already uses some numbers (e.g. `SYS_trace=22`, `SYS_history=23`),
  skip them and start at the next free number (as in the Caesar cipher problem → 24, 25).
- Number order does NOT need to match the file order — just keep them unique.

---

## 3. Register-to-Value Helpers (read call args)

```c
// Integer argument (arg n, 0-based) from trapframe->a[n]
int x;
if(argint(0, &x) < 0) return -1;

// Pointer / address argument
uint64 addr;
if(argaddr(0, &addr) < 0) return -1;
```

| Helper    | Read | Write to | Use for |
|-----------|------|----------|---------|
| `argint(n, int*)`  | register int   | `int*`   | ints, flags, numbers |
| `argaddr(n, uint64*)` | register addr | `uint64*`| pointers to string/struct |

---

## 4. Memory Transfer Helpers (kernel ↔ user)

> You can **never dereference a user pointer** inside the kernel. Always copy.

```c
// String in  : user string -> kernel char buffer (null-terminated)
char str[128];
if(fetchstr(str_addr, str, sizeof(str)) < 0) return -1;

// Struct in  : user struct -> kernel struct
struct foo u, *u_addr;
if(copyin(myproc()->pagetable, (char *)&u, u_addr, sizeof(u)) < 0) return -1;

// Struct out : kernel struct -> user struct (write result back)
if(copyout(myproc()->pagetable, u_addr, (char *)&u, sizeof(u)) < 0) return -1;

// Scalar out : single int back to user
int result;
if(copyout(myproc()->pagetable, res_addr, (char *)&result, sizeof(result)) < 0)
  return -1;
```

| Helper    | Direction             | Data | Pattern |
|-----------|-----------------------|------|---------|
| `fetchstr`| user string → kernel  | `char[]` | strings |
| `copyin`  | user memory → kernel  | struct/array | copy struct in |
| `copyout` | kernel → user memory  | struct/array | write result back |

**Golden rule:** read the whole struct in with `copyin`, modify it in the kernel,
write it all back with `copyout`.

---

## 5. Standard `sys_xxx` Template

```c
#include "syscall.h"
#include "spinlock.h"
#include "string.h"

uint64
sys_mycall(void)
{
  struct mybuf ubuf;
  uint64 addr;
  int n, i;

  // 1. read integer and/or pointer args
  if(argint(0, &n) < 0) return -1;
  if(argaddr(0, &addr) < 0) return -1;

  // 2. bring data in
  if(copyin(myproc()->pagetable, (char *)&ubuf, addr, sizeof(ubuf)) < 0)
    return -1;

  // 3. do work (validate sizes first!)
  if(ubuf.len > (int)sizeof(ubuf.data)) ubuf.len = sizeof(ubuf.data);
  for(i = 0; i < ubuf.len; i++){ /* ... */ }

  // 4. write result back
  if(copyout(myproc()->pagetable, addr, (char *)&ubuf, sizeof(ubuf)) < 0)
    return -1;

  return 0;               // success; or return a computed count
}
```

---

## 6. Kernel-State Persistence

Static variables at file scope in `kernel/sysproc.c` persist across calls
(that's how "set a seed / target / mask / key / mode" problems work):

```c
static int  seed_value = 0;
static char target_char = 0;
static int  cipher_key = 0;
static int  strip_mode = 0;
```

The setter stores the value; the worker uses it on later calls.

---

## 7. Character / String Tricks

```c
// a-z or A-Z test
if(c >= 'a' && c <= 'z') ...
if(c >= 'A' && c <= 'Z') ...

// case swap (+32 / -32)
if(c >= 'a' && c <= 'z') c -= 32;   // to upper
else if(c >= 'A' && c <= 'Z') c += 32; // to lower

// vowel check
int is_vowel(char c){
  return c=='a'||c=='e'||c=='i'||c=='o'||c=='u'||
         c=='A'||c=='E'||c=='I'||c=='O'||c=='U';
}

// Caesar shift with wrap (z->a, Z->A), key 1..25
c = 'a' + (c - 'a' + key) % 26;
c = 'A' + (c - 'A' + key) % 26;

// compact while skipping chars (write-index w)
int w = 0;
for(i = 0; i < len; i++)
  if(keep(buf.data[i])) buf.data[w++] = buf.data[i];
buf.data[w] = '\0';
buf.len = w;

// read a char arg from command line
setTargetChar((int)argv[2][0]);
```

---

## 8. User Program Skeleton

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct mybuf buf;

  if(argc < 3){
    fprintf(2, "Usage: prog <op> <arg>\n");
    exit(1);
  }

  if(strcmp(argv[1], "op") == 0){
    // copy string into struct, call syscall, print
    buf.len = strlen(argv[2]);
    safestrcpy(buf.data, argv[2], sizeof(buf.data));
    if(mycall(&buf) < 0){ fprintf(2, "failed\n"); exit(1); }
    printf("Result : %s\n", buf.data);
  } else {
    fprintf(2, "Usage: prog <op> <arg>\n");
    exit(1);
  }
  exit(0);
}
```

- `atoi(argv[i])` — string to int.
- `strlen`, `safestrcpy`, `memset` — string helpers.
- Pass `&struct` to the syscall; the kernel fills it back.

---

## 9. Return-Value Conventions

| Return | Meaning |
|--------|---------|
| `0`  | success |
| `-1` | error (bad arg / copy fail / full / empty) |
| `count` | a computed number returned directly in `a0` (e.g. frequency, replacements) |

`myproc()` gives the current process; use it only for `pagetable` in copyin/copyout.

---

## 10. Submission

```bash
make clean
make
# test against sample I/O
git add --all
git diff HEAD > ../{studentID}.patch
```

---

## Quick Checklist (before finishing a syscall)

- [ ] syscall.h `#define` (unique number ≥ 22)
- [ ] syscall.c `extern` + array entry
- [ ] sysproc.c `sys_name()` implemented
- [ ] user.h prototype (+ structs)
- [ ] usys.pl `entry(...)`
- [ ] user program `.c` created
- [ ] Makefile `$U/_prog\` added
- [ ] `make clean && make` builds
- [ ] output matches sample I/O

---

## Flow Explanation (to the Teacher / Viva)

Use this short script to explain how one of your syscalls works end-to-end.
Walk through it in order — it covers the whole journey from user command to result.

### 1. User side — the command

> "I run `myprog op arg` on the shell. `main()` in `user/myprog.c` parses the
> argument with `argc`/`argv` and calls the syscall wrapper `mycall(...)`."

### 2. User → kernel — the trap

> "The wrapper in `user.h`/`usys.pl` makes a trap into the kernel. The args are
> passed in CPU registers `a0`, `a1`, `a2` and saved in the process trapframe."

### 3. Dispatch — syscall.c

> "The kernel looks up the syscall number in the `syscalls[]` table
> (`[SYS_mycall] sys_mycall,`) and calls the handler `sys_mycall()`."

### 4. Read the args — argint / argaddr

> "Inside `sys_mycall()`, I pull the values out of the trapframe:
> `argint(0,&n)` for a number, `argaddr(0,&addr)` for a pointer. The user pointer
> is a *virtual* address — I must not dereference it directly."

### 5. Copy data in — fetchstr / copyin

> "Because it's user memory, I copy it safely into the kernel:
> `fetchstr` for a string, or `copyin` to copy a whole struct. Now I can work on
> a kernel copy."

### 6. Do the work — kernel logic

> "I validate the size (e.g. cap `len` to the buffer), then apply the algorithm:
> loop over characters, count / shift / replace / filter as required."

### 7. Copy result out — copyout

> "I write the modified struct back to the user's address with `copyout`, so the
> user's original buffer now holds the result."

### 8. Return

> "I return `0` on success, `-1` on failure, or a computed count — this lands in
> `a0` and comes back to the user program."

### 9. Back on the user side

> "Back in `main()`, the user program prints the result, matching the expected
> sample I/O."

### One-liner summary

> "User calls wrapper → trap into kernel → dispatch table routes to `sys_xxx` →
> read args from registers → `copyin`/`fetchstr` to bring data in → compute →
> `copyout` to send the result back → return to user."