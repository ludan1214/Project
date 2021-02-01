// Luc Le (LEXXX764)
// GopherChat Server
#include "gc.h"

int serverfd;
int KEEP_GOING = 1;             // control variable for loop
const char *userfolder = "userinfo/"; // Folder where logininfo will be stored
const char *tempfolder = "servertemp/"; // Folder to temporarily store files for sending

void handler( int signum ) {
	printf("Server Shutting Down\n");
	KEEP_GOING = 0;
	shutdown(serverfd, SHUT_RDWR);
	close(serverfd);
	// cleanup and close up stuff here
	exit(signum);
}

int nConns;	//total # of data sockets/clients
struct pollfd peers[MAX_CONCURRENCY_LIMIT+1];	//sockets to be monitored by poll()
struct CONN_STAT connStat[MAX_CONCURRENCY_LIMIT+1];	//app-layer stats of the sockets
int totalSent[MAX_CONCURRENCY_LIMIT+1][8];
int totalRecv[MAX_CONCURRENCY_LIMIT+1][8];

void reset_conn_stat(struct CONN_STAT * pStat){
	memset(pStat, 0, sizeof(CONN_STAT));
}

void SetNonBlockIO(int fd) {
	int val = fcntl(fd, F_GETFL, 0);
	if (fcntl(fd, F_SETFL, val | O_NONBLOCK) != 0) {
		Error("Cannot set nonblocking I/O.");
	}
}

// Removes a client from the server
void RemoveConnection(int i) {
	close(peers[i].fd);
	if (i < nConns) {
		memmove(peers + i, peers + i + 1, (nConns-i) * sizeof(struct pollfd));
		memmove(connStat + i, connStat + i + 1, (nConns-i) * sizeof(struct CONN_STAT));
	}
	nConns--;
}


//Group Messaging (return second element as int)
int getGroup(char* info){
	printf("info: %s\n", info);
	char infoBuf[INFOLEN];
	memset(infoBuf, 0, INFOLEN);
	strcpy(infoBuf, info);
	char *tok = strtok(infoBuf, " ");
	tok = strtok(NULL, " ");
	return(atoi(tok));
}

////////////////////////////
// LOGIN LOGOUT FUNCTIONS //
////////////////////////////
// Admit client into the server
void admit_client(char* user, int i) {
	// Set Client Name
	connStat[i].name  = (char *)malloc(9);
	memset(connStat[i].name, 0, 9);
	strncpy(connStat[i].name, user, 9);
	connStat[i].logged_in = 1;
}

// Checks if username/password pair exists
int verify_cred(char* username, char* password) {
	char path[32];
	strcpy(path,userfolder);
	strcat(path,username);
	// Open for reading
	char stored_pwd[9];
	memset(stored_pwd, 0, 9);
	FILE* userfile = fopen(path, "r");
	if (userfile == NULL) {
		return 0;
	}
	else { // User Exists, Check Password
		fgets(stored_pwd, sizeof(stored_pwd), userfile);
		int res = strcmp(stored_pwd, password);
		if (res == 0) { // Correct password
			fclose(userfile);
			return 1;
		}
		else {
			fclose(userfile);
			return 0;
		}
	}
}

int create_user(char* username, char* password) {
	struct stat file;

	char path[32];
	if (stat(userfolder, &file) == -1) {
		mkdir(userfolder, 0700);
	}
	strcpy(path,userfolder);
	strcat(path,username);
	int fd = open(path, O_CREAT | O_WRONLY | O_EXCL, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		close(fd);
		return 0;
	}
	else {
		write(fd, password, strlen(password));
		close(fd);
		printf("User: '%s' created\n", username);
		return 1;
	}
}

// Checks all logged in fd's for associated username.
int get_user_fd(char* username) {
	for (int i = 0; i < nConns+1; i++) {
		if(connStat[i].logged_in == 1 && strcmp(username, connStat[i].name) == 0) {
			return peers[i].fd;
		}
	}
	return -1;
}

