//====new code =====
#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char *command_argv[MAXARG];
  int i;

  if(argc < 3) {
    fprintf(2, "Usage: %s sys_call_num command\n", argv[0]);
    exit(1);
  }
  if(trace(atoi(argv[1])) < 0) {
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }
  for(i = 2; i < argc && i - 2 < MAXARG - 1; i++)
    command_argv[i - 2] = argv[i];
  command_argv[i - 2] = 0;
  exec(command_argv[0], command_argv);
  fprintf(2, "%s: exec failed\n", command_argv[0]);
  exit(1);
}
