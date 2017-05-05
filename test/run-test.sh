#!/usr/bin/env bash

# Copyright 2013-2017 Fabian Groffen
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
EFLAGS="-Htest.hostname -t"
DIFF="diff -Nu"
POST="cat"
CNFCLN=( sed -e '/^configuration:/,/^parsed configuration follows:/d'
             -e '/starting carbon-c-relay v/d'
             -e 's/^\[[0-9][0-9\-]* [0-9][0-9:]*\] //'
             -e 's/_stub_[0-9a-fx][0-9a-fx]*__/_stub_0xc0d3__/')

run_test() {
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
		run_test "${EFLAGS}" "${t}.tst" || : $((tstfail++))
	elif [[ -e ${t}.dbg ]] ; then
		: $((tstcnt++))
		run_test "${EFLAGS} -d" "${t}.dbg" || : $((tstfail++))
	fi
done
echo "Ran ${tstcnt} tests with ${tstfail} failing"
exit ${tstfail}
