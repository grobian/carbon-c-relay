#!/usr/bin/env bash
# Copyright 2013-2018 Fabian Groffen
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

EXEC=../relay
SMEXEC=../sendmetric
EFLAGS="-Htest.hostname -t"
DIFF="diff -Nu"
POST="cat"
CNFCLN=( sed -e '/^configuration:/,/^parsed configuration follows:/d'
             -e '/starting carbon-c-relay v/d'
             -e 's/^\[[0-9][0-9\-]* [0-9][0-9:]*\] //'
             -e 's/_stub_[0-9a-fx][0-9a-fx]*__/_stub_0xc0d3__/')

run_configtest() {
	local eflags="$1"
	local test=${2%.*}
	local conf="${test}.conf"
	local tdiff

	[[ -e ${conf} ]] || conf="../issues/${conf}"
	echo -n "${test}: "
	tdiff=$(cat ${2} \
		| ( ${EXEC} ${eflags} -f "${conf}" ; trigger_bash_segv_print=$?) 2>&1 \
		| "${CNFCLN[@]}" \
		| ${DIFF} "${test}.out" - --label "${test}.out" --label "${test}.out" \
		| ${POST} \
		; exit ${PIPESTATUS[3]})
	if [[ $? == 0 ]] ; then
		echo "PASS"
		return 0
	else
		echo "FAIL"
		echo "${tdiff}"
		return 1
	fi
}

run_singleservertest() {
	local tmpdir=$(mktemp -d)
	local output="${tmpdir}"/relay.out
	local pidfile="${tmpdir}"/pidfile
	local conf="${tmpdir}"/conf
	local dataout="${tmpdir}"/data.out
	local confarg=$1
	local payload=$2
	local payloadexpect="${payload}out"
	local test=${confarg%.*}

	# determine a free port to use
	local port=3020  # TODO
	local unixsock="${tmpdir}/sock.${port}"

	# write config file with known listener
	{
		echo "listen type linemode"
		echo "    ${unixsock} proto unix"
		echo "    ;"
		echo
		echo "cluster default"
		echo "    file ${dataout}"
		echo "    ;"
		echo
		echo "# contents from ${confarg} below this line"
		cat "${confarg}"
	} > "${conf}"

	echo -n "${test}: "
	${EXEC} -f "${conf}" -Htest.hostname -s -D -l "${output}" -P "${pidfile}"
	if [[ $? != 0 ]] ; then
		# hmmm
		echo "failed to start relay"
		return 1
	fi

	${SMEXEC} "${unixsock}" < "${payload}"
	if [[ $? != 0 ]] ; then
		# hmmm
		echo "failed to send payload"
		return 1
	fi

	# kill and wait for relay to come down
	local pid=$(< "${pidfile}")
	kill ${pid}
	local i=10
	while [[ ${i} -gt 0 ]] ; do
		sleep 1
		ps -p ${pid} >& /dev/null || break
		echo -n "."
		: $((i--))
	done
	# if it didn't yet die, make it so
	[[ ${i} == 0 ]] && kill -KILL ${pid}

	# compare some notes
	local ret
	tdiff=$(${DIFF} "${payloadexpect}" "${dataout}" \
			--label "${payloadexpect}" --label "${payloadexpect}" \
		| ${POST} \
		; exit ${PIPESTATUS[0]})
	if [[ $? == 0 ]] ; then
		echo "PASS"
		ret=0
	else
		echo "FAIL"
		echo "${tdiff}"
		ret=1
	fi

#	echo "dropping shell in ${tmpdir}"
#	( cd ${tmpdir} && bash )

	# cleanup
	rm -Rf "${tmpdir}"

	return ${ret}
}

while [[ -n $1 ]] ; do
	case "$1" in
		--approve|-a)
			POST="patch"
			;;
		--)
			shift
			break
			;;
		*)
			break
			;;
	esac
	shift
done

tstcnt=0
tstfail=0
for t in $* ; do
	if [[ -e ${t}.tst ]] ; then
		: $((tstcnt++))
		run_configtest "${EFLAGS}" "${t}.tst" || : $((tstfail++))
	elif [[ -e ${t}.dbg ]] ; then
		: $((tstcnt++))
		run_configtest "${EFLAGS} -d" "${t}.dbg" || : $((tstfail++))
	elif [[ -e ${t}.stst ]] ; then
		: $((tstcnt++))
		run_singleservertest "${t}.stst" "${t}.payload" || : $((tstfail++))
	fi
done
echo "Ran ${tstcnt} tests with ${tstfail} failing"
exit ${tstfail}
