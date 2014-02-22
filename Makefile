# This file is part of carbon-c-relay.
#
# carbon-c-relay is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# carbon-c-relay is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with carbon-c-relay.  If not, see <http://www.gnu.org/licenses/>.


CFLAGS ?= -O2 -Wall

GIT_VERSION := $(shell git describe --abbrev=6 --dirty --always || date +%F)
GVCFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"

override CFLAGS += $(GVCFLAGS) `pkg-config openssl --cflags` -pthread

SOCKET_LIBS =
ifeq ($(shell uname), SunOS)
SOCKET_LIBS += -lsocket  -lnsl
endif

override LIBS += `pkg-config openssl --libs` $(SOCKET_LIBS) -pthread

OBJS = \
	relay.o \
	carbon-hash.o \
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
