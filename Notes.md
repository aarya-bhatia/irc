# IRC Project

## Special Commands

The following commands have been designed for demonstration and are not part of the specification.

The following command is asynchronous in nature, i.e. they involve communicating with the entire network to create the full responsne.

Edge cases:

Command: TEST_LIST_SERVER
Replies:

- 901 RPL_TEST_LIST_SERVER_START
- 902 RPL_TEST_LIST_SERVER
- 903 RPL_TEST_LIST_SERVER_END

Algorithm:

- User requests origin server to list all servers on the network.
- server adds user to map `test_list_server_map` and initialises a struct with the following members:
  - a reference to the connection struct to send messages
  - a set containing all the peers of this server
    - this set inidicates which peers still need to send a reply
- we send the original client all the reply messages for the original server first.
- now we request the peers of the original server to also send a list reply for the same request.
- we can continue processing other commands from the user while waiting for replies for the TEST_LIST_SERVER request.
- The reply is sent in a multipart message, so that when one of the peers recursively sends back a reply for servers behind their connection, we relay this information to the original client in turn.
- In particular, when one of the peers sends a RPL_TEST_LIST_SERVER to the origin server, we add this response to the original client's message queue.
- when one of the peers sends a RPL_TEST_LIST_SERVER_END to the origin server, this indicates that the peer has finished listing all the servers behind their server.
So this peer is removed from the set.
- When the set is empty, we have finished listing all servers so we can send a RPL_TEST_LIST_SERVER_END to the original client and remove their entry from the map.
- If during this period a new server joins, we simply ignore them from the response.
- If during this period the user sends another list request, we do not acknowledge it.

## How to keep consistent state across the network

- Event of server disconnect:
  - Broadcast a SQUIT message for each server behind that connection
  - Broadcast a QUIT for each client behind that connection
- Event of new server connection:
  - Both servers share the names of their peers with each other
  - Both servers share the NICKs of their clients with each other
  - Both servers share their channels with each other
- Event of client disconnect:
  - Broadcast a QUIT for that client to all peers

## Routing messages

Observe that the IRC network is a spanning tree structure, so it has no cycles. So, it is simple to use a BFS like algorithm
to route messages from one server to another.

Suppose we have three clients alice and bob and cat.
There are three servers A,B and C.
There are the following edges in the network graph: A <-> B and A <-> C.
Suppose alice is connected to A, bob is connected to B and cat is connected to C.

Suppose alice wants to send a message "hello" to cat.
The following actions will take place:

1. alice will send a message "PRIVMSG cat :hello" to server A
2. server A will check if cat is a client known to it.
3. Since server A knows C, it will check if cat is a client on the server A.
4. Since cat is not a client on server A, A will relay this message to each of its peers, namely B and C.
5. server B and server C recursively do the similar action.
6. server B knows cat and cat does not live on server B. At the point server B can ignore the message because
server B's only peer is A and no server should relay a message back to the sender.
Without this, there would be an infinite loop of messages.
7. server C will finally see that cat lives on that server. server C will not relay this message any further to avoid wasting bandwidth.
8. server C will add this message to cat's message queue. When cat is ready to receive messages, this server will write this message over the socket to cat's client program.
9. the client program will display this message to cat.

As you can see, all messages are propogated down the network until they reach the destination or an edge. The messages make their way to the destination through a series of relays on intermediate servers. This strategy would work on any number of servers as long as they follow the critical rule that the network is a spanning tree.

Note: The server can optimise this message delivery process even further if it wishes to do so. There are optional features built into
the protocol to allow the servers to gain more insight about the network. For example, we can attach a "hop count" parameter to some messages so that servers can know the distance between two nodes on the network. With more information, the server can pre compute the shortest path to the client and only relay messages to a few peers to save bandwidth. This strategy is not implemented in my project, but it is simple to extend the functionality of the relaying.