//Prepare response
void update_response_msg(int i){
	char infoBuf[INFOLEN];
	memset(infoBuf, 0, INFOLEN);
	strcpy(infoBuf, connStat[i].info);
	char *tok;
	char args[MAX_ARGUMENTS][16];
	int argc = 0;
	memset(connStat[i].response, 0, INFOLEN);
	tok = strtok(infoBuf, " ");
	while(tok != NULL){
		strcpy(args[argc++],tok);
		tok = strtok(NULL, " ");
	}
	if (strcmp(args[0],"SEND") == 0) {
		connStat[i].needData = true;
		snprintf(connStat[i].response, INFOLEN, "%s %s %d", args[0], connStat[i].name, connStat[i].datasize);
	} else if (strcmp(args[0], "REGISTER") == 0) {
		connStat[i].datasize = 0;
		connStat[i].destFd = peers[i].fd;
		if (create_user(args[1], args[2]) == 1) {
			snprintf(connStat[i].response, INFOLEN, "REGISTER SUCCESS %s", args[1]);
		}
		else { // Already Exists
			snprintf(connStat[i].response, INFOLEN, "REGISTER FAILED");
		}
	} else if (strcmp(args[0], "LOGIN") == 0) {
		connStat[i].destFd = peers[i].fd;
		int userFd;
		userFd = (get_user_fd(args[1]));
		if (userFd == -1 && (verify_cred(args[1],args[2]) == 1)) {
			admit_client(args[1], i);
			snprintf(connStat[i].response, INFOLEN, "LOGIN SUCCESS %s", args[1]);
		} else { // Failed to verify
			snprintf(connStat[i].response, INFOLEN, "LOGIN FAILED %s", args[1]);
			printf("Sending Login Rejection: %s\n", connStat[i].response);
		}
	} else if (strcmp(args[0],"SEND2") == 0) { // Private MSG
		connStat[i].needData = true;
		int destFd = get_user_fd(args[1]);
		if (destFd == -1) { // User not found
			connStat[i].failedSend = true;
			connStat[i].destFd = peers[i].fd;
			snprintf(connStat[i].response, INFOLEN, "%s FAILED 0", args[0]);
		} else { // User found
			connStat[i].destFd = destFd;
			// Format response message "SEND2 [Src User] MSG-SIZE";
			snprintf(connStat[i].response, INFOLEN, "%s %s %d", args[0], connStat[i].name, connStat[i].datasize);
		}
	} else if (strcmp(args[0],"SENDA") == 0) { // Anonymous public MSG
		connStat[i].needData = true;
		snprintf(connStat[i].response, INFOLEN, "%s %d", args[0], connStat[i].datasize);
	} else if (strcmp(args[0],"SENDA2") == 0) { // Anonymous Private MSG
		connStat[i].needData = true;
		int destFd = get_user_fd(args[1]);
		if (destFd == -1) { // User not found
			connStat[i].destFd = peers[i].fd;
			connStat[i].failedSend = true;
			snprintf(connStat[i].response, INFOLEN, "%s FAILED 0", args[0]);
		} else { // User found
			connStat[i].destFd = destFd;
			// Format response message "SENDA2 [Src User] MSG-SIZE";
			snprintf(connStat[i].response, INFOLEN, "%s %d", args[0], connStat[i].datasize);
		}
	} else if (strcmp(args[0],"SENDF") == 0) {
		connStat[i].needData = true;
		snprintf(connStat[i].response, INFOLEN, "%s %s %s %d", args[0], args[1], args[2], connStat[i].datasize);
	} else if (strcmp(args[0],"SENDF2") == 0) {
		connStat[i].needData = true;
		int destFd = get_user_fd(args[1]);
		if (destFd == -1) { // User not found
			connStat[i].destFd = peers[i].fd;
			connStat[i].failedSend = true;
			snprintf(connStat[i].response, INFOLEN, "%s FAILED 0", args[0]);
		} else { // User found
			connStat[i].destFd = destFd;
			// Format response message "SENDA2 [Src User] MSG-SIZE";
			snprintf(connStat[i].response, INFOLEN, "%s %s %s %s %d", args[0], args[1], args[2], args[3], connStat[i].datasize);
		}
	} else if (strcmp(args[0], "JOING") == 0) {
			int g = atoi(args[1]);
			if (connStat[i].groups[g] == true) {
				snprintf(connStat[i].response, INFOLEN, "%s FAILED 0", args[0]);
			} else {
				connStat[i].groups[g] = true;
				snprintf(connStat[i].response, INFOLEN, "%s SUCCESS 0", args[0]);
			}
	} else if (strcmp(args[0], "SENDG") == 0) { // USE: SENDG [GNUM #0-9] [MSG]
			// Format response message "SENDG [GNum] [Src User] [SIZE]"
			connStat[i].needData = true;
			int g = atoi(args[1]);
			if (connStat[i].groups[g] == false) { // Not in Group
				connStat[i].destFd = peers[i].fd;
				connStat[i].failedSend = true;
				snprintf(connStat[i].response, INFOLEN, "%s FAILED 0", args[0]);
			} else {
				snprintf(connStat[i].response, INFOLEN, "%s %s %s %d", args[0], args[1], connStat[i].name, connStat[i].datasize);
			}
	} else if (strcmp(args[0], "LEAVEG") == 0) { // LEAVEG [GNumber] [size]
			int g = atoi(args[1]);
			if (connStat[i].groups[g] == false) {
				snprintf(connStat[i].response, INFOLEN, "%s FAILED 0", args[0]);
			} else {
				connStat[i].groups[g] = false;
				snprintf(connStat[i].response, INFOLEN, "%s SUCCESS 0", args[0]);
			}
	}else if (strcmp(args[0],"LOGOUT") == 0) {
		connStat[i].datasize = 0;
		snprintf(connStat[i].response, INFOLEN, "LOGOUT %s %d", connStat[i].name, 0);
	} else if (strcmp(args[0],"LIST") == 0) {
		for (int cli = 0; cli < nConns+1; cli++) {
			char namebuf[9];
			if (connStat[cli].logged_in == 1) {
				memset(namebuf, 0, 9);
				snprintf(namebuf, 9, "%s\n", connStat[cli].name);
				strcat(connStat[i].data, namebuf);
			}
		}
		int size = strlen(connStat[i].data);
		snprintf(connStat[i].response, INFOLEN, "LIST %d", size);
		connStat[i].datasize = size;
		connStat[i].dataRecvd = true;
		connStat[i].destFd = peers[i].fd;
	} else {
		printf("Unrecognized command, '%s'\n", args[0]);
	}
}

void DoServer(int svrPort, int maxConcurrency) {
	//Set up connection
	int listenFD = socket(AF_INET, SOCK_STREAM, 0);
	if (listenFD < 0) {
		Error("Cannot create listening socket.");
	}
	serverfd = listenFD;
	SetNonBlockIO(listenFD);
	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(struct sockaddr_in));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons((unsigned short) svrPort);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	int n;
	int optval = 1;
	int r = setsockopt(listenFD, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (r != 0) {
		Error("Cannot enable SO_REUSEADDR option.");
	}
	signal(SIGPIPE, SIG_IGN);

	if (bind(listenFD, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0) {
		Error("Cannot bind to port %d.", svrPort);
	}

	if (listen(listenFD, 16) != 0) {
		Error("Cannot listen to port %d.", svrPort);
	}

	nConns = 0;
	memset(peers, 0, sizeof(peers));
	peers[0].fd = listenFD;
	peers[0].events = POLLRDNORM;
	memset(connStat, 0, sizeof(connStat));
	printf("GopherChat Server Initialized, Listening on port %d...\n", svrPort);
	while (KEEP_GOING == 1) {
		// Listen for socket activity
		r = poll(peers, nConns + 1, -1); // On readable event
		if (r < 0) {
			Error("Invalid poll() return value.");
		}

		//Accept a new connection
		struct sockaddr_in clientAddr;
		socklen_t clientAddrLen = sizeof(clientAddr);
		if ((peers[0].revents & POLLRDNORM) && (nConns < maxConcurrency)) {
			int fd = accept(listenFD, (struct sockaddr *)&clientAddr, &clientAddrLen);
			if (fd != -1) {
				printf("fd %d connected.\n", fd);
				SetNonBlockIO(fd);
				nConns++;
				peers[nConns].fd = fd;
				peers[nConns].events = POLLRDNORM;
				peers[nConns].revents = 0;
				memset(&connStat[nConns], 0, sizeof(struct CONN_STAT));
				for (int i = 0; i < MAX_GROUPS; i++){
					memset(&connStat[nConns].groups[i], false, sizeof(bool));
				}
				connStat[nConns].failedSend = false;
				connStat[nConns].needData = false;
				connStat[nConns].dataRecvd = false;
			}
		}

		// Check for events

		for (int i=1; i<=nConns; i++) {
			if (peers[i].revents & (POLLRDNORM | POLLERR | POLLHUP)) { //Socket becomes readable/has data to read

				int fd = peers[i].fd;

				// Receive Response
				if (connStat[i].needData == true) {
					printf("Trying to receive %d bytes...\n", connStat[i].datasize);
					//
					if (Recv_NonBlocking(fd, (BYTE *)&connStat[i].data, connStat[i].datasize, &connStat[i], &peers[i]) < 0) {
						RemoveConnection(i);
						continue;
					}
					if(connStat[i].nRecv == connStat[i].datasize){
						connStat[i].dataRecvd = true;
						printf("%d bytes data received!\n", connStat[i].nRecv);
						connStat[i].nRecv = 0;
					}
				} else {
						if (Recv_NonBlocking(fd, (BYTE *)&connStat[i].info, INFOLEN, &connStat[i], &peers[i]) < 0) {
							RemoveConnection(i);
							continue;
						}
						if(connStat[i].nRecv == INFOLEN){ // Info fully Received
							printf("%d bytes RECVD Query: %s\n", connStat[i].nRecv, connStat[i].info);
							connStat[i].datasize = getSize(connStat[i].info);
							update_response_msg(i);
							connStat[i].nRecv = 0;
						}
					}
				//send response
				if (connStat[i].datasize == 0) { // MSG with Empty Body
					if (isPublic(connStat[i].info) == 1) { // Public
						for(int cli = 0; cli < nConns + 1; cli++){
							if(connStat[cli].name != NULL) { // Send only to logged in users
								int cli_fd = peers[cli].fd;
								printf("%d bytes SENT server response to fd %d: %s\n", INFOLEN, cli_fd, connStat[i].response);
								n = Send_Blocking(cli_fd, (BYTE *) connStat[i].response, INFOLEN);
								if (n < 0){
									RemoveConnection(i);
									continue;
								}
							}
						}
					} else { // Private Message with Empty Body
						int dest_fd = connStat[i].destFd; // Is the src fd if dest user doesnt exist
						printf("%d bytes SENT server response to fd %d: %s\n", INFOLEN, dest_fd, connStat[i].response);
						n = Send_Blocking(dest_fd, (BYTE *) connStat[i].response, INFOLEN);
						if (n < 0){
							RemoveConnection(i);
							continue;
						}
					}
				} // Response with a body
				if (connStat[i].datasize > 0 && connStat[i].dataRecvd == true) {
					connStat[i].nRecv = 0; // Reset nRecv Buffer
					// Send a public reply with data
					if (isPublic(connStat[i].info) == 1) { // Public, send to every client
						for(int cli = 0; cli < nConns + 1; cli++){
							if(connStat[cli].name != NULL) { // Send only to logged in users
								int cli_fd = peers[cli].fd;
								// Send the info header
								printf("%d bytes SENT public response to fd %d: %s\n", INFOLEN, cli_fd, connStat[i].response);
								n = Send_Blocking(cli_fd, (BYTE *) connStat[i].response, INFOLEN);
								if (n < 0){
									RemoveConnection(i);
									continue;
								}// Send the data
								if (connStat[i].datasize > MAX_MSG_LEN) {
									printf("%d bytes SENT public file to fd %d\n", connStat[i].datasize, cli_fd);
								} else {
									printf("%d bytes SENT public msg to fd %d: %s\n", connStat[i].datasize, cli_fd, connStat[i].data);
								}
								n = Send_Blocking(cli_fd, (BYTE *) connStat[i].data, connStat[i].datasize);
								if (n < 0){
									RemoveConnection(i);
									continue;
								}
							}
						} // Group response with body
					} else if (isGroup(connStat[i].info) == true && connStat[i].failedSend == false) {
							int group = getGroup(connStat[i].info);
							for(int cli = 0; cli < nConns + 1; cli++) {
								if (connStat[cli].groups[group] == true) {
									int cli_fd = peers[cli].fd;
									printf("%d bytes SENT group response to fd %d: %s\n", INFOLEN, cli_fd, connStat[i].response);
									n = Send_Blocking(cli_fd, (BYTE *) connStat[i].response, INFOLEN);
									if (n < 0){
										RemoveConnection(i);
										continue;
									}// Send the data
									if (connStat[i].failedSend == false) {
										if (connStat[i].datasize > MAX_MSG_LEN) {
											printf("%d bytes SENT group file to fd %d\n", connStat[i].datasize, cli_fd);
										} else {
											printf("%d bytes SENT group msg to fd %d: %s\n", connStat[i].datasize, cli_fd, connStat[i].data);
										}
										n = Send_Blocking(cli_fd, (BYTE *) connStat[i].data, connStat[i].datasize);
										if (n < 0){
											RemoveConnection(i);
											continue;
										}
									} else {
										connStat[i].failedSend = false;
									}
								}
							}
					} else { // Private response with a body
						int dest_fd = connStat[i].destFd;
						// Send the info header
						printf("%d bytes SENT private response to fd %d: %s\n", INFOLEN, dest_fd, connStat[i].response);
						n = Send_Blocking(dest_fd, (BYTE *) connStat[i].response, INFOLEN);
						if (n < 0){
							RemoveConnection(i);
							continue;
						}// Send the data
						if (connStat[i].failedSend == false) {
							if (connStat[i].datasize > MAX_MSG_LEN) {
								printf("%d bytes SENT private file to fd %d\n", connStat[i].datasize, dest_fd);
							} else {
								printf("%d bytes SENT private msg to fd %d: %s\n", connStat[i].datasize, dest_fd, connStat[i].data);
							}
							n = Send_Blocking(dest_fd, (BYTE *) connStat[i].data, connStat[i].datasize);
							if (n < 0){
								RemoveConnection(i);
								continue;
							}
						} else {
							connStat[i].failedSend = false;
						}
					}
					// Reset stats if done sending/receiving
					memset(connStat[i].data, 0, MAX_REQUEST_SIZE);
					connStat[i].needData = false;
					connStat[i].dataRecvd = false; //Data Sent
				}
			}
		}
	}
}

void cleanup_server() {
	system("rm userinfo/*");
	system("rm downloads/*");
	system("rmdir downloads");
	system("rmdir userinfo");
}

int main(int argc, char * * argv) {
	if (argc != 2) {
		Log("Usage: %s [server Port]", argv[0]);
		return -1;
	}
	// Set up signal handling
	signal(SIGINT, handler);
	signal(SIGTERM, handler);
	if (strcmp(argv[1], "reset") == 0){
		cleanup_server();
		return 0;
	}
	int port = atoi(argv[1]);
	int maxConcurrency = 32;
	DoServer(port, maxConcurrency);

	return 0;
}
