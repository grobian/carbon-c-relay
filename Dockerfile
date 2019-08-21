FROM alpine:3.10 AS builder

ADD . /opt/carbon-c-relay-build
WORKDIR /opt/carbon-c-relay-build

RUN \
  apk add --no-cache git bc build-base curl automake autoconf && \
  ./configure && make

FROM alpine:3.10

MAINTAINER Fabian Groffen

RUN mkdir -p /etc/carbon-c-relay

COPY --from=builder /opt/carbon-c-relay-build/relay /usr/bin/carbon-c-relay

EXPOSE 2003

ENTRYPOINT ["carbon-c-relay", "-f", "/etc/carbon-c-relay/carbon-c-relay.conf"]

