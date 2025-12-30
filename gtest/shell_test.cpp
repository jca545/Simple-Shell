#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <limits.h>
#include <pthread.h>
#include <regex>
#include <signal.h>
#include <stack>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_set>

int input_fd[2];
int fd[2];
int err_fd[2];
int externalScore = 0;
int internalScore = 0;
int historyScore = 0;

class ShellTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    pipe(input_fd);
    pipe(fd);
    pipe(err_fd);

    saved_stdin = dup(fileno(stdin));
    dup2(input_fd[0], fileno(stdin));

    saved_stdout = dup(fileno(stdout));
    dup2(fd[1], fileno(stdout));

    // start the shell as a child process
    pid = fork();
    if (pid == 0) {
      saved_stderr = dup(fileno(stderr));
      dup2(err_fd[1], fileno(stderr));

      close(input_fd[0]);
      close(fd[1]);
      close(err_fd[1]);

      execl("./shell", "NULL", NULL);
    } else if (pid > 0) {
      close(input_fd[0]);
      close(fd[1]);
      close(err_fd[1]);
    } else {
      perror("fork");
      exit(1);
    }
  }

  virtual void TearDown() {
    dup2(saved_stdout, fileno(stdout));
    dup2(saved_stdin, fileno(stdin));
    dup2(saved_stderr, fileno(stderr));

    close(input_fd[1]);
    close(fd[0]);
    close(err_fd[0]);
    kill(pid, SIGKILL);
  }
  int saved_stdout;
  int saved_stdin;
  int saved_stderr;
  pid_t pid;
};

int writeInput(const char *command, bool closePipe) {
  ssize_t write_status = write(input_fd[1], command, strlen(command));
  if (write_status == -1) {
    perror("Error writing to the pipe");
    return -1;
  }

  fd_set set;
  FD_ZERO(&set);
  FD_SET(input_fd[1], &set);

  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  int select_status = select(input_fd[1] + 1, NULL, &set, NULL, &timeout);
  if (select_status == -1) {
    perror("Error using select()");
    return -1;
  } else if (select_status == 0) {
    fprintf(stderr,
            "Timeout waiting for input_fd to become ready for writing\n");
    return -1;
  }

  if (closePipe) {
    int close_status = close(input_fd[1]);
    if (close_status == -1) {
      perror("Error closing the pipe");
      return -1;
    }
  }
  return 0;
}

std::string getOutputInternal(int fd_internal) {
  std::string output;
  char buffer[1024];

  fd_set set;
  FD_ZERO(&set);
  FD_SET(fd_internal, &set);

  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  while (true) {
    int select_status = select(fd_internal + 1, &set, NULL, NULL, &timeout);
    if (select_status == -1) {
      perror("Error in select");
      exit(1);
    } else if (select_status == 0) {
      break;
    } else {
      if (FD_ISSET(fd_internal, &set)) {
        int n = read(fd_internal, buffer, sizeof(buffer));
        if (n == -1) {
          perror("Error reading from the pipe");
        }
        output.append(buffer, n);
      }
    }
  }
  return output;
}

std::string getOutput() { return getOutputInternal(fd[0]); }
std::string getErrOutput() { return getOutputInternal(err_fd[0]); }

std::string parsedOutput(std::string output) {
  std::string firstLine;
  std::getline(std::istringstream(output), firstLine);
  auto start = firstLine.find("$");
  if (start == std::string::npos) {
    std::cerr << "Failed to find '$' in output" << std::endl;
    exit(1);
  }

  firstLine = firstLine.substr(start + 1);
  firstLine.erase(0, firstLine.find_first_not_of(" "));
  return firstLine;
}

int getExitStatus(pid_t child_pid) {
  int status;
  waitpid(child_pid, &status, WNOHANG);
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  } else {
    return -1;
  }
}

void adjustScore(bool result, int val, int &scoreType) {
  if (result)
    scoreType += val;
}

