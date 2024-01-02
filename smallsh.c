#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <stdint.h>
#include <fcntl.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

char *words[MAX_WORDS];
size_t wordsplit(char const *line);
char * expand(char const *word);



void sigint_handler(int signal) {
	//printf("in sigint_handler\n");
} /* sigint handler does nothing */

pid_t last_bg_pid = 0;
pid_t last_fg_pid = 0;
int last_fg_status = 0;
int wait_fg = 0;
char* input_filename = NULL;
char* output_filename = NULL;
int append_to_output = 0;
char* ps1;
int free_ps1 = 0;

void free_words() {
  if(input_filename != NULL) {
	free(input_filename);
	input_filename = NULL;
  }
  if(output_filename != NULL) {
	free(output_filename);
	output_filename = NULL;
  }

  // clean up words memory
  for(int i = 0; i < MAX_WORDS; i++) {
    if(words[i] != NULL) {
      free(words[i]);
      words[i] = NULL;
    }
  }
  free(*words);
  if(free_ps1) {
	free(ps1);
  }
}

void sigchld_handler(int signal) {
  /* wait for any process to complete and store its status */
  /* WNOHANG | WUNTRACED makes this not block */
  pid_t cur_pid = 0;
  int cur_status = 0;
  int prev_errno = errno;
  //printf("in sigchld_handler\n");
  while((cur_pid = waitpid(WAIT_MYPGRP, &cur_status, WNOHANG | WUNTRACED)) > 0) {
    //fprintf(stderr, "got here\n");
	fflush(stderr);
    if(WIFSTOPPED(cur_status)) {
      fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t)cur_pid);
      last_bg_pid = cur_pid;
      kill(cur_pid, SIGCONT);
    }
    else if(WIFEXITED(cur_status)) {
  	  if(cur_pid == last_fg_pid) {
		last_fg_status = WEXITSTATUS(cur_status);
		wait_fg = 0;
	  }
	  else {
		fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t)cur_pid, WEXITSTATUS(cur_status));
	  }
    }
    else if(WIFSIGNALED(cur_status)) {
	  int sig_num = WTERMSIG(cur_status);
      
	  if(cur_pid == last_fg_pid) {
		wait_fg = 0;
		//printf("signal: %d\n", sig_num);
		last_fg_status = sig_num + 128;
	  }
	  else {
		fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t)cur_pid, sig_num);
	  }
    }
  }
  //printf("cur_pid = %d errno = %d\n", cur_pid, errno);
  errno = prev_errno;
  fflush(stderr);
}

int main(int argc, char *argv[])
{
  FILE *input = stdin;
  char *input_fn = "(stdin)";
  if (argc == 2) {
    input_fn = argv[1];
    input = fopen(input_fn, "re");
    if (!input) err(1, "%s", input_fn);
  } else if (argc > 2) {
    errx(1, "too many arguments");
  }
  
  char *line = NULL;
  size_t n = 0;
  char* pEnd;
  char *pid_str =  NULL;
  char *fg_status_str = NULL;
  char *bg_pid_str = NULL;
  int run_bg = 0;

  int fd_out = STDOUT_FILENO;

  /* ignore SIGTSTP signal if received by smallsh */
  signal(SIGTSTP, SIG_IGN);

  /* register SIGINT to an empty signal handler and set SA_RESTART to restart process instead of quit*/
  
  struct sigaction sigint;
  sigint.sa_handler = sigint_handler;
  sigemptyset(&sigint.sa_mask);
  sigint.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sigint, NULL);
  
  struct sigaction sigchld;
  sigchld.sa_handler = sigchld_handler;
  sigemptyset(&sigchld.sa_mask);
  sigchld.sa_flags = SA_RESTART;
  sigaction(SIGCHLD, &sigchld, NULL);

  for (;;) {
    run_bg = 0;
	fd_out = STDOUT_FILENO;
//prompt:;
    /* TODO: prompt */
    ps1 = getenv("PS1");
    if(ps1 == NULL) {
	  // default to $
	  ps1 = malloc(2); // $ and null terminator
	  ps1[0] = '$';
	  ps1[1] = 0;
	  free_ps1 = 1;
    }

    if (input == stdin) {
    	fprintf(stderr, "%s", ps1);
		fflush(stderr);
    }
    ssize_t line_len = getline(&line, &n, input);
    if (line_len < 0) {
		//if(input == stdin) {
		//	err(1, "%s", input_fn);
		//}
		exit(last_fg_status);
	}
	
    
    size_t nwords = wordsplit(line);
    //fprintf(stderr, "nwords: %lu\n", nwords);
    for (size_t i = 0; i < nwords; ++i) {
      //fprintf(stderr, "Word %zu: %s\n", i, words[i]);
      char *exp_word = expand(words[i]);
      free(words[i]);
      words[i] = exp_word;
      //fprintf(stderr, "Expanded Word %zu: %s\n", i, words[i]);
      /* handle $$ expansion */
      //if(strcmp(words[i], "<PID>") == 0) {
      //  free(words[i]);
      //  pid_t pid_num = getpid();
      //  pid_str = malloc(10);
      //  memset(pid_str, 0, 10);
      //  sprintf(pid_str, "%d", pid_num);
      //  words[i] = pid_str;
      //}
      /* handle $? expansion */
      //else if(strcmp(words[i], "<STATUS>") == 0) {
      //  free(words[i]);
      //  fg_status_str = malloc(10);
      //  memset(fg_status_str, 0, 10);
      //  sprintf(fg_status_str, "%d", last_fg_status);
      //  words[i] = fg_status_str;
      //}
      /* handle $! expansion */
      //else if(strcmp(words[i], "<BGPID>") == 0) {
      //  free(words[i]);
      //  bg_pid_str = malloc(20);
      //  memset(bg_pid_str, 0, 20);
      //  sprintf(bg_pid_str, "%d", last_bg_pid);
      //  words[i] = bg_pid_str;
      //}
      /* handle background process */
	  if(strcmp(words[i], "&") == 0) {
      //else if(strcmp(words[i], "&") == 0) {
        if(i == (nwords-1)) {
          run_bg = 1;
          nwords--;
          free(words[i]);
          words[i] = 0;
        }
      }
	  else if(strcmp(words[i], "<") == 0) {
		/* redirect from file to stdin */
		int size = strlen(words[i+1]) + 1;
		input_filename = malloc(size);
		memset(input_filename, 0, size);
		strcpy(input_filename, words[i+1]);
		free(words[i]);
		free(words[i+1]);
		for(int next = i; next < nwords-2; next++) {
			words[next] = words[next+2];
		}
		/* decrement i so that it stays at the same position next time through loop */
		i--; 
		/* decrease nwords by 2 because we have taken two elements out of the array*/
		/* before doing that set last two elements to null */
		words[nwords-1] = NULL;
		words[nwords-2] = NULL;
		nwords -= 2;
	  }
	  else if(strcmp(words[i], ">") == 0) {
		//fprintf(stderr, "in > output_filename: %s\n", output_filename);
		int size = strlen(words[i+1]) + 1;
		output_filename = malloc(size);
		memset(output_filename, 0, size);
		strcpy(output_filename, words[i+1]);
		free(words[i]);
		free(words[i+1]);
		for(int next = i; next < nwords-2; next++) {
			words[next] = words[next+2];
		}
		/* decrement i so that it stays at the same position next time through loop */
		i--; 
		/* decrease nwords by 2 because we have taken two elements out of the array*/
		/* before doing that set last two elements to null */
		words[nwords-1] = NULL;
		words[nwords-2] = NULL;
		nwords -= 2;	


		// if not first > encountered, close the previous one
		if(fd_out != STDOUT_FILENO) {
			//fprintf(stderr, "> closing %d\n", fd_out);
			close(fd_out);
		}

		fd_out = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
		//fprintf(stderr, "> opened fd_out %d output_filename: %s\n", fd_out, output_filename);
		//fflush(stderr);

		if(fd_out < 0) {
			fprintf(stderr, "error writing file %s\n", output_filename);
			return EXIT_FAILURE;
		}
	  }
	  else if(strcmp(words[i], ">>") == 0) {
		//fprintf(stderr, "in >> output_filename: %s\n", output_filename);
		int size = strlen(words[i+1]) + 1;
		output_filename = malloc(size);
		memset(output_filename, 0, size);
		strcpy(output_filename, words[i+1]);
		free(words[i]);
		free(words[i+1]);
		for(int next = i; next < nwords-2; next++) {
			words[next] = words[next+2];
		}
		/* decrement i so that it stays at the same position next time through loop */
		i--; 
		/* decrease nwords by 2 because we have taken two elements out of the array*/
		/* before doing that set last two elements to null */
		words[nwords-1] = NULL;
		words[nwords-2] = NULL;
		nwords -= 2;	

		// if not first >> encountered, close the previous one
		if(fd_out != STDOUT_FILENO) {
			//fprintf(stderr, ">> closing %d\n", fd_out);
			close(fd_out);
		}

		fd_out = open(output_filename, O_WRONLY | O_CREAT | O_APPEND, 0777);
		//fprintf(stderr, ">> opened fd_out %d output_filename: %s\n", fd_out, output_filename);

		if(fd_out < 0) {
			fprintf(stderr, "error writing file %s\n", output_filename);
			return EXIT_FAILURE;
		}
	  }
      /* handle parameter expansion */
      //else if(strstr(words[i], "<Parameter:") != NULL) {
	//	//printf("got here1\n");
      //  char* token = strtok(words[i], " ");
      //  if(strcmp(token, "<Parameter:") == 0) {
      //    /* found parameter, get the parameter */
      //    token = strtok(NULL, " ");
      //    /* strip off > */
      //    token[strlen(token)-1] = 0;
      //    //printf("parameter: %s\n", token);
	//	  char* env_name = getenv(token);
	//	  if(env_name != 0) {
	//		  //printf("***%s***\n", env_name);
	//		  free(words[i]);
	//		  words[i] = malloc(strlen(env_name)+1);
	//		  strcpy(words[i], env_name);
	//	  }
	//	  else {
	//		//fprintf(stderr, "Error expanding environment variable: %s\n", token);
	//		//goto end_loop;
	//		free(words[i]);
	//		words[i] = malloc(1);
	//		strcpy(words[i], "");
	//	  }
      //  }
      //}
    }

    if(nwords == 0) {
      goto end_loop;
    }

    /* built in command exit */
    if(strcmp(words[0], "exit") == 0) {
      if(nwords == 1) {
        free_words();
        free(line);
        return last_fg_status;
      }
      else if(nwords == 2) {
        long int ret_val = strtol(words[1], &pEnd, 10);
        if(errno != 0 || *pEnd != 0) {
          fprintf(stderr, "Invalid exit value\n");
          errno = 0;
        }
        else {
          free_words();
          free(line);
          return ret_val;
        }
      }
      else {
        fprintf(stderr, "Exit only takes one arguments\n");
      }
    }
    /* build in command cd */
    else if(strcmp(words[0], "cd") == 0) {
      if(nwords == 1) {
        char* home = getenv("HOME");
        if(chdir(home) != 0) {
          warn("cd: ");
        }
        errno = 0;
      }
      else if(nwords == 2) {
        if(chdir(words[1]) != 0) {
          warn("cd: ");   
        }
        errno = 0;
      }
      else {
        fprintf(stderr, "cd takes at most 1 argument\n");
      }
    }
    /* non build in commands */
    else {
      //printf("executing non built in command\n");
      int pid = fork();
      /* child */
      if(pid == 0) {
        /* restore SIGTSTP and SIGINT to default behavior in child */
        signal(SIGTSTP, SIG_DFL);
        signal(SIGINT, SIG_DFL);

		/* if input file provided, read from input instead of stdin */
		//int fd_in = STDIN_FILENO;
		if(input_filename != NULL) {
			/* changes input from stdin to the specified input file */
			int fd_in = open(input_filename, O_RDONLY);
			if(fd_in < 0) {
				fprintf(stderr, "error reading file %s\n", input_filename);
				return EXIT_FAILURE;
			}
			/* redirect stdin to fd_in */
			if(dup2(fd_in, STDIN_FILENO) < 0) {
				fprintf(stderr, "dup2 error on <\n");
				return EXIT_FAILURE;
			}
			/* set input fd to automatically be closed after the exec command */
			if(fcntl(fd_in, F_SETFD, FD_CLOEXEC) < 0) {
				fprintf(stderr, "fcntl error on <\n");
				return EXIT_FAILURE;
			}
		}

		if(fd_out != STDOUT_FILENO) {
			/* redirect stdout to fd_out */
			if(dup2(fd_out, STDOUT_FILENO) < 0) {
				fprintf(stderr, "dup2 error on >\n");
				return EXIT_FAILURE;
			}	

			if(fcntl(fd_out, F_SETFD, FD_CLOEXEC) < 0) {
				fprintf(stderr, "fcntl error on > or >>\n");
				return EXIT_FAILURE;
			}
		}

		//fprintf(stderr, "DEBUG nwords = %lu\n", nwords);
		//for(int i = 0; i < nwords; i++) {
		//	fprintf(stderr, "words[%d] = %s\n", i, words[i]);
		//}
		//fprintf(stderr, "fd_in: %d\n", fd_in);
		//fprintf(stderr, "fd_out: %d\n", fd_out);
		//fflush(stderr);

        execvp(words[0], words);

        fprintf(stderr, "smallsh: execvp: command not found\n");
        return EXIT_FAILURE;
      }
      /* parent */
      else if(pid > 0) {
        if(run_bg == 0) {
          last_fg_pid = pid;
		  wait_fg = 1; /* set to zero in SIGCHLD handler */
          while(wait_fg) ;
        }
        else {
          last_bg_pid = pid;
        }
      }
      /* fork() error */
      else {
        warn("fork: ");
        errno = 0;
      }
    }
end_loop:
    // clean up words memory
    free_words();
    free(line);
    line = NULL;
  }
 
}

/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 */
size_t wordsplit(char const *line) {
  size_t wlen = 0;
  size_t wind = 0;

  char const *c = line;
  for (;*c && isspace(*c); ++c); /* discard leading space */

  for (; *c;) {
    if (wind == MAX_WORDS) break;
    /* read a word */
    if (*c == '#') break;
    for (;*c && !isspace(*c); ++c) {
      if (*c == '\\') ++c;
      void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
      if (!tmp) err(1, "realloc");
      words[wind] = tmp;
      words[wind][wlen++] = *c; 
      words[wind][wlen] = '\0';
    }
    ++wind;
    wlen = 0;
    for (;*c && isspace(*c); ++c);
  }
  return wind;
}


/* Find next instance of a parameter within a word. Sets
 * start and end pointers to the start and end of the parameter
 * token.
 */
char
param_scan(char const *word, char **start, char **end)
{
  static char *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = NULL;
  *end = NULL;
  char *s = strchr(word, '$');
  if (s) {
    char *c = strchr("$!?", s[1]);
    if (c) {
      ret = *c;
      *start = s;
      *end = s + 2;
    }
    else if (s[1] == '{') {
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = '{';
        *start = s;
        *end = e + 1;
      }
    }
  }
  prev = *end;
  return ret;
}

/* Simple string-builder function. Builds up a base
 * string by appending supplied strings/character ranges
 * to it.
 */
char *
build_str(char const *start, char const *end)
{
  static size_t base_len = 0;
  static char *base = 0;

  if (!start) {
    /* Reset; new base string, return old one */
    char *ret = base;
    base = NULL;
    base_len = 0;
    return ret;
  }
  /* Append [start, end) to base string 
   * If end is NULL, append whole start string to base string.
   * Returns a newly allocated string that the caller must free.
   */
  size_t n = end ? (size_t)(end - start) : strlen(start);
  size_t newsize = sizeof *base *(base_len + n + 1);
  void *tmp = realloc(base, newsize);
  if (!tmp) err(1, "realloc");
  base = tmp;
  memcpy(base + base_len, start, n);
  base_len += n;
  base[base_len] = '\0';

  return base;
}

/* Expands all instances of $! $$ $? and ${param} in a string 
 * Returns a newly allocated string that the caller must free
 */
char *
expand(char const *word)
{
  char const *pos = word;
  char *start, *end;
  char c = param_scan(pos, &start, &end);
  build_str(NULL, NULL);
  build_str(pos, start);
  while (c) {
    if (c == '!') {
		if(last_bg_pid != 0) {
			char* bg_pid_str = malloc(20);
			memset(bg_pid_str, 0, 20);
			sprintf(bg_pid_str, "%d", last_bg_pid);
			build_str(bg_pid_str, NULL);
		}
	}
    else if (c == '$') {
		pid_t pid_num = getpid();
        char* pid_str = malloc(10);
        memset(pid_str, 0, 10);
        sprintf(pid_str, "%d", pid_num);
		build_str(pid_str, NULL);
	}
    else if (c == '?') {
		char* fg_status_str = malloc(10);
        memset(fg_status_str, 0, 10);
        sprintf(fg_status_str, "%d", last_fg_status);
		build_str(fg_status_str, NULL);
	}
    else if (c == '{') {
      
      //build_str(start + 2, end - 1);
	  //fprintf(stderr, "length: %ld\n", ((end-1)-(start+2)+1));
	  int size = (end-1)-(start+2)+1;
	  char* param_str = malloc(size);
	  memset(param_str, 0, size);
	  strncpy(param_str, start+2, size-1);
	  //fprintf(stderr, "param_str: %s\n", param_str);
	  char* env_name = getenv(param_str);
	  if(env_name != 0) {
		//fprintf(stderr, "env_name: %s\n", env_name);
		build_str(env_name, NULL);
	  }
	  //else {
		//fprintf(stderr, "variable not found\n");
		//build_str("", NULL);
      //}
    }
    pos = end;
    c = param_scan(pos, &start, &end);
    build_str(pos, start);
  }
  return build_str(start, NULL);
}

/*
        char* token = strtok(words[i], " ");
        if(strcmp(token, "<Parameter:") == 0) {
          // found parameter, get the parameter
          token = strtok(NULL, " ");
          // strip off > 
          token[strlen(token)-1] = 0;
          //printf("parameter: %s\n", token);
		  char* env_name = getenv(token);
		  if(env_name != 0) {
			  //printf("***%s***\n", env_name);
			  free(words[i]);
			  words[i] = malloc(strlen(env_name)+1);
			  strcpy(words[i], env_name);
		  }
		  else {
			//fprintf(stderr, "Error expanding environment variable: %s\n", token);
			//goto end_loop;
			free(words[i]);
			words[i] = malloc(1);
			strcpy(words[i], "");
		  }
        }*/