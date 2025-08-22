#include <stdio.h>
#include <unistd.h>
#include <glob.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>



struct termios dlt,new_ts;
char prevDir [2048] = "";
typedef struct Node{
    char *command;
    struct Node*next;
    struct Node*prev;
}Node;

typedef struct list{
    Node*head;
    Node*tail;
    int count;
}list;


list history = {NULL,NULL,0};

bool noncanonical(struct termios *t){
    tcgetattr(STDIN_FILENO, t);
    t->c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, t);
    return 1;
}

void restore(struct termios *t){
    tcsetattr(STDIN_FILENO, TCSANOW, t);
}

bool readInput(char *input){
    /*
        ->reads the input(i.e the file with FD no 0) and updates the argv
        ->returns the input removing thre trailing '\n'
    */
    char * flag = fgets(input,2049,stdin); // reading from stdin
    if(flag){
        size_t l = strlen(input);
        if(l > 0 && input[l-1] == '\n'){
            input[l-1] = '\0';
        }
        return 1;
    }
    return 0;
}


char **parsing(char *inp,char delim1,char delim2){
    bool b = 1;
    bool state = 0;
    size_t s = strlen(inp);
    char prev = '\0';
    char token[2048];
    memset(token,0,sizeof(token));
    int count = 0;
    char **argv = malloc(100*sizeof(char*));
    int argc = 0;
    for(int i = 0;i<s;i++){
        if(delim1 == '\0')state = 1;
        if(b == 0){
            token[count] = inp[i];
            count++;
            if(inp[i] == '\'' || inp[i] == '\"')b = 1;
        }
        else{
            if(state == 0 && inp[i] == delim1){
                state = 1;
            }
            else if(inp[i] == delim2 && state == 1){
                if(delim2 != ' ' || (delim2 == ' ' && token[0] != '\0')){
                    argv[argc] = strdup(token);
                    argc++;
                }
                memset(token,0,sizeof(token));
                count = 0;
                state = 0;
            }
            else if(state == 1 && delim1 != '\0'){
                token[count] = delim1;
                count++;
                token[count] = inp[i];
                count++;
                state = 0;
            }
            else{
                token[count] = inp[i];
                count++;
            }
            if(inp[i] == '\'' || inp[i] == '\"')b = 0;
            prev = inp[i];
        }
        fflush(stdout);
    }
    if(count > 0 || (state == 1 && delim1 != '\0')){
        if(state == 1){
            token[count] = delim1;
            count++;
        }
        argv[argc] = strdup(token);
        argc++;
    }
    argv[argc] = NULL;
    return argv;
}


bool removeQuotes(char *str){
    size_t len = strlen(str);
    if(len > 1 && ((str[0] == '\'' && str[len-1] == '\'') || (str[0] == '\"' && str[len-1] == '\"'))){
        str[len-1] = '\0';
        memmove(str, str+1, len-1);
    }
    return 1;
}

int to_int(char *str){
    for(int i = 0;i < strlen(str);i++){
        if(str[i] < '0' || str[i] > '9')return -1;
    }
    return atoi(str);
}

void freeStringList(char **strList){
    /*
        frees up the memory allocated to the list of strings
        Careful with freeing garbage pointers ;if not allocated by malloc ,that means you are deleting adress that is not accessable
    */
    for(int i = 0;strList[i] != NULL;i++){
        if(strList[i])free(strList[i]);
    }
    free(strList);
}


/*
Handling history from here
stored in a linked list
added by the function addInputToHistory
and accessed by the function printHistory
*/



bool addInputToHistory(char *input){
    Node*node = malloc(sizeof(Node));
    if(!node){
        perror("malloc failed for adding to history");
        return 0;
    }
    node->command = strdup(input);
    node->next = NULL;
    node->prev = NULL;
    if(history.count == 0){
        history.head = node;
        history.tail = node;
        history.count = 1;
        return 1;
    }
    history.tail->next = node;
    node->prev = history.tail;
    history.tail = node;

    if(history.count == 2048){
        Node*toremove = history.head;
        history.head = toremove->next;
        history.head->prev = NULL;
        history.count--;
        free(toremove);
    }
    history.count ++;
    return true;
}

