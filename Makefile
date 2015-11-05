# Copyright 2013-2015 Fabian Groffen
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


CFLAGS ?= -O2 -Wall
# if your compiler doesn't support OpenMP, comment out this line, or
# define OPENMP_FLAGS to be empty
OPENMP_FLAGS ?= -fopenmp
override CC += $(OPENMP_FLAGS)

GIT_VERSION := $(shell git describe --abbrev=6 --dirty --always || date +%F)
GVCFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"

override CFLAGS += $(GVCFLAGS) -pthread

SOCKET_LIBS =
ifeq ($(shell uname), SunOS)
SOCKET_LIBS += -lsocket  -lnsl
endif

# should be accepted sort of anywhere
MATH_LIBS = -lm

override LIBS += $(SOCKET_LIBS) $(MATH_LIBS) -pthread

OBJS = \
	relay.o \
	md5.o \
	consistent-hash.o \
	receptor.o \
	dispatcher.o \
	router.o \
	queue.o \
	server.o \
	collector.o \
	aggregator.o

relay: $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

VERSION = $(shell sed -n '/VERSION/s/^.*"\([0-9.]\+\)".*$$/\1/p' relay.h)
dist:
	git archive \
		--format=tar.gz \
		--prefix=carbon-c-relay-$(VERSION)/ v$(VERSION) \
		> carbon-c-relay-$(VERSION).tar.gz

clean:
	rm -f *.o relay
