/**********************************************************************************************************************************************************
 * Simple UNIX Shell
 * @author: NLPTrinh - HTVy - NTTTruc
 * @StudentID: 1751044 - 1751027 - 1751112
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>


#define MAX_LINE 80 /* 80 chars per line, per command */

char* history[10][MAX_LINE/2 + 1]; // save the history command
int history_wait[10];
int buffHead = 0; // save index of history command
void init_history(void);
void free_history(void);
void print_history(void);
char** history_computation(char** args,int *needWait);
int check_redirection(char** args, int *av);
int check_pipe(char** args, int *av);

int main(void)
{
	char *args[MAX_LINE/2 + 1];	/* command line (of 80) has max of 40 arguments */
    int should_run = 1;
	
	/*int i, upper;*/
		
    init_history();
    while (should_run){   
        printf("osh>");
        fflush(stdout);

        char cmd_line[MAX_LINE+1]; // A char array to store the input command line
        char *sptr = cmd_line; // create pointer sptr point to the command line string
        int av = 0; // The index of each word win command line

        // Read command line. 
        // If cmd line does not match the regex ([^\n]-"except \n" and *1[\n]) then return an int value < 1. 
        if (scanf("%[^\n]%*1[\n]", cmd_line)<1) { 
            if (scanf("%1[\n]", cmd_line)<1) {
                printf("STDIN FAILED\n");
                return 1;
            }
            continue;
        }
        /****** Parse cmd_line ******/
        while (*sptr==' ' || *sptr=='\t')
            // while pointer sptr meets space or tabs
            sptr++;
        while (*sptr!='\0') {
            char *tempBuff = (char*)malloc((MAX_LINE+1)*sizeof(char));
            // initalize args[av] as a dynamic array of char
            args[av] = (char*)malloc((MAX_LINE+1)*sizeof(char));
            // read a word and copy into pointer args[av] 
            int ret = sscanf(sptr,"%[^ \t]",args[av]);
            sptr += strlen(args[av]);
            if(ret < 1) {
                printf("INVALID COMMAND\n");
                return 1;
            }
            // read the " " and \t and copy to tempBuff
            ret = sscanf(sptr,"%[ \t]",tempBuff);
            if (ret > 0) 
                sptr += strlen(tempBuff);
            av++;
            // deallocate tempBuff
            free(tempBuff);
        }
        // check "&" exist in command. If yes parent will wait for the child process 
        // to exit before continuing. If no, they run concurrently
        int need_to_wait = 1;
        if (strlen(args[av-1]) == 1 && args[av-1][0] == '&') {
            need_to_wait = 0;
            free(args[av-1]);
            args[av-1] = NULL;
        } else {
            args[av] = NULL;
        }
        // check if command is exit, then set should_run to 0 and terminate the program
        if(strcmp(args[0],"exit")==0){
            should_run = 0;
            free_history();
            return 0;
        }
        /****** History Computation ******/
        if(args[1]==NULL && strcmp(args[0],"history")==0) {
            print_history();
            continue;
        }
        char **argsPtr = history_computation(args, &need_to_wait);


         /**
        After reading user input, the steps are:
        (1) fork a child process using fork()
        (2) child process will invoke execvp()
        (3) if command included &, parent will not invoke wait()
        */

        /* Redirection */
        int re = 0; // the position of < or > in the command
        int redirect = check_redirection(argsPtr, &re);  
        if (redirect == 1){
            printf("INVALID COMMAND\n");
            return 1;
        }
        else if (redirect != 0)
        {
            pid_t pid = fork(); // (1) create child process
            if (pid < 0) { // if fork returns negative value, unsuccessful
                printf("FORK FAILED\n");
                return 1;
            } 
            
            else if (pid == 0) { // (2) inside child process, fork return 0
                        // InputRedirection
                        if (redirect == 2) { 
                            // open file by using system call open()
                            // handle returned results
                            int fd = open(argsPtr[0], O_RDONLY, 0644);
                            if (fd < 0){ // error opening file
                                perror("open error");
                                exit(1);
                            }
                            // successfully opening file, then do the copy by invoking dup2() system call
                            dup2(fd, 0) ; // here 0 is fd(file descriptor) of stdin
                            // close the opened file by invoking close() system call
                            close(fd);
                            char *sub_args[MAX_LINE/2 + 1]; int i = 0;
                            while (i < re){
                                sub_args[i] = (char*)malloc((MAX_LINE+1)*sizeof(char));
                                strcpy(sub_args[i], args[i]);
                                i += 1;
                            }

                            if (execvp(argsPtr[0], argsPtr)) {
                                printf("INVALID COMMAND\n");
                                return 1;
                            }
                        }
                        // OutputRedirection
                        if (redirect == 3) { 
                            // open file by using system call open()
                            int fd = open(argsPtr[re + 1], O_WRONLY | O_CREAT, 0644); //sub_args[0] contain file name
                            // handle returned results
                            if (fd < 0) { // error opening file
                                perror("open error");
                                exit(1);
                            }
                            // successfully opening file, then do the copy by invoking dup2() system call
                            dup2(fd, 1) ; // here 1 is fd(file descriptor) of stdout
                            // close the opened file by invoking close() system call
                            close(fd);

                            // create the array to store left-hand side argument of redirecting
                            char *sub_args[MAX_LINE/2 + 1]; int i = 0;
                            while (i < re){
                                sub_args[i] = (char*)malloc((MAX_LINE+1)*sizeof(char));
                                strcpy(sub_args[i], args[i]);
                                i += 1;
                            }
                            // (2) execute command
                            if (execvp(sub_args[0], sub_args) < 0) { 
                                printf("INVALID COMMAND\n");
                                return 1;
                            }
                        }
            } 
            // (3) inside parent process, fork return a positive value - processID of child
            else { 
                if (need_to_wait) { 
                    while(wait(NULL) != pid);
                }
                else { 
                    printf("[1]%d\n",pid);
                }
            }

        }
        /* Pipe */
        int pi = 0; // the position of | in the command
        int hasPipe = check_pipe(argsPtr, &pi);
        if (hasPipe == 1){
            printf("em INVALID COMMAND\n");
            return 1;
        }
        else if (hasPipe == 2) {
            int pipefd[2]; //storing file descriptor of the two part of pipe(1 is used for writing, 0 is used for reading)	
			pid_t pid;//storing process id of two part of pipe
			
			//check whether pipe can be initialized or not
			if (pipe(pipefd) < 0){
				printf("\nPipe could not be initialized");		
			}
			pid = fork(); // fork a child process 
			if(pid < 0){
				printf("\nFailed to fork child process 1!");
			}
			else if (pid == 0){//inside child process
				close(pipefd[0]);// close read end, only need write end 
				dup2(pipefd[1], STDOUT_FILENO);
				close(pipefd[1]);// now close read end fd					
                // create sub-array to store pipe 1
                char *sub_args[MAX_LINE/2 + 1]; int i = 0;
                while (i < pi){
                    sub_args[i] = (char*)malloc((MAX_LINE+1)*sizeof(char));
                    strcpy(sub_args[i], args[i]);
                    i += 1;
                }
                 // (2) execute command
				if (execvp(sub_args[0], sub_args) < 0){ 
					printf("INVALID COMMAND\n");
                    return 1;
				}
			}
			else {
				wait(NULL); //wait for first process to be finished
				pid = fork(); // (1) fork another child process
				if (pid < 0){
					printf("\nFailed to fork child process 2!");			
				}
				else if (pid == 0){ 
					close(pipefd[1]); // close write end, only need read end
					dup2(pipefd[0], STDIN_FILENO);
					close(pipefd[0]);
                    
                    // create sub-array to store pipe 2
                    char *sub_args[MAX_LINE/2 + 1]; int i = pi;
                    while (args[i] != NULL){
                        sub_args[i] = (char*)malloc((MAX_LINE+1)*sizeof(char));
                        strcpy(sub_args[i], args[i]);
                        i += 1;
                    }
                    // (2) execute command
					if (execvp(sub_args[0], sub_args) < 0){
						printf("\nFailed to execute second command ..");
						exit(0);				
					}		
				}	
                // (3) inside parent process, fork return a positive value - processID of child
				else{
					if (need_to_wait) { 
                    while(wait(NULL) != pid);
                    }
                    else { 
                        printf("[1]%d\n",pid);
                    }
				}
			}
        }

        if (redirect == 0 && hasPipe == 0){
            pid_t pid = fork(); // (1) create child process
            if (pid < 0) { // if fork returns negative value, unsuccessful
                printf("FORK FAILED\n");
                return 1;
            } else if(pid == 0){
				if (execvp(argsPtr[0], argsPtr)) {
                    printf("INVALID COMMAND\n");
                    return 1;
                }
			} 
			// (3) inside parent process, fork return a positive value - processID of child
            else{
                if (need_to_wait) { 
                while(wait(NULL) != pid);
                }
                else { 
                    printf("[1]%d\n",pid);
                }
            }
        }
			
            
    }
	return 0;
}

