all:
	gcc -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare -g -O2 -o wmiistatus wmiistatus.c
