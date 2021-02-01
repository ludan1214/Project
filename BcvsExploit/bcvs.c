//This code is BUGGY and badly written - that is its purpose.

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <errno.h>

#define CHECKIN 0
#define CHECKOUT 1

#define CHILD -10

#define REPOSITORY ".bcvs"
#define LOGEXT ".comments"
#define BLOCK_LIST_PATH ".bcvs/block.list"
#define MAX_BLOCK_PRE 64
#define PATH_MAX 512

void help(char*);
int copyFile(char*);
void writeLog(FILE*);

int opcode;

char *block_prefix[MAX_BLOCK_PRE];
int max_prefix = 0;

void read_block_list() {
   FILE *block = fopen(BLOCK_LIST_PATH, "r");
   char temp_string[PATH_MAX] = {0};
   if (block == NULL) {
      printf("BCVS blocklist missing!\n");
      exit(-1);
   }
   while (max_prefix < MAX_BLOCK_PRE && !feof(block)) {
      if (fgets(temp_string, PATH_MAX, block) != NULL) {
	 max_prefix++;
	 block_prefix[max_prefix] = strdup(temp_string);
	 //kill newline
	 char *p;
	 if (p = strchr(block_prefix[max_prefix], '\n')) {
	    *p = '\0';
	 }
      }
   }
}

int is_blocked(char *filename) {
   char canonical_pathname[PATH_MAX];
   int i;
   char *path = realpath(filename, canonical_pathname);
   if (path == NULL) {
      return 0; /* filename does not exist, so not blocked!! */
   }
   for(i = 0; i <= max_prefix; i++) {
      if(block_prefix[i] && (strstr(path, block_prefix[i]) == path)) {
	    return 1;
      }
   }
   return 0; /* path did not start with any of the blocked prefixes */
} 

int main(int argc, char* argv[]) {
        int i;
        char log[256] = {0};
	FILE *logfile;

	//check arguments
	if (argc != 3) {
		help(argv[0]);
		return 1;
	}

	if (strncmp("co", argv[1], 255) == 0) {
		opcode = CHECKOUT;
	}
	else if (strncmp("ci", argv[1], 255) == 0) {
		opcode = CHECKIN;
	} else {
		help(argv[0]);
		return 1;
	}

	read_block_list();
	if (is_blocked(argv[2])) {
       	    printf("User asked to copy forbidden file.\n");
	    return 3;
	}
	
	//Now that we know the user can write to the file, let's take
	//a moment and have the user put in a comment to the logfile
	//construct log paths
	strcpy(log, REPOSITORY);
	strcat(log, "/");
	strcat(log, argv[2]);
	strcat(log, LOGEXT);

	logfile = fopen(log, "a");
	writeLog(logfile); //does error checking for us
	if(logfile != NULL){
	  fclose(logfile);
	}

	i = copyFile(argv[2]);
	
	if (i == CHILD) return CHILD;
	if (i < 0) {
	    //let's still write the logfile, even if we failed to copy
	    printf("Copy failed!\n");
	    return -9;
	 }



	return 0;
}

void help(char* name) {
	printf("Usage: %s [ci | co] filename\n", name);
}

int copyFile(char* arg) {
	int c;
        int i;
	pid_t pid;
	int status = 0;
	char user[16] = {0};
	char tempString[72] = {0};
	FILE *sourcefile, *destfile;
	char chmodString[] = "0700";
	char src[72] = {0};
	char dst[72] = {0};

	if (opcode == CHECKOUT) {

		strcpy(dst, arg); //copy destination file

		//construct repository path
		strcpy(src, REPOSITORY);
		strcat(src, "/");
		strcat(src, dst);
	}
	else if (opcode == CHECKIN) {

		strcpy(src, arg); //copy source file

		//construct repository path
		strcpy(dst, REPOSITORY);
		strcat(dst, "/");
		strcat(dst, src);
	}
	else {
		//huh?
		return -1;
	}

	sourcefile = fopen(src, "r");
	destfile = fopen(dst, "w");

	if (!sourcefile || !destfile) {
		// could not open one or both of the files
		printf("Error while opening files.\n");
		return -1;
	}

	i = 0;
	c = getc(sourcefile);
	while (c != EOF) {
		fputc(c, destfile);
		i++; 
		c = getc(sourcefile);
	}

	fclose(sourcefile);
	fclose(destfile);

	if (opcode == CHECKOUT) {
	    //chown
	    pid = fork();
	    if (pid < 0) return -1;
	    else if (pid == 0) {
	       //child
	       strcat(user, getenv("USER"));
	       if (user != 0) execlp("chown", "chown", user, dst, (char *)0);
	       printf("Warning: chown failed!\n");
	       perror("execlp");
	       return CHILD;
	    } else {
	       //parent
	       waitpid(pid, &status, NULL);
	    }
	   
	    //chmod
	    pid = fork();
	    if (pid < 0) return -1;
	    else if (pid == 0) {
	       //child
	      execlp("chmod", "chmod", chmodString, dst, (char *)0);
		//chmod(dst, S_IREAD | S_IWRITE);
	       printf("Warning: chmod failed!\n");
	       perror("execlp");
		return CHILD;
	    } else {
	       //parent
	       waitpid(pid, &status, NULL);
	    }
	}

	return i;
}

void writeLog(FILE* logfile) {
	int i;
	char c;
	char *tempString;

	if (!logfile) {
		// could not open one or both of the files
		printf("Error while opening log file.\n");
		return;
	}

	//no one needs more than 1024 chars of note space!
	tempString = calloc(1024, sizeof(char));

	printf("Please write a SHORT explanation:\n");
	c = i = 0;
	while (c != '\n' && c != EOF) {
		c = getchar();
		tempString[i] = c;
		fwrite(&c, sizeof(char), 1, logfile);
		i++;
	}

	printf(tempString);
	printf("written to log.\n");
	free(tempString);
}

