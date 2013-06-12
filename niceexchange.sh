#!/bin/sh
# upload data to some provider where somebody else can find it ;)
PRIVATE_KEY_FILE=~/.ssh/id_rsa
PUBLIC_KEY_FILE=~/.ssh/id_rsa.pub

ISCALLER=$1
shift

HOSTNAME=$1
shift

MODE=$1
shift


while [ $# != 0 ]; do
	ARG=$1

	if [ `echo $ARG | grep @` ]; then
		PROVIDER=`echo $ARG | rev | cut -d@ -f1 | rev`

		LEN=`echo ${PROVIDER} | wc -c`
		OPTIONS=`echo $ARG | head -c-$[LEN+1]`
	else
		PROVIDER=$ARG
		OPTIONS=
	fi

	if [ "$MODE" = 'publish' ]; then
		cat - $PUBLIC_KEY_FILE $NICE_LOCAL_CRT | ./exchange_providers/${PROVIDER} ${ISCALLER} ${MODE} ${OPTIONS}
	fi

	if [ "$MODE" = 'unpublish' ]; then
		./exchange_providers/${PROVIDER} ${ISCALLER} ${MODE} ${OPTIONS}
	fi

	if [ "$MODE" = 'lookup' ]; then
		TMP_CREDENTIALS_FILE=$(tempfile -s.cre -p.n)
		TMP_REST=$(tempfile -s.r -p.n)
		TMP_NEW_REMOTE_PUBLIC_KEY_FILE=$(tempfile -s.pub -p.n)
		if [ -z "$NICE_REMOTE_CRT" ]; then
			NICE_REMOTE_CRT=`tempfile -s.pub -p.nice`
		fi
		
		# split information: (credentials, public key, certificate)
		./exchange_providers/${PROVIDER} ${ISCALLER} ${MODE} ${OPTIONS} > $TMP_REST
		head -n 1 $TMP_REST
		tail -n+2 $TMP_REST | head -n 1 > $TMP_NEW_REMOTE_PUBLIC_KEY_FILE
		tail -n+3 $TMP_REST > $NICE_REMOTE_CRT
	
		# calculate fingerprints
		NEW_FINGERPRINT=`ssh-keygen -f $TMP_NEW_REMOTE_PUBLIC_KEY_FILE -l | cut -d' ' -f2`
		OLD_FINGERPRINT=`ssh-keygen -F $NICE_REMOTE_HOSTNAME  -l | grep -v '#' | grep -i rsa | head -n 1 | cut -d' ' -f2`

		ok=yes
		# compare finger prints
		if [ "$OLD_FINGERPRINT" = '' ]; then
			echo "This seems to be a new public key! (fingerprint: '$NEW_FINGERPRINT')!" 1>&2
			echo "Maybe you want to add it to your ~/.ssh/known_hosts?" 1>&2
			cat $TMP_NEW_REMOTE_PUBLIC_KEY_FILE | cut -d' ' -f1-2 1>&2
			ok=no
		else
			LEN=`echo $NEW_FINGERPRINT | wc -c`
			if [ "$NEW_FINGERPRINT" != "$OLD_FINGERPRINT" -a $LEN = "48" ]; then
				echo "Incorrect fingerprint (was: '$NEW_FINGERPRINT', expected '$OLD_FINGERPRINT')!" 1>&2
				ok=no
			else
				# check if cert was signed with this public key
				REMOTE_CRT_PUBKEY=$(tempfile -p.n)
				REMOTE_PUBKEY_SSH=$(tempfile -p.n)
				REMOTE_PUBKEY=$(tempfile -p.n)

				ssh-keygen -F $NICE_REMOTE_HOSTNAME | tail -n 1 | sed -e 's/.*ssh-rsa/ssh-rsa/' > $REMOTE_PUBKEY_SSH
				ssh-keygen -f $REMOTE_PUBKEY_SSH -e -m PKCS8 > $REMOTE_PUBKEY

				openssl x509 -noout -pubkey -in $NICE_REMOTE_CRT > $REMOTE_CRT_PUBKEY

				diff $REMOTE_CRT_PUBKEY $REMOTE_PUBKEY
				if [ $? -ne 0 ]; then
					echo "Received certificate was not signed by the correct public key!"
					ok=no
				fi
			fi
		fi
		rm -f $TMP_NEW_REMOTE_PUBLIC_KEY_FILE
		rm -f $TMP_CREDENTIALS_FILE

		if [ $ok = "no" ]; then
			exit 1
		fi
	fi

	if [ $? -ne 0 ]; then
		exit 1
	fi

	shift
done

exit 0
