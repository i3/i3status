all: i3status.1

i3status.1: asciidoc.conf i3status.man
	a2x -f manpage --asciidoc-opts="-f asciidoc.conf" i3status.man
clean:
	rm -f i3status.xml i3status.1 i3status.html
