# MultiplayerGame
In this project a gambling station for horses' race was build.
Having one hourse race administrator and serveral gamblers, the implementation is done by
one server and its' clients.

## A bit about the Implementation
The server asks each client over TCP connection on which horse he would like
to put money and the amount.
The moment the server accepted two legal (or more) clients, the race begins, and
over different UDP session the server updates all clients by multicast for the current
state of the race.

In order to serve several clients simultaneously, the server uses two sessions with
each client, one over TCP and one over UDP (with select() function, the UDP will be
multicast transmission), each one is implemented over different thread.
