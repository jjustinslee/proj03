#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALPHABET_LEN 26

int count_letters(const char *file_name, int *counts) {
  FILE *file = fopen(file_name, "r");
  if (!file) {
    // Match the exact error message format expected by the test
    perror("fopen");
    return -1;
  }
  memset(counts, 0, ALPHABET_LEN * sizeof(int));
  int c;
  while ((c = fgetc(file)) != EOF) {
    if (isalpha(c)) {
      counts[tolower(c) - 'a']++;
    }
  }
  fclose(file);
  return 0;
}

int process_file(const char *file_name, int out_fd) {
  int counts[ALPHABET_LEN];
  if (count_letters(file_name, counts) == -1) {
    return -1;
  }
  write(out_fd, counts, sizeof(counts));
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 1) {
    return 0;
  }

  int pipe_fds[2];
  if (pipe(pipe_fds) == -1) {
    perror("pipe");
    return 1;
  }

  for (int i = 1; i < argc; i++) {
    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      return 1;
    } else if (pid == 0) {
      close(pipe_fds[0]);
      if (process_file(argv[i], pipe_fds[1]) == -1) {
        exit(1);
      }
      close(pipe_fds[1]);
      exit(0);
    }
  }

  close(pipe_fds[1]);
  int total_counts[ALPHABET_LEN] = {0};
  int buffer[ALPHABET_LEN];
  ssize_t bytes_read;
  while ((bytes_read = read(pipe_fds[0], buffer, sizeof(buffer))) > 0) {
    for (int i = 0; i < ALPHABET_LEN; i++) {
      total_counts[i] += buffer[i];
    }
  }
  close(pipe_fds[0]);

  while (wait(NULL) > 0)
    ;

  for (int i = 0; i < ALPHABET_LEN; i++) {
    printf("%c Count: %d\n", 'a' + i, total_counts[i]);
  }
  return 0;
}