TEST_F(ShellTest, TestFork) {

  int writeStatus = writeInput("sleep 5\n", true);
  if (writeStatus == -1)
    exit(1);

  getOutput();

  std::string command = "ps --ppid " + std::to_string(pid);

  FILE *ps = popen(command.c_str(), "r");
  if (!ps) {
    perror("popen");
    exit(1);
  }

  // size keeps track of the amount of lines. The first one will be the header.
  int size = 0;
  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), ps) != NULL) {
    std::cout << buffer;
    size++;
  }

  pclose(ps);

  adjustScore(size == 2, 5, externalScore);

  EXPECT_EQ(size, 2);
}

TEST_F(ShellTest, TestForkInBackground) {

  int writeStatus1 = writeInput("sleep 10 &\n", false);
  if (writeStatus1 == -1)
    exit(1);

  getOutput();

  int writeStatus2 = writeInput("sleep 10 &\n", false);
  if (writeStatus2 == -1)
    exit(1);

  getOutput();

  int writeStatus3 = writeInput("sleep 10 &\n", false);
  if (writeStatus3 == -1)
    exit(1);

  getOutput();

  int writeStatus4 = writeInput("sleep 10 &\n", true);
  if (writeStatus4 == -1)
    exit(1);

  getOutput();

  std::string command = "ps --ppid " + std::to_string(pid);

  FILE *ps = popen(command.c_str(), "r");
  if (!ps) {
    perror("popen");
    exit(1);
  }

  std::regex regex("([0-9]+)");
  std::smatch num;

  std::unordered_set<int> children{pid};

  // size keeps track of the amount of lines. The first one will be the header.
  int size = 0;
  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), ps) != NULL) {
    std::string child_pid(buffer);
    if (std::regex_search(child_pid, num, regex)) {
      if (children.count(std::stoi(num.str())))
        break;
      children.insert(std::stoi(num.str()));
    }
    size++;
  }

  pclose(ps);

  adjustScore(size == 5, 5, externalScore);

  EXPECT_EQ(size, 5);
}

TEST_F(ShellTest, TestForkMixed) {

  int writeStatus = writeInput("sleep 10 &\n", false);
  if (writeStatus == -1)
    exit(1);
  getOutput();

  int writeStatus1 = writeInput("sleep 10\n", false);
  if (writeStatus1 == -1)
    exit(1);
  getOutput();

  int writeStatus2 = writeInput("sleep 5\n", true);
  if (writeStatus2 == -1)
    exit(1);
  getOutput();

  std::string command = "ps --ppid " + std::to_string(pid);

  FILE *ps = popen(command.c_str(), "r");
  if (!ps) {
    perror("popen");
    exit(1);
  }

  std::regex regex("([0-9]+)");
  std::smatch num;

  std::unordered_set<int> children{pid};

  int size = 0;
  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), ps) != NULL) {
    std::string child_pid(buffer);
    if (std::regex_search(child_pid, num, regex)) {
      if (children.count(std::stoi(num.str())))
        break;
      children.insert(std::stoi(num.str()));
    }
    size++;
  }

  pclose(ps);

  adjustScore(size == 3, 5, externalScore);

  EXPECT_EQ(size, 3);
}

TEST_F(ShellTest, TestWait) {

  int writeStatus = writeInput("sleep 10\n", false);
  if (writeStatus == -1)
    exit(1);
  getOutput();

  int writeStatus1 = writeInput("ping\n", false);
  if (writeStatus1 == -1)
    exit(1);
  getOutput();

  int writeStatus2 = writeInput("sleep 5\n", false);
  if (writeStatus2 == -1)
    exit(1);
  getOutput();

  std::string command = "ps --ppid " + std::to_string(pid);

  FILE *ps = popen(command.c_str(), "r");
  if (!ps) {
    perror("popen");
    exit(1);
  }

  int size = 0;
  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), ps) != NULL) {
    std::cout << buffer;
    size++;
  }

  pclose(ps);

  adjustScore(size == 2, 5, externalScore);

  EXPECT_EQ(size, 2);
}

TEST_F(ShellTest, InvalidExecCommand) {

  std::string expected_output = getcwd(NULL, 0);
  expected_output += "$ ";
  expected_output += expected_output;

  int writeStatus = writeInput("la adf\n", true);
  if (writeStatus == -1)
    exit(1);

  std::string output = getOutput();
  std::string errLine = getErrOutput();

  ASSERT_TRUE(output == expected_output);
  ASSERT_TRUE(errLine == "shell: unable to execute command\n");
  adjustScore(output == expected_output, 2, externalScore);
  adjustScore(errLine == "shell: unable to execute command\n", 3,
              externalScore);
}

TEST_F(ShellTest, TestInvalidCommand) {

  std::string expected_output = getcwd(NULL, 0);
  expected_output += "$ ";
  expected_output += expected_output;

  int writeStatus = writeInput("Invalid Command\n", true);
  if (writeStatus == -1)
    exit(1);

  std::string output = getOutput();
  std::string errLine = getErrOutput();

  ASSERT_TRUE(expected_output == output);
  ASSERT_TRUE(errLine == "shell: unable to execute command\n");
  adjustScore(expected_output == output, 2, externalScore);
  adjustScore(errLine == "shell: unable to execute command\n", 2,
              externalScore);
}

/* -------------------------- Internal Tests ----------------------------- */

TEST_F(ShellTest, Pwd) {

  int writeStatus = writeInput("pwd\n", true);
  if (writeStatus == -1)
    exit(1);

  std::string output = getOutput();

  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    std::string expected_output = cwd;
    adjustScore(parsedOutput(output) == expected_output, 3, internalScore);
    EXPECT_EQ(parsedOutput(output), expected_output);
  }
}

TEST_F(ShellTest, PwdWithArgument) {

  int writeStatus = writeInput("pwd argument\n", true);
  if (writeStatus == -1)
    exit(1);

  std::string errLine = getErrOutput();

  ASSERT_TRUE(errLine == "pwd: too many arguments\n");
  adjustScore(errLine == "pwd: too many arguments\n", 2, internalScore);
}

TEST_F(ShellTest, PathPrompt) {

  int writeStatus = writeInput("pwd\n", true);
  if (writeStatus == -1)
    exit(1);

  std::string output = getOutput();

  std::string firstLine;
  std::getline(std::istringstream(output), firstLine);
  auto end = firstLine.find("$");
  if (end == std::string::npos) {
    std::cerr << "Failed to find '$' in output" << std::endl;
    exit(1);
  }

  firstLine = firstLine.substr(0, end);

  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    std::string expected_output = cwd;
    adjustScore(firstLine == expected_output, 2, internalScore);
    EXPECT_EQ(firstLine, expected_output);
  }
}

TEST_F(ShellTest, Exit) {

  int writeStatus = writeInput("ls -l\n", false);
  if (writeStatus == -1)
    exit(1);
  getOutput();

  int writeStatus1 = writeInput("exit\n", true);
  if (writeStatus1 == -1)
    exit(1);

  // wait for the shell to finish executing with a timeout of 1 second
  int status;
  int timeout = 1;
  int ret = waitpid(pid, &status, WNOHANG);
  while (ret == 0 && timeout > 0) {
    sleep(1);
    timeout--;
    ret = waitpid(pid, &status, WNOHANG);
  }

  if (ret == 0)
    EXPECT_TRUE(false) << "Shell did not exit within the timeout period";

  // Exit status of the shell program
  if (WIFEXITED(status)) {
    adjustScore(true, 3, internalScore);
    EXPECT_EQ(WEXITSTATUS(status), 0);
  } else {
    EXPECT_TRUE(false) << "Shell did not exit normally";
  }
}

