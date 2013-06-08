#!/bin/sh

#manuel@canttouchthis:~/Projekte/nicepipe$ ssh-keygen -F blackbox | tail -c 381 > /tmp/foo.pub
#manuel@canttouchthis:~/Projekte/nicepipe$ ssh-keygen -f ~/.ssh/id_rsa.pub -e -m pem

PUBLIC_KEY_FILE=~/.ssh/id_rsa.pub

openssl rsa -in ~/.ssh/id_rsa -pubout > /tmp/testkey.pubssl

openssl sha1  -binary |
openssl pkeyutl -verify -pubin -inkey /tmp/testkey.pubssl -pkeyopt digest:sha1

#verify 
#openssl rsa -in ~/.ssh/id_rsa -pubout > /tmp/KEY.pub
#openssl pkeyutl -verify -sigfile signature \
#-pubin -in /tmp/data -inkey /tmp/KEY.key -pkeyopt digest:sha1

