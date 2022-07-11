********************************************************************************
AdrianClinansmithA1
CIS3210 F20 Assignment 1

By: Adrian Clinansmith
October 2020
********************************************************************************

Run application
================

$ make                          ... compile the client and server
$ make runserver                ... run the server 
$ make runclient                ... the client sends AliceInWonderland.txt to the cis server

$ ./bin/client hostname file    ... run the client & pass a specific host and file

To Watch For
================

Sometimes a file may appear to be prepended with � or Ôªø (int value -17). 
This may or may not be visible in some text editors as it is a Byte Order Mark.
My program isn't producing it, it is simply present in certain files, thus it is not
an error. I chose to send files as they are, rather detecting & deleting BOMs.