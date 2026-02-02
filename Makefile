PROTOCOLS = /usr/share/wayland-protocols

fueguito: fuego.cpp xdg-shell-client-protocol.h xdg-shell-protocol.o
	g++ -g -o fueguito fuego.cpp xdg-shell-protocol.o $(shell pkg-config --libs --cflags wayland-client xkbcommon)

xdg-shell-protocol.o: xdg-shell-protocol.c
	gcc -c -o xdg-shell-protocol.o xdg-shell-protocol.c -lwayland-client -lrt

xdg-shell-protocol.c: $(PROTOCOLS)/stable/xdg-shell/xdg-shell.xml
	wayland-scanner private-code $< $@
	
.PHONY: clean
clean:
	rm fueguito xdg-shell-protocol.o xdg-shell-protocol.c
