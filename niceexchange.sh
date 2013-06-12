#!/bin/sh
# upload data to some provider where somebody else can find it ;)
PRIVATE_KEY_FILE=~/.ssh/id_rsa
PUBLIC_KEY_FILE=~/.ssh/id_rsa.pub
NICE_KNOWN_HOSTS=~/.nice_known_hosts

if [ ! -e "$NICE_KNOWN_HOSTS" ]; then
	echo "# This is nicepipe's known_hosts file" > $NICE_KNOWN_HOSTS
	echo "# It has the same format as ssh's $HOME/.ssh/known_hosts (see sshd(8) manpage for details)" >> $NICE_KNOWN_HOSTS
fi

ISCALLER=$1
shift

HOSTNAME=$1
shift

MODE=$1
shift

set +x

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
		cat - $NICE_LOCAL_CRT | ./exchange_providers/${PROVIDER} ${ISCALLER} ${MODE} ${OPTIONS}
	fi

	if [ "$MODE" = 'unpublish' ]; then
		./exchange_providers/${PROVIDER} ${ISCALLER} ${MODE} ${OPTIONS}
	fi

	if [ "$MODE" = 'lookup' ]; then
		TMP_CREDENTIALS_FILE=$(tempfile -s.cre -p.n)
		TMP_NEW_REMOTE_CERTIFICATE=$(tempfile -s.r -p.n)
		TMP_NEW_REMOTE_PUBLIC_KEY_SSL=$(tempfile -s.pub -p.n)
		TMP_NEW_REMOTE_PUBLIC_KEY_SSH=$(tempfile -s.pub -p.n)

		# split information: (credentials, certificate)
		TMP_RECEIVED=$(tempfile -s.r -p.n)
		./exchange_providers/${PROVIDER} ${ISCALLER} ${MODE} ${OPTIONS} > $TMP_RECEIVED
		head -n 1 $TMP_RECEIVED > $TMP_CREDENTIALS_FILE
		tail -n+2 $TMP_RECEIVED > $TMP_NEW_REMOTE_CERTIFICATE

		cat $TMP_CREDENTIALS_FILE

		# extract public key from certificate and convert to ssh public key
		openssl x509 -noout -pubkey -in $TMP_NEW_REMOTE_CERTIFICATE > $TMP_NEW_REMOTE_PUBLIC_KEY_SSL
		ssh-keygen -f $TMP_NEW_REMOTE_PUBLIC_KEY_SSL -i -m PKCS8 > $TMP_NEW_REMOTE_PUBLIC_KEY_SSH
	
		# calculate fingerprints
		NEW_FINGERPRINT=`ssh-keygen -f $TMP_NEW_REMOTE_PUBLIC_KEY_SSH -l | cut -d' ' -f2`
		OLD_FINGERPRINT=`ssh-keygen -F $NICE_REMOTE_HOSTNAME -f $NICE_KNOWN_HOSTS -l | grep -v '#' | grep -i rsa | head -n 1 | cut -d' ' -f2`

		ok=yes
		# compare finger prints
		if [ "$OLD_FINGERPRINT" = '' ]; then
			echo "This seems to be a new public key:" 1>&2
			ssh-keygen -f $TMP_NEW_REMOTE_PUBLIC_KEY_SSH -lv 1>&2
			echo "Maybe you want to add it to your ~/.ssh/known_hosts?" 1>&2
			echo "" 1>&2
			echo "If so, execute this command:" 1>&2
			echo "  echo $NICE_REMOTE_HOSTNAME `cat $TMP_NEW_REMOTE_PUBLIC_KEY_SSH` >> $HOME/.nice_known_hosts" 1>&2
			echo "and run nicepipe again." 1>&2
			ok=no
		else
			LEN=`echo $NEW_FINGERPRINT | wc -c`
			if [ "$NEW_FINGERPRINT" != "$OLD_FINGERPRINT" -a $LEN = "48" ]; then
				echo "Incorrect fingerprint (was: '$NEW_FINGERPRINT', expected '$OLD_FINGERPRINT')!" 1>&2
				ok=no
			else
				# check if cert was signed with this public key
				OLD_REMOTE_PUBKEY_SSH=$(tempfile -p.n)

				ssh-keygen -F $NICE_REMOTE_HOSTNAME -f $NICE_KNOWN_HOSTS | tail -n 1 | sed -e 's/.*ssh-rsa/ssh-rsa/' > $OLD_REMOTE_PUBKEY_SSH

				diff $TMP_NEW_REMOTE_PUBLIC_KEY_SSH $OLD_REMOTE_PUBKEY_SSH
				if [ $? -ne 0 ]; then
					echo "Received certificate was not signed by the correct public key!"
					ok=no
				fi
			fi
		fi
		rm -f $TMP_CREDENTIALS_FILE \
			$TMP_NEW_REMOTE_PUBLIC_KEY_SSL \
			$TMP_NEW_REMOTE_PUBLIC_KEY_SSH \
			$TMP_RECEIVED

		if [ $ok = "no" ]; then
			exit 1
		fi


		if [ -z "$NICE_REMOTE_CRT" ]; then
			NICE_REMOTE_CRT=`tempfile -s.pub -p.nice`
		fi
		cat $TMP_NEW_REMOTE_CERTIFICATE > $NICE_REMOTE_CRT
	fi

	if [ $? -ne 0 ]; then
		exit 1
	fi

	shift
done

exit 0
