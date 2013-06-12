nicepipe
========

Netcat's p2p buddy

It allows you to setup a network connection between two peers even if they are behind a NAT and uses your SSH keypairs to secure the connection.

`nicepipe` solves two problems two problems that usually occur when you want to connect two computers somewhere on the planet:


1) No domain name required

`nicepipe` uses exchange_provider scripts to exchange IPs of the hosts


2) No NAT configuration required:

`nicepipe` uses STUN/ICE to circumvent your NAT and so you don't have to modify your firewall configuration.


Installation
------------

`nicepipe` requires `glib`, `libnice`, `socat` and your SSH RSA key pairs (`$HOME/.ssh/id_rsa`).
To compile just run `make`.


Usage
-----

`nicepipe` uses your SSH key pairs (`$HOME/.ssh/id_rsa`) to setup a secure connection between the two peers.
Currently a file hosting service like Dropbox or Ubuntu One setup (*MUST be the same account*) on each peers.

1) Make sure you have generated an SSH key pairs (otherwise use `ssh-keygen`).

2) Change into your file hosting service's directory on both machines (called `alice` and `bob`), e.g.

    alice$ cd ~/Dropbox


    bob$ cd ~/Dropbox


3a) Run nicepipe in pipe-mode on both machines:

    alice ~/Dropbox$ echo Hello Bob! | ./nicepipe stdio -c 1 -H <PEERNAME>
    Hello Alice!`
 

    bob ~/Dropbox$ echo Hello Alice! |  ./nicepipe stdio -c 0 -H <PEERNAME>
    Hello Bob!

where `<PEERNAME>` corresponds to the hostname of bob's (alice's) public key on alice's (bob's) machine in `$HOME/.nice_known_hosts` (`<PEERNAME>` MAY not be reachable (traditionally) by the local machine).

3b) Run nicepipe in vpn-mode on both machines:
    
    alice ~/Dropbox$ sudo ./nicepipe tun -c 1 -H <PEERNAME>
    Creating new network interface with IP# 10.0.1.2/24

    bob ~/Dropbox$ sudo ./nicepipe tun -c 0 -H <PEERNAME>
    Creating new network interface with IP# 10.0.1.1/24

Then you can connect between the two machines:

    alice $ echo Hello Bob! | nc -l 10.0.1.2 10000
    Hello Alice!


    bob $ echo Hello Alice! | nc 10.0.1.2 10000
    Hello Bob!


Troubleshooting
---------------

### Address already in use

Either the address is really still in use (look at `netstat -nl`) or your previous session did not end gracefully. In the latter case, wait some seconds and try again

### Connection is not established
Remove `$HOME/Dropbox/.nice*` and try again or add the argument `-s stunserver.org`.



You have a better idea for exchanging IP addresses?
---------------------------------------------------

Just implement another exchange provider. They take two arguments:

### First Argument
`0` if it's the caller (client) and `1` if it's the callee (server).

### Second Argument
If the second argument is `publish` then the `stdin` MUST be written where the other peer finds it.
If the second argument is `lookup` then the data published by the other peer MUST be written to `stdout`.
If the second argument is `unpublish` then the data previously published SHOULD be unpublished (optional).

e.g.

    alice$ ./exchange_providers/dummy 1 publish             # writes stdin to a file where bob can find it


License
-------
GPL

Terms MAY, MUST, SHOULD etc. according to [RFC 2119](https://www.ietf.org/rfc/rfc2119.txt).