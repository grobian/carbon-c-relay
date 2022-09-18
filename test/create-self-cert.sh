#!/bin/sh

openssl req -nodes -newkey rsa:2048 -keyout key.pem -out cert.csr \
	-subj "/C=NL/ST=NH/L=Amsterdam/O=carbon-c-relay/CN=relay" < /dev/null
openssl x509 -in cert.csr -out cert.pem -req -signkey key.pem -days 1001
if test -s cert.pem ; then
	cp cert.pem dual-ssl.cert
	cat key.pem >> dual-ssl.cert
	cp key.pem dual-mtls.key
	cp cert.pem dual-mtls.mcert
fi
rm -f key.pem cert.pem cert.csr privkey.pem 
