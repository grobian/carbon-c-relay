# ------------------------------------------------
# Generic Makefile
#
# Author: jriguera@gmail.com
# Date  : 2014-10-12
#
# ------------------------------------------------

# project name (generate executable with this name)
PREFIX           = /usr/local
TARGET           = carbon-c-relay
GIT_VERSION     := $(shell git describe --abbrev=6 --dirty --always || date +%F)
GVCFLAGS        += -DGIT_VERSION=\"$(GIT_VERSION)\"

CC               = gcc
# compiling flags here
CFLAGS          ?= -O2 -Wall
override CFLAGS += $(GVCFLAGS) `pkg-config openssl --cflags` -pthread

LINKER           = gcc -o
# linking flags here
override LIBS   += `pkg-config openssl --libs` -pthread
ifeq ($(shell uname), SunOS)
override LIBS   += -lsocket  -lnsl
endif
LFLAGS           = -O2 -Wall -lm $(LIBS)


# change these to set the proper directories where each files shoould be
SRCDIR   = src
OBJDIR   = obj
BINDIR   = bin

SOURCES  := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(SRCDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RM       = rm -f
MD       = mkdir -p
GIT      = git
# Debian
# dh_make --native -c apache -s -p carbon-c-relay_0.35

all: folders $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJECTS)
	$(LINKER) $@ $(OBJECTS) $(LFLAGS)
	@echo "Linking complete. Binary file created!"

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"

.PHONY: folders
folders:
	@$(MD) $(BINDIR) 
	@$(MD) $(OBJDIR)

.PHONY: install
install: $(BINDIR)/$(TARGET)
	install -m 0755 $@ $(PREFIX)/$(BINDIR)/$(TARGET)

.PHONY: install
uninstall:
	$(RM) $(PREFIX)/$(BINDIR)/$(TARGET)


VERSION = $(shell sed -n '/VERSION/s/^.*"\([0-9.]\+\)".*$$/\1/p' $(SRCDIR)/relay.h)
.PHONY: dist
dist:
	@$(GIT) archive \
		--format=tar.gz \
		--prefix=$(TARGET)-$(VERSION)/ v$(VERSION) \
		> $(TARGET)-$(VERSION).tar.gz
	@echo "Created "$(TARGET)-$(VERSION).tar.gz" successfully!"


# check if the local environment is suitable to generate a package
# we check environment variables and a gpg private key matching
# these variables. this is necessary as we sign our packages.
.PHONY: checkenv
checkenv:
ifndef DEBEMAIL
	echo "Missing environment variable DEBEMAIL"
	@exit 1
endif
ifndef DEBFULLNAME
	echo "Missing environment variable DEBFULLNAME"
	@exit 1
endif
	gpg --list-secret-keys "$(DEBFULLNAME) <$(DEBEMAIL)>" >/dev/null

# creates the .deb package and other related files
# all files are placed in ../
.PHONY: builddeb
builddeb: checkenv
	dpkg-buildpackage -i '-Itmp' -I.git -I$(TARGET)-$(VERSION).tar.gz -rfakeroot


# create a new release based on PW_VERSION variable
.PHONY: newrelease
newrelease:
	debchange --changelog debian/changelog --urgency high --newversion $(VERSION)-1 "Releasing $(TARGET) $(VERSION)"

# creates a new version in debian/changelog
.PHONY: newversion
newversion:
	debchange --changelog debian/changelog -i --urgency high

# allow user to enter one or more changelog comment manually
.PHONY: changelog
changelog:
	debchange --changelog debian/changelog --force-distribution dist --urgency high -r
	debchange --changelog debian/changelog -a 

.PHONY: clean
clean:
	@$(RM) $(OBJECTS)
	@echo "Cleanup complete!"

.PHONY: dist-clean
dist-clean: clean
	@$(RM) $(BINDIR)/$(TARGET)
	@echo "Executable removed!"