TEST_F(ShellTest, ExitWithArgument) {

  int writeStatus = writeInput("exit argument\n", true);
  if (writeStatus == -1)
    exit(1);

  std::string errLine = getErrOutput();

  ASSERT_TRUE(errLine == "exit: too many arguments\n");
  adjustScore(errLine == "exit: too many arguments\n", 2, internalScore);
}

TEST_F(ShellTest, CdForward) {
  // Create a temporary directory
  int create = mkdir("testdir", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  // Change directory
  int writeStatus = writeInput("cd testdir\n", true);
  if (writeStatus == -1)
    exit(1);

  std::string output = getOutput();

  auto start = output.find("$");
  if (start == std::string::npos) {
    std::cerr << "Failed to find '$' in output" << std::endl;
    exit(1);
  }

  output = output.substr(start + 2);

  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    std::string expected_output = cwd;
    adjustScore(output == expected_output + "/testdir$ ", 3, internalScore);
    EXPECT_EQ(output, expected_output + "/testdir$ ");
  }

  // cleanup
  int remove = rmdir("testdir");
}

TEST_F(ShellTest, CdBackward) {
  // Save the current working directory
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    // getCwd failed
  }
  std::string current_dir = cwd;

  int writeStatus = writeInput("cd ..\n", true);
  if (writeStatus == -1)
    exit(1);

  std::string output = getOutput();

  auto start = output.find("$");
  if (start == std::string::npos) {
    std::cerr << "Failed to find '$' in output" << std::endl;
    exit(1);
  }

  output = output.substr(start + 2);

  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    std::string expected_output =
        current_dir.substr(0, current_dir.find_last_of("/\\"));
    adjustScore(output == expected_output + "$ ", 2, internalScore);
    EXPECT_EQ(output, expected_output + "$ ");
  }
}

TEST_F(ShellTest, CDInvalidArgument) {
  int writeStatus = writeInput("cd sgdrerg\n", true);
  if (writeStatus == -1)
    exit(1);

  std::string errLine = getErrOutput();

  ASSERT_TRUE(errLine == "cd: unable to change directory\n");
  adjustScore(errLine == "cd: unable to change directory\n", 2, internalScore);
}

TEST_F(ShellTest, InternalHelpCommand) {

  std::string expected_output = "cd: change the current directory";

  int writeStatus1 = writeInput("help cd\n", true);
  if (writeStatus1 == -1)
    exit(1);
  std::string output = getOutput();

  bool found = true;
  if (output.find(expected_output) == std::string::npos)
    found = false;

  adjustScore(found, 1, internalScore);
  EXPECT_TRUE(found);
}

TEST_F(ShellTest, ExternalHelpCommand) {

  std::string expected_output = "echo: external command or application";

  int writeStatus2 = writeInput("help echo\n", true);
  if (writeStatus2 == -1)
    exit(1);
  std::string output = getOutput();

  bool found = true;
  if (output.find(expected_output) == std::string::npos)
    found = false;

  adjustScore(found, 1, internalScore);
  EXPECT_TRUE(found);
}

TEST_F(ShellTest, HelpCommandNoArgument) {

  std::vector<std::string> commands = {"help", "cd", "exit", "pwd", "history"};

  int status = writeInput("help\n", true);
  if (status == -1)
    exit(1);
  std::string output = getOutput();

  bool all_commands_found = true;
  for (const std::string &command : commands) {
    if (output.find(command) == std::string::npos) {
      all_commands_found = false;
      break;
    }
  }
  adjustScore(all_commands_found, 3, internalScore);
  EXPECT_TRUE(all_commands_found);
}

/* -------------------------- History Tests ----------------------------- */

