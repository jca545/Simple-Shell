#pragma once

#define TMA_MSG "too many arguments"
#define HELP_HELP_MSG "show help information"
#define EXTERN_HELP_MSG "external command or application"
#define EXIT_HELP_MSG "exit the shell"
#define PWD_HELP_MSG "print the current directory"
#define CD_HELP_MSG "change the current directory"
#define HISTORY_HELP_MSG "print the history of commands"
#define HISTORY_INVALID_MSG "command invalid"
#define HISTORY_NO_LAST_MSG "no command entered"
#define GETCWD_ERROR_MSG "unable to get current directory"
#define CHDIR_ERROR_MSG "unable to change directory"
#define READ_ERROR_MSG "unable to read command"
#define FORK_ERROR_MSG "unable to fork"
#define EXEC_ERROR_MSG "unable to execute command"
#define WAIT_ERROR_MSG "unable to wait for child"

/*
 * Concatenate a command and a message.
 * Both arguments must be string constants, not variables.
 *
 * Example:
 * const char *msg = FORMAT_MSG("cd", CD_HELP_MSG);
 *
 * Then you can use `msg` as a string for `write()`.
 */
#define FORMAT_MSG(cmd, msg) cmd ": " msg "\n"

/*
 * Concatenate a number and a command for `history`.
 * Both arguments must be string constants, not variables.
 *
 * Example:
 * const char *msg = FORMAT_HISTORY("0", "cd");
 *
 * Then you can use `msg` as a string for `write()`.
 */
#define FORMAT_HISTORY(num, cmd) num "\t" cmd "\n"
