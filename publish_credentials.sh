#!/bin/sh
PRIVATE_KEY_FILE=~/.ssh/id_rsa

openssl sha1  -binary |
openssl pkeyutl -sign -inkey $PRIVATE_KEY_FILE -pkeyopt digest:sha1

#verify 
#openssl rsa -in ~/.ssh/id_rsa -pubout > /tmp/KEY.pub
#openssl pkeyutl -verify -sigfile signature \
#-pubin -in /tmp/data -inkey /tmp/KEY.key -pkeyopt digest:sha1

