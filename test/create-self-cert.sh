#!/bin/sh

openssl req -nodes -newkey rsa:2048 -keyout key.pem -out cert.csr \
	-subj "/C=NL/ST=NH/L=Amsterdam/O=carbon-c-relay/CN=relay" < /dev/null
openssl x509 -in cert.csr -out cert.pem -req -signkey key.pem -days 1001 && \
	cat key.pem>>cert.pem
test -s cert.pem && cp cert.pem ssl-dual.cert
rm -f key.pem cert.pem cert.csr privkey.pem 
