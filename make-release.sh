#!/usr/bin/env bash

die() {
	echo $* > /dev/stderr
	exit 1
}

# find current release according to configure.ac
CURRELEASE=$(sed -n '/^AC_INIT/s/^.*\[\([0-9.]\+\)\].*$/\1/p' configure.ac)
[[ -z ${CURRELEASE} ]] && die "no release found from configure.ac"

# check this is the last release in the ChangeLog
CLRELEASE=$(sed -n 's/^# \([0-9.]\+\) ([0-9-]\{10\})/\1/p' ChangeLog.md \
	| head -n1)
[[ -z ${CLRELEASE} ]] && die "could not get latest release from ChangeLog.md"
[[ ${CURRELEASE} != ${CLRELEASE} ]] && \
	die "configure.ac ($CURRELEASE) and ChangeLog.md (${CLRELEASE}) disagree"

# prepare new version and release date
TODAY=$(date +%F)
EUTODAY=$(date +%d-%m-%Y)
if [[ $1 == "-M" || $1 == "--major" ]] ; then
	# major version bump
	NEXTRELEASE="$(( ${CURRELEASE%%.*} + 1 )).0"
	FUTURERELEASE="${NEXTRELEASE%.0}.1"
elif [[ $1 == "-B" || $1 == "--bugfix" ]] ; then
	# bugfix release
	MINOR=${CURRELEASE#*.}
	SUF=0
	if [[ ${MINOR} == *.* ]] ; then
		SUF=${MINOR#*.}
		MINOR=${MINOR%.*}
	fi
	NEXTRELEASE="${CURRELEASE%%.*}.${MINOR}.$(( ${SUF} + 1 ))"
	FUTURERELEASE="${CURRELEASE%%.*}.$(( ${MINOR} + 1 ))"
else
	# minor version bump
	MINOR=${CURRELEASE#*.}
	NEXTRELEASE="${CURRELEASE%%.*}.$(( ${MINOR%%.*} + 1 ))"
	FUTURERELEASE="${CURRELEASE%%.*}.$(( ${MINOR%%.*} + 2 ))"
fi

echo "Performing version bump from ${CURRELEASE} to ${NEXTRELEASE} (${TODAY})"

# prepare modifications to configure.ac and ChangeLog.md
CHANGES=.make-release-tmp.$$
mkdir "${CHANGES}"
trap "rm -Rf ${CHANGES}" EXIT

sed -e "/^AC_INIT/s:\[${CURRELEASE}\]:[${NEXTRELEASE}]:" \
	-e "/^AC_SUBST(\[RELEASEDATE\]/d" \
	-e "/^AC_INIT/a AC_SUBST([RELEASEDATE], [${TODAY}])" \
	-e "/^AM_MAINTAINER_MODE/s:\[enable\]:[disable\]:" \
	configure.ac \
	| diff -u \
		--label "${CURRELEASE}/configure.ac" \
		--label "${NEXTRELEASE}/configure.ac" \
		configure.ac - \
	>> "${CHANGES}"/srcdiffs

{
	echo "# ${FUTURERELEASE} (unreleased master branch)"
	echo
	echo "### New Features"
	echo
	echo "### Bugfixes"
	echo
	echo
	echo "# ${NEXTRELEASE} (${EUTODAY})"
	sed -e "1d" ChangeLog.md
}	| diff -u \
		--label "${CURRELEASE}/ChangeLog.md" \
		--label "${NEXTRELEASE}/ChangeLog.md" \
		ChangeLog.md - \
	>> "${CHANGES}"/srcdiffs

cat "${CHANGES}"/srcdiffs
read -p "do you want to apply these changes? [yN] " answ
[[ ${answ} == "y" ]] || die "aborting"

patch -p1 < "${CHANGES}"/srcdiffs

echo "regenerating build files"
autoreconf -fiv || die
echo "committing and tagging version bump"
git commit -am "version bump to ${NEXTRELEASE}" || die
git tag "v${NEXTRELEASE}" || die
echo "restoring maintainer-mode in configure.ac"
sed -i \
	-e "/^AM_MAINTAINER_MODE/s:\[disable\]:[enable\]:" \
	configure.ac || die
autoreconf -fiv || die
git commit -am "configure.ac: restoring maintainer mode"

echo "building release tar"
SRCDIR=${PWD}
mkdir "${CHANGES}"/build || die
pushd "${CHANGES}"/build || die
git clone "${SRCDIR}" carbon-c-relay
pushd carbon-c-relay
git checkout "v${NEXTRELEASE}" || die
libtoolize || glibtoolize  # get m4 macros for dist
./configure
make dist
mv carbon-c-relay-${NEXTRELEASE}.tar.* "${SRCDIR}"/ || die
popd || die
popd || die

RELEASETAR=$(echo carbon-c-relay-${NEXTRELEASE}.tar.*)
echo "please git push && git push --tags if ${RELEASETAR} is ok"
