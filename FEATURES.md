# Features


## User Registration

- User can register with NICK and USER commands
- NICK can be used to update nick at any time
- Server keeps track of all the nicks that have been used by users
- User may skip NICK to reuse previous NICK if there is one
- USER can only be used once at the start to set username and realname.
- username is private to the user, it is the main identification source
- Users can use NICKs to send messages to another user
- User can use any NICK that is owned by a user

## Client

The client implements a thread-based model with four threads:

### Client Outbox thread

- The outbox thread blocks if the outbox queue is empty.
- This thread pulls messages from the queue and sends them to the server over existing tcp connection.
- If a 'QUIT' message is discovered this thread will quit immediately after sending it to the server.

### Client Inbox thread

- The inbox thread will block if the inbox queue is empty.
- The inbox thread pulls messages from the inbox queue and handles them.
- Most commonly the messages are printed to stdout for the user to see.
- On an ERROR message the inbox thread will quit immediately. The ERROR message is also a reply to a QUIT message.

### Client Reader thread

- The reader thread will asynchronously read() from the server using the epoll API.
- This thread will use a buffer to store incomplete messages.
<!-- - It will use the Message parser to parse the messages received from the server.  -->
- It will handle the case if there are multiple messages sent at once.
- It will add each message to the inbox queue for the inbox thread to handle. 
- It will never block on an enqueue as the queue has no maximum size.


### Client Main thread

- It is the duty of the main thread to read user input from stdin.
- This thread blocks on the getline() instruction.
- If the input is a valid IRC command, the string will be terminated by CRLF appropriately.
- If the input is a special command starting with a /, the client will generate the corresponding command.
- The irc command is added to the outbox queue to send to the server.