int printHistory(int n){
    Node *node = history.head;
    if(n < history.count){
        if(n == 0)return 1;
        node = history.tail;
        int ind = 1;
        while(ind != n){
            node = node->prev;
            ind++;
        }
    }
    while(node){
        printf("%s\n",node->command);
        fflush(stdout);
        node = node ->next;
    }
    return 1;
}

char **expand(char **argv){
    char **expanded = malloc(2048 * sizeof(char*));
    int count = 0;
    for(int i = 0;argv[i] != NULL;i++){
        glob_t globbuf;
        int flag = glob(argv[i], GLOB_NOCHECK, NULL, &globbuf);
        if(flag == 0){
            for(int j = 0; j < globbuf.gl_pathc; j++){
                expanded[count] = strdup(globbuf.gl_pathv[j]);
                count++;
            }
        }
        globfree(&globbuf);
    }
    expanded[count] = NULL;
    return expanded;
}


int excuteBasic(char *command){
    char **tempcommand = parsing(command,'\0',' ');
    char **parsedcommand = expand(tempcommand);

    for(int i = 1;parsedcommand[i] != NULL;i++){
        if(!removeQuotes(parsedcommand[i])){
            fprintf(stderr, "Error at %s:%d: failed to remove quotes from '%s'\n", __FILE__, __LINE__, parsedcommand[i]);
            return -1;
        }
    }


    freeStringList(tempcommand);

    if(parsedcommand[0] == NULL)return 1;
    if(strcmp(parsedcommand[0],"exit") == 0){
        restore(&dlt);
        exit(0);
    }
    
    if(strcmp(parsedcommand[0],"cd") == 0){
        char cwd[2048];
        if(parsedcommand[1] != NULL && parsedcommand[2] != NULL)return -1;
        if(getcwd(cwd,sizeof(cwd)) == NULL){
            return -1;
        }
        if(parsedcommand[1] == NULL || parsedcommand[1][0] == '~'){
            strcpy(prevDir,cwd);
            const char *home = getenv("HOME");
            int s = strlen(home);
            char path[4048];
            strcpy(path,home);
            if(parsedcommand[1] != NULL && parsedcommand[1][1] == '/'){
                strcat(path,parsedcommand[1]+1);
            }
            chdir(path);
        }
        else if(strcmp(parsedcommand[1],"-") == 0){
            if(prevDir[0] == '\0')return -1;
            if(chdir(prevDir) == 0){
                printf("%s\n", prevDir);
                strcpy(prevDir,cwd);
            }
            else return -1;
        }
        else{
            strcpy(prevDir,cwd);
            if(chdir(parsedcommand[1]) != 0)return -1;
        }
        return 1;
    }
    if(strcmp(parsedcommand[0],"history") == 0){
        int n = 10000;
        if(parsedcommand[1] != NULL){
            n = to_int(parsedcommand[1]);
            if(n < 0)return -1;
        }
        return printHistory(n);
    }
    pid_t pid = fork();
    if(pid == 0){
        execvp(parsedcommand[0],parsedcommand);
        exit(127);
    }
    else if(pid < 0){
        return -1;
    }
    int status;
    waitpid(pid,&status,0);
    if(WIFEXITED(status)){
        if(WEXITSTATUS(status) == 0)return 1;
        else if(WEXITSTATUS(status) == 127)return -1;
        else return 0;
    }
    return -1;
}

int justexec(char *command){
    char **tempcommand = parsing(command,'\0',' ');
    char **expandedcommand = expand(tempcommand);

    for(int i = 1;expandedcommand[i] != NULL;i++){
        if(!removeQuotes(expandedcommand[i])){
            fprintf(stderr, "Error at %s:%d: failed to remove quotes from '%s'\n", __FILE__, __LINE__, expandedcommand[i]);
            return -1;
        }
    }

    fflush(stdout);
    freeStringList(tempcommand);

    execvp(expandedcommand[0], expandedcommand);
    fprintf(stderr, "Error at %s:%d: failed to execvp '%s'\n", __FILE__, __LINE__, expandedcommand[0]);
    return -1;
}

