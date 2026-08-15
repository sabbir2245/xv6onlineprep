# Detailed Explanation of the XV6 System Call Tracing and Statistics Implementation

This patch adds system call tracing and statistics collection functionality to the XV6 operating system. Let me break down what's happening in detail.

## 🎯 High-Level Overview

This implementation adds two major features:

- **System Call Tracing** - Allows a process to trace a specific system call, printing its arguments and return value
- **System Call Statistics** - Collects and reports usage statistics (call count and total execution time) for each system call

The implementation includes:

- Two new system calls: `trace()` and `history()`
- Two new user programs: `trace` and `history`
- Kernel-level instrumentation to record timing and argument information

## 📂 File-by-File Breakdown

### 1. Makefile Changes

```makefile
CFLAGS += -march=rv64g
ASFLAGS += -march=rv64g
```

**Purpose:** Adds RISC-V 64-bit architecture support flags for compilation and assembly.

**User Programs Added:**

```makefile
UPROGS=\
    ...existing...
    $U/_trace\
    $U/_history\
```

Adds `trace` and `history` to the list of user-space programs to be built.

### 2. kernel/defs.h - Function Declarations

```c
void syscall_stats_init(void);
void syscall_stat_get(int, char *, int *, int *);
```

**Purpose:** Declares new kernel functions for:

- Initializing system call statistics data structures
- Retrieving statistics for a specific system call

### 3. kernel/main.c - Initialization

```c
syscall_stats_init(); // system call history locks and counters
```

**Purpose:** Initializes spinlocks for each system call's statistics entry during kernel boot. This ensures thread-safe access to statistics.

### 4. kernel/proc.h & proc.c - Process Structure

New field added to `struct proc`:

```c
int trace_sys_num;  // System call number traced by this process
```

**Changes in proc.c:**

- `allocproc()`: Initializes `trace_sys_num = 0` when allocating a new process
- `freeproc()`: Resets `trace_sys_num = 0` when freeing a process
- `fork()`: Inherits the `trace_sys_num` from parent to child

**Purpose:** Each process tracks which system call number it should trace (if any). This is inherited across forks.

### 5. kernel/syscall.c - Core Implementation

This is the heart of the implementation. Let's break it down:

#### A. New System Call Handlers

```c
extern uint64 sys_trace(void);
extern uint64 sys_history(void);
```

Declares handlers for the two new system calls.

#### B. System Call Name Mapping

```c
static char *syscall_names[] = {
    [SYS_fork] "fork", [SYS_exit] "exit", ...
    [SYS_trace] "trace", [SYS_history] "history",
};
```

**Purpose:** Maps system call numbers to human-readable names for tracing output.

#### C. Statistics Data Structure

```c
struct syscall_stat_entry {
    struct spinlock lock;     // Prevents race conditions
    int count;                // Number of times called
    int accum_time;           // Total execution time in ticks
};
static struct syscall_stat_entry syscall_stats[NELEM(syscalls)];
```

**Purpose:** Maintains per-system-call statistics with thread-safe locking.

#### D. Statistics Functions

```c
void syscall_stats_init(void) {
    for(i = 1; i < NELEM(syscalls); i++)
        initlock(&syscall_stats[i].lock, "syscall_stat");
}
```

Initializes spinlocks for each system call entry.

```c
static uint syscall_ticks(void) {
    // Returns current ticks value atomically
    acquire(&tickslock);
    now = ticks;
    release(&tickslock);
    return now;
}
```

Safely reads the current timer value.

```c
static void syscall_stat_record(int number, uint elapsed) {
    acquire(&syscall_stats[number].lock);
    syscall_stats[number].count++;
    syscall_stats[number].accum_time += elapsed;
    release(&syscall_stats[number].lock);
}
```

Records statistics for a system call execution.

#### E. Tracing Argument Printing

