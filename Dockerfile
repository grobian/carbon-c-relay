FROM alpine:3.15 AS builder

ADD . /opt/carbon-c-relay-build
WORKDIR /opt/carbon-c-relay-build

RUN \
  apk add --no-cache build-base && \
  touch conffile.tab.c conffile.tab.h conffile.yy.c \
  configure.ac Makefile.am aclocal.m4 configure \
  Makefile.in config.h.in && \
  ./configure --disable-maintainer-mode && make

FROM alpine:3.15

MAINTAINER Fabian Groffen

RUN mkdir /etc/carbon-c-relay

COPY --from=builder /opt/carbon-c-relay-build/relay /usr/bin/carbon-c-relay

EXPOSE 2003

ENTRYPOINT ["/usr/bin/carbon-c-relay", "-f", "/etc/carbon-c-relay/carbon-c-relay.conf"]

