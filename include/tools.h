typedef struct {
  char hist[10][1024];
  int index;
  int count;
} History;

void display_prompt();

void exe_cmd(char *input, History *h);

void exe_internal_cmd(char *argv[], History *h);

void display_help(char *argv[]);

int is_internal_cmd(char *cmd);

void handle_sigint(int signum);

void setup_handle_sigint();

void remove_oldest_hist();

void add_cmd_hist(History *h, char *cmd);

void display_hist(History *h);
