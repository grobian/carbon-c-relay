FROM alpine:3.6

MAINTAINER Fabian Groffen

RUN mkdir -p /opt/carbon-c-relay-build

RUN mkdir /etc/carbon-c-relay

COPY . /opt/carbon-c-relay-build

RUN \
  apk --no-cache update && \
  apk --no-cache upgrade && \
  apk --no-cache add git bc build-base curl && \
  cd /opt/carbon-c-relay-build && \
  ./configure; make && \
  cp relay /usr/bin/carbon-c-relay && \
  apk del --purge git bc build-base ca-certificates curl && \
  rm -rf /opt/* /tmp/* /var/cache/apk/* /opt/carbon-c-relay-build

EXPOSE 2003

ENTRYPOINT ["carbon-c-relay", "-f", "/etc/carbon-c-relay/carbon-c-relay.conf"]

