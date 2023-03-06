# IRC Project

## Timeline

### Checkpoint #1

- Implement a single IRC server that communicates asynchronously with clients using the epoll api
- Implement the data structures to associate the sock_to_user_map with user data in the server
- Implement simple CLI client that can communicate messages from user to server
- Start the server with a configuration file that specifies the hostname and ports to be used

List of commands to implement:

1. NICK

The USER and NICK command should be the first messages sent by a new client to complete registration.

Syntax: `NICK <nickname>`
Example: `NICK aarya\r\n`

Replies:

- RPL_WELCOME
- ERR_NONICKNAMEGIVEN
- ERR_NICKNAMEINUSE

2. USER

The USER and NICK command should be the first messages sent by a new client to complete registration.

- Syntax: `USER <username> * * :<realname>`

- Description: USER message is used at the beginning to specify the username and realname of new user.
- It is used in communication between servers to indicate new user
- A client will become registered after both USER and NICK have been received.
- Realname can contain spaces.

Example: `USER aaryab2 * * :Aarya Bhatia`

Replies:

- RPL_WELCOME
- ERR_NEEDMOREPARAMS
- ERR_ALLREADYREGISTERED

3. PRIVMSG

The PRIVMSG command is used to deliver a message to from one client to another within an IRC network.

- Sytnax: `[:prefix] PRIVMSG <receiver> :<text>`
- Example: `PRIVMSG Aarya :hello, how are you?`

Replies

- RPL_AWAY
- ERR_NORECIPEINT
- ERR_NOSUCHNICK
- ERR_TOOMANYTARGETS

4. QUIT

The QUIT command should be the final message sent by client to close the connection.

- Syntax: `QUIT :<message>`
- Example: `QUIT :Gone to have lunch.`

Replies:

- ERROR

5. PING/PONG

The PING message can be used to test the presence of active server or client.

Syntax: `PING <server>`
Example: `PING example.com`

The PONG message is used to reply to a PING message.

Example: `PONG`

6. WHO

The WHO command is used to receive a list of clients that are connected to the IRC network at present.

Replies:

- RPL_WHOREPLY
- RPL_ENDOFWHO

7. WHOIS

Example:

- `WHOIS example.org`: ask server for information about example.org
- `WHOIS aarya`: ask server for information about nick aarya

Replies:

- RPL_WHOISSERVER
- RPL_WHOISUSER
- RPL_ENDOFWHOIS
- ERR_NOSUCHSERVER
- ERR_NONICKNAMEGIVEN
- ERR_NOSUCHNICK

Notes:

- If any message other than NICK or USER is recieved before user is registed, server should send ERR_NOTREGISTERED reply.
- NICK can be used after registration to change nickname.
- Server will attach its hostname as prefix for all replies.

### Checkpoint #2

- Allow multiple IRC servers to communicate with each other and the clients
- Server password authentication
- List of commands to implement: (TODO)

### Checkpoint #3

- Implement IRC chat channels to allow user to send message to multiple clients
- List of commands to implement: (TODO)

### Extra

- Server uses a routing algorithm to deliver client messages on different servers
- Ping server periodically to know if they are still running and track the network congestion

#### Connection Timeout

PING: A ping message will be sent at regular intervals from server to client if no other activity detected. If connection fails to responed to PING within some time, that connection is closed.

## Design

Server data structues

- sock_to_user_map: `Map<int,User*>` maps socket to user struct
- username_to_user_map: `Map<char*,User*>` maps username to user struct
- online_nick_to_username_map: `Map<char*,char*>` maps nick to username of online user
- offline_nick_to_username_map: `Map<char*,char*>` maps nick to username of offline user
- channels: `Map<char*,Channel*>` maps channel name to channel struct

## Server start event

- initialise empty `online_map` and `offline_map` which maps nick to username.
- load channels from file `data/channels.txt` into map from channel name to struct.

## User connect event

- inialize user struct
- add socket -> user struct entry in `sock_to_user_map` map

## NICK

- set nick
- if registered update/add entry in `online_map`

## USER

- set username, realname
- if registered update/add entry in `online_map`

## AWAY

- move nick to `offline_map`
- todo

## QUIT

- remove entry from maps
- their nick is free to use
- remove user from all channels

## JOIN

- add username to channel list

## PART

- remove username from channel list

## PRIVMSG

- if user in offline map-> send AWAY to user
- if user in online map-> send message to target user
- If user not found -> send error to user
- If target is channel -> send to all online username_to_user_map in channel

## Server stop event

- save channels only
