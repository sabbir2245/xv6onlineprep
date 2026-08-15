# Online Exam — xv6 System Calls (Section C2, 25 minutes)

## Question

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

### Submission

```bash
git add --all
git diff HEAD > ../{studentID}.patch
```

---

## Solution

> Note: syscall numbers 22/23 are already used (`SYS_trace`, `SYS_history`), so the
> new calls use **24** and **25**. Below, files are relative to the repo root.

### 1. `kernel/syscall.h` — add syscall numbers

```c
#define SYS_setCipherKey     24
#define SYS_transformBuffer  25
```

### 2. `kernel/sysproc.c` — implement both syscalls

Add a global (kernel-wide) cipher key, the `msg_buffer` struct, and the handlers.

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

### 3. `kernel/syscall.c` — register the handlers

Add the extern declarations:

```c
extern uint64 sys_setCipherKey(void);
extern uint64 sys_transformBuffer(void);
```

Add to the `syscalls[]` array:

```c
[SYS_setCipherKey]     sys_setCipherKey,
[SYS_transformBuffer]  sys_transformBuffer,
```

### 4. `user/user.h` — user-facing prototypes

```c
struct msg_buffer {
  int  len;
  char data[32];
};

int setCipherKey(int);
int transformBuffer(struct msg_buffer*);
```

### 5. `user/usys.pl` — generate assembly stubs

```perl
entry("setCipherKey");
entry("transformBuffer");
```

### 6. `user/cipher.c` — the two user commands

One program handles both `key` and `run` subcommands.

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

### 7. `Makefile` — add the user program

Inside the `UPROGS=` list:

```make
	$U/_cipher\
```

---

## Verification (expected output)

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

## Tips / common pitfalls

- **Key range check:** return an error if `key` is outside 1..25.
- **Struct copied with `copyin`/`copyout`:** you cannot dereference a user pointer in
  the kernel; copy the whole struct in, modify it, and copy it back so `data` is
  updated in place.
- **Modulo arithmetic:** `(c - 'a' + key) % 26` handles the `z -> a` wrap for both
  upper and lower case.
- **Non-alpha skip:** leave digits, spaces and punctuation untouched (e.g. `xv6 -> yw6`).
- **Make sure to `make clean` and rebuild**, then `git add --all && git diff HEAD > {id}.patch`.