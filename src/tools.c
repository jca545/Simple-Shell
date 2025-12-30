#include "tools.h"
#include "msgs.h"
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

static char prev_dir[1024] = "";

void add_cmd_hist(History *h, char *cmd) {
  snprintf(h->hist[h->index], 1000, "%s", cmd);
  // Update index
  h->index = (h->index + 1) % 10;
  // Update count
  h->count++;
}

void display_hist(History *h) {
  char buffer[1024];
  // loop through array to print
  int j = 0;
  for (int i = (h->index - 1); i >= 0; i--) {
    int len = snprintf(buffer, sizeof(buffer), "%d\t%s\n", (h->count - j),
                       h->hist[i]);
    write(STDOUT_FILENO, buffer, len);
    j++;
  }
  if (h->count > 10) {
    for (int i = 9; i >= h->index; i--) {
      int len = snprintf(buffer, sizeof(buffer), "%d\t%s\n", (h->count - j),
                         h->hist[i]);
      write(STDOUT_FILENO, buffer, len);
      j++;
    }
  }
}

void display_prompt() {
  char curr_dir[1024];

  if (getcwd(curr_dir, sizeof(curr_dir)) == NULL) {
    char *msg = FORMAT_MSG("shell", GETCWD_ERROR_MSG);
    write(STDERR_FILENO, msg, strlen(msg));
  } else {
    write(STDOUT_FILENO, curr_dir, strlen(curr_dir));
    write(STDOUT_FILENO, "$ ", 2);
  }
}

void check_back(char *args[], int *arg_amount, int *back) {
  int last_arg_index = *arg_amount - 1;
  char *last_arg = args[last_arg_index];
  int last_arg_len = strlen(last_arg);
  // if & (no space separate) || (has space)
  if (last_arg[last_arg_len - 1] == '&') {
    *back = 1;
    // if has space
    if (last_arg_len == 1) {
      args[last_arg_index] = NULL;
      (*arg_amount)--;
      // no space
    } else {
      last_arg[last_arg_len - 1] = '\0';
    }
  }
}

int is_internal(char *cmd) {
  return ((strcmp(cmd, "exit") == 0) || (strcmp(cmd, "pwd") == 0) ||
          (strcmp(cmd, "cd") == 0) || (strcmp(cmd, "help") == 0) ||
          (strcmp(cmd, "history") == 0));
}

void display_help(char *argv[]) {
  char *help_help = FORMAT_MSG("help", HELP_HELP_MSG);
  char *exit_help = FORMAT_MSG("exit", EXIT_HELP_MSG);
  char *pwd_help = FORMAT_MSG("pwd", PWD_HELP_MSG);
  char *cd_help = FORMAT_MSG("cd", CD_HELP_MSG);
  char *history_help = FORMAT_MSG("history", HISTORY_HELP_MSG);
  // Have help <an argument>
  if (argv[1] != NULL) {
    // Have too much argument
    if (argv[2] != NULL) {
      char *msg = FORMAT_MSG("cd", TMA_MSG);
      write(STDERR_FILENO, msg, strlen(msg));
      // Have 1 argument, and is internal
    } else if (is_internal(argv[1])) {
      if (strcmp(argv[1], "help") == 0) {
        write(STDOUT_FILENO, help_help, strlen(help_help));
      } else if (strcmp(argv[1], "exit") == 0) {
        write(STDOUT_FILENO, exit_help, strlen(exit_help));
      } else if (strcmp(argv[1], "pwd") == 0) {
        write(STDOUT_FILENO, pwd_help, strlen(pwd_help));
      } else if (strcmp(argv[1], "cd") == 0) {
        write(STDOUT_FILENO, cd_help, strlen(cd_help));
      } else if (strcmp(argv[1], "history") == 0) {
        write(STDOUT_FILENO, history_help, strlen(history_help));
      }
      // Have 1 argument, external
    } else {
      // char *extern_cmd; //= FORMAT_MSG(argv[1], EXTERN_HELP_MSG);
      // extern_cmd = argv[1];
      // const char *extern_help = FORMAT_MSG(extern_cmd, EXTERN_HELP_MSG);
      char extern_help[1024];
      snprintf(extern_help, sizeof(extern_help), "%s: %s\n", argv[1],
               EXTERN_HELP_MSG);
      write(STDOUT_FILENO, extern_help, strlen(extern_help));
    }
    // Only help, no argument
  } else {
    write(STDOUT_FILENO, help_help, strlen(help_help));
    write(STDOUT_FILENO, exit_help, strlen(exit_help));
    write(STDOUT_FILENO, pwd_help, strlen(pwd_help));
    write(STDOUT_FILENO, cd_help, strlen(cd_help));
    write(STDOUT_FILENO, history_help, strlen(history_help));
  }
}

