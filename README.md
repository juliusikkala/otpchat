# OTPCHAT

A simple chat program which uses one-time pads for encryption. Each user has
their own personal key which their client uses to encrypt the messages. The key
must be transferred physically, e.g. on a USB drive. Receiving users must have
a copy of the sender's key in order to decrypt the data.

**Use at your own risk!** There is no guarantee that the program is secure. 
Furthermore, there is no warranty of any kind. See the "LICENSE" file for legal
details.

![The program in use](https://i.imgur.com/LO5zHiL.png "Screenshot")

## Usage

### Generating keys
```
otpchat --generate <size> <new-key-file>
```
Generates a key from `/dev/urandom`.

Generate at least two keys, one for yourself and one for the person you want
to chat with. Transfer *both* of the keys to the person on a physical medium
such as a thumb drive.

### Chatting
In all cases, you have to specify your local key and the expected remote key.

```
otpchat <local-key> <remote-key> [port]
```
Starts listening on port 14137 unless another port is specified.

```
otpchat <local-key> <remote-key> <address>[:<port>]
```
Attempts to connect to the given address and port. If the port isn't specified,
defaults to port 14137.

## Commands

A command is preceded by '/'. For example, the command to quit the program is
"/quit".

|  Command   |    Arguments     |              Function                |
| :--------- | :--------------- | :----------------------------------- |
| quit       |                  | Quits the program                    |
| connect    | address\[:port\] | Connects to the given address        |
| disconnect |                  | Disconnects from the current session |
| listen     | \[port\]         | Starts listening for connections     |
| endlisten  |                  | Stops listening for connections      |
