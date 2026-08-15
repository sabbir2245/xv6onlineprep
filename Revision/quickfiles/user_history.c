//====new code =====
#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

static void
print_history(int syscall_number)
{
  struct syscall_stat stat;

  if(history(syscall_number, &stat) < 0) {
    fprintf(2, "history: invalid syscall number %d\n", syscall_number);
    return;
  }
  printf("%d: syscall: %s, #: %d, time: %d\n", syscall_number,
         stat.syscall_name, stat.count, stat.accum_time);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc == 1) {
    for(i = SYS_fork; i <= SYS_history; i++)
      print_history(i);
  } else if(argc == 2) {
    print_history(atoi(argv[1]));
  } else {
    fprintf(2, "Usage: history [sys_call_num]\n");
    exit(1);
  }
  exit(0);
}