// Displays the last 10 commands
TEST_F(ShellTest, DisplayLast10Commands) {
  std::vector<std::string> commands{"echo Hello World\n",
                                    "ls -l\n",
                                    "pwd\n",
                                    "du\n",
                                    "ls\n",
                                    "clear\n",
                                    "ps\n",
                                    "ls\n",
                                    "pwd\n",
                                    "help\n",
                                    "pwd\n",
                                    "help\n"};
  for (auto command : commands) {
    int writeStatus = writeInput(command.c_str(), false);
    if (writeStatus == -1)
      exit(1);
    getOutput();
  }

  int status = writeInput("history\n", true);
  if (status == -1)
    exit(1);
  std::string output = getOutput();

  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    // getCwd failed
  }
  std::string current_dir = cwd;

  std::string expected_output = "12\thistory\n11\thelp\n10\tpwd\n9\thelp\n8\tpw"
                                "d\n7\tls\n6\tps\n5\tclear\n4\tls\n3\tdu\n" +
                                current_dir + "$ ";

  EXPECT_EQ(expected_output, output);

  adjustScore(expected_output == output, 3, historyScore);
}

// Displays arguments
TEST_F(ShellTest, DisplayArguments) {
  std::vector<std::string> commands{"echo Hello World\n", "ls -l\n", "ls -la\n",
                                    "du\n", "ls -lah\n"};
  for (auto command : commands) {
    int writeStatus = writeInput(command.c_str(), false);
    if (writeStatus == -1)
      exit(1);
    getOutput();
  }

  int status = writeInput("history\n", true);
  if (status == -1)
    exit(1);
  std::string output = getOutput();

  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    // getCwd failed
  }
  std::string current_dir = cwd;

  std::string expected_output = "5\thistory\n4\tls -lah\n3\tdu\n2\tls "
                                "-la\n1\tls -l\n0\techo Hello World\n" +
                                current_dir + "$ ";

  EXPECT_EQ(expected_output, output);

  adjustScore(expected_output == output, 2, historyScore);
}

// Minimum command starts at 0
TEST_F(ShellTest, MinimumCommandNumber) {
  std::vector<char> numbers{'3', '2', '1', '0'};

  int status1 = writeInput("echo Hello World\n", false);
  if (status1 == -1)
    exit(1);
  getOutput();

  int status2 = writeInput("ls\n", false);
  if (status2 == -1)
    exit(1);
  getOutput();

  int status3 = writeInput("pwd\n", false);
  if (status3 == -1)
    exit(1);
  getOutput();

  // Check the command history
  int status = writeInput("history\n", true);
  if (status == -1)
    exit(1);
  std::string output = getOutput();

  std::istringstream iss(output);
  std::string line;
  bool numbering = true;
  int i = 0;
  for (int i = 0; i < numbers.size(); i++) {
    std::getline(iss, line);
    numbering = (line[0] == numbers[i]) && numbering;
  }
  adjustScore(numbering, 3, historyScore);
  ASSERT_TRUE(numbering);
}

// Commands listed in reverse order
TEST_F(ShellTest, CommandReverseOrder) {
  std::stack<std::string> stack;
  int status1 = writeInput("pwd\n", false);
  if (status1 == -1)
    exit(1);
  getOutput();
  stack.push("0\tpwd");
  int status2 = writeInput("ls\n", false);
  if (status2 == -1)
    exit(1);
  getOutput();
  stack.push("1\tls");

  // Check the command history
  int status = writeInput("history\n", true);
  if (status == -1)
    exit(1);
  std::string output = getOutput();
  stack.push("2\thistory");

  std::istringstream iss(output);
  std::string line;
  bool commandReverse = true;
  while (!stack.empty()) {
    std::string command = stack.top();
    stack.pop();
    std::getline(iss, line);
    commandReverse = (command == line) && commandReverse;
    EXPECT_EQ(command, line);
  }
  adjustScore(commandReverse, 3, historyScore);
}

TEST_F(ShellTest, CorrectCommandNumber) {
  std::vector<std::string> commands{"echo Hello World\n",
                                    "ls -l\n",
                                    "pwd\n",
                                    "du\n",
                                    "ls\n",
                                    "ds\n",
                                    "ps\n",
                                    "top\n",
                                    "pwd\n",
                                    "help\n",
                                    "pwd\n",
                                    "help\n"};
  for (int i = 0; i < 19; i++) {
    int writeStatus = writeInput("pwd\n", false);
    if (writeStatus == -1)
      exit(1);
    getOutput();
  }

  int status = writeInput("history\n", true);
  if (status == -1)
    exit(1);
  std::string output = getOutput();

  std::istringstream iss(output);
  std::string line;
  bool correctNumbering = true;
  for (int i = 19; i >= 10; i--) {
    std::getline(iss, line);
    int num = std::stoi(line.substr(0, 2));
    correctNumbering = (i == num) && correctNumbering;
  }
  adjustScore(correctNumbering, 4, historyScore);
  ASSERT_TRUE(correctNumbering);
}

TEST_F(ShellTest, ReRunPreviousCommand) {
  std::string output;

  int status1 = writeInput("echo test\n", false);
  if (status1 == -1)
    exit(1);
  getOutput();

  int status2 = writeInput("!!\n", true);
  if (status2 == -1)
    exit(1);
  output = getOutput();

  std::istringstream iss(output);
  std::string firstLine;
  std::string secondLine;
  std::getline(iss, firstLine);
  std::getline(iss, secondLine);
  bool commandShown = firstLine.find("echo test") != std::string::npos;
  bool commandOutput = firstLine.find("test") != std::string::npos;
  adjustScore(commandShown && commandOutput, 2, historyScore);
  ASSERT_TRUE(commandShown);
  ASSERT_TRUE(commandOutput);
}

TEST_F(ShellTest, ReRunNthCommand) {
  std::string output;
  std::string expected_output;

  int status1 = writeInput("echo Hello World!\n", false);
  if (status1 == -1)
    exit(1);
  expected_output = getOutput();

  int status2 = writeInput("ls\n", false);
  if (status2 == -1)
    exit(1);
  getOutput();

  int status3 = writeInput("pwd\n", false);
  if (status3 == -1)
    exit(1);
  getOutput();

  int status4 = writeInput("!0\n", true);
  if (status4 == -1)
    exit(1);
  output = getOutput();

  std::istringstream iss(output);
  std::string firstLine;
  std::string secondLine;

  std::getline(iss, firstLine);
  std::getline(iss, secondLine);

  bool command = firstLine == "echo Hello World!";
  bool command_output = secondLine == "Hello World!";

  ASSERT_TRUE(command);
  ASSERT_TRUE(command_output);

  adjustScore(command && command_output, 2, historyScore);
}

TEST_F(ShellTest, ReRunCommandInHistory) {
  std::stack<std::string> stack;
  std::string output;
  std::string expected_output;
  int status1 = writeInput("ls\n", false);
  if (status1 == -1)
    exit(1);
  getOutput();
  stack.push("0\tls");

  int status2 = writeInput("pwd\n", false);
  if (status2 == -1)
    exit(1);
  getOutput();
  stack.push("1\tpwd");

  int status3 = writeInput("echo Hello World from my shell!\n", false);
  if (status3 == -1)
    exit(1);
  getOutput();
  stack.push("2\techo Hello World from my shell!");

  int status4 = writeInput("!1\n", false);
  if (status4 == -1)
    exit(1);
  output = getOutput();
  stack.push("3\tpwd");

  int status5 = writeInput("!0\n", false);
  if (status5 == -1)
    exit(1);
  output = getOutput();
  stack.push("4\tls");

  int status6 = writeInput("history\n", true);
  if (status6 == -1)
    exit(1);
  output = getOutput();
  stack.push("5\thistory");

  std::istringstream iss(output);
  std::string line;
  bool commandInHistory = true;
  while (!stack.empty()) {
    std::string command = stack.top();
    stack.pop();
    std::getline(iss, line);
    commandInHistory = (line == command) && commandInHistory;
    EXPECT_EQ(line, command);
  }
  adjustScore(commandInHistory, 2, historyScore);
  ASSERT_TRUE(commandInHistory);
}

