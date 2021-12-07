FROM alpine:3.15 AS builder

ADD . /opt/carbon-c-relay-build
WORKDIR /opt/carbon-c-relay-build

RUN \
  apk add --no-cache git bc build-base curl automake autoconf libtool && \
  ./configure && make

FROM alpine:3.15

MAINTAINER Fabian Groffen

RUN mkdir /etc/carbon-c-relay

COPY --from=builder /opt/carbon-c-relay-build/relay /usr/bin/carbon-c-relay

EXPOSE 2003

ENTRYPOINT ["/usr/bin/carbon-c-relay", "-f", "/etc/carbon-c-relay/carbon-c-relay.conf"]

