GopherChat Server and Client by Luc Le (LEXXX764)

/////////
//Files//
/////////
gc.h // GopherChat Header file, contains shared functions/constants/library headers/etc
server.cc // Contains main chat server loop and server related functions
client.cc // Contains main chat client loop and client related functions
/testcases // Directory that contains various test cases for the 

/////////
//Setup//
/////////
Ensure you are logged in as SuperUser or else prefix every command with 'sudo'

Run the Make file
> make

Running the program:
To start the server
./server [port number]

To connect to the server with the client:
./client [server ip address] [port] [script file name]

If the script file argument isn't provided, then the client will run in manual user input mode.

Files will be stored in a folder called 'downloads' which will be created relative to where the client executable is.

To Cleanup/Delete all generated files:
Run
> make clean
or
> ./server cleanup


///////////////
//Description//
///////////////
All commands were implmented as specified with mostly blocking Sends/Receives, 
but some effort was made on the serverside which can receive without blocking.
Group chat was implemented as bonus feature and is described in a section below.

/////////////////////////////
// Implementation Overview //
/////////////////////////////

The client and server interact with a single persistent TCP connection.

After establishing a socket connection to the server, the client and server
communicate in a 2 step process. Namely, the server and client both first send
an "info header" array that is always 80 bytes and then sends a file or message
data with the size and destination info in the info header. This is to ensure 
that the server and client both know how much data to receive.

Thie info header contains five space separated parameters which are tokenized 
and processed in both the server and client. Some attributes that these could 
include would be filenames, usernames, sendernames, and datasizes. The last 
parameter of the info header will always be the message/file datasize. It helps
determine what the client/server should do with subsequent message or file
data. However, some commands from the clientside do not have message or file 
data like LOGIN or REGISTER for example. In those cases only an info header
is sent, but the server may still reply with data in the case of LIST which
sends a message to the client with the list of logged-in users.

To demonstrate how basic communication works I will walk through what happens
when the client sends the "SEND Helloworld" (given that the user is already
logged-in and the client has already connected).

In client.cc
1. The string is fed to the clnt_handle_input_msg() function which tokenizes
the string into separate arguments. 
2. This function generates the 80 byte info string denoted by the buffer called
"response" and secondly a buffer called "inputBuf."
3. These buffers are sent in two individual sends to the server.
Now in server.cc
4. Since the server is polling for data, the POLLRDNORM flag should be set
and the server will attempt to receive the infoheader. Once the infoheader 
has been fully received, the server will process it in update_response_msg().
5.This will determine whether or not there will be a second send from the client
to receive, in this case there is, so update_response_msg() will set the needData
flag to true. The response info header is also generated and stored in the 'info'
character buffer in the connStat struct.
5. The server will now retrieve the data in a separate Recv_nonblocking call which
will receive n bytes of data which is always specified by the last parameter in the
info header. The data will be stored in a character buffer within the connStat struct
titled 'data.'
6. The server will now send both the response info header and then the message data to
all logged-in clients. This is done in a for loop accross all file descriptors. The server
considers a client is logged-in if they have a set name attached to their connStat struct.
Back to client.cc
7. The client has a separate pthread for receiving data. The client will first receive
the info header which will determine how much data the client should receive for the
message data after it is tokenized.
8. Now that the client has recieved the data, the message is printed out and the exchange
is complete.

///////////
// INPUT //
///////////
To separate separate manual input (for practical usage and debugging), a thread 
was created solely to receive input using POSIX pthreads. If a script file is
provided, the thread will simply feed the contents of the file line-by-line until
it reaches the end. The thread will then transition back to manual user input after
the script has run its course.


//////////////////////
// Command Overview //
//////////////////////
REGISTER [username] [password] Register a new account
// In the server, userdata is stored in a file with the same name as the client in a
// folder titled "userinfo"
// The password is stored as the contents of that file

LOGIN [username] [password] Log in with an existing account and enter the chat room
// Until they are logged-in, users can only send the DELAY, REGISTER, or LOGIN commands 
// If a user is already logged in, they cannot LOGIN until they haved logged out or
// reset the client.
// If a user tries to LOGIN with the credentials of a currently logged in user,
// their request will be denied by the server.

LOGOUT Log out and leave the chat room
// Logout will sign the user out and close the client.
// The server will also notify other users that this client has logged out.

SEND [msg] Send a public message
// This will send a message to all logged in users
// Messages of without content or greater than 256 characters will not be sent and an error message
// will be displayed instead

SEND2 [username] [msg] Send a private message to a user
// This will send a message only to a specified user
// If the user is not logged in or doesnt exist, the server will reply with a rejection message.
// Messages of without content or greater than 256 characters will not be sent and an error message
// will be displayed instead

SENDA [msg] Send an anonymous public message
// A message will be sent to all logged-in users anonymously
// Messages of without content or greater than 256 characters will not be sent and an error message
// will be displayed instead

SENDA2 [username] [msg] Send an anonymous private message to a user
// This will send an anonymous message only to a specified user
// If the user is not logged in or doesnt exist, the server will reply with a rejection message.
// Messages of without content or greater than 256 characters will not be sent and an error message
// will be displayed instead

SENDF [local file] Send a file publicly
// This will send a file only to all logged-in users.
// If the user is not logged in or doesnt exist, the server will reply with a rejection message.
// The client will not send anything if the file isn't found or is too large

SENDF2 [username] [local file] Send a file to a user privately
// This will send a file only to a specified user
// If the user is not logged in or doesnt exist, the server will reply with a rejection message.
// The client will not send anything if the file isn't found or is too large

LIST List all online users
// This will cause the server to reply with a character array with all currently logged-in users.

DELAY [N] 
// This will cause the input loop to sleep() for N seconds.

/////////////////////////////////////////
// New feature Implemented: Group Chat //
/////////////////////////////////////////
Users will now be able to join group chat channels specified by a single integer between
0-MAX_GROUPS. 

The server has a limited number of group chat rooms, defined by the constant "MAX_GROUPS" in gc.h.
Three commands were implemented to allow for this feature:

JOING [# between 0-MAX_GROUPS] Join a chat group/room
// This command will allow a user to join a chat room
// Users can be in multiple chat rooms
// If the user is already in that room then the server will notify the client with a rejection message

LEAVEG [# between 0-MAX_GROUPS] Leave a chat group/room
// This command will remove the user from the specified chat room
// If the user isn't in that room then the server will notify the client with a rejection message

SENDG [# between 0-MAX_GROUPS] [Message] Send a message to a chat group/room
// This command will send a message to every user in the specified chat room
// Will only work if the user is already in that chat room otherwise server will reply with a rejection message

Testcases to demonstrate this functionality are provided in the testcases folder titled: 
'grp.user1.txt'
'grp.user2.txt'
'grp.user3.txt' 
