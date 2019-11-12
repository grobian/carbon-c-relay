# Copyright 2013-2019 Fabian Groffen
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

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

CFLAGS ?= -O2 -Wall -Wshadow -pipe

GIT_VERSION := $(shell git describe --abbrev=6 --dirty --always 2>/dev/null || echo $(RELEASEDATE))
GVCFLAGS = -DGIT_VERSION=\"$(GIT_VERSION)\"

override CFLAGS += $(DEF_EXTENSIONS) $(GVCFLAGS) -pthread

conffile.tab.c conffile.tab.h: conffile.y
	bison -d $^

conffile.yy.c: conffile.l conffile.tab.h
	flex -o $@ $<

router.c: conffile.tab.h

man:
	sed -e '/travis-ci.org\/grobian\/carbon-c-relay.svg/d' carbon-c-relay.md | \
	ronn \
		--manual="Graphite data collection and visualisation" \
		--organization=Graphite \
		--roff \
	> relay.1

bin_PROGRAMS = relay
check_PROGRAMS = sendmetric
lib_LTLIBRARIES = libfaketime.la
relay_LINK = $(CCLD) $(AM_LDFLAGS) $(LDFLAGS) -o $@
relay_LDADD = $(LIBGZIP) $(LIBLZ4) $(LIBSNAPPY) $(LIBSSL) \
			  $(LIBONIGURUMA) $(LIBPCRE2) $(LIBPCRE)
relay_SOURCES = \
	relay.c \
	md5.c \
	consistent-hash.c \
	receptor.c \
	dispatcher.c \
	conffile.tab.c \
	conffile.yy.c \
	allocator.c \
	router.c \
	queue.c \
	server.c \
	collector.c \
	aggregator.c \
	$(NULL)
sendmetric_LINK = $(CCLD) $(AM_LDFLAGS) $(LDFLAGS) -o $@
sendmetric_LDADD = $(LIBGZIP) $(LIBLZ4) $(LIBSNAPPY) $(LIBSSL)
sendmetric_SOURCES = sendmetric.c
libfaketime_la_SOURCES = faketime.c
noinst_HEADERS = \
	relay.h \
	md5.h \
	fnv1a.h \
	consistent-hash.h \
	conffile.h \
	conffile.tab.h \
	receptor.h \
	dispatcher.h \
	allocator.h \
	router.h \
	queue.h \
	server.h \
	collector.h \
	aggregator.h \
	posixregex.h \
	$(NULL)
man_MANS = relay.1

EXTRA_DIST = \
	issues \
	test \
	ChangeLog.md \
	LICENSE.md \
	carbon-c-relay.md \
	relay.1 \
	README.md \
	conffile.tab.c conffile.tab.h conffile.y \
	conffile.yy.c conffile.l \
	$(NULL)

CRTESTS = \
	issue10 \
	issue27 \
	issue117 \
	issue156 \
	issue157 \
	issue163 \
	issue165 \
	issue180 \
	issue184 \
	issue202 \
	issue213 \
	issue218 \
	issue228 \
	issue235 \
	issue236 \
	issue246 \
	issue252 \
	issue253 \
	issue263 \
	issue267 \
	issue288 \
	issue293 \
	issue310 \
	issue357 \
	issue369 \
	server-type \
	basic \
	metriclimits \
	buftest \
	large \
	dual-gzip \
	large-gzip \
	dual-large-gzip \
	dual-lz4 \
	large-lz4 \
	dual-large-lz4 \
	$(NULL)

if SSL_ENABLED
CRTESTS += dual-ssl large-ssl dual-large-ssl
endif

test: check
test-%: relay libfaketime.la sendmetric
	@( cd test && ./run-test.sh $* )
approve-%: relay sendmetric
	@( cd test && ./run-test.sh -a $* )
check-local:
	@( cd test && ./run-test.sh $(CRTESTS) )
install-exec-local:  # libtool refuses to build shared for check or noinst trg
	$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=uninstall \
		rm -f "$(DESTDIR)$(libdir)/libfaketime.la"
