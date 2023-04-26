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
USER aarya aaryab2 :Aarya Bhatia
```

You can text a friend using the `PRIVMSG` command:

```
PRIVMSG <Nick> :Hello!
```

To close the client enter: `QUIT [:Bye!]`, where the parameter is optional.

To close the server type CTRL-C.
