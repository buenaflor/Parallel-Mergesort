/**
 * @file main.c
 * @author Giiancarlo Buenaflor <e51837398@student.tuwien.ac.at>
 * @date 18.12.2020
 *
 * @brief Main program module.
 * 
 * This program takes a list of strings in stdin as input and sorts them via a parallel mergesort algorithm (forksort) using forks and pipes
 **/

#include <stdio.h>	
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>

/** Defines the step size size in which the lines array can grow if the max number of elements is reached. */
#define STEPSIZE 10;

/** The program name. */
static char *pgm_name;

/** Defines the max buffer size for the merge sort buffers. */
static ssize_t max_buffer_size = 0;

/**
 * Error exit function.
 * @brief This function writes helpful error information about the program to stderr and exits with an EXIT_FAILURE status
 * @param msg The message as char*.
 */
void error_exit(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

/**
 * Read Line function
 * @brief This function reads a line from a stream and exits if the line couldn't be read.
 * @param lineptr The pointer of a line (char*)
 * @param n The pointer of a size_t variable
 * @param stream The stream that is read from
 */
static void readline(char **lineptr, size_t *n, FILE *stream) {
    if (getline(lineptr, n, stream) == -1) {
        free(*lineptr);
        error_exit("Could not read line");
    }
}

/**
 * Print function
 * @brief This function prints a line and strips its newline if it exists before printing to ensure all strings are handled equally
 * @param line The line that is printed
 */
static void print(char *line) {
    size_t len = strlen(line);
    if (line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }
    printf("%s\n", line);
}

/**
 * Mandatory usage function.
 * @brief This function writes helpful usage information about the program to stderr.
 * @details global variables: pgm_name
 */
static void usage(void) {
	(void) fprintf(stderr, "USAGE: %s\n", pgm_name);
	exit(EXIT_FAILURE);
}

/**
 * Compare and lock element function
 * @brief This function compares two strings and locks elements accordingly and increases the processed count
 * @details This function sets new locks on smaller strings, depending on if the smaller string is already locked or not
 * @param left The line of left list
 * @param right The line of right list
 * @param lock The index of the locked state (0 = no lock, 1 = elem of left list is locked, 2 = elem of right list is locked)
 * @param processed_c1 Number of processed elements of left list
 * @param processed_c2 Number of processed elements of right list
 */
static void cmp_lock(char *left, char *right, int *lock, int *processed_c1, int *processed_c2) {
    switch (*lock) {
        case 0:
            if (strcmp(left, right) < 0) {
                print(left);
                *lock = 2;
                *processed_c1 += 1;
            } else {
                print(right);
                *lock = 1;
                *processed_c2 += 1;
            }
            break;
        case 1:
            if (strcmp(left, right) < 0) {
                print(left);
                *lock = 2;
                *processed_c1 += 1;
            } else {
                print(right);
                *processed_c2 += 1;
            }
            break;
        case 2:
            if (strcmp(left, right) < 0) {
                print(left);
                *processed_c1 += 1;
            } else {
                print(right);
                *lock = 1;
                *processed_c2 += 1;
            }
            break;
        default:
            error_exit("buffstate cannot be > 2");
    }
}

/**
 * Mergesort function
 * @brief This function opens the two file descriptors as streams and sorts the input using mergesort without using char* arrays
 * @details This function reads each line (visually from left list and right list) and compares them via strcmp(). The smaller line is printed and the bigger line is
 * saved in a buffer and the bufferstate is set accordingly. The list of the smaller value is kept being read while the buffer element is locked until either
 * the processed count is the same count as the smaller list or the element of the smaller list is in some iteration bigger than the buffer.
 * At the end, the locked element is printed. Finally, the remaining words are printed of the list, where the processing count is smaller than list count
 * This works, since every sublist is sorted in itself! Trivial case: lists with one element are already sorted.
 *
 *  Example: 

            First Part: iterate until p_c1 or p_c2 reaches the wr_count
            AN, HE 	         p_c1 p_c2       DO, HU, TH
           
            (AN < DO)	       1   0	     lock = 2
            (DO < HE)	       1   1	     lock = 1
            (HE < HU)	       2   1	     lock = 2

            Second Part: print remaining lock
            (HU)

            Third Part: print remaining elements
            (TH)
            
            >> AN, DO, HE, HU, TH
            
 *
 * @param fd1 File descriptor from rd pipe 1 read end
 * @param fd2 File descriptor from rd pipe 2 read end
 * @param wr_count1 Number of elements that were written into pipe 1
 * @param wr_count2 Number of elements that were written into pipe 2
 */
static void mergesort(int fd1, int fd2, int wr_count1, int wr_count2) {
    FILE *file1 = fdopen(fd1, "r");	
    FILE *file2 = fdopen(fd2, "r");	
    if (file1 == NULL || file2 == NULL) {
        error_exit("File is null");
    }

    char *left = NULL;
    char *right = NULL;
    size_t len1 = 0;
    size_t len2 = 0;
    int lock = 0;
    int processed_c1 = 0, processed_c2 = 0;

    // Trivial case: both lists only have one element
    if ((wr_count1 == 1 && wr_count2 == 1)) {
        readline(&left, &len1, file1);
        readline(&right, &len2, file2);
        if (strcmp(left, right) < 0) {
            print(left); 
            print(right);
        } else {
            print(right);
            print(left);
        }

        // free resources
        free(left);
        free(right);
        fclose(file1);
        fclose(file2);
        exit(EXIT_SUCCESS);
    }

    while (processed_c1 != wr_count1 && processed_c2 != wr_count2) {
        switch(lock) {
            case 0:
                // no list is locked, continue reading from both lists
                readline(&left, &len1, file1);
                readline(&right, &len2, file2);
                break;
            case 1:
                // left list is locked, continue reading from right list
                readline(&right, &len2, file2);
                break;
            case 2:
                // right list is locked, continue reading from left list
                readline(&left, &len1, file1);
                break;
            default:
                error_exit("lock cannot be > 2");
        }
        cmp_lock(left, right, &lock, &processed_c1, &processed_c2);
    }
    
    // lock must(!) contain 1 or 2
    switch (lock) {
        case 1:
            print(left);
            processed_c1 += 1;
            break;
        case 2:
            print(right);
            processed_c2 += 1;
            break;
        default:
            error_exit("lock cannot be 0 or > 2");
    }

    // Read and print the remaining elements
    if (processed_c1 != wr_count1) {
        for (int i = processed_c1; i < wr_count1; i++) {
            readline(&left, &len1, file1);
            print(left);
        }
    } else if (processed_c2 != wr_count2) {
        for (int i = processed_c2; i < wr_count2; i++) {
            readline(&right, &len2, file2);
            print(right);
        }
    }

    // free resources
    free(left);
    free(right);
    fclose(file1);
    fclose(file2);
}

int main(int argc, char *argv[]) {
    pgm_name = argv[0];

    if(argc != 1) {
        usage();
    }

    /* Read lines from stdin */

    char *line = NULL;
    int numlines = 0;
    int arrlen = STEPSIZE;
    char **lines = (char **) malloc(arrlen * sizeof(char *));
    
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, stdin)) != -1) {
        if (read > max_buffer_size) {
            max_buffer_size = read;
        } 
        lines[numlines] = line;
        if (numlines == arrlen) {
            arrlen += STEPSIZE;
            char** newlines = realloc(lines, arrlen * sizeof(char *));
            if (!newlines) {
                error_exit("Unable to reallocate memory for lines");
            }
            lines = newlines;
        }   

        numlines += 1;
        line = NULL;
    }

	if (numlines == 1) {
        fprintf(stdout, "%s", lines[0]);
        exit(EXIT_SUCCESS);
    }


    /* Create Pipes and then fork() */

	// wr... from where the parent is going to write to
	// rd... from where the parent is going to read from
	int wr_pipe_1[2];
    int rd_pipe_1[2];
    int wr_pipe_2[2];
    int rd_pipe_2[2];

	if (pipe(wr_pipe_1) == -1) {
        error_exit("wr_pipe_1 pipe creation error");
    }
	if (pipe(wr_pipe_2) == -1) {
        error_exit("wr_pipe_2 pipe creation error");
    }
	if (pipe(rd_pipe_1) == -1) {
        error_exit("rd_pipe_1 pipe creation error");
    }
	if (pipe(rd_pipe_2) == -1) {
        error_exit("rd_pipe_2 pipe creation error");
    }

	int wr_count1 = 0, wr_count2 = 0;
	pid_t pid1, pid2;
	pid1 = fork();
	switch (pid1) {
		case -1:
			error_exit("fork2 failed");
		case 0:
			// child process 1
			close(wr_pipe_1[1]);
			if (dup2(wr_pipe_1[0], STDIN_FILENO) == -1) {
				error_exit("dup2 on wr_pipe_1[0] in child process 1 failed");
			}
			close(wr_pipe_1[0]);

			close(rd_pipe_1[0]);
			if (dup2(rd_pipe_1[1], STDOUT_FILENO) == -1) {
				error_exit("dup2 on rd_pipe_1[1] in child process 1 failed");
			}
			close(rd_pipe_1[1]);
			
			close(rd_pipe_2[0]);
			close(rd_pipe_2[1]);
			close(wr_pipe_2[0]);
			close(wr_pipe_2[1]);

			execlp(pgm_name, pgm_name, NULL);
        	error_exit("should not be reached");
		default:
			close(rd_pipe_1[1]); 
			close(wr_pipe_1[0]);
			FILE *wr_file;
			if ((wr_file = fdopen (wr_pipe_1[1], "w")) == NULL) {
				error_exit("fdopen failed");
			}
			for(int i = 0; i < numlines / 2; i++) {
				if (fputs(lines[i], wr_file) == EOF) {
					error_exit("Error writing into file");
				}
				wr_count1 += 1;
				free(lines[i]);
    		}
			fclose(wr_file);
			close(wr_pipe_1[1]);
	}    

	pid2 = fork();
	switch (pid2) {
		case -1:
			error_exit("fork2 failed");
		case 0:
			// child process 2
			close(wr_pipe_2[1]);
			if (dup2(wr_pipe_2[0], STDIN_FILENO) == -1) {
				error_exit("dup2 on wr_pipe_2[0] in child process 2 failed");
			}
			close(wr_pipe_2[0]);

			close(rd_pipe_2[0]);
			if (dup2(rd_pipe_2[1], STDOUT_FILENO) == -1) {
				error_exit("dup2 on rd_pipe_2[1] in child process 2 failed");
			}
			close(rd_pipe_2[1]);

			close(rd_pipe_1[0]);
			close(rd_pipe_1[1]);
			close(wr_pipe_1[0]);
			close(wr_pipe_1[1]);

			execlp(pgm_name, pgm_name, NULL);
			error_exit("should not be reached");
		default:
			close(rd_pipe_2[1]); 
			close(wr_pipe_2[0]);
			FILE *wr_file;
			if ((wr_file = fdopen (wr_pipe_2[1], "w")) == NULL) {
				error_exit("fdopen failed");
			}
			for(int i = wr_count1; i < numlines; i++) {
				if (fputs(lines[i], wr_file) == EOF) {
					error_exit("Error writing into file");
				}
				wr_count2 += 1;
				free(lines[i]);
			} 
			fclose(wr_file);
			close(wr_pipe_2[1]);
	}

     // free resources
    free(lines);
    free(line);
    

    /* Wait for child processes and then merge parts */
	
	int status1, status2;
	if (waitpid(pid1, &status1, 0) == -1 || status1 != EXIT_SUCCESS) { 
		error_exit("Error occured during waiting for child: pid1");
	}
	if (waitpid(pid2, &status2, 0) == -1 || status2 != EXIT_SUCCESS) {
		error_exit("Error occured during waiting for child: pid2");
	}

    mergesort(rd_pipe_1[0], rd_pipe_2[0], wr_count1, wr_count2);

	exit(EXIT_SUCCESS);
}