# xv6 Exam Quick Reference Card

One-page cheat-sheet for adding a system call to xv6 and submitting the patch.

---

## 1. The 7-Step Syscall Checklist

Follow in this exact order so you never miss a file.

| Step | File | What to add |
| --- | --- | --- |
| 1 | `kernel/syscall.h` | `#define SYS_<name> <num>` — pick the **next free** number (read the file first; 22/23 = trace/history, 24/25 = cipher may be taken) |
| 2 | `kernel/sysproc.c` | `uint64 sys_<name>(void) { ... }` handler. Read args with `argint`/`argaddr`/`fetchstr`; copy structs with `copyin`/`copyout`. **Never dereference user pointers.** |
| 3 | `kernel/syscall.c` | `extern uint64 sys_<name>(void);` **and** `[SYS_<name>] sys_<name>,` in the `syscalls[]` array |
| 4 | `user/user.h` | declare structs + `int <name>(...);` prototype |
| 5 | `user/usys.pl` | `entry("<name>");` (generates the user-mode stub) |
| 6 | `user/<prog>.c` | write the command program (use the generated wrapper `<name>()`) |
| 7 | `Makefile` | add `$U/_<prog>\` to the `UPROGS=` list |

**Golden rules:**
- Args → `argint(i,&int)` / `argaddr(i,&ptr)` / `argstr(i, buf, max)` / `argraw(i)`
- Kernel state that must persist → `static` kernel-global (target char, seed, key, base, password...)
- Out-params (values written back to user) → `copyout` the result
- Strings → `fetchstr`; structs → `copyin` then `copyout`
- Always bounds-check lengths and validate integer ranges before narrowing

---

## 2. The `syscall()` Instrumentation Pattern (for trace/history)

```c
num = p->trapframe->a7;
if(num > 0 && num < NELEM(syscalls) && syscalls[num]){
  for(i = 0; i < 6; i++) args[i] = argraw(i);   // capture BEFORE call
  start = syscall_ticks();                        // read ticks (with tickslock)
  p->trapframe->a0 = syscalls[num]();             // run the handler
  syscall_stat_record(num, syscall_ticks() - start); // count + accum time
  if(p->trace_sys_num == num)                    // only if tracing enabled
    printf("pid: %d, syscall: %s, args: (...), return: %d\n", ...);
}
```

- `trace_sys_num` field in `struct proc` (proc.h): init 0 in `allocproc`, reset in `freeproc`, copy in `fork`.
- Stats array: `struct syscall_stat_entry { spinlock lock; int count; int accum_time; }`, **one spinlock per syscall** (fine-grained locking). Init in `kernel/main.c` via `syscall_stats_init()`.
- `SYS_exit` special case: it never returns, record & trace before calling.
- `SYS_exec` special case: save pathname string before it runs.

---

## 3. Scheduler Change Pattern (SJF / LJF / Priority)

1. `Makefile` → `CPUS := 1` (deterministic).
2. `kernel/proc.h` → add fields to `struct proc` (e.g. `job_length`, `elapsed` or `priority`, `tick_count`).
3. `kernel/proc.c` → init fields in `allocproc()`; modify `scheduler()` loop to pick min (SJF) / max (LJF / priority) runnable process.
4. `kernel/trap.c` → in `usertrap()`, inside `if(which_dev == 2){ ... }` decrement/age fields on each tick; **skip PIDs 1 and 2**; `setkilled(p)` when done; `yield()`.
5. Register a `setlength` / `setpriority` syscall (steps 1–5 above) so `testloop.c` can set its own value.

---

## 4. Build + Test + Submit Flow

Start from a fresh copy of the repo:

```bash
git clone https://github.com/shuaibw/xv6-riscv --depth=1
```

Then build, test, and generate the patch:

```bash
make clean
make
make qemu           # boot xv6 in QEMU to run/test your program in the shell
# (exit QEMU with Ctrl+A then X)
make clean          # rebuild clean before generating patch
git add --all
git diff HEAD > ../{studentID}.patch
```

**Verify the patch against a fresh clone (test before submitting):**

```bash
cd ..                              # go outside your working repo
git clone https://github.com/shuaibw/xv6-riscv --depth=1 testing
cd testing
git apply ../{studentID}.patch     # apply your patch to the fresh clone
make qemu                          # boot and confirm your program works
```

**Submission rules (from assignment):**
- Patch file name = your 7-digit student ID, e.g. `2205192.patch`
- Do **not** commit your changes
- Submit only the `.patch` file — no repo, no zip
- Verify it applies to a fresh clone (see above): `git apply studentID.patch`
- Do not copy solutions from anyone (plagiarism = -100%)

---

## 5. Marking Scheme (100 total)

| Task | Sub-task | Marks |
| --- | --- | --- |
| Trace | Properly tracing system calls | 15 |
| Trace | Tracing only for the calling process | 10 |
| Trace | Printing system call name | 5 |
| Trace | Printing with system call arguments | 10 |
| History | Designing `history.c` | 10 |
| History | Counting system calls | 15 |
| History | Calculating system call time | 15 |
| History | Using appropriate locking | 15 |
| | Proper submission | 5 |

---

## 6. Common Pitfalls

- Wrong syscall number (conflicts with 22–25 already used) → read `syscall.h` first.
- Dereferencing a user pointer directly in the kernel → use `copyin`/`fetchstr`.
- Missing `entry()` in `usys.pl` → user program won't link.
- Missing array entry in `syscall.c` → "unknown sys call" at runtime.
- No lock / coarse lock on stats → lost updates on multi-CPU (fine-grained lock = +15).
- Forgetting `make clean` before `git diff` → stale patch.
- Printing from kernel mode for `history` → forbidden; print only in user mode.