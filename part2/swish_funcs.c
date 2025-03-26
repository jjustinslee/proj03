#include "swish_funcs.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "string_vector.h"

#define MAX_ARGS 10

/*
 * Helper function to run a single command within a pipeline.
 */
int run_piped_command(strvec_t *tokens, int *pipes, int n_pipes, int in_idx,
                      int out_idx) {
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return -1;
  } else if (pid == 0) { // Child process
    if (in_idx != -1) {  // If there's an input pipe
      dup2(pipes[in_idx], STDIN_FILENO);
    }
    if (out_idx != -1) { // If there's an output pipe
      dup2(pipes[out_idx], STDOUT_FILENO);
    }

    // Check for input redirection
    for (size_t i = 0; i < tokens->length; i++) {
      if (strcmp(tokens->data[i], "<") == 0 && i + 1 < tokens->length) {
        FILE *input_file = fopen(tokens->data[i + 1], "r");
        if (!input_file) {
          perror("fopen");
          exit(EXIT_FAILURE);
        }
        dup2(fileno(input_file), STDIN_FILENO);
        fclose(input_file);
        tokens->length = i; // Remove `< filename` from arguments
        break;
      }
    }

    // Check for output redirection
    for (size_t i = 0; i < tokens->length; i++) {
      if (strcmp(tokens->data[i], ">") == 0 && i + 1 < tokens->length) {
        FILE *output_file = fopen(tokens->data[i + 1], "w");
        if (!output_file) {
          perror("fopen");
          exit(EXIT_FAILURE);
        }
        dup2(fileno(output_file), STDOUT_FILENO);
        fclose(output_file);
        tokens->length = i; // Remove `> filename` from arguments
        break;
      }
    }

    // Close all pipe file descriptors in child process
    for (int i = 0; i < n_pipes; i++) {
      close(pipes[i]);
    }

    // Convert strvec_t to char* array
    char *args[MAX_ARGS];
    for (size_t i = 0; i < tokens->length && i < MAX_ARGS - 1; i++) {
      args[i] = tokens->data[i];
    }
    args[tokens->length] = NULL;

    // Execute command
    execvp(args[0], args);
    perror("execvp"); // Only reached if execvp fails
    exit(EXIT_FAILURE);
  }

  return pid; // Return child process ID
}

/*
 * Runs a sequence of pipelined commands.
 */
int run_pipelined_commands(strvec_t *tokens) {
  int num_commands = 1;

  // Count the number of pipes to determine how many commands exist
  for (size_t i = 0; i < tokens->length; i++) {
    if (strcmp(tokens->data[i], "|") == 0) {
      num_commands++;
    }
  }

  int pipes[2 * (num_commands - 1)]; // File descriptors for pipes
  for (int i = 0; i < num_commands - 1; i++) {
    if (pipe(pipes + i * 2) < 0) {
      perror("pipe");
      return -1;
    }
  }

  int command_index = 0;
  int in_idx = -1, out_idx = -1;
  strvec_t command_tokens;
  strvec_init(&command_tokens);

  // Iterate through tokens and execute each command
  for (size_t i = 0; i < tokens->length; i++) {
    if (strcmp(tokens->data[i], "|") == 0) {
      // Execute current command
      out_idx = (command_index * 2) + 1; // Write to next pipe
      run_piped_command(&command_tokens, pipes, 2 * (num_commands - 1), in_idx,
                        out_idx);

      // Prepare next command
      strvec_clear(&command_tokens);
      in_idx = (command_index * 2); // Read from previous pipe
      command_index++;
    } else {
      strvec_add(&command_tokens, tokens->data[i]);
    }
  }

  // Execute last command
  run_piped_command(&command_tokens, pipes, 2 * (num_commands - 1), in_idx, -1);
  strvec_clear(&command_tokens);

  // Close all pipes in parent process
  for (int i = 0; i < 2 * (num_commands - 1); i++) {
    close(pipes[i]);
  }

  // Wait for all children to finish
  for (int i = 0; i < num_commands; i++) {
    wait(NULL);
  }

  return 0;
}
