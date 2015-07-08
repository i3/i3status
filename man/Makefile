all: i3status.1

A2X?=a2x
A2X_FLAGS=

i3status.1: asciidoc.conf i3status.man
	${A2X} -f manpage --asciidoc-opts="-f asciidoc.conf" ${A2X_FLAGS} i3status.man
clean:
	rm -f i3status.xml i3status.1 i3status.html