TEST_F(ShellTest, IllegalHistoryCommand) {
  int status1 = writeInput("ls\n", false);
  if (status1 == -1)
    exit(1);
  getOutput();

  int status2 = writeInput("pwd\n", false);
  if (status2 == -1)
    exit(1);
  getOutput();

  int status3 = writeInput("!hi\n", false);
  if (status3 == -1)
    exit(1);
  std::string output = getErrOutput();

  std::istringstream iss(output);
  std::string line;
  std::getline(iss, line);

  // make sure the first line isn't the command prompt. Should be an error
  // message instead
  bool errorCommand =
      (line.find(getcwd(NULL, 0)) == std::string::npos) && line.size() != 0;

  ASSERT_TRUE(errorCommand);

  int status4 = writeInput("history\n", true);
  if (status4 == -1)
    exit(1);
  std::string out = getOutput();

  std::istringstream is(out);
  std::string out_line;
  int num_of_lines = 0;
  while (std::getline(is, out_line))
    num_of_lines++;

  adjustScore(num_of_lines == 4, 2, historyScore);
  EXPECT_EQ(num_of_lines, 4);
}

TEST_F(ShellTest, IllegalNthCommand) {
  int status1 = writeInput("ls\n", false);
  if (status1 == -1)
    exit(1);
  getOutput();

  int status2 = writeInput("pwd\n", false);
  if (status2 == -1)
    exit(1);
  getOutput();

  int status3 = writeInput("!-1\n", false);
  if (status3 == -1)
    exit(1);
  std::string output = getErrOutput();

  std::istringstream iss(output);
  std::string line;
  std::getline(iss, line);

  // make sure the first line isn't the command prompt. Should be an error
  // message instead
  bool errorCommand =
      line.find(getcwd(NULL, 0)) == std::string::npos && line.size() != 0;

  ASSERT_TRUE(errorCommand);

  int status4 = writeInput("history\n", true);
  if (status4 == -1)
    exit(1);
  std::string out = getOutput();

  std::istringstream is(out);
  std::string out_line;
  int num_of_lines = 0;
  while (std::getline(is, out_line))
    num_of_lines++;

  adjustScore(num_of_lines == 4, 2, historyScore);
  EXPECT_EQ(num_of_lines, 4);
}

TEST_F(ShellTest, CtrlCInformation) {
  std::string output;
  bool score = true;
  getOutput();

  int status = kill(pid, SIGINT);

  // Check that the signal was correctly sent
  EXPECT_EQ(status, 0);

  output = getOutput();

  EXPECT_TRUE(output.find("help") != std::string::npos);
  score = (output.find("help") != std::string::npos) && score;
  EXPECT_TRUE(output.find("cd") != std::string::npos);
  score = (output.find("cd") != std::string::npos) && score;
  EXPECT_TRUE(output.find("exit") != std::string::npos);
  score = (output.find("exit") != std::string::npos) && score;
  EXPECT_TRUE(output.find("pwd") != std::string::npos);
  score = (output.find("pwd") != std::string::npos) && score;
  EXPECT_TRUE(output.find("history") != std::string::npos);
  score = (output.find("history") != std::string::npos) && score;

  // Check that the shell has resumed normal operation and is showing the
  // command prompt
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    std::string current_dir = cwd;
    std::string expected_output = current_dir + "$ ";
    EXPECT_TRUE(output.find(expected_output) != std::string::npos);
    score = (output.find(expected_output) != std::string::npos) && score;
  }
  adjustScore(score, 5, historyScore);
  std::cout << "success " << std::endl;
}