// Check if command has redirecting Input or Output
int check_redirection(char** args, int *av){
    bool hasIn = false, hasOut = false;
    while(args[*av] != NULL){
        if (strcmp(args[*av], "<") == 0){
            hasIn = true;
            break;
        }
        if (strcmp(args[*av], ">") == 0){
            hasOut = true;
            break;
        }
        *av += 1;
    }
    if (!hasIn && !hasOut) 
        return 0;
    if (args[*av + 1] == NULL && (hasIn || hasOut) ) // If after '<' or '>' is NULL then return 1: invalid command
        return 1;
    else if (hasIn) 
        return 2;
    else if (hasOut) 
        return 3;
}

// Check if command has pipe "|"
int check_pipe(char** args, int *av){
    bool hasPipe = false;
    while(args[*av] != NULL){
        if (strcmp(args[*av], "|") == 0){
            hasPipe = true;
            break;
        }
        *av += 1;
    }
    if (args[*av + 1] == NULL && hasPipe)// If after '|' is NULL then return 1: invalid command
        return 1;
    if (hasPipe) 
        return 2; 
    return 0;
}

char** history_computation(char **args, int *needWait) {
    int i;

    // if user enter "!!" then get the history command
    if (args[1] == NULL && strcmp(args[0],"!!") == 0) {
        if (buffHead > 0){ // if recent command does exist
            strcpy(args[0], history[(buffHead - 1) % 10][0]);
            for(i = 1; history[(buffHead - 1) % 10][i] != NULL; i++) {
                args[i] = (char*)malloc((MAX_LINE+1)*sizeof(char));
                strcpy(args[i], history[(buffHead - 1) % 10][i]);
            }
            args[i] = NULL;
            *needWait = history_wait[(buffHead-1)%10];
            printf("Recent command: ");
            printf(history[0][0]);
            printf("\n");
        } else { // if no recent command in history
            printf("No commands in history\n");
            return args;
        }
    } 

    // delete the old command of history 
    for(i = 0; i < (MAX_LINE/2 + 1) && history[buffHead % 10][i] != NULL; i++)
        free(history[buffHead % 10][i]);

    // copy recent command into the history
    for(i = 0; args[i] != NULL; i++) {
        history[buffHead % 10][i] = args[i];
    }
    history[buffHead % 10][i] = args[i];

    history_wait[buffHead % 10] =* needWait;

    return history[(buffHead++) % 10];
}
void init_history(void) {
    int i, j;
    for(i = 0; i < 10; i++) {
        for(j = 0; j < (MAX_LINE/2 + 1); j++) {
            history[i][j] = NULL;
        }
        history_wait[i] = 0;
    }
}
void free_history(void) {
    int i, j;
    for (i = 0; i < 10 && i < buffHead; i++) {
        for (j = 0; history[i][j] != NULL; j++) {
            if (history[i][j])
                free(history[i][j]);
        }
    }
}
void print_history(void) {
    int i, j;
    for(i = 0; i < 10 && i < buffHead; i++) {
        int index;
        if (buffHead > 10)
            index = buffHead - 9 + i;
        else
            index = i + 1;
        printf("[%d] ",index);
        for(j = 0; history[(index-1)%10][j]!=NULL; j++) {
            printf("%s ",history[(index-1)%10][j]);
        }
        if (history_wait[(index-1)%10] == 0) {
            printf("&");
        }
        printf("\n");
    }
}