void handle_sigint(int signum) {
  write(STDOUT_FILENO, "\n", 1);
  char *argv[] = {"help", NULL};
  display_help(argv);
  display_prompt();
}

void setup_handle_sigint() {
  // Declaring the struce
  struct sigaction handler;
  handler.sa_handler = &handle_sigint;
  sigemptyset(&handler.sa_mask);
  handler.sa_flags = SA_RESTART;
  sigaction(SIGINT, &handler, NULL);
}

void exe_internal_cmd(char *argv[], History *h) {
  char *msg;
  // exit
  if (strcmp(argv[0], "exit") == 0) {
    // check if more than 1 argument
    if (argv[1] != NULL) {
      msg = FORMAT_MSG("exit", TMA_MSG);
      write(STDERR_FILENO, msg, strlen(msg));
    } else {
      exit(0);
    }
    // pwd
  } else if (strcmp(argv[0], "pwd") == 0) {
    // with argu
    if (argv[1] != NULL) {
      msg = FORMAT_MSG("pwd", TMA_MSG);
      write(STDERR_FILENO, msg, strlen(msg));
      // no argu
    } else {
      char cwd[1024];
      if (getcwd(cwd, sizeof(cwd)) == NULL) {
        msg = FORMAT_MSG("pwd", GETCWD_ERROR_MSG);
        write(STDERR_FILENO, msg, strlen(msg));
      } else {
        getcwd(cwd, sizeof(cwd));
        write(STDOUT_FILENO, cwd, strlen(cwd));
        write(STDOUT_FILENO, "\n", 1);
      }
    }
    // cd
  } else if (strcmp(argv[0], "cd") == 0) {
    // Check if too much argument
    if (argv[1] != NULL && argv[2] != NULL) {
      msg = FORMAT_MSG("cd", TMA_MSG);
      write(STDERR_FILENO, msg, strlen(msg));
      return;
      // right amount of argument
    } else {
      char *dir = argv[1];
      // cd to home
      if (dir == NULL || strcmp(dir, "~") == 0 || dir[0] == '~') {
        struct passwd *pwd = getpwuid(getuid());
        // if getuid goes error
        if (pwd == NULL) {
          msg = FORMAT_MSG("cd", CHDIR_ERROR_MSG);
          write(STDERR_FILENO, msg, strlen(msg));
          return;
        }

        // If dir is ~
        if (dir == NULL || strcmp(dir, "~") == 0) {
          // change dir to home dir
          dir = pwd->pw_dir;
        } else {
          dir = pwd->pw_dir;
          // change to ~ first, then set dir to ~/...'s ...dir
          if (chdir(dir) != 0) {
            msg = FORMAT_MSG("cd", CHDIR_ERROR_MSG);
            write(STDERR_FILENO, msg, strlen(msg));
            return;
          } else {
            // set dir to ~/... 's ...
            char sub_dir[1024];
            dir = argv[1];
            strncpy(sub_dir, dir + 2, 1024);
            dir = sub_dir;
          }
        }
        // cd prev dir (Not the parent dir)
      } else if (strcmp(dir, "-") == 0) {
        char cur_dir[1024];

        if (getcwd(cur_dir, sizeof(cur_dir)) == NULL) {
          msg = FORMAT_MSG("cd", CHDIR_ERROR_MSG);
          write(STDERR_FILENO, msg, strlen(msg));
        }

        dir = prev_dir;
        if (chdir(dir) != 0) {
          msg = FORMAT_MSG("cd", CHDIR_ERROR_MSG);
          write(STDERR_FILENO, msg, strlen(msg));
        }
        strncpy(prev_dir, cur_dir, 1024);
        return;
      }

      // store the cur dir to "prev_dir", then take the change dir aciton
      if (getcwd(prev_dir, sizeof(prev_dir)) == NULL) {
        msg = FORMAT_MSG("cd", CHDIR_ERROR_MSG);
        write(STDERR_FILENO, msg, strlen(msg));
      }
      if (chdir(dir) != 0) {
        msg = FORMAT_MSG("cd", CHDIR_ERROR_MSG);
        write(STDERR_FILENO, msg, strlen(msg));
      }
    }
    // help
  } else if (strcmp(argv[0], "help") == 0) {
    display_help(argv);
    // history
  } else if (strcmp(argv[0], "history") == 0) {
    // too much argument
    if (argv[1] != NULL) {
      msg = FORMAT_MSG("history", TMA_MSG);
      write(STDERR_FILENO, msg, strlen(msg));
    } else {
      display_hist(h);
    }
  }
}

void exe_cmd(char *input, History *h) {
  if (input == NULL) {
    return;
  }
  char *msg;

  // !!: re run last cmd
  if (strcmp(input, "!!") == 0) {
    // this is the first cmd
    if (h->count == -1) {
      msg = FORMAT_MSG("history", HISTORY_NO_LAST_MSG);
      write(STDERR_FILENO, msg, strlen(msg));
      return;
      // not the very first cmd
    } else {
      int i = h->index - 1;
      // if reach over than head index, go to back
      if (i < 0) {
        i = 9;
      }
      input = strdup(h->hist[i]);
      write(STDOUT_FILENO, input, strlen(input));
      write(STDOUT_FILENO, "\n", 1);
    }
    // !# cmd
  } else if (input[0] == '!' && input[1] != '\0') {
    // n = count shown in history display
    int n = atoi(&input[1]);
    // check if n is within recent 10 cmd
    if (input[1] == '0' && h->count < 10) {
      input = strdup(h->hist[0]);
      write(STDOUT_FILENO, input, strlen(input));
      write(STDOUT_FILENO, "\n", 1);
    } else if (n <= 0 || n > h->count || n <= (h->count - 10)) {
      msg = FORMAT_MSG("history", HISTORY_INVALID_MSG);
      write(STDERR_FILENO, msg, strlen(msg));
      return;
      // n is with recent 10 cmd
    } else {
      int i = (n) % 10;
      input = strdup(h->hist[i]);
      write(STDOUT_FILENO, input, strlen(input));
      write(STDOUT_FILENO, "\n", 1);
    }
  }
  add_cmd_hist(h, input);

  char *token;
  char *rest = input;

  token = strtok_r(rest, " ", &rest);
  // If no input
  if (token == NULL) {
    return;
  }

  // separate the inputs
  char *args[10];
  int i = 0;

  while (token != NULL) {
    args[i] = token;
    token = strtok_r(rest, " ", &rest);
    i++;
  }
  args[i] = NULL;

  // Determine if forground or back(&)
  // 0: fore / 1: back
  int back = 0;
  // also remove & from args[]
  check_back(args, &i, &back);

  // If internal cmd, no fork
  if (args[0] != NULL && is_internal(args[0])) {

    exe_internal_cmd(args, h);
    // Else not internal, fork
    // Fork child process
  } else {
    pid_t pid = fork();

    // Child Process
    if (pid == 0) {

      if (execvp(args[0], args) < 0) {
        msg = FORMAT_MSG("shell", EXEC_ERROR_MSG);
        write(STDERR_FILENO, msg, strlen(msg));
        exit(EXIT_FAILURE);
      }
      // Parent Process
    } else if (pid > 0) {

      int wstatus = 0;
      // Process in foreground: wait for child to finish, waitpid
      if (!back) {
        if (waitpid(pid, &wstatus, 0) < 0) {
          // if (errno == EINTR) {
          // char *intr = "shell: interrupted while waiting for child\n";
          // write(STDOUT_FILENO, intr, strlen(intr));
          //} else {
          msg = FORMAT_MSG("shell", WAIT_ERROR_MSG);
          write(STDERR_FILENO, msg, strlen(msg));
          exit(EXIT_FAILURE);
          //}
        }
        // Clean terminated zombie process
        while (waitpid(-1, &wstatus, WNOHANG) > 0) {
        }
        // Process in background: no wait, but clean zombie
      } else {
        // Clean Zombie process
        while (waitpid(-1, NULL, WNOHANG) > 0)
          ;
      }
      // Fork fails
    } else {
      char *msg = FORMAT_MSG("shell", FORK_ERROR_MSG);
      write(STDERR_FILENO, msg, strlen(msg));
    }
  }
}