int handlePiping(char *input){
    char **commands = parsing(input,'\0','|');
    if(commands[1] == NULL){
        freeStringList(commands);
        return -2;
    }
    int fd[2];
    //printf("parent id is %d\n", getpid());
    if(pipe(fd) == -1){
        fprintf(stderr, "Error at %s:%d: failed to create pipe\n", __FILE__, __LINE__);
        freeStringList(commands);
        return -1;
    }
    pid_t pid1 = fork();
    if(pid1 < 0){
        fprintf(stderr, "Error at %s:%d: failed to fork for first command\n", __FILE__, __LINE__);
        freeStringList(commands);
        return -1;
    }
    if(pid1 == 0){
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);
        justexec(commands[0]);
        //printf("child 1 command : %s\n", commands[0]);
        fprintf(stderr, "Error at %s:%d: failed to execvp '%s'\n", __FILE__, __LINE__, commands[0]);
        exit(127);
    }
    pid_t pid2 = fork();
    if(pid2 < 0){
        fprintf(stderr, "Error at %s:%d: failed to fork for second command\n", __FILE__, __LINE__);
        freeStringList(commands);
        return -1;
    }
    if(pid2 == 0){
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        //printf("child 2 command : %s\n", commands[1]);
        justexec(commands[1]);
        fprintf(stderr, "Error at %s:%d: failed to execvp '%s'\n", __FILE__, __LINE__, commands[1]);
        exit(127);
    }

    close(fd[0]);
    close(fd[1]);
    //printf("parent pid is %d for process %d\n", getppid(),getpid());
    int status1,status2;
    waitpid(pid1,&status1,0);
    //printf("child 1 exited with status %d\n", WEXITSTATUS(status1));
    waitpid(pid2,&status2,0);
    //printf("child 2 exited with status %d\n", WEXITSTATUS(status2));
    if(WIFEXITED(status1) && WIFEXITED(status2)){
        if(WEXITSTATUS(status1) == 0 && WEXITSTATUS(status2) == 0)return 1;
        else if(WEXITSTATUS(status1) == 127 || WEXITSTATUS(status2) == 127)return -1;
        else return 0;
    }
    return -1;
}

