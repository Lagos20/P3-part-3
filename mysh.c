#include <stdio.h>  //standard I/O 
#include <stdlib.h> //memory managment
#include <string.h> //strchr, strcmp
#include <unistd.h> //fork,chdir,execv,dup2, getcwd
#include <fcntl.h> // open
#include <sys/wait.h> //waitpid
#include <sys/types.h> //the data types in system calls 
#include <sys/stat.h> //definitions in regards to file status
#include <dirent.h> //opendir, readdir
#include <errno.h> //pzerror
#include <signal.h> //the signal handling
#include <fnmatch.h> //fnmatch

//constants
#define PROMPT "mysh> " 
#define BUFSIZE 1024 
#define MAX_ARGS 100 
#define PATHS "/usr/local/bin:/usr/bin:/bin"

void ChangeDirectory(const char *path); 


void PrintsError(const char *message)
{ 
    fprintf(stderr, "Error:%s\n", message);
}


void salutations() //this is the welcome message for interactive mode
{ 
    printf("Welcome to my noble shell! Type precisely!\n");
}
void goodbye() //goodbye message for interactive mode
{ 
    printf("Exiting my noble shell.\n");
}

int InInteractiveMode() // will see if its in interactive mode
{
    return isatty(STDIN_FILENO);
}

void PrintCurrentDirectory() //for printing thee directory
{ 
    char cwd[BUFSIZE];
    if (getcwd(cwd, sizeof(cwd))){
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
}



char *SearchFullPath(const char *command) 
{ 
    static char full_path[BUFSIZE];
    char *paths = strdup(PATHS); //copyies path avoiding changing constant string
    if (!paths) {
        perror("strdup");
                return NULL;
    }


    char *start = paths;
    while (*start) {
        char *end = start;
        while (*end != ':' && *end != '\0') end++;

        char saved = *end; //temp terminate path
        *end = '\0';

        snprintf(full_path, BUFSIZE, "%s/%s", start, command); //CONSTRUCT fullpath
        if (access(full_path, X_OK) == 0) { //executable check
            free(paths);
                return full_path;
        }

        *end = saved; //restore  OG path separator  
        start = (saved == ':') ? end + 1 : end;
    }

    free(paths);
    return NULL; 
}
void WhichCommand(const char *command) //helps find location of command
{ 
    if (!command) {                       
        fprintf(stderr, "which: missing agrument\n");
        return;
    }

    char *path = SearchFullPath(command);
    if (path) {
        printf("%s\n", path);
    } else {
        fprintf(stderr, "which: command not found: %s\n", command);
    }
}

void ChangeDirectory(const char *path) //change current directory
{ 
    if(!path) {
        fprintf(stderr, "cd: missing agrument \n");
        return;
    }

    if (chdir(path) != 0) {
            perror("cd"); 
    }
}

void ControlRedirection(char **args, int *in_fd, int *out_fd) //I/O redirection 
{  //int_fd - input file descriptor // out_fd  - output file descriptor
    for(int i =0; args[i]; i++) {
        if (strcmp(args[i], "<") == 0) {
            *in_fd = open(args[i + 1], O_RDONLY);
            if (*in_fd < 0 ) {
                PrintsError("Input redirection failed");
                    exit(EXIT_FAILURE);
            }
            args[i] = NULL; //rid of redirection from Arg
        } else if (strcmp(args[i], ">") == 0) {
            *out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (*out_fd < 0) {
                PrintsError("Output redirection failed");
                    exit(EXIT_FAILURE);
            }
                args[i] = NULL; //x2
        }
    }
}



void ExpandWildcards(char *token, char **args, int *arg_index)  //expands wildcard char in given token from matchin files
{ 
   char *asterisk = strchr(token, '*');
   if (!asterisk) { //no wildcard //add token
    args[(*arg_index)++] = strdup(token);
    return; 
   }

   char prefix[BUFSIZE] = {0} , suffix[BUFSIZE] = {0};
   strncpy(prefix, token, asterisk - token);// out prefix
   strcpy(suffix, asterisk + 1); //out suffix

   DIR *dir = opendir(".");
   if (!dir) {
    perror("opendir");
    return;
   }

   struct dirent *entry ;
   while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.' && token[0] != '.')continue; //will skip hidden files //unless rrequested

    if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0 && //prefix suffix match
        strcmp(entry->d_name + strlen(entry->d_name)-strlen(suffix), suffix) == 0) {
        args[(*arg_index)++] = strdup(entry->d_name);
    } 
   }
   closedir(dir); //no match add token asez
   if (*arg_index == 0) {
    args[(*arg_index)++] = strdup(token);
   }
}

void ExecuteCommand(char **args, int in_fd, int out_fd) 
{  
    if (in_fd != STDIN_FILENO) {
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);
    }
    if (out_fd != STDIN_FILENO) {
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);
    }

    if(execv(args[0], args) < 0) {
        PrintsError("Execution failed");
        exit(EXIT_FAILURE);
    }
}

void ControlPipeline(char *line)
{ 
    char *commands[2];
    char *args1[MAX_ARGS], *args2[MAX_ARGS];
    int pipefd[2];


    char *pipe_pos = strchr(line, '|');
    if(!pipe_pos) {
        fprintf(stderr, "Invalid pipeline command \n");
        return;
    }

    *pipe_pos = '\0'; 
    commands[0] = line;
    commands[1] = pipe_pos + 1;

    char *token = commands[0];
    int arg_index = 0; 
    while (*token && arg_index < MAX_ARGS -1) {
        while (*token == '\0') token++;
        if (*token == '\0') break;

    
        char *start = token;
        while (*token != ' ' && *token != '\0') token++;
        if (*token != '\0') *token++ = '\0';

        args1[arg_index++] = start;
    }
    args1[arg_index] = NULL;

    token = commands[1];
    arg_index = 0;
    while (*token == arg_index < MAX_ARGS -1){
        while (*token == ' ') token++;
         if (*token == '\0') break;

        char *start = token;
        while (*token != ' ' && *token != '\0') token++;
        if (*token != '\0') *token++ = '\0';

        args2[arg_index++] = start;
    }
    args2[arg_index] = NULL;

    if(pipe(pipefd) < 0) {
        perror("pipe");
        return;
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
       
        close(pipefd[0]); //closed the unused read end 
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        char *executable1 = SearchFullPath(args1[0]);
        if (!executable1) {
            fprintf(stderr, "Command not found: %s\n", args1[0]);
            exit(EXIT_FAILURE);

        }
        args1[0] = executable1;
        execv(args1[0], args1);
        perror("execv");
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        
        close(pipefd[1]); //close unuse write endddd
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        char *executable2 = SearchFullPath(args2[0]);
        if (!executable2) {
            fprintf(stderr, "Command not found: %s\n", args2[0]);
            exit(EXIT_FAILURE);
        }
        args2[0] = executable2;
        execv(args2[0], args2);
        perror("execv");
        exit(EXIT_FAILURE);

    }

    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

void InterpretCommand(char *line, int InInteractiveMode)
{ 
    char *args[MAX_ARGS] = {NULL};
    int in_fd = STDIN_FILENO, out_fd = STDOUT_FILENO;
    int arg_index = 0;

    char *token_start = line;

    for (int i =0; line[i] != '\0'; i++) {
        if (line[i] == ' ' || line[i] == '\t' || line[i] == '\0') {
            if (token_start != &line[i]) {

                args[arg_index] = malloc(i - (token_start - line) + 1);
                if (!args[arg_index]) {
                    perror("malloc failed");
                    exit(EXIT_FAILURE);
                }

                strncpy(args[arg_index], token_start, i - (token_start - line));
                args[arg_index][i - (token_start - line)] = '\0';
                arg_index++;
            }
            if (line[i] == '\0') break;
            token_start = &line[i + 1];
        }
    }

    if (token_start != &line[strlen(line)]) {
        args[arg_index] = strdup(token_start);
        arg_index++;
    } 
    
    args[arg_index] = NULL;

    if (!args[0]) return; 
    if (strcmp(args[0], "cd") == 0) {
        ChangeDirectory(args[1]);
    } else if (strcmp(args[0], "pwd") == 0) {
        PrintCurrentDirectory();
    } else if (strcmp(args[0], "which") == 0){
        WhichCommand(args[1]);
    } else if (strcmp(args[0], "exit") == 0) {
        if (InInteractiveMode) goodbye();
        exit(0);
    } else {
        ControlRedirection(args, &in_fd, &out_fd);
        char *executable = SearchFullPath(args[0]);
        if (!executable) {
            fprintf(stderr, "Command not found: %s\n", args[0]);
            return;
        }
      
        pid_t pid = fork(); 
        if(pid == 0) { 
            ExecuteCommand(args, in_fd, out_fd);
        } else {
            wait(NULL);
        }  
    }

    for (int i = 0; args[i]; i++) {
        free(args[i]);
    }
}


void RunTheShell(FILE *input, int interactive) 
{ 
    if (interactive) salutations(); 

    char buffer[BUFSIZE];
    while(1) {
        if (interactive) printf(PROMPT);
        ssize_t bytes_read = read(STDIN_FILENO, buffer, BUFSIZE - 1);
        if (bytes_read <= 0) break;

        buffer[bytes_read -1] = '\0';
        InterpretCommand(buffer, interactive);
    }

    if (interactive) goodbye();
}

int main(int argc, char **argv) 
{ 
    if (argc > 2) { 
        fprintf(stderr, "Usage: %s [batch_file]\n", argv[0]);
            exit(EXIT_FAILURE);
    }

    FILE *input = stdin;
    int interactive = isatty(STDIN_FILENO);


    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (!input) {
            perror("Failed to open batch file");
                exit(EXIT_FAILURE);
        }
        interactive = 0;
    }

    RunTheShell(input, interactive);


    if (input != stdin) fclose(input);
    return 0;
}


