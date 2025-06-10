#include "gsh.h"

int gsh_pipe(char **left, char **right) {
    int pipefd[2];
    pid_t pid1, pid2;

    // fd[0] will be fd for read end of the pipe
    // fd[1] will be fd for write end of the pipe

    if (pipe(pipefd) == -1) { // success ? 0 : -1
        perror("gsh: pipe");
        return 1;
    }

    pid1 = fork();
    if (!pid1) {
        // first child: left command
        dup2(pipefd[1], STDOUT_FILENO); // redirect stdout to write end
        close(pipefd[0]); // close unused read end
        close(pipefd[1]); // we've already copied this file to stdout
        execvp(left[0], left);
        perror("gsh");
        exit(EXIT_FAILURE);
    }

    pid2 = fork();
    if (!pid2) {
        // Second child: right command
        dup2(pipefd[0], STDIN_FILENO); // redirect stdin to read end
        close(pipefd[1]); // close unused write end
        close(pipefd[0]);
        execvp(right[0], right);
        perror("gsh");
        exit(EXIT_FAILURE);
    }

    // parent
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 1;
}
int gsh_launch(char **args) {
    __pid_t pid, wpid;
    int status;

    pid = fork(); // creates a new child process
    // returns pid of child process to parent process or 0 to child process
    if (pid == 0) { 
        // child process
        if (execvp(args[0],args) == -1) {
            perror("gsh");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error in forking
        perror("gsh");
    } else {
        // Parent process
        do {
            wpid = waitpid(pid, &status, WUNTRACED); 
            // obtains status information about one of the caller's child processes
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        // keeps running till all processes are terminated normally
    }
    return 1;
}

char *str[] = {
    "cd",
    "help",
    "exit"
};

int num() {
    return sizeof(str) / sizeof(char*);
}

int cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "gsh: expected argument to cd\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("gsh");
        }
    }
    return 1;
}

int gsh_exit(char **args) {
    return 0;
}

int help(char **args) {
    printf("Type program names and arguments and hit enter, the following are builtin:\n");
    for (int i = 0; i < num(); i++) {
        printf(" %s\n", str[i]);
    }
    return 1;
}   

int (*func[]) (char **) = {
    &cd, &help, &gsh_exit
};

int gsh_exec(char **args) {
    if (args[0] == NULL) {
        return 1; // empty command
    }

    int i = 0;
    while (args[i] != NULL) { // for piping
        if (strcmp(args[i], "|") == 0) { // detects if a pipe exists in a command
            args[i] = NULL;
            return gsh_pipe(args, &args[i+1]);
        }
        i++;
    }

    for (int i = 0; i < num(); i++) { // for builtin commands
        if (strcmp(args[0], str[i]) == 0) {
            return (*func[i])(args);
        }
    }

    return gsh_launch(args);
}

char *gsh_read() {
    __ssize_t buffer = 0; // ssize_t used since buffer > 0
    char *line = NULL;

    if (getline(&line, &buffer, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS); // received EOF
        } else {
            perror("readline");
            exit(EXIT_FAILURE); // didnt receieve EOF so exits in failure
        }
    }

    return line;
}

#define GSH_TOK_BUFSIZE 64 // this is the buffer by which we increase size required
#define GSH_TOK_DELIM " \t\r\n\a" // delimiter: arguments will be seperated by them
char **gsh_parse(char *line)
{
  int bufsize = GSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) { // failed to allocate memory through malloc
    fprintf(stderr, "gsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, GSH_TOK_DELIM); // returns ptr to beginning of next token
  while (token != NULL) {
    tokens[position] = token;
    position++; // creates an array of ptrs to each token

    if (position >= bufsize) { // creates more space for tokens
      bufsize += GSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "gsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, GSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens; // returns the array of ptrs
}

void gsh_loop() {
    char *line;
    char **args;
    int status;

    do {
        printf("> ");
        line = gsh_read(); // read the command given by user
        args = gsh_parse(line); // parse the command string into args
        status = gsh_exec(args); // run the parsed command

        free(line);
        free(args);
    } while (status);
}

int main(int argc, char **argv) {
    gsh_loop();
    return EXIT_SUCCESS;
}