#!/bin/sh

set -e
set -x

TOOLSPATH=`realpath ${0}`
TOOLSDIR=`dirname ${TOOLSPATH}`

. "${TOOLSDIR}/test.common.sub"

MLOG="${BUILDDIR}/monitor.log"
TESTS="100trying ACK OPTIONS"

cd "${SRCDIR}"
PATH="${PATH}:${IDF_TOOLCHAIN}/bin" python "${TOOLSDIR}/ptyrun.py" -o "${MLOG}" \
  ${IDF_PATH}/tools/idf.py monitor &
MON_RC=${?}
MON_PID=${!}
i=0
while [ ${i} -lt 10 ]
do
  BRD_IP=`grep 'sta ip: ' "${MLOG}" | sed 's|.*sta ip: ||; s|,.*||'`
  if [ ! -z "${BRD_IP}" ]
  then
    break
  fi
  sleep 1
  i=$((${i} + 1))
done
if [ ! -z "${BRD_IP}" ]
then
  for tst in ${TESTS}
  do
    REQFILE="${BUILDDIR}/${tst}.req"
    RESFILE="${BUILDDIR}/${tst}.res"
    sed "s|%%BRD_IP%%|${BRD_IP}|g" "${TOOLSDIR}/${tst}.raw" > "${REQFILE}"
    nc -w 1 -u "${BRD_IP}" 5060 < "${REQFILE}" > "${RESFILE}"
  done
  sleep 3
fi
kill -TERM ${MON_PID}
wait ${MON_PID} || true
if [ -z "${BRD_IP}" ]
then
  exit 1
fi
grep -q 'Waiting for data' "${MLOG}"
grep -q 'SIP/2.0 100 Trying' "${MLOG}"
for tst in ${TESTS}
do
  REQFILE="${BUILDDIR}/${tst}.req"
  RESFILE="${BUILDDIR}/${tst}.res"
  ${DIFF} "${REQFILE}" "${RESFILE}"
done
