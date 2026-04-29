SHELL = /bin/bash

all: hackode.dict

hackode.dict:
	cut -d $$'\t' -f1 \
		/usr/share/dictd/{devil,gcide,jargon,vera}.index \
	| sed -e 's/ /_/g' \
	| sort -u >$@

