# Text Chat CLI Application

## Objective
Develop the client and server components of a text chat application, consisting of one chat server and multiple chat clients over TCP connections.

## Explaination
The chat application follows a typical client-server model, whereby we will have one server instance and two or more client instances. Given that we will be testing on the five dedicated hosts listed before, you can assume that at most four clients will be online at any given time.

The clients, when launched, log in to the server, identify themselves and obtain the list of other clients that are connected to the server. Clients can either send a unicast message to any one of the other clients or broadcast a message to all the other clients.

Note that the clients maintain an active connection only with the server and not with any other clients. Consequently, all messages exchanged between the clients must flow through the server. Clients never exchange messages directly with each other.

The server exists to facilitate the exchange of messages between the clients. The server can exchange control messages with the clients. Among other things, it maintains a list of all clients that are connected to it, and their related information (IP address, port number, etc.). Further, the server stores/buffers any messages destined to clients that are not logged in at the time of the receipt of the message at the server from the sender, to be delivered at a later time when the client logs in to the server. You do NOT need to buffer messages for EXITed clients or from BLOCKed clients (see section 5.6). You can assume that the total number of buffered messages will not exceed 100.

## Running your program
Program will take 2 command line parameters:
1. The first parameter (s/c) indicates whether your program instance should run as a server or a client.
2. The second parameter (number) is the port number on which your process will listen for incoming connections. In the rest of the document, this port is referred to as the listening port.

- To run as a server listening on port 4322<br>
`$ ./assignment1 s 4322`

- To run as a client listening on port 4322<br>
`$ ./assignment1 c 4322`

The source code can be found in `sagarjit/src/sagarjit_assignment1.c`. The screenshot for each command can be found in the report named `PA1_report.pdf`.

## Collaborator:
- Pushkaraj Joshi
- Sagar Thacker
