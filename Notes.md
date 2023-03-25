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
