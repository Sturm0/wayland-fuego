PROTOCOLS = /usr/share/wayland-protocols

fueguito: fuego.cpp xdg-shell-client-protocol.h xdg-shell-protocol.o xdg-decoration-client.h xdg-decoration-client.o
	g++ -g -o fueguito fuego.cpp xdg-shell-protocol.o xdg-decoration-client.o $(shell pkg-config --libs --cflags wayland-client xkbcommon)

xdg-shell-protocol.o: xdg-shell-protocol.c
	gcc -c -o xdg-shell-protocol.o xdg-shell-protocol.c -lwayland-client -lrt

xdg-shell-protocol.c: $(PROTOCOLS)/stable/xdg-shell/xdg-shell.xml
	wayland-scanner private-code $< $@
	
xdg-shell-client-protocol.h: $(PROTOCOLS)/stable/xdg-shell/xdg-shell.xml
	wayland-scanner client-header $< $@

xdg-decoration-client.o: xdg-decoration-client.c
	gcc -c -o $@ xdg-decoration-client.c $(shell pkg-config --libs --cflags wayland-client)

xdg-decoration-client.c: $(PROTOCOLS)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
	wayland-scanner private-code $< $@

xdg-decoration-client.h: $(PROTOCOLS)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
	wayland-scanner client-header $< $@
	
.PHONY: clean
clean:
	rm fueguito xdg-shell-protocol.o xdg-shell-protocol.c
