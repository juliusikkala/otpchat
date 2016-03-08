OTPCHAT
=======

A simple chat program which uses one-time pads for encryption. Each user has
their own personal key which their client uses to encrypt the messages. The key
must be transferred physically, for example on a USB drive. Receiving users
must have a copy of the sender's key in order to decrypt the data.

There is no way to stop impersonation, so every user must be trusted. This
should not be a problem since they must have been already trusted with the
keys.

![The program in use](https://i.imgur.com/LO5zHiL.png "Screenshot")