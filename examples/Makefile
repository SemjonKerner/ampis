CC=gcc
CFLAGS=-Wall -g -lasound

alsarawmidiin: alsarawmidiin.c
	$(CC) $< $(CFLAGS) -o $@

alsarawmidiout: alsarawmidiout.c
	$(CC) $< $(CFLAGS) -o $@

alsarawportlist: alsarawportlist.c
	$(CC) $< $(CFLAGS) -o $@

midiecho: midiecho.c
	$(CC) $< $(CFLAGS) -o $@

midiinthread: midiinthread.c
	$(CC) $< $(CFLAGS) -lpthread -o $@

nonblockinginput: nonblockinginput.c
	$(CC) $< $(CFLAGS) -o $@

clk: clk.c
	$(CC) $< $(CFLAGS) -o $@

all: alsarawmidiin alsarawmidiout alsarawportlist midiecho midiinthread	nonblockinginput clk

clean:
	-rm alsarawmidiin
	-rm alsarawmidiout
	-rm alsarawportlist
	-rm midiecho
	-rm midiinthread
	-rm nonblockinginput
	-rm clk