TEST_F(ShellTest, CtrlCNoCrash) {
  getOutput();

  int signal_status = kill(pid, SIGINT);

  // Check that the signal was correctly sent
  EXPECT_EQ(signal_status, 0);

  int status;
  int timeout = 1;
  int ret = waitpid(pid, &status, WNOHANG);
  while (ret == 0 && timeout > 0) {
    sleep(1);
    timeout--;
    ret = waitpid(pid, &status, WNOHANG);
  }

  if (ret != 0)
    EXPECT_TRUE(false) << "Shell exited";

  // Exit status of the shell program
  if (!WIFEXITED(status)) {
    adjustScore(true, 2, historyScore);
    EXPECT_NE(WEXITSTATUS(status), true);
  } else {
    EXPECT_TRUE(false) << "Shell exited";
  }
}

TEST_F(ShellTest, CDWithoutArgument) {
  std::string output;
  std::string home_path;

  int status1 = writeInput("cd\n", false);
  if (status1 == -1)
    exit(1);
  getOutput();

  int status2 = writeInput("pwd\n", false);
  if (status2 == -1)
    exit(1);
  output = getOutput();
  home_path = getenv("HOME");

  adjustScore(output.find(home_path) != std::string::npos, 3, internalScore);
  ASSERT_TRUE(output.find(home_path) != std::string::npos);
}

TEST_F(ShellTest, CDHome) {
  std::string output;
  std::string home_path;

  int status1 = writeInput("cd ~\n", false);
  if (status1 == -1)
    exit(1);
  getOutput();

  int status2 = writeInput("pwd\n", false);
  if (status2 == -1)
    exit(1);
  output = getOutput();
  home_path = getenv("HOME");

  adjustScore(output.find(home_path) != std::string::npos, 3, internalScore);
  ASSERT_TRUE(output.find(home_path) != std::string::npos);
}

TEST_F(ShellTest, ChangeToHomeDirectory) {
  std::string output;
  std::string home_path = getenv("HOME");
  std::string directory_path = home_path + "/testdir";
  std::string expected_output = directory_path;

  int create =
      mkdir(directory_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  int status1 = writeInput("cd ~/testdir\n", false);
  if (status1 == -1)
    exit(1);
  getOutput();

  int status2 = writeInput("pwd\n", true);
  if (status2 == -1)
    exit(1);
  output = getOutput();

  adjustScore(output.find(expected_output) != std::string::npos, 4,
              internalScore);
  ASSERT_TRUE(output.find(expected_output) != std::string::npos);

  int remove = rmdir(directory_path.c_str());
}

TEST_F(ShellTest, cdBack) {
  std::string output;
  std::string prev_path;
  std::string intermediate;

  int status1 = writeInput("pwd\n", false);
  if (status1 == -1)
    exit(1);
  prev_path = getOutput();

  int status2 = writeInput("cd /usr\n", false);
  if (status2 == -1)
    exit(1);
  getOutput();

  int status3 = writeInput("pwd\n", false);
  if (status3 == -1)
    exit(1);
  intermediate = getOutput();

  ASSERT_TRUE(intermediate.find(parsedOutput(prev_path)) == std::string::npos);

  int status4 = writeInput("cd -\n", false);
  if (status4 == -1)
    exit(1);
  getOutput();

  int status5 = writeInput("pwd\n", true);
  if (status5 == -1)
    exit(1);
  output = getOutput();

  int end = output.find('\n');
  output = output.substr(0, end);

  adjustScore(output.find(parsedOutput(prev_path)) != std::string::npos, 5,
              internalScore);
  ASSERT_TRUE(output.find(parsedOutput(prev_path)) != std::string::npos);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();
  std::cout << "External: " << externalScore << std::endl;
  std::cout << "Internal: " << internalScore << std::endl;
  std::cout << "History: " << historyScore << std::endl;
  int total = externalScore + internalScore + historyScore;
  std::cout << "Final score: " << total << std::endl;
  return result;
}
