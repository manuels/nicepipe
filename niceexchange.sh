#!/bin/sh

# upload data to some provider where somebody else can find it ;)

ISCALLER=$1
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
	./exchange_providers/${PROVIDER} ${ISCALLER} ${MODE} ${OPTIONS}
	if [ $? -ne 0 ]; then
		exit 1
	fi

	shift
done

exit 0