int handleIO(char *input){

    char **commands1 = parsing(input,'\0','>');
    char **commands2 = parsing(input,'\0','<');
    char **commands3 = parsing(input,'>','>');
    
    if(commands1[1] == NULL && commands2[1] == NULL && commands3[1] == NULL){
        freeStringList(commands1);
        freeStringList(commands2);
        freeStringList(commands3);

        return -2;
    }

    pid_t pid = fork();

    if(pid < 0){
        fprintf(stderr, "Error at %s:%d: failed to fork for IO handling\n", __FILE__, __LINE__);
        freeStringList(commands1);
        freeStringList(commands2);
        freeStringList(commands3);
        return -1;
    }
    
    if(pid == 0){
        int fd = -1;
        if(commands3[1] != NULL){
            char **files = parsing(commands3[1],'\0',' ');
            char file[2048];
            strcpy(file, files[0]);
            freeStringList(files);
            fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if(fd == -1){
                fprintf(stderr, "Error at %s:%d: failed to open '%s'\n", __FILE__, __LINE__, commands1[1]);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            justexec(commands3[0]);
        }
        if(commands2[1] != NULL){
            char **files = parsing(commands2[1],'\0',' ');
            char file[2048] ;
            strcpy(file, files[0]);
            freeStringList(files);
            fd = open(file, O_RDONLY);
            if(fd == -1){
                fprintf(stderr, "Error at %s:%d: failed to open '%s'\n", __FILE__, __LINE__, commands2[1]);
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            justexec(commands2[0]);
        }
        if(commands1[1] != NULL){
            char **files = parsing(commands1[1],'\0',' ');
            char file[2048] ;
            //printf("file: %s\n", files[0]);
            strcpy(file, files[0]);
            freeStringList(files);
            fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd == -1){
                fprintf(stderr, "Error at %s:%d: failed to open '%s'\n", __FILE__, __LINE__, commands1[1]);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            justexec(commands1[0]);
        }
    }
    int status;
    waitpid(pid,&status,0);
    //printf("IO handling completed\n");
    //printf("file descriptor for stdout is : %d\n", stdout->_fileno);
    if(WIFEXITED(status)){
        if(WEXITSTATUS(status) == 0)return 1;
        else if(WEXITSTATUS(status) == 127)return -1;
        else return 0;
    }
    return -1;
}

int handleAND(char *input){
    char **parsed = parsing(input,'&','&');
    bool flag = true;
    for(int i = 0; parsed[i] != NULL && flag; i++){
        if(parsed[i] == NULL || parsed[i][0] == '\0'){
            freeStringList(parsed);
            return -1;
        }
        int temp = handlePiping(parsed[i]);
        if(temp != -2){
            if(temp == -1){
                freeStringList(parsed);
                return -1;
            }
            flag = (temp == 1);
            continue;
        }
        temp = handleIO(parsed[i]);
        if(temp != -2){
            if(temp == -1){
                freeStringList(parsed);
                return -1;
            }
            flag = (temp == 1);
            continue;
        }
        //printf("Executing command: %s\n", parsed[i]);
        temp = excuteBasic(parsed[i]);
        if(temp == -1){
            fprintf(stderr, "Error at %s:%d: failed to execute '%s'\n", __FILE__, __LINE__, parsed[i]);
            flag = false;
            return -1;
        }
        flag = (temp == 1);
    }
    freeStringList(parsed);
    return flag ? 1 : 0;
}

int handle(char *input){
    //printf("%s\n", input);
    fflush(stdout);
    char **commands = parsing(input,'\0',';');

    for(int i = 0; commands[i] != NULL; i++){
        if(commands[i][0] == '\0'){
            freeStringList(commands);
            return -1;
        }
        int status = handleAND(commands[i]);
        if(status == -1){
            freeStringList(commands);
            return -1;
        }
    }
    freeStringList(commands);
    return 0;
}

void handleTab(char *input, int *ind) {
    struct dirent *entry;
    int size = 0;
    int index = *ind;
    while (index - size - 1 >= 0) {
        if (input[index - size - 1] == ' ') {
            break;
        }
        size++;
    }

    DIR *d = opendir(".");
    if (d) {
        while ((entry = readdir(d)) != NULL) {
            if (strlen(entry->d_name) < size) continue;

            bool flag = true;
            for (int i = 0; i < size; i++) {
                if (entry->d_name[i] != input[index - size + i]) {
                    flag = false;
                    break;
                }
            }

            if (flag) {
                for (int i = size; entry->d_name[i] != '\0'; i++) {
                    putchar(entry->d_name[i]);
                    input[index++] = entry->d_name[i];
                    fflush(stdout);
                }
                input[index] = '\0';
                break; 
            }
        }
        closedir(d);
    }
    *ind = index;
}

int handleTerminal(char *input){
    int count = 0;
    while(true){
        char c = getchar();
        if(c == '\n'){
            putchar('\n');
            input[count] = '\0';
            fflush(stdout);
            return count;
        }
        else if(c == 3){ // Ctrl+C
            restore(&dlt);
            exit(0);
            break;
        }
        else if(c == 4){ // Ctrl+D
            return count;
        }
        else if(c == '\t'){
            handleTab(input,&count);
        }
        else if((c == '\b' || c == 127)){
            if(count == 0)continue;
            input[--count] = '\0';
            printf("\b \b");
            fflush(stdout);
        }
        else{
            putchar(c);
            fflush(stdout);
            input[count++] = c;
        }
    }
    return count;
}

void debug_print(const char *input) {
    printf("input (len=%zu): [", strlen(input));
    for (size_t i = 0; input[i] != '\0'; i++) {
        unsigned char c = input[i];
        if (c == '\n') printf("\\n");
        else if (c == '\r') printf("\\r");
        else if (c == '\t') printf("\\t");
        else if (c == ' ')  printf("␣");   // optional: show spaces as ␣
        else if (c < 32 || c > 126) printf("\\x%02X", c); // non-printables
        else putchar(c);
    }
    printf("]\n");
}

int main(){

    tcgetattr(STDIN_FILENO, &dlt);
    new_ts = dlt;
    noncanonical(&new_ts);

    char *entry = "shell > ";
    while(true){
        printf("%s",entry);
        char input[2048];
        int count = handleTerminal(input);
        if(count == 0)continue;
        if(!addInputToHistory(input)){
            fprintf(stderr, "Error at %s:%d: failed to add input to history\n", __FILE__, __LINE__);
            continue;
        }
        int status = handle(input);
        if(status == -1){
            printf("Invalid Command\n");
            continue;
        }
    }
    restore(&dlt);
    return 0;
}