// Luc Le (LEXX764)
// GopherChat Client
#include "gc.h"

const char *downloadfolder = "downloads/"; // Folder to store files received

pthread_t threads[MAX_THREADS+2];

struct pollfd server[1];

char* script_file;
char cli_name[9];
int admitted = false;
int script_flag = false;  // Run script if set to 1
int sockFD;
int th_cnt = 0;

void handler(int sig_num) {
	printf("Logging out and shutting down client...\n");
	// close(sockFD);
	// shutdown(sockFD, SHUT_RDWR);
	pthread_cancel(threads[0]);
	exit(0);
}


// Checks if a given char array is alphanumeric and between 4-8 characters
int isValidId(char* credential, int len){
	if (len < 4 || len > 8) {
		return 0;
	}
	for (int i = 0; i < len; i++) {
		if ( isdigit(credential[i]) == 0 && isalpha(credential[i]) == 0) {
			return 0;
		}
	}
	// Is alphanumeric
	return 1;
}

// Logs the client out of the server
void logout(){
	printf("Closing fd %d\n", sockFD);
	close(sockFD);
	printf("Shutting down socket\n");
	shutdown(sockFD, SHUT_RDWR);
	pthread_cancel(threads[0]);
	exit(0);
}

int saveFile(char * filebuf, char* filename, int size) {
	struct stat folder;
	char path[32];
	if (stat(downloadfolder, &folder) == -1) {
		mkdir(downloadfolder, 0700);
	}
	strcpy(path,downloadfolder);
	strcat(path,filename);
	FILE * file = fopen(path, "w");
	if (file == NULL) {
		fclose(file);
		return 0;
	}
	else {
		fwrite(filebuf, size, 1, file);
		fclose(file);
		return 1;
	}
}

void clnt_handle_recv_msg(char* msg){
	char *tok;
	char args[MAX_ARGUMENTS][16];
	char * recvd_msg = new char[MAX_REQUEST_SIZE];
	int size, maxToks, n; int argc = 0;
	memset(recvd_msg, 0, MAX_REQUEST_SIZE);
	// Tokenize the response
	tok = strtok(msg, " ");
	maxToks = tokenCount(tok);
	while(tok != NULL && argc < maxToks){
		strcpy(args[argc++],tok);
		tok = strtok(NULL, " ");
	}
	if (strcmp(args[0],"SEND") == 0) {
		// If it's a message, receive it
		size = atoi(args[2]);
		n = Recv_Blocking(sockFD, (BYTE*) recvd_msg, size);
		if (n < 0){
			printf("Client Disconnected\n");
			exit(0);
		}
		printf("%s: %s\n", args[1], recvd_msg);
	} else if (strcmp(args[0],"REGISTER") == 0) {
		if (strcmp(args[1],"SUCCESS") == 0) {
			printf("Registration for user: '%s' SUCCESSFUL, Please Login\n",args[2]);
		} else {
			printf("Registration Failed\n");
		}
	} else if (strcmp(args[0],"LOGIN") == 0) {
		if (strcmp(args[1],"SUCCESS") == 0) {
			memset(cli_name, 0, 9);
			strcpy(cli_name, args[2]);
			admitted = true;
			printf("Login SUCCESSFUL\nGreetings %s, Welcome to GopherChat!\n",cli_name);
		} else {
			printf("Login Failed\n");
		}
	} else if (strcmp(args[0],"LOGOUT") == 0) {
		printf("%s has left the chat.\n", args[1]);
	} else if (strcmp(args[0],"SEND2") == 0) {
		if (strcmp(args[1],"FAILED") == 0) {
			printf("THIS USER WAS NOT FOUND\n");
		} else {
			size = atoi(args[2]);
			n = Recv_Blocking(sockFD, (BYTE*) recvd_msg, size);
			if (n < 0){
				printf("Client Disconnected\n");
				exit(0);
			}
			printf("Private MSG from %s: %s\n", args[1], recvd_msg);
		}
	} else if (strcmp(args[0],"LIST") == 0) {
		size = atoi(args[1]);
		n = Recv_Blocking(sockFD, (BYTE*) recvd_msg, size);
		if (n < 0){
			printf("Client Disconnected\n");
			exit(0);
		}
		printf("________\n\n%s________\n",recvd_msg);
	} else if (strcmp(args[0],"SENDA") == 0) { // SENDA [MSG} [SIZE]
		size = atoi(args[1]);
		n = Recv_Blocking(sockFD, (BYTE*) recvd_msg, size);
		if (n < 0){
			printf("Client Disconnected\n");
			exit(0);
		}
		printf("Anonymous: %s\n",recvd_msg);
	} else if (strcmp(args[0],"SENDA2") == 0) {
		if (strcmp(args[1],"FAILED") == 0) {
			printf("THIS USER WAS NOT FOUND\n");
		} else {
			size = atoi(args[1]);
			n = Recv_Blocking(sockFD, (BYTE*) recvd_msg, size);
			if (n < 0){
				printf("Client Disconnected\n");
				exit(0);
			}
			printf("Private MSG from anonymous: %s\n", recvd_msg);
		}
	} else if (strcmp(args[0],"SENDF") == 0) {
		size = atoi(args[3]);
		n = Recv_Blocking(sockFD, (BYTE*) recvd_msg, size);
		if (n < 0){
			printf("Client Disconnected\n");
			exit(0);
		}
		saveFile(recvd_msg, args[2], size);
		printf("File Received from %s: %s\n", args[1], args[2]);

	} else if (strcmp(args[0],"SENDF2") == 0) {
			if (strcmp(args[1],"FAILED") == 0) {
				printf("THIS USER WAS NOT FOUND!\n");
			} else {
				size = atoi(args[4]);
				n = Recv_Blocking(sockFD, (BYTE*) recvd_msg, size);
				if (n < 0){
					printf("Client Disconnected\n");
					exit(0);
				}
				saveFile(recvd_msg, args[3], size);
				printf("Private File Received from %s: %s\n", args[2], args[3]);
			}
	} else if (strcmp(args[0], "JOING") == 0) {
			if (strcmp(args[1],"SUCCESS") == 0) {
				printf("Successfully joined group\n");
			} else {
				printf("Failed to join group (You're already in this group!)\n");
			}
	} else if (strcmp(args[0], "LEAVEG") == 0) {
			if (strcmp(args[1],"FAILED") == 0) {
				printf("Failed to leave group (You're not in this group!)\n");
			} else {
				printf("You have left the group\n");
			}
	} else if (strcmp(args[0], "SENDG") == 0) { // USE: SENDG [GNUM #0-9] [MSG]
			// Format response message "SENDG [GNum] [Src User] [SIZE]"
			if (strcmp(args[1],"FAILED") == 0) {
				printf("You're not a part of this group!\n");
			} else {
				size = atoi(args[3]);
				n = Recv_Blocking(sockFD, (BYTE*) recvd_msg, size);
				if (n < 0){
					printf("Client Disconnected\n");
					exit(0);
				}
				printf("Group #%s, Message from %s: %s\n", args[1], args[2], recvd_msg);
			}
	} else {
		printf("%s",args[0]);
	}
	free(recvd_msg);
}

void *listen_worker(void *arg){
	int r, n;
	char reply[INFOLEN];
	memset(server, 0, sizeof(server));
	server[0].fd = sockFD;
	server[0].events = POLLRDNORM;
	while(1) {
		memset(reply, 0, MAX_MSG_LEN); // Clear the reply buffer
		r = poll(server, 1, -1); // On readable event
		if (r < 0) {
			Error("Invalid poll() return value.");
		}
		if (server[0].revents & (POLLRDNORM | POLLERR | POLLHUP)) {
			// Receive the message info
			n = Recv_Blocking(sockFD, (BYTE*) reply, INFOLEN);
			if (n < 0){
				printf("Client Disconnected\n");
				exit(0);
			}
			//Process the message
			clnt_handle_recv_msg(reply);
		}
	}
	th_cnt--;
	pthread_cancel(threads[0]);
	return NULL;
}

// Checks if the Message syntax is correct and sends to server if so
void clnt_handle_input_msg(char* input){
	char *filebuf = new char[MAX_REQUEST_SIZE];
	bool isFile = false;
	char inputBuf[MAXLINE];
	char *tok;
	char args[MAX_ARGUMENTS][16];
	char response[INFOLEN];
	int maxToks, valid, offset, len; int argc = 0; int size = 0;
	//Clear out buffers
	memset(inputBuf, 0, MAXLINE);
	memset(response, 0, INFOLEN);
	memset(args, 0, INFOLEN);
	strcpy(inputBuf, input); // Backup the full string before it gets tokenized
	tok = strtok(input, " ");
	maxToks = tokenCount(tok);
	while(tok != NULL && argc < maxToks){
		if(tok[strlen(tok)-1] == '\n'){ //Remove newline from fgets
			tok[strlen(tok)-1] = '\0';
		}
		// printf("Arg %d: %s ", argc, tok);
		strcpy(args[argc++],tok);
		tok = strtok(NULL, " ");
	}
	// printf("\n");
	// Prepare Data to Send
	if (strcmp(args[0], "LOGIN") == 0 && admitted == true) {
		printf("ALREADY LOGGED IN!\n");
		return;
	} else if (strcmp(args[0], "REGISTER") == 0) {
		len = strlen(args[1]);
		valid = isValidId(args[1], len);
		// Check if username is valid
		if (args[1] == NULL || valid == 0) {
			printf("Invalid Username\n");
			return;
		} else { // Check if password is valid
			len = strlen(args[2]);
			valid = isValidId(args[2], len);
			if (args[2] == NULL || valid == 0){
				printf("Invalid Password\n");
				return;
			} else { // Everything is Valid, send request
				printf("Registering...\n");
				len = 0;
				snprintf(response, INFOLEN, "%s %s %s %d", args[0], args[1], args[2], len);
			}
		}
	} else if (strcmp(args[0], "LOGIN") == 0) {
		len = strlen(args[1]);
		valid = isValidId(args[1], len);
		// Check if username is valid
		if (args[1] == NULL || valid == 0) {
			printf("Username and Password must be alphanumeric and between 4-8 Characters\n");
			return;
		} else { // Check if password is valid
			len = strlen(args[2]);
			valid = isValidId(args[2], len);
			if (args[2]== NULL || valid == 0){
				printf("Username and Password must be alphanumeric and between 4-8 Characters\n");
				return;
			} else { // Everything is Valid, send request
				printf("Logging in...\n");
				snprintf(response, INFOLEN, "%s %s %s %d", args[0], args[1], args[2], 0);
			}
		}
	} else if ((strcmp(args[0], "SEND") == 0) && admitted == true) {
		// Format response message "SEND [USERNAME] MSG-SIZE";
		inputBuf[strlen(inputBuf)-1] = '\0'; // Remove \n from fgets()
		offset = strlen(args[0])+1;
		size = strlen(inputBuf+offset);
		if (size > MAX_MSG_LEN) {
			printf("Message is too long!\n");
			return;
		}
		snprintf(response, INFOLEN, "%s %s %d", args[0], cli_name, size);

	} else if ((strcmp(args[0], "SEND2") == 0) && admitted == true) {
		// Format response message "SEND2 [Dest User] [Src User] [SIZE]"
		inputBuf[strlen(inputBuf)-1] = '\0'; // Remove \n from fgets()
		offset = strlen(args[0])+strlen(args[1])+2;
		size = strlen(inputBuf+offset);
		if (size > MAX_MSG_LEN) {
			printf("Message is too long!\n");
			return;
		}
		snprintf(response, INFOLEN, "%s %s %s %d", args[0], args[1], cli_name, size);
	} else if ((strcmp(args[0], "SENDA") == 0) && admitted == true) { // SENDA [Dest User} [SIZE]
		inputBuf[strlen(inputBuf)-1] = '\0'; // Remove \n from fgets()
		offset = strlen(args[0])+1;
		size = strlen(inputBuf+offset);
		if (size > MAX_MSG_LEN) {
			printf("Message is too long!\n");
			return;
		}
		snprintf(response, INFOLEN, "%s %s %d", args[0], args[1], size);
	} else if ((strcmp(args[0],"SENDA2") == 0) && admitted == true) {
		// Format response message "SENDA2 [Dest User] [SIZE]";
		inputBuf[strlen(inputBuf)-1] = '\0'; // Remove \n from fgets()
		offset = strlen(args[0])+strlen(args[1])+2;
		size = strlen(inputBuf+offset);
		if (size > MAX_MSG_LEN) {
			printf("Message is too long!\n");
			return;
		}
		snprintf(response, INFOLEN, "SENDA2 %s %d", args[1], size);
	} else if ((strcmp(args[0], "SENDF") == 0) && admitted == true) {
		isFile = true;
		FILE* file = fopen(args[1], "r");
		if (file == NULL){
			printf("File Not Found\n");
			return;
		} else {
			struct stat st;
			stat(args[1], &st);
			size = st.st_size;
			if ( size > MAX_REQUEST_SIZE){
				printf("File is too large! Must be less than 10 MB\n");
				return;
			} else {
				fread(filebuf, size, 1, file);
				snprintf(response, INFOLEN, "%s %s %s %d", args[0], cli_name, args[1], size);
			}
		} // SENDF2 [Dest name] [Src name] [FileName] [FileSize]
	} else if ((strcmp(args[0], "SENDF2") == 0) && admitted == true) {
			isFile = true;
			FILE* file = fopen(args[2], "r");
			if (file == NULL){
				printf("File Not Found\n");
				return;
			} else {
				struct stat st;
				stat(args[2], &st);
				size = st.st_size;
				if ( size > MAX_REQUEST_SIZE){
					printf("File is too large! Must be less than 10 MB\n");
					return;
				} else {
					fread(filebuf, size, 1, file);
					snprintf(response, INFOLEN, "%s %s %s %s %d", args[0], args[1], cli_name, args[2], size);
				}
			} // JOING [GNumber]
	} else if ((strcmp(args[0], "JOING") == 0) && admitted == true) {
			int g = atoi(args[1]);
			if(g <= 0 || g >= MAX_GROUPS) {
				printf("Must be a group number between 0-%d\n", MAX_GROUPS);
				return;
			}
			snprintf(response, INFOLEN, "%s %s %d", args[0], args[1], 0);
	} else if ((strcmp(args[0], "LEAVEG") == 0) && admitted == true) { // LEAVEG [GNumber] [size]
			int g = atoi(args[1]);
			if(g <= 0 || g >= MAX_GROUPS) {
				printf("Must be a group number between 0-%d\n", MAX_GROUPS);
				return;
			}
			snprintf(response, INFOLEN, "%s %s %d", args[0], args[1], 0);
	} else if ((strcmp(args[0], "SENDG") == 0) && admitted == true) { // USE: SENDG [GNUM #0-9] [MSG]
			// Format response message "SENDG [GNum] [Src User] [SIZE]"
			int g = atoi(args[1]);
			if(g <= 0 || g >= MAX_GROUPS) {
				printf("Must be a group number between 0-%d\n", MAX_GROUPS);
				return;
			}
			inputBuf[strlen(inputBuf)-1] = '\0'; // Remove \n from fgets()
			offset = strlen(args[0])+strlen(args[1])+2;
			size = strlen(inputBuf+offset);
			if (size > MAX_MSG_LEN) {
				printf("Message is too long!\n");
				return;
			}
			snprintf(response, INFOLEN, "%s %s %s %d", args[0], args[1], cli_name, size);
	} else if ((strcmp(inputBuf, "LOGOUT\n") == 0) && admitted == true) {
			printf("Logging out and shutting down...\n");
			snprintf(response, INFOLEN, "LOGOUT %s %d", cli_name, 0);
			admitted = 0;
			Send_Blocking(sockFD, (BYTE*)response, INFOLEN);
			logout();
	} else if ((strcmp(inputBuf, "LIST\n") == 0) && admitted == true) {
			tok[strlen(tok)-1] = '\0'; // Remove newline
			snprintf(response, INFOLEN, "%s %d", tok, 0);
	} else if (strcmp(args[0], "DELAY") == 0) {
			if (args[1] == NULL) {
				Log("Usage: DELAY [N]");
			} else {
				int n = atoi(args[1]);
				printf("Waiting %d seconds...\n", n);
				sleep(n);
				printf("Resuming input...\n");
				return;
		}
	} else if (admitted == true) {
			inputBuf[strlen(inputBuf)-1] = '\0'; // Remove newline
			printf("\nINVALID COMMAND: '%s'\n", inputBuf);
			printf("args[0] is %s\n", args[0]);
			return;
	} else {
			printf("YOU MUST SIGN IN!\n");
			return;
	}
	// Send message contents
	Send_Blocking(sockFD, (BYTE*)response, INFOLEN);
	if (size > 0 && isFile == false) { // Send a msg
		Send_Blocking(sockFD, (BYTE*)inputBuf+offset, size);
	} else if (size > 0 && isFile == true) { // Send a file
		Send_Blocking(sockFD,(BYTE*)filebuf, size);
		free(filebuf);
	}
}

void *input_worker(void *arg) {
	char message[MAX_MSG_LEN];
	memset(message, 0, MAX_MSG_LEN);
	if (script_flag == true) { // Send the script to standard input if provided
		FILE* script = fopen(script_file, "r");
		if (script == NULL) {
			printf("Script file not found\n");
			exit(0);
		} else{
			while (fgets(message, MAX_MSG_LEN, script) != 0) {
				printf("%s\n",message);
				clnt_handle_input_msg(message);
				memset(message, 0, MAX_MSG_LEN);
			}
			script_flag = 0;
			fclose(script);
		}
	}
	// Enter a Input Loop from STDIN
	while(1) {// Input Loop
		// Enter a message to send
		fgets(message, MAX_MSG_LEN, stdin);
		//Handle the message
		clnt_handle_input_msg(message);
		memset(message, 0, MAX_MSG_LEN);
	}
	th_cnt--;
	pthread_cancel(threads[1]);
	return NULL;
}

void DoClient(const char * svrIP, int svrPort) {
	// Set up connection
	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons((unsigned short) svrPort);
	inet_pton(AF_INET, svrIP, &serverAddr.sin_addr);
	signal(SIGPIPE, SIG_IGN); //ignore the SIGPIPE signal that may crash the program in some corner cases
	//Create client socket
	sockFD = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFD == -1) {
		Error("Cannot create socket.");
	}
	// Connect to server
	if (connect(sockFD, (const struct sockaddr *) &serverAddr, sizeof(serverAddr)) != 0) {
		Error("Cannot connect to server %s:%d.", svrIP, svrPort);
	}
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("Connected to %s\n", svrIP);
	// Create a thread for getting user input.
	pthread_create(&threads[th_cnt++], NULL, input_worker, NULL);
	// Create a thread for listening to the server.
	pthread_create(&threads[th_cnt++], NULL, listen_worker, NULL);
	pthread_join(threads[0], NULL); // wait for threaeds to return
    pthread_join(threads[1], NULL);
}

int main(int argc, char * * argv) {
	const char * serverIP = argv[1];
	int port = atoi(argv[2]);
	if (argc != 3 && argc != 4) {
		Log("Usage: %s [server IP] [server Port] [Script File]", argv[0]);
		return -1;
	}
	// Setup signal handling
	signal(SIGINT, handler);
	signal(SIGTERM, handler);
	if (argc == 4) { // Run the script file if provided
		script_file = argv[3];
		script_flag = 1;
		FILE* script = fopen(script_file, "r");
		if (script == NULL) {
			printf("Script file not found\n");
			exit(0);
		}
	}
	DoClient(serverIP, port);
	return 0;
}
