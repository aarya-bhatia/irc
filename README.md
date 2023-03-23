# Features

Project in progress...

## Connection Struct

- The connection struct is generic and helps read/write data to any socket connection. It is used by both client and server.
- The connection struct has a type parameter which can either be UNKNOWN_CONNECTION, CLIENT_CONNECTION, PEER_CONNECTION, or USER_CONNECTION.  A new connection on the server is set to be Unknown. It is later promoted to a Peer or a User connection when the client sends the initial messages (NICK/USER or SERVER/PASS).
- A connection can also store arbitary data for the client. This parameter is used to store a pointer to the Peer or User struct in a polymorphic way. It is initialised as soon as the connection type is determined.
- A connection contains incoming and outgoing message queues aside from the request and response buffers. This allows us to handle multiple messages from the client at one time and also prepare multiple messages to send to the client. It is also used to store chat messages sent by another client. The request and response buffer only store the data that is currently being read or sent.
- Note that users and peers have internal message queues as well. Initially messages are put in the main message queue, but after client registration, the messages are put in the internal message queues. This is done partly so we don't have to deal with Connection structs in the request handlers.

## Server

### Data structures

- A hashtable `connections` to map socket to a connection struct for each client and peer.
- A hashtable `nick_to_user_map` to map nick to user struct for each user. The users get a random nick in the beginning. As they update their nicks, the entry in the hashmap for that user is also updated. This map is useful to check which nicks are available and also quickly fetch the User when messages are to be delivered.
- A hashtable `name_to_channel_map` to map each channel name to a channel struct. Each server has their own copy of the channel. Since different users are connected to different servers, a channel message must be propogated to the entire network in order to reach all channel members. But a single server does not know all the clients in the channel.
- A hashtable `name_to_peer_map` is used to map the name of a peer to a Peer struct. When a server-to-server connection is established the remote server becomes a peer for the current server. The server which initiates the connection is known as the ACTIVE_SERVER and the server which accepts the connections is known as PASSIVE_SERVER.
- A hashtable `nick_to_serv_name_map` is used to determine which users are connected to each server. This map is updated when a peer advertises a new user connection or relays the message from another server.

### Summary

- The core of the server is single-thread and uses epoll API and nonblocking IO.
- The server uses buffers in the user data struct to receive/send messages.
- The user data uses a message queue that allows the server to prepare multiple messages to send to one client.
- The message queue is also useful for message chat as messages for a target user can be pushed to their respective queues.

## Logic

The main logic is that the epoll listens for Read/Write events on all the available client sock_to_user_map as well as the server listening socket. If the event is on the listening socket, that implies the server can connect to new clients and initialise their user data. This is done through the `Server_process_request()` function.
In the case the event is on a client socket, I handle three cases: read, write and error. On error, I disconnect the client. On write, we send any pending messages from that user's message queue. Lastly, on read, we receive any data and parse the message if it is complete. Otherwise, we store the bytes in the user's buffer for later.

There are various functions to handle the commands in the form of `Server_handle_XXXX()`.
These functions use the predefined message reply strings in `reply.h` and substitute the reply parameters such as user's nick.
The message parser in `message.h` is used to parse message details safely.

The server currently supports the following commands from the client: MOTD, NICK, USER, PING, QUIT.

## QUIT

The QUIT command is used by a client to indicate their wish to leave the server.
All data associated with this user is freed and socket is closed.
An ERROR reply is sent before closing the socket to allow the reader thread at the client to quit gracefully.
This is done through a 'quit' flag in the user data that is set to true when the QUIT request comes.
When the final message to the client is sent and if a quit flag is seen, the server knows to close that connection at that point.

## MOTD

This command sends a Message of the Day to the requesting user.
It looks up the current day's message from a file (motd.txt) that contains a list of quotes.
This file was downloaded using the `zenquotes.io` API using the script in `download_quotes.py`.
This list can be updated by repeating this script.
The server simply gets the line that is at position `day_of_year % total_lines`, where `total_lines < 365`.
The filename can be changed at any time as the server has a string to store the filename.

## NICK/USER: User Registration

- User can register with NICK and USER commands
- NICK can be used to update nick at any time
- Server keeps track of all the nicks that have been used by username_to_user_map
- User may skip NICK to reuse previous NICK if there is one
- USER can only be used once at the start to set username and realname.
- username is private to the user, it is the main identification source
- Users can use NICKs to send messages to another user
- User can use any NICK that is owned by a user

The code for registering is found in `register.c`.

## PRIVMSG

- This command is only enabled after registration for both sender and receiver.
- User can send message to another user using their nick.
- Server can accept any nick that user has owned as a valid target.
- Server searches the userame to nicks map to find the username of the target user.
- The server can look up the user's data through their username and check if they are online or not.
- The server will not send the message to a user that is offline.

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
<!-- - It will use the Message parser to parse the messages received from the server. -->
- It will handle the case if there are multiple messages sent at once.
- It will add each message to the inbox queue for the inbox thread to handle.
- It will never block on an enqueue as the queue has no maximum size.

### Client Main thread

- It is the duty of the main thread to read user input from stdin.
- This thread blocks on the getline() instruction.
- If the input is a valid IRC command, the string will be terminated by CRLF appropriately.
- If the input is a special command starting with a /, the client will generate the corresponding command.
- The irc command is added to the outbox queue to send to the server.

## Command-Line Args

The client can accept the following command line args in the given order:

1. hostname
2. port
3. nick
4. username
5. realname

*Example*: `./client localhost 5000 aaryab2 aarya.bhatia1678 "Aarya Bhatia"`.
