# Inter Relay Chat

In this project, I have implemented a server and client program that follow the
Inter Relay chat protocol. The IRC is an application layer protocol used for
real time text communication between multiple users on the Internet. The
protocol was developed in the late 1980s and has since become a popular means
of online communication.

## Usage

To configure the servers, edit the [config.csv](config.csv) file. Each line 
should have the Name, IP address, port and password of the server. For example:
`server1,127.0.0.1,5000,test1`.

To general format for the config.csv file is as follows: `ServerName,ServerHostname,ServerPort,ServerPassword`.

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
For documentation on IRC commands please visit: <https://modern.ircdocs.horse/>.

To text a friend, you can use the `PRIVMSG` command:

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

Example:

```
/client <LoginFile>
JOIN #network
TOPIC #network :computer networking
PRIVMSG #network :Hello
PART #network
```

The above command logs in a user, they join a channel called `#network`, and set its topic.
Then, the user sends a message to everyone on the channel. Finally, the user leaves the channel.
Further messages to the channel will not be delivered to the user.

## CONNECT

You can use the `CONNECT` command to establish a connection between two known IRC servers.
Before you do this, you must add the entry `<Name, IP, Port, Password>` to the config.

Suppose you are running server1 and server2 on two machines. A user can login and issue the 
following command to connect the servers. The user can connect to either server and 
link it with the other one. For this example, let's say the user is on server1.
They run `CONNECT server2` to establish the connection between server1 and server2.

Now users on server1 can talk to users on server2!



