MODULE = playback.so

all: ${MODULE}

.SUFFIXES: .cpp .so
.cpp.so:
	znc-buildmod $<

clean:
	rm -f ${MODULE}