```c
static void trace_string(uint64 address) {
    char string[MAXPATH];
    if(fetchstr(address, string, sizeof(string)) < 0)
        printf("%p", (void *)address);
    else
        printf("%s", string);
}
```

Safely fetches and prints a string from user memory.

```c
static void trace_arguments(int number, uint64 args[]) {
    switch(number) {
        case SYS_fork: case SYS_getpid: case SYS_uptime:
            break;  // No arguments
        case SYS_exit: case SYS_kill:
            printf("%d", (int)args[0]); break;
        // ... more cases for each system call
    }
}
```

Prints arguments in the correct format for each system call type.

#### F. The Main syscall() Function - Instrumented

This is where the magic happens:

```c
void syscall(void) {
    int num;
    uint start_ticks;
    uint64 args[6];
    char exec_path[MAXPATH] = {0};
    struct proc *p = myproc();

    num = p->trapframe->a7;  // Get system call number

    if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        // 1. Capture arguments BEFORE system call executes
        for(i = 0; i < NELEM(args); i++)
            args[i] = argraw(i);

        // 2. Special handling for exec (save pathname)
        if(num == SYS_exec) {
            if(fetchstr(args[0], exec_path, sizeof(exec_path)) < 0)
                safestrcpy(exec_path, "???", sizeof(exec_path));
        }

        // 3. Start timing
        start_ticks = syscall_ticks();

        // 4. Special case: SYS_exit (never returns)
        if(num == SYS_exit) {
            syscall_stat_record(num, 0);
            if(p->trace_sys_num == num) {
                printf("pid: %d, syscall: %s, args: (%d), return: 0\n",
                       p->pid, syscall_names[num], (int)args[0]);
            }
            p->trapframe->a0 = syscalls[num]();
            return;
        }

        // 5. Execute the system call
        p->trapframe->a0 = syscalls[num]();

        // 6. Record statistics (execution time)
        syscall_stat_record(num, syscall_ticks() - start_ticks);

        // 7. Tracing output (if this process is tracing this syscall)
        if(p->trace_sys_num == num) {
            printf("pid: %d, syscall: %s, args: (", p->pid, syscall_names[num]);
            if(num == SYS_exec)
                printf("%s, %p", exec_path, (void *)args[1]);
            else
                trace_arguments(num, args);
            printf("), return: %d\n", (int)p->trapframe->a0);
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
    }
}
```

**Key Points:**

- Captures arguments BEFORE the system call executes
- Records execution time using ticks
- Special handling for `exec` (preserves pathname) and `exit` (never returns)
- Outputs tracing information if the process's `trace_sys_num` matches
- Always records statistics regardless of tracing

### 6. kernel/syscall.h - New System Call Numbers

```c
#define SYS_trace   22
#define SYS_history 23
```

Adds the new system call numbers to the enumeration.

### 7. kernel/sysproc.c - System Call Implementations

#### A. sys_trace()

```c
uint64 sys_trace(void) {
    int syscall_number;
    argint(0, &syscall_number);
    if(syscall_number <= 0 || syscall_number > SYS_history)
        return -1;
    myproc()->trace_sys_num = syscall_number;
    return 0;
}
```

**Purpose:** Sets the current process's `trace_sys_num`. When set to a valid system call number, the kernel will print tracing information for that system call.

**Behavior:**

- Takes one argument: the system call number to trace
- Returns `-1` on error, `0` on success

#### B. sys_history()

```c
uint64 sys_history(void) {
    int syscall_number;
    uint64 user_stat_address;
    struct syscall_stat stat;

    argint(0, &syscall_number);
    argaddr(1, &user_stat_address);
    if(syscall_number <= 0 || syscall_number > SYS_history)
        return -1;

    syscall_stat_get(syscall_number, stat.syscall_name, &stat.count,
                     &stat.accum_time);
    if(copyout(myproc()->pagetable, user_stat_address, (char *)&stat,
               sizeof(stat)) < 0)
        return -1;
    return 0;
}
```

