#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p){
    int fd;

    // Handle input redirection ("<")
    if (p -> in_file != NULL) {
        fd = open(p -> in_file, O_RDONLY);  // Open the input file
        if (fd == -1) {
            perror("open for input redirection failed");
	    exit(EXIT_FAILURE);
        }
        // Redirect stdin to the input file
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("dup2 for input redirection failed");
	    exit(EXIT_FAILURE);
        }
        close(fd);  // Close the file descriptor
    }

    // Handle output redirection (">")
    if (p -> out_file != NULL) {
        fd = open(p -> out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);  // Open/create output file
        if (fd == -1) {
            perror("open for output redirection failed");
	    exit(EXIT_FAILURE);
        }
        // Redirect stdout to the output file
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2 for output redirection failed");
	    exit(EXIT_FAILURE);
        }
        close(fd);  // Close the file descriptor
    }
    
    
    // Handle input/output redirection with pipes
    if (p -> in != STDIN_FILENO) {
        dup2(p -> in, STDIN_FILENO);
        close(p -> in);
    }
    if (p -> out != STDOUT_FILENO) {
        dup2(p -> out, STDOUT_FILENO);
        close(p -> out);
    }

}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p)
{
    pid_t pid;
    int status;

    pid = fork();
    
    if (pid == 0) {
        //This is child process
	//execute the external cmd
        redirection(p);

	if (execvp(p -> args[0], p -> args) == -1) {
            perror("execvp failed");
            exit(EXIT_FAILURE);  // If execvp fails, exit the child process
       	}

    } else if (pid < 0) {
        
	// Fork failed
        perror("fork failed");
     	return -1;

    } else {

        // This is parent process
        // Wait until the child complete
	
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            return -1;
        }

        // Check if the child process terminated normally
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);  // Return the exit status of the child
        } else {
            return -1;  // if the child did not terminate normally
        }
    }
    
    return 1;    
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd)
{
    int cmd_num = 0;
    struct cmd_node *current = cmd -> head;

    // Count the number of commands
    while (current) {
        cmd_num++;
        current = current->next;
    }

    // Create pipes for all but the last command
    int pipe_fds[2 * (cmd_num - 1)];  
    for (int i = 0; i < cmd_num - 1; i++) {
        if (pipe(pipe_fds + i * 2) < 0) {
            perror("pipe failed");
            return -1;
        }
    }

    current = cmd -> head;
    for (int i = 0; i < cmd_num; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Child process
            // If not the first command, get input from the previous pipe
            if (i > 0) {
                dup2(pipe_fds[(i - 1) * 2], STDIN_FILENO);
            }
            // If not the last command, output to the next pipe
            if (i < cmd_num - 1) {
                dup2(pipe_fds[i * 2 + 1], STDOUT_FILENO);
            }
            // Close all pipe file descriptors in the child process
            for (int j = 0; j < 2 * (cmd_num - 1); j++) {
                close(pipe_fds[j]);
            }
            redirection(current);  // Apply any redirection for the command
            execvp(current -> args[0], current -> args);
            perror("execvp failed");
            exit(EXIT_FAILURE);
        } else if (pid < 0) {  // Fork failed
            perror("fork failed");
            return -1;
        }
        current = current -> next;
    }

    // Close all pipe file descriptors in the parent process
    for (int i = 0; i < 2 * (cmd_num - 1); i++) {
        close(pipe_fds[i]);
    }

    // Wait for all child processes to finish
    for (int i = 0; i < cmd_num; i++) {
        int status;
        wait(&status);
    }

    return 0;  // All commands executed successfully 
}

// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				// recover shell stdin and stdout
				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		//if (status == 0)
		//a little change here only when status == -1 or user input "exit" will terminate the code
	        if (status == -1 && strcmp(temp->args[0], "exit") == 0)break;
			
	}
}
