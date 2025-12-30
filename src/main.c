#include "msgs.h"
#include "tools.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
  // Set up history
  History history;
  history.index = 0;
  history.count = -1;

  setup_handle_sigint();
  while (1) {
    display_prompt();

    char *input = NULL;
    size_t inputsize = 0;
    size_t len = getline(&input, &inputsize, stdin);
    if (len == -1) {
      char *msg = FORMAT_MSG("shell", READ_ERROR_MSG);
      write(STDERR_FILENO, msg, strlen(msg));
    } else {
      input[len - 1] = '\0';
      exe_cmd(input, &history);
    }
  }
  return 0;
}
