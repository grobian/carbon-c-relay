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

export DYLD_FORCE_FLAT_NAMESPACE=1
export DYLD_INSERT_LIBRARIES=../.libs/libfaketime.dylib
export LD_PRELOAD=../.libs/libfaketime.so

buftest_generate() {
	i=1
	end=1500
	rm -f buftest.payload buftest.payloadout
	while [ $i -le $end ]; do
		echo "foo.bar.${i} 1 349830001" >> buftest.payloadout
		i=$(($i+1))
	done
	type gzip > /dev/null && gzip -c < buftest.payloadout > buftest.payload.gz
	type lz4 > /dev/null && lz4 -c < buftest.payloadout > buftest.payload.lz4
	ln -s buftest.payloadout buftest.payload.gzout
	ln -s buftest.payloadout buftest.payload.lz4out
}


large_generate() {
	i=1
	end=1500
	rm -f large.payload large.payloadout
	while [ $i -le $end ]; do
		echo "foo.bar.${i} 1 349830001" >> large.payload
		i=$(($i+1))
	done
	ln -sf large.payload large.payloadout
}

large_ssl_generate() {
	i=1
	end=1500
	rm -f large-ssl.payload large-ssl.payloadout
	while [ $i -le $end ]; do
		echo "ssl.foo.bar.${i} 1 349830001" >> large-ssl.payload
		echo "through-ssl.foo.bar.${i} 1 349830001" >> large-ssl.payloadout
		i=$(($i+1))
	done
}

large_gzip_generate() {
	i=1
	end=1500
	rm -f large-gzip.payload large-gzip.payloadout
	while [ $i -le $end ]; do
		echo "gzip.foo.bar.${i} 1 349830001" >> large-gzip.payload
		echo "through-gzip.foo.bar.${i} 1 349830001" >> large-gzip.payloadout
		i=$(($i+1))
	done
}

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

run_servertest() {
	local confarg=$1
	local payload=$2
	local transport=$3

	local mode=SINGLE
	local tmpdir=$(mktemp -d)
	local output=
	local pidfile=
	local unixsock=
	local port=
	local output2=
	local pidfile2=
	local port2=
	local dataout="${tmpdir}"/data.out
	local payloadexpect="${payload}out"
	local test=${confarg%.*}
	local confarg2=${test}-2.${confarg##*.}

	if [[ -n "${transport}" && \
		"${transport}" != "gzip" && "${transport}" != "lz4" ]] ;
	then
		echo "unsupported transport ${transport}"
		return 1
	fi

	[[ -e ${confarg2} ]] && mode=DUAL

	local start_server_result
	local start_server_lastport=3020  # TODO
	start_server() {
		local id=$1
		local remoteport=$2
		local confarg=$3
		local transport="$4"
		# determine a free port to use
		local port=${start_server_lastport}  # TODO
		: $((start_server_lastport++))  # TODO
		local unixsock="${tmpdir}/sock.${port}"
		local cert="${test}.cert"
		local ca="${test}.cert"
		local conf="${tmpdir}"/conf
		local output="${tmpdir}"/relay-${id}.out
		local pidfile="${tmpdir}"/pidfile-${id}

		local relayargs=
		[[ -e ${test}.args ]] && relayargs=$(< ${test}.args)
		[[ -e ${test}-${id}.args ]] && relayargs=$(< ${test}-${id}.args)
		[[ -e ${ca} ]] && relayargs+=" -C ${ca}"

		if [ "${transport}" != "" ]; then
			transport="transport ${transport}"
		fi

		# write config file with known listener
		{
			echo "# relay ${id}, mode ${mode}"
			echo "listen type linemode ${transport}"
			echo "    ${unixsock} proto unix"
			echo "    ;"
			echo
			echo "cluster default"
			echo "    file ${dataout}"
			echo "    ;"
			echo
			if [[ -n ${relayargs} ]] ; then
				echo "# extra arguments given to ${EXEC}:"
				echo "#   ${relayargs}"
				echo
			fi
			echo "# contents from ${confarg} below this line"
			sed \
				-e "s/@port@/${port}/g" \
				-e "s/@remoteport@/${remoteport}/g" \
				-e "s/@cert@/${cert}/g" \
				"${confarg}"
		} > "${conf}"

		${EXEC} -d -w 1 -f "${conf}" -Htest.hostname -s -D \
			-l "${output}" -P "${pidfile}" ${relayargs}
		if [[ $? != 0 ]] ; then
			# hmmm
			echo "failed to start relay ${id} in ${PWD}:"
			echo ${EXEC} -d -f "${conf}" -Htest.hostname -s -D -l \
				"${output}" -P "${pidfile}" ${relayargs}
			echo "=== ${conf} ==="
			cat "${conf}"
			echo "=== ${output} ==="
			cat "${output}"
			return 1
		fi
		echo -n "relay ${id} "

		start_server_result=( ${port} ${unixsock} ${pidfile} ${output} )
	}

	echo -n "${test}: "
	[[ "${transport}" != "" ]] && echo -n "${transport} "

	if [[ "${HAVE_GZIP}" == "0" ]] ; then
		if grep -qE '^listen type linemode transport gzip ' "${confarg}" ; then
			echo "SKIP"
			return 0
		fi
	fi
	if [[ "${HAVE_LZ4}" == "0" ]] ; then
		if grep -qE '^listen type linemode transport lz4 ' "${confarg}" ; then
			echo "SKIP"
			return 0
		fi
	fi

	start_server 1 "" ${confarg} "${transport}" || return 1
	port=${start_server_result[0]}
	unixsock=${start_server_result[1]}
	pidfile=${start_server_result[2]}
	output=${start_server_result[3]}
	if [[ ${mode} == DUAL ]] ; then
		if ! start_server 2 ${port} ${confarg2} "" ; then
			kill -KILL $(< ${pidfile})
			return 1
		fi
		port2=${start_server_result[0]}
		unixsock=${start_server_result[1]}
		pidfile2=${start_server_result[2]}
		output2=${start_server_result[3]}
	fi

	${SMEXEC} "${unixsock}" < "${payload}"
	if [[ $? != 0 ]] ; then
		# hmmm
		echo "failed to send payload"
		return 1
	fi
	# allow everything to be processed
	sleep 2

	# kill and wait for relay to come down
	local pids=$(< "${pidfile}")
	[[ ${mode} == DUAL ]] && pids+=" $(< "${pidfile2}")"
	kill ${pids}
	local i=10
	while [[ ${i} -gt 0 ]] ; do
		ps -p ${pids} >& /dev/null || break
		echo -n "."
		sleep 1
		: $((i--))
	done
	# if it didn't yet die, make it so
	[[ ${i} == 0 ]] && kill -KILL ${pids}

	# add errors to the mix
	sed -n 's/^.*(ERR)/relay 1:/p' ${output} >> "${dataout}"
	[[ -n ${output2} ]] && \
		sed -n 's/^.*(ERR)/relay 2:/p' ${output2} >> "${dataout}"

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

	if [[ -n ${RUN_TEST_DROP_IN_SHELL} ]] ; then
		echo "dropping shell in ${tmpdir}"
		( unset DYLD_FORCE_FLAT_NAMESPACE DYLD_INSERT_LIBRARIES LD_PRELOAD;
		  cd ${tmpdir} && ${SHELL} )
	fi

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

echo -n "generating datasets ..."
buftest_generate
large_generate
large_ssl_generate
large_gzip_generate
echo " done"

${EXEC} -v | grep -w gzip >/dev/null && HAVE_GZIP=1 || HAVE_GZIP=0
${EXEC} -v | grep -w lz4 >/dev/null && HAVE_LZ4=1 || HAVE_LZ4=0

tstcnt=0
tstfail=0
for t in $* ; do
	if [[ -e ${t}.tst ]] ; then
		: $((tstcnt++))
		run_configtest "${EFLAGS}" "${t}.tst" || : $((tstfail++))
	elif [[ -e ${t}.dbg ]] ; then
		: $((tstcnt++))
		run_configtest "${EFLAGS} -d" "${t}.dbg" || : $((tstfail++))
	else
		if [[ -e ${t}.stst ]] ; then
			: $((tstcnt++))
			run_servertest "${t}.stst" "${t}.payload" || : $((tstfail++))
		fi
		if [[ -e ${t}.gz.stst \
			&& -e ${t}.payload.gz \
			&& "${HAVE_GZIP}" == "1" ]]
		then
			: $((tstcnt++))
			run_servertest "${t}.gz.stst" "${t}.payload.gz" \
				"gzip" || : $((tstfail++))
		fi
		if [[ -e ${t}.lz4.stst \
			&& -e ${t}.payload.lz4 \
			&& "${HAVE_LZ4}" == "1" ]]
		then
			: $((tstcnt++))
			run_servertest "${t}.lz4.stst" "${t}.payload.lz4" \
				"lz4" || : $((tstfail++))
		fi
	fi
done

rm -f buftest.payload.{gz,lz4} buftest.payload{,.gz,.lz4}out \
	large.payload large.payloadout large-ssl.payload \
	large-ssl.payloadout large-gzip.payload large-gzip.payloadout

echo "Ran ${tstcnt} tests with ${tstfail} failing"
exit ${tstfail}
