#!/bin/sh

set -e

if [ -z "${IDF_PATH}" ]
then
  echo "IDF_PATH needs to be set" >&2
  exit 1
fi

TOOLSPATH=`realpath ${0}`
TOOLSDIR=`dirname ${TOOLSPATH}`

cd ${TOOLSDIR}/../src
python ${IDF_PATH}/tools/idf.py monitor > monitor.log&
MON_RC=${?}
MON_PID=${!}
sleep 20
kill -TERM ${MON_PID}
wait ${MON_PID}
cat monitor.log