**Purpose:** Retrieves statistics for a specific system call and copies them to user space.

**Behavior:**

- Takes two arguments: system call number and a pointer to a `struct syscall_stat`
- Returns `-1` on error, `0` on success
- Copies the statistics structure to user memory

### 8. user/trace.c - User Program

```c
int main(int argc, char *argv[]) {
    if(argc < 3) {
        fprintf(2, "Usage: %s sys_call_num command\n", argv[0]);
        exit(1);
    }

    if(trace(atoi(argv[1])) < 0) {
        fprintf(2, "%s: trace failed\n", argv[0]);
        exit(1);
    }

    // Build argument array for exec
    for(i = 2; i < argc && i - 2 < MAXARG - 1; i++)
        command_argv[i - 2] = argv[i];
    command_argv[i - 2] = 0;

    exec(command_argv[0], command_argv);
    fprintf(2, "%s: exec failed\n", command_argv[0]);
    exit(1);
}
```

**Purpose:** A user-space utility that enables system call tracing for another program.

- **Usage:** `trace <syscall_number> <command> [args...]`
- **Example:** `trace 1 ls` (traces the 'exit' system call when running `ls`)

**Behavior:**

- Sets the process's `trace_sys_num` using the `trace()` system call
- Executes the specified command with its arguments
- The kernel will print tracing information for the specified system call

### 9. user/history.c - User Program

```c
static void print_history(int syscall_number) {
    struct syscall_stat stat;

    if(history(syscall_number, &stat) < 0) {
        fprintf(2, "history: invalid syscall number %d\n", syscall_number);
        return;
    }
    printf("%d: syscall: %s, #: %d, time: %d\n", syscall_number,
           stat.syscall_name, stat.count, stat.accum_time);
}

int main(int argc, char *argv[]) {
    if(argc == 1) {
        // Print all system calls
        for(i = SYS_fork; i <= SYS_history; i++)
            print_history(i);
    } else if(argc == 2) {
        // Print specific system call
        print_history(atoi(argv[1]));
    } else {
        fprintf(2, "Usage: history [sys_call_num]\n");
        exit(1);
    }
    exit(0);
}
```

**Purpose:** A user-space utility that displays system call usage statistics.

**Usage:**

- `history` - Shows statistics for ALL system calls
- `history <number>` - Shows statistics for a specific system call

**Output format:** `<syscall_number>: syscall: <name>, #: <count>, time: <accumulated_time>`

### 10. user/ulib.c - atoi() Enhancement

```c
int atoi(const char *s) {
    int n;
    int sign = 1;

    // Skip whitespace
    while(*s == ' ' || *s == '\t')
        s++;

    // Handle sign
    if(*s == '-') { sign = -1; s++; }
    else if(*s == '+') { s++; }

    // Convert digits
    n = 0;
    while('0' <= *s && *s <= '9')
        n = n*10 + *s++ - '0';

    return n * sign;  // Now supports negative numbers
}
```

**Purpose:** Enhanced `atoi()` to handle negative numbers and whitespace.

### 11. user/user.h - New Declarations

```c
struct syscall_stat {
    char syscall_name[16];
    int count;
    int accum_time;
};

int trace(int);
int history(int, struct syscall_stat*);
```

- Defines the statistics structure for user-space programs
- Declares the two new system calls

### 12. user/usys.pl - System Call Stubs

```perl
entry("trace");
entry("history");
```

Generates user-space wrapper functions for the two new system calls.

## 🧠 How It All Works Together

### Scenario 1: Using the trace program

```bash
$ trace 5 ls
```

**Flow:**

1. `trace` program starts
2. Calls `trace(5)` system call → sets `myproc()->trace_sys_num = 5` (SYS_read)
3. `trace` calls `exec("ls", ...)`
4. When `ls` calls system call #5 (read), the kernel:
   - Captures arguments
   - Records start time
   - Executes the system call
   - Records statistics
   - Prints tracing information (because `trace_sys_num == 5`)
5. Other system calls don't print tracing information

### Scenario 2: Using the history program

```bash
$ history 1
```

**Flow:**

1. `history` program starts
2. Calls `history(1, &stat)` system call
3. Kernel retrieves statistics for SYS_exit (call count and total time)
4. Kernel copies `stat` structure to user space
5. `history` prints the statistics

### Scenario 3: Kernel Statistics Collection

- Every time any system call executes, the kernel records:
  - Call count (incremented by 1)
  - Execution time (in ticks)
- Statistics are stored in `syscall_stats[]` array with spinlocks for thread safety
- This happens regardless of whether tracing is enabled

## 🔒 Thread-Safety Considerations

- Each system call statistic has its own spinlock to prevent race conditions
- The `tickslock` is used to read the current timer value safely
- Locking is minimal and fine-grained to reduce overhead

## 📊 Data Flow Diagram

```text
User Space                     Kernel Space
===========                    ============
trace program                  sys_trace()
    |                               |
    +--- trace(5) ----------------> myproc()->trace_sys_num = 5
    |
    +--- exec("ls", ...) --------> syscall() [traps into kernel]
                                        |
                                        +--- Captures args
                                        +--- Records start time
                                        +--- Calls syscall handler
                                        +--- Records statistics
                                        +--- If trace_sys_num matches:
                                            printf(trace output)
                                        |
history program                  sys_history()
    |                               |
    +--- history(1, &stat) -------> syscall_stat_get(1, ...)
                                        |
                                        +--- Acquires lock
                                        +--- Reads count/time
                                        +--- Copies to user space
```

## 🎯 Summary of Key Features

| Feature | Implementation | Purpose |
| --- | --- | --- |
| System Call Tracing | `trace()` syscall + kernel instrumentation | Debug specific system calls in a process |
| Call Statistics | `syscall_stats[]` array + `history()` syscall | Monitor system call usage and performance |
| Argument Capture | `trace_arguments()` function | Shows what arguments were passed to traced calls |
| Timing | `syscall_ticks()` function | Measures execution time of system calls |
| Process Inheritance | Fork copies `trace_sys_num` | Tracing follows child processes |
| Thread Safety | Spinlocks per system call | Prevents race conditions in statistics |

## 🚀 Example Usage

```bash
# Trace all 'read' system calls when running 'cat'
trace 5 cat README

# Show statistics for all system calls
history

# Show statistics for system call 21 (close)
history 21
```

This implementation provides powerful debugging and performance analysis tools for the XV6 operating system!

---

# 📍 New Code Location Map

This document maps all the newly added or modified code locations in the XV6 operating system for the system call tracing and statistics implementation.

## 📁 Root Directory

### Makefile

| Location | Change Type | Description |
| --- | --- | --- |
| Line ~62-63 | Modified | Added `-march=rv64g` to CFLAGS and ASFLAGS |
| Line ~127 | Modified | Added `#====new code =====` marker |
| Line ~145-146 | Added | Added `$U/_trace\` and `$U/_history\` to UPROGS |
| Line ~184 | Removed | Removed trailing empty line |

## 🧠 Kernel Directory (kernel/)

### kernel/defs.h

| Line | Change Type | Code Added |
| --- | --- | --- |
| ~142-143 | Added | `void syscall_stats_init(void);`<br>`void syscall_stat_get(int, char *, int *, int *);` |

### kernel/main.c

| Line | Change Type | Code Added |
| --- | --- | --- |
| ~23-24 | Added | `//====new code =====`<br>`syscall_stats_init(); // system call history locks and counters` |

### kernel/proc.c

| Line | Change Type | Code Added |
| --- | --- | --- |
| ~127 | Added | `p->trace_sys_num = 0;` (in allocproc) |
| ~173 | Added | `p->trace_sys_num = 0;` (in freeproc) |
| ~309 | Added | `np->trace_sys_num = p->trace_sys_num;` (in fork) |

### kernel/proc.h

| Line | Change Type | Code Added |
| --- | --- | --- |
| ~107-108 | Added | `//====new code =====`<br>`int trace_sys_num; // System call number traced by this process.` |

### kernel/syscall.c

| Line | Change Type | Code Added |
| --- | --- | --- |
| ~104-105 | Added | `extern uint64 sys_trace(void);`<br>`extern uint64 sys_history(void);` |
| ~132-133 | Added | `[SYS_trace] sys_trace,`<br>`[SYS_history] sys_history,` |
| ~135-145 | Added | `static char *syscall_names[] = { ... };` |
| ~147-150 | Added | `struct syscall_stat_entry { ... };` |
| ~152-153 | Added | `static struct syscall_stat_entry syscall_stats[NELEM(syscalls)];` |
| ~155-161 | Added | `void syscall_stats_init(void) { ... }` |
| ~163-172 | Added | `static uint syscall_ticks(void) { ... }` |
| ~174-181 | Added | `static void syscall_stat_record(int number, uint elapsed) { ... }` |
| ~183-193 | Added | `void syscall_stat_get(int number, char *name, int *count, int *accum_time) { ... }` |
| ~195-203 | Added | `static void trace_string(uint64 address) { ... }` |
| ~205-227 | Added | `static void trace_arguments(int number, uint64 args[]) { ... }` |
| ~233-237 | Added | Argument capture loop: `for(i = 0; i < NELEM(args); i++) args[i] = argraw(i);` |
| ~238-244 | Added | Exec pathname preservation |
| ~246 | Added | `start_ticks = syscall_ticks();` |
| ~248-258 | Added | Special handling for SYS_exit |
| ~264-266 | Added | `syscall_stat_record(num, syscall_ticks() - start_ticks);` |
| ~267-277 | Added | Tracing output code |

### kernel/syscall.h

| Line | Change Type | Code Added |
| --- | --- | --- |
| ~23-24 | Added | `#define SYS_trace 22`<br>`#define SYS_history 23` |

### kernel/sysproc.c

| Line | Change Type | Code Added |
| --- | --- | --- |
| ~8 | Added | `#include "syscall.h"` |
| ~10-14 | Added | `struct syscall_stat { ... };` |
| ~16-26 | Added | `uint64 sys_trace(void) { ... }` |
| ~28-46 | Added | `uint64 sys_history(void) { ... }` |

## 👤 User Directory (user/)

### user/history.c (New File)

| Lines | Content |
| --- | --- |
| 1-34 | Entire file - history command implementation |

### user/trace.c (New File)

| Lines | Content |
| --- | --- |
| 1-26 | Entire file - trace command implementation |

### user/ulib.c

| Line | Change Type | Code Added |
| --- | --- | --- |
| ~104-115 | Added | Sign handling in `atoi()` function |

### user/user.h

| Line | Change Type | Code Added |
| --- | --- | --- |
| ~2-7 | Added | `struct syscall_stat { ... };` |
| ~8-9 | Added | `int trace(int);`<br>`int history(int, struct syscall_stat*);` |

### user/usertests.c

| Line | Change Type | Code Modified |
| --- | --- | --- |
| ~244 | Modified | `void rwsbrk()` → `void rwsbrk(char *s)` |

### user/usys.pl

| Line | Change Type | Code Added |
| --- | --- | --- |
| ~39-40 | Added | `entry("trace");`<br>`entry("history");` |

## 📊 Summary Table by File

| File Path | Type | Lines Added | Lines Modified | Lines Removed |
| --- | --- | --- | --- | --- |
| Makefile | Modified | 4 | 1 | 1 |
| kernel/defs.h | Modified | 2 | 0 | 0 |
| kernel/main.c | Modified | 2 | 0 | 0 |
| kernel/proc.c | Modified | 3 | 0 | 0 |
| kernel/proc.h | Modified | 2 | 0 | 0 |
| kernel/syscall.c | Modified | ~80 | ~5 | 0 |
| kernel/syscall.h | Modified | 2 | 0 | 0 |
| kernel/sysproc.c | Modified | ~30 | 0 | 0 |
| user/history.c | New File | 34 | 0 | 0 |
| user/trace.c | New File | 26 | 0 | 0 |
| user/ulib.c | Modified | ~10 | 0 | 0 |
| user/user.h | Modified | ~8 | 0 | 0 |
| user/usertests.c | Modified | 0 | 1 | 0 |
| user/usys.pl | Modified | 2 | 0 | 0 |

## 🗺️ Visual Directory Structure

```text
xv6/
├── Makefile                         ← Modified (CFLAGS + UPROGS)
├── kernel/
│   ├── defs.h                       ← Modified (new function declarations)
│   ├── main.c                       ← Modified (syscall_stats_init call)
│   ├── proc.c                       ← Modified (trace_sys_num management)
│   ├── proc.h                       ← Modified (trace_sys_num field)
│   ├── syscall.c                    ← Modified (core tracing/statistics)
│   ├── syscall.h                    ← Modified (new syscall numbers)
│   └── sysproc.c                    ← Modified (sys_trace & sys_history)
└── user/
    ├── history.c                    ← NEW FILE
    ├── trace.c                      ← NEW FILE
    ├── ulib.c                       ← Modified (atoi enhancement)
    ├── user.h                       ← Modified (syscall_stat struct)
    ├── usertests.c                  ← Modified (function signature)
    └── usys.pl                      ← Modified (new syscall stubs)
```

## 🎯 Key Code Locations by Feature

### Feature 1: Process Tracing

- **Process Structure:** `kernel/proc.h:107-108`
- **Initialization:** `kernel/proc.c:127`
- **Cleanup:** `kernel/proc.c:173`
- **Inheritance:** `kernel/proc.c:309`
- **System Call Handler:** `kernel/sysproc.c:16-26`

### Feature 2: Statistics Collection

- **Data Structure:** `kernel/syscall.c:147-150`
- **Initialization:** `kernel/main.c:23-24` & `kernel/syscall.c:155-161`
- **Recording:** `kernel/syscall.c:263-266`
- **Retrieval:** `kernel/syscall.c:183-193` & `kernel/sysproc.c:28-46`

### Feature 3: Tracing Output

- **Arguments Capture:** `kernel/syscall.c:233-237`
- **Timing:** `kernel/syscall.c:246`
- **Argument Formatting:** `kernel/syscall.c:205-227`
- **Output Generation:** `kernel/syscall.c:267-277`

### Feature 4: User Programs

- **trace Command:** `user/trace.c:1-26`
- **history Command:** `user/history.c:1-34`

## 🔍 Quick Reference: Where to Find...

| What | Where |
| --- | --- |
| New system call numbers | `kernel/syscall.h:23-24` |
| Syscall name mapping | `kernel/syscall.c:135-145` |
| Statistics data | `kernel/syscall.c:152-153` |
| sys_trace implementation | `kernel/sysproc.c:16-26` |
| sys_history implementation | `kernel/sysproc.c:28-46` |
| syscall argument printing | `kernel/syscall.c:205-227` |
| Tracing logic | `kernel/syscall.c:267-277` |
| trace program | `user/trace.c` |
| history program | `user/history.c` |
| Statistics structure (user) | `user/user.h:2-7` |
| User syscall stubs | `user/usys.pl:39-40` |

## 📝 Marker Usage

All new code sections are marked with:

```c
//====new code =====
```

This makes it easy to locate all modifications by searching for this comment in the codebase.

**Total New Code:** ~200 lines of new/modified code across 14 files

**New Files:** 2 (`user/trace.c` and `user/history.c`)