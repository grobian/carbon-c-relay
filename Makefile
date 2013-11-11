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

GIT_VERSION := $(shell git describe --abbrev=6 --dirty --always)
GVCFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"

override CFLAGS += $(GVCFLAGS) `pkg-config openssl --cflags` -pthread

override LIBS += `pkg-config openssl --libs` -pthread

relay: relay.c carbon-hash.o receptor.o dispatcher.o router.o queue.o
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $^

clean:
	rm -f *.o relay
