# Inter Relay Chat

In this project, I have implemented a server and client program that follow the
Inter Relay chat protocol. The IRC is an application layer protocol used for
real time text communication between multiple users on the Internet. The
protocol was developed in the late 1980s and has since become a popular means
of online communication.

## Usage

To configure the servers, edit the `config.csv` file. Each line 
should have the Name, IP address, port and password of the server. For example:
`server1,127.0.0.1,5000,test1`.

To compile this code, please use a Linux machine.

1. Download the source code: `git clone https://github.com/aarya-bhatia/irc`
2. Go to the project directory: `cd irc`
3. To build the server and client, run: `make`
4. To start the server run: `build/server <Name>`, where `Name` can be any name in the `config.csv`.
5. To use the client run: `build/client <Name>`, where `Name` is the server to connect to.

You can use the client as either a user or a server. 
To register yourself as a user, use the `NICK` and `USER` command:

Example:

```
NICK aarya
USER aaryab2 * * :Aarya Bhatia
```

Alternatively, you can save your login information in a file as follows:
`nick username :realname`. You can login with the following command:

```
/client <filename>
```

Here are some basic IRC commands that you can try after registering with the server.
You can text a friend using the `PRIVMSG` command:

```
PRIVMSG <Nick> :Hello!
```

The `<Nick>` parameter should be replaced with the nick of the person you are texting. 
The message after the ':' can contain your message.

To close the client enter: `QUIT [:Bye!]`, where the parameter is optional.

To close the server type CTRL-C.

To get help on a specific IRC command, type the following: `HELP <Command>`. To see 
what options are available, type `HELP`. To see the currently supported commands, 
type `HELP USERCMDS`.

Here are some channel specific commands: `JOIN, PART, LIST, TOPIC`.

- JOIN: join/create a channel: `JOIN #channelName`. 
You can also see the channel list in the file `data/channels.txt`
- PART: leave a channel: `PART #channelName`
- LIST: get a list of available channels on the server
    - list specific channel information: `LIST #channel`
    - list all channel information: `LIST` 
- PRIVMSG: to send a message to a channel, include the channel name as target: 
`PRIVMSG #channelName :This message is sent to all members who have joined the channel.`
- TOPIC: get or set channel topic
    - get channel topic: `TOPIC #channelName`
    - set channel topic: `TOPIC #channelName :The topic goes here`

Other commands:

- WHO: this command is used to query user and channel information
    - To get information about a user and what channels they are on: `WHO <nick>`
    - To get information about which users are on a channel: `WHO #channelName`
