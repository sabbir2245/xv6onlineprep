# Practice Problems — xv6 System Calls

**Index (jump to a problem):**

| # | Topic | Syscalls | Syscall #s | Sample command |
|---|-------|----------|-----------|----------------|
| [1](#problem-1--reverse-string) | Reverse String | `setReverseMode`, `reverseString` | 26/27 | `rev run hello` |
| [2](#problem-2--character-counter) | Character Counter | `setTargetChar`, `countChar` | 28/29 | `cnt run banana` |
| [3](#problem-3--vowel-remover) | Vowel Remover | `setVowelMode`, `removeVowels` | 30/31 | `vow run hello` |
| [4](#problem-4--password-checker) | Password Checker | `setPassword`, `checkPassword` | 32/33 | `pw check secret` |
| [5](#problem-5--running-sum) | Running Sum | `setBase`, `addToBase` | 34/35 | `sum add 5` |
| [Common Checklist](#common-checklist-all-problems) | All steps | — | — | — |

This document presents 5 practice problems modeled on the online exam format. Each
problem adds two system calls to xv6, mirroring the structure of the Caesar-cipher
exam so you can practice the full flow: `kernel/syscall.h` numbering, `kernel/sysproc.c`
handlers, `kernel/syscall.c` registration, `user/user.h` prototypes, `user/usys.pl`
stubs, a user program, and the `Makefile`.

> Note: syscall numbers 22/23 are already used (`SYS_trace`, `SYS_history`) and
> 24/25 are used by the cipher problem, so each practice problem below is assigned a
> fresh pair of numbers. Files are relative to the repo root.

---

## Problem 1 — Reverse String

### Question

Add two system calls to set a "reverse mode" flag in kernel memory and reverse a
string payload in place.

**System call 1 — `setReverseMode(int on)`**
Sets an internal kernel flag to `1` (on) or `0` (off).

**System call 2 — `reverseString(struct str_buf *buf)`**
If the reverse-mode flag is on, reverses the characters in `buf->data` in place;
otherwise returns without changing the buffer.

The structure is:

```c
struct str_buf {
  int  len;        // length of string
  char data[32];   // string buffer
};
```

**Two user commands:**

- `rev on`  → prints `Reverse mode on`
- `rev run text` → prints `Reversed : <text>`

### Sample I/O

```
$ rev on
Reverse mode on
$ rev run hello
Reversed : olleh
$ rev run xv6
Reversed : 6vx
$ rev off
Reverse mode off
$ rev run hello
Reversed : hello
```

### Solution

#### 1. `kernel/syscall.h`

```c
#define SYS_setReverseMode 26
#define SYS_reverseString  27
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"

// Kernel-wide reverse-mode flag.
static int reverse_mode = 0;

struct str_buf {
  int  len;
  char data[32];
};

uint64
sys_setReverseMode(void)
{
  int on;
  if(argint(0, &on) < 0)
    return -1;
  if(on != 0 && on != 1)
    return -1;
  reverse_mode = on;
  return 0;
}

uint64
sys_reverseString(void)
{
  struct str_buf ubuf, *buf;
  int i, j;
  char tmp;

  if(argaddr(0, (uint64 *)&buf) < 0)
    return -1;
  if(copyin(myproc()->pagetable, (char *)&ubuf, (uint64)buf,
            sizeof(ubuf)) < 0)
    return -1;
  if(ubuf.len > sizeof(ubuf.data))
    ubuf.len = sizeof(ubuf.data);

  if(reverse_mode){
    for(i = 0, j = ubuf.len - 1; i < j; i++, j--){
      tmp = ubuf.data[i];
      ubuf.data[i] = ubuf.data[j];
      ubuf.data[j] = tmp;
    }
  }

  if(copyout(myproc()->pagetable, (uint64)buf, (char *)&ubuf,
             sizeof(ubuf)) < 0)
    return -1;
  return 0;
}
```

#### 3. `kernel/syscall.c`

```c
extern uint64 sys_setReverseMode(void);
extern uint64 sys_reverseString(void);
```

```c
[SYS_setReverseMode]  sys_setReverseMode,
[SYS_reverseString]   sys_reverseString,
```

#### 4. `user/user.h`

```c
struct str_buf {
  int  len;
  char data[32];
};

int setReverseMode(int);
int reverseString(struct str_buf*);
```

#### 5. `user/usys.pl`

```perl
entry("setReverseMode");
entry("reverseString");
```

#### 6. `user/rev.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct str_buf buf;

  if(argc < 3){
    fprintf(2, "Usage: rev <on|off|run> <arg>\n");
    exit(1);
  }

  if(strcmp(argv[1], "on") == 0){
    if(setReverseMode(1) < 0){
      fprintf(2, "rev: invalid mode\n");
      exit(1);
    }
    printf("Reverse mode on\n");
  } else if(strcmp(argv[1], "off") == 0){
    if(setReverseMode(0) < 0){
      fprintf(2, "rev: invalid mode\n");
      exit(1);
    }
    printf("Reverse mode off\n");
  } else if(strcmp(argv[1], "run") == 0){
    memset(buf.data, 0, sizeof(buf.data));
    buf.len = strlen(argv[2]);
    if(buf.len > (int)sizeof(buf.data))
      buf.len = sizeof(buf.data);
    safestrcpy(buf.data, argv[2], sizeof(buf.data));
    if(reverseString(&buf) < 0){
      fprintf(2, "rev: reverse failed\n");
      exit(1);
    }
    printf("Reversed : %s\n", buf.data);
  } else {
    fprintf(2, "Usage: rev <on|off|run> <arg>\n");
    exit(1);
  }
  exit(0);
}
```

#### 7. `Makefile`

```make
	$U/_rev\
```

### Tips

- Only reverse when the kernel flag is `1`; otherwise copy the struct back unchanged.
- Guard `ubuf.len` so you never read/write past the 32-byte array.
- Use the two-pointer swap `i < j` to reverse in place.

---

## Problem 2 — Character Counter

### Question

Add two system calls to store a target character in kernel memory and count how many
times it appears in a string payload.

**System call 1 — `setTargetChar(int ch)`**
Stores a target byte (`ch`) in kernel memory.

**System call 2 — `countChar(struct msg_buffer *buf)`**
Counts occurrences of the stored target byte in `buf->data` and writes the result to
`buf->count`.

The structure is:

```c
struct msg_buffer {
  int  len;        // length of string
  char data[32];   // string buffer
  int  count;      // number of target char found
};
```

**Two user commands:**

- `cnt set c`  → prints `Target char set to c`
- `cnt run text` → prints `Count of <c> : <count>`

### Sample I/O

```
$ cnt set a
Target char set to a
$ cnt run banana
Count of a : 3
$ cnt set x
Target char set to x
$ cnt run xv6
Count of x : 1
```

### Solution

#### 1. `kernel/syscall.h`

```c
#define SYS_setTargetChar 28
#define SYS_countChar     29
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"

// Kernel-wide target character.
static char target_char = 0;

struct msg_buffer {
  int  len;
  char data[32];
  int  count;
};

uint64
sys_setTargetChar(void)
{
  int ch;
  if(argint(0, &ch) < 0)
    return -1;
  if(ch < 0 || ch > 255)
    return -1;
  target_char = (char)ch;
  return 0;
}

uint64
sys_countChar(void)
{
  struct msg_buffer ubuf, *buf;
  int i, n = 0;

  if(argaddr(0, (uint64 *)&buf) < 0)
    return -1;
  if(copyin(myproc()->pagetable, (char *)&ubuf, (uint64)buf,
            sizeof(ubuf)) < 0)
    return -1;
  if(ubuf.len > sizeof(ubuf.data))
    ubuf.len = sizeof(ubuf.data);

  for(i = 0; i < ubuf.len; i++)
    if(ubuf.data[i] == target_char)
      n++;
  ubuf.count = n;

  if(copyout(myproc()->pagetable, (uint64)buf, (char *)&ubuf,
             sizeof(ubuf)) < 0)
    return -1;
  return 0;
}
```

#### 3. `kernel/syscall.c`

```c
extern uint64 sys_setTargetChar(void);
extern uint64 sys_countChar(void);
```

```c
[SYS_setTargetChar]  sys_setTargetChar,
[SYS_countChar]      sys_countChar,
```

#### 4. `user/user.h`

```c
struct msg_buffer {
  int  len;
  char data[32];
  int  count;
};

int setTargetChar(int);
int countChar(struct msg_buffer*);
```

#### 5. `user/usys.pl`

```perl
entry("setTargetChar");
entry("countChar");
```

#### 6. `user/cnt.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct msg_buffer buf;

  if(argc < 3){
    fprintf(2, "Usage: cnt <set|run> <arg>\n");
    exit(1);
  }

  if(strcmp(argv[1], "set") == 0){
    if(setTargetChar((int)argv[2][0]) < 0){
      fprintf(2, "cnt: invalid char\n");
      exit(1);
    }
    printf("Target char set to %c\n", argv[2][0]);
  } else if(strcmp(argv[1], "run") == 0){
    memset(buf.data, 0, sizeof(buf.data));
    buf.len = strlen(argv[2]);
    if(buf.len > (int)sizeof(buf.data))
      buf.len = sizeof(buf.data);
    safestrcpy(buf.data, argv[2], sizeof(buf.data));
    buf.count = 0;
    if(countChar(&buf) < 0){
      fprintf(2, "cnt: count failed\n");
      exit(1);
    }
    printf("Count of %c : %d\n", target_char == 0 ? '?' : target_char, buf.count);
  } else {
    fprintf(2, "Usage: cnt <set|run> <arg>\n");
    exit(1);
  }
  exit(0);
}
```

> The `target_char` symbol above is not visible in user space. In practice, track the
> target char in the user program and print it there, e.g. keep it in a local
> `char target` variable. The kernel only stores and uses it for the count.

#### 7. `Makefile`

```make
	$U/_cnt\
```

### Tips

- `argint` reads a 4-byte signed int; validate the range before narrowing to a char.
- Read the whole struct with `copyin`, update `ubuf.count`, then write it back with
  `copyout` so the count reaches user space.
- Don't dereference user pointers directly in the kernel.

---

## Problem 3 — Vowel Remover

### Question

Add two system calls to store an "enable" flag in kernel memory and remove all vowels
from a string payload in place.

**System call 1 — `setVowelMode(int on)`**
Sets an internal flag (`1` = remove vowels, `0` = leave unchanged).

**System call 2 — `removeVowels(struct str_buf *buf)`**
Removes all vowels (`a e i o u`, both cases) from `buf->data`, compacting the string,
and updates `buf->len`. If the flag is off, leaves the buffer unchanged.

The structure is:

```c
struct str_buf {
  int  len;        // length of string
  char data[32];   // string buffer
};
```

**Two user commands:**

- `vow on`  → prints `Vowel mode on`
- `vow run text` → prints `Stripped : <text>`

### Sample I/O

```
$ vow on
Vowel mode on
$ vow run hello
Stripped : hll
$ vow run xv6
Stripped : xv6
$ vow off
Vowel mode off
$ vow run hello
Stripped : hello
```

### Solution

#### 1. `kernel/syscall.h`

```c
#define SYS_setVowelMode 30
#define SYS_removeVowels 31
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"

static int vowel_mode = 0;

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
sys_setVowelMode(void)
{
  int on;
  if(argint(0, &on) < 0)
    return -1;
  if(on != 0 && on != 1)
    return -1;
  vowel_mode = on;
  return 0;
}

uint64
sys_removeVowels(void)
{
  struct str_buf ubuf, *buf;
  int i, w = 0;

  if(argaddr(0, (uint64 *)&buf) < 0)
    return -1;
  if(copyin(myproc()->pagetable, (char *)&ubuf, (uint64)buf,
            sizeof(ubuf)) < 0)
    return -1;
  if(ubuf.len > sizeof(ubuf.data))
    ubuf.len = sizeof(ubuf.data);

  if(vowel_mode){
    for(i = 0; i < ubuf.len; i++){
      if(!is_vowel(ubuf.data[i]))
        ubuf.data[w++] = ubuf.data[i];
    }
    ubuf.data[w] = '\0';
    ubuf.len = w;
  }

  if(copyout(myproc()->pagetable, (uint64)buf, (char *)&ubuf,
             sizeof(ubuf)) < 0)
    return -1;
  return 0;
}
```

#### 3. `kernel/syscall.c`

```c
extern uint64 sys_setVowelMode(void);
extern uint64 sys_removeVowels(void);
```

```c
[SYS_setVowelMode]  sys_setVowelMode,
[SYS_removeVowels]  sys_removeVowels,
```

#### 4. `user/user.h`

```c
struct str_buf {
  int  len;
  char data[32];
};

int setVowelMode(int);
int removeVowels(struct str_buf*);
```

#### 5. `user/usys.pl`

```perl
entry("setVowelMode");
entry("removeVowels");
```

#### 6. `user/vow.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct str_buf buf;

  if(argc < 3){
    fprintf(2, "Usage: vow <on|off|run> <arg>\n");
    exit(1);
  }

  if(strcmp(argv[1], "on") == 0){
    if(setVowelMode(1) < 0){
      fprintf(2, "vow: invalid mode\n");
      exit(1);
    }
    printf("Vowel mode on\n");
  } else if(strcmp(argv[1], "off") == 0){
    if(setVowelMode(0) < 0){
      fprintf(2, "vow: invalid mode\n");
      exit(1);
    }
    printf("Vowel mode off\n");
  } else if(strcmp(argv[1], "run") == 0){
    memset(buf.data, 0, sizeof(buf.data));
    buf.len = strlen(argv[2]);
    if(buf.len > (int)sizeof(buf.data))
      buf.len = sizeof(buf.data);
    safestrcpy(buf.data, argv[2], sizeof(buf.data));
    if(removeVowels(&buf) < 0){
      fprintf(2, "vow: strip failed\n");
      exit(1);
    }
    printf("Stripped : %s\n", buf.data);
  } else {
    fprintf(2, "Usage: vow <on|off|run> <arg>\n");
    exit(1);
  }
  exit(0);
}
```

#### 7. `Makefile`

```make
	$U/_vow\
```

### Tips

- Use a "write index" `w` to compact the string as you skip vowels.
- Null-terminate `ubuf.data` after compacting and update `ubuf.len`.
- Keep non-letters (digits, spaces) intact.

---

## Problem 4 — Password Checker

### Question

Add two system calls to store a password in kernel memory and validate a candidate
string against it.

**System call 1 — `setPassword(char *pw)`**
Copies a password string from user space into a kernel buffer (max 16 chars).

**System call 2 — `checkPassword(char *candidate, int *ok)`**
Compares `candidate` against the stored password; sets `*ok` to `1` if they match,
`0` otherwise.

**Two user commands:**

- `pw set secret`  → prints `Password set`
- `pw check guess` → prints `Match` or `No match`

### Sample I/O

```
$ pw set secret
Password set
$ pw check secret
Match
$ pw check wrong
No match
```

### Solution

#### 1. `kernel/syscall.h`

```c
#define SYS_setPassword   32
#define SYS_checkPassword 33
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"
#include "spinlock.h"
#include "string.h"

// Kernel-wide stored password.
static char password[16];
static int  has_password = 0;

uint64
sys_setPassword(void)
{
  char tmp[16];
  uint64 addr;
  int n;

  if(argaddr(0, &addr) < 0)
    return -1;
  // Copy at most one byte less than the buffer to guarantee termination.
  n = fetchstr(addr, tmp, sizeof(tmp));
  if(n < 0)
    return -1;
  safestrcpy(password, tmp, sizeof(password));
  has_password = 1;
  return 0;
}

uint64
sys_checkPassword(void)
{
  char tmp[16];
  uint64 addr;
  uint64 ok_addr;
  int ok;

  if(argaddr(0, &addr) < 0)
    return -1;
  if(argaddr(1, &ok_addr) < 0)
    return -1;
  if(fetchstr(addr, tmp, sizeof(tmp)) < 0)
    return -1;

  ok = (has_password && strncmp(tmp, password, sizeof(tmp)) == 0) ? 1 : 0;

  if(copyout(myproc()->pagetable, ok_addr, (char *)&ok, sizeof(ok)) < 0)
    return -1;
  return 0;
}
```

#### 3. `kernel/syscall.c`

```c
extern uint64 sys_setPassword(void);
extern uint64 sys_checkPassword(void);
```

```c
[SYS_setPassword]    sys_setPassword,
[SYS_checkPassword]  sys_checkPassword,
```

#### 4. `user/user.h`

```c
int setPassword(char*);
int checkPassword(char*, int*);
```

#### 5. `user/usys.pl`

```perl
entry("setPassword");
entry("checkPassword");
```

#### 6. `user/pw.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int ok;

  if(argc < 3){
    fprintf(2, "Usage: pw <set|check> <arg>\n");
    exit(1);
  }

  if(strcmp(argv[1], "set") == 0){
    if(setPassword(argv[2]) < 0){
      fprintf(2, "pw: set failed\n");
      exit(1);
    }
    printf("Password set\n");
  } else if(strcmp(argv[1], "check") == 0){
    if(checkPassword(argv[2], &ok) < 0){
      fprintf(2, "pw: check failed\n");
      exit(1);
    }
    printf("%s\n", ok ? "Match" : "No match");
  } else {
    fprintf(2, "Usage: pw <set|check> <arg>\n");
    exit(1);
  }
  exit(0);
}
```

#### 7. `Makefile`

```make
	$U/_pw\
```

### Tips

- Use `fetchstr` to pull a user string into the kernel; it bounds the copy.
- Always keep a null terminator in the kernel `password` buffer.
- Write the boolean result back to user memory with `copyout` since `ok` is an
  out-parameter.

---

## Problem 5 — Running Sum

### Question

Add two system calls to store an integer in kernel memory and compute a running sum
with a payload value.

**System call 1 — `setBase(int base)`**
Stores an integer `base` in kernel memory.

**System call 2 — `addToBase(int x, int *result)`**
Adds `x` to the stored base, stores the new value back as the base, and writes the
result (the new base) to `*result`.

**Two user commands:**

- `sum set 10`  → prints `Base set to 10`
- `sum add 5` → prints `Sum : 15`

### Sample I/O

```
$ sum set 10
Base set to 10
$ sum add 5
Sum : 15
$ sum add 7
Sum : 22
```

### Solution

#### 1. `kernel/syscall.h`

```c
#define SYS_setBase 34
#define SYS_addToBase 35
```

#### 2. `kernel/sysproc.c`

```c
#include "syscall.h"

static int base_value = 0;

uint64
sys_setBase(void)
{
  int base;
  if(argint(0, &base) < 0)
    return -1;
  base_value = base;
  return 0;
}

uint64
sys_addToBase(void)
{
  int x, result;
  uint64 result_addr;

  if(argint(0, &x) < 0)
    return -1;
  if(argaddr(1, &result_addr) < 0)
    return -1;

  base_value += x;
  result = base_value;

  if(copyout(myproc()->pagetable, result_addr, (char *)&result,
             sizeof(result)) < 0)
    return -1;
  return 0;
}
```

#### 3. `kernel/syscall.c`

```c
extern uint64 sys_setBase(void);
extern uint64 sys_addToBase(void);
```

```c
[SYS_setBase]    sys_setBase,
[SYS_addToBase]  sys_addToBase,
```

#### 4. `user/user.h`

```c
int setBase(int);
int addToBase(int, int*);
```

#### 5. `user/usys.pl`

```perl
entry("setBase");
entry("addToBase");
```

#### 6. `user/sum.c`

```c
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int result;

  if(argc < 3){
    fprintf(2, "Usage: sum <set|add> <arg>\n");
    exit(1);
  }

  if(strcmp(argv[1], "set") == 0){
    if(setBase(atoi(argv[2])) < 0){
      fprintf(2, "sum: set failed\n");
      exit(1);
    }
    printf("Base set to %d\n", atoi(argv[2]));
  } else if(strcmp(argv[1], "add") == 0){
    if(addToBase(atoi(argv[2]), &result) < 0){
      fprintf(2, "sum: add failed\n");
      exit(1);
    }
    printf("Sum : %d\n", result);
  } else {
    fprintf(2, "Usage: sum <set|add> <arg>\n");
    exit(1);
  }
  exit(0);
}
```

#### 7. `Makefile`

```make
	$U/_sum\
```

### Tips

- The base value is a single int in kernel memory; it persists across `add` calls.
- `result` is an out-parameter, so write it back with `copyout`.
- Keep the state inside the kernel, not in the user program, so it survives across
  separate invocations.

---

## Common Checklist (all problems)

- **`kernel/syscall.h`:** pick fresh unused numbers (22/23/24/25 taken).
- **`kernel/sysproc.c`:** implement handlers; never dereference user pointers.
- **`kernel/syscall.c`:** add `extern` declarations and entries in the `syscalls[]`
  array, keyed by the `SYS_*` macros.
- **`user/user.h`:** declare structs and function prototypes.
- **`user/usys.pl`:** add `entry("...");` lines to generate assembly stubs.
- **`user/<prog>.c`:** write the command program using the generated wrappers.
- **`Makefile`:** add `$U/_<prog>\` to `UPROGS`.
- Validate all integer/range inputs; bound all string and buffer copies.
- `make clean`, rebuild, then verify with the expected sample I/O.