# compile with HYPRLAND_HEADERS=<path_to_hl> make all
# make sure that the path above is to the root hl repo directory, NOT src/
# and that you have ran `make protocols` in the hl dir.

WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

CXXFLAGS=-shared -Wall -fPIC --no-gnu-unique -I "${HYPRLAND_HEADERS}" -I "/usr/include/pixman-1" -I "/usr/include/libdrm" -std=c++23 

OBJS=riverLayout.o RiverLayoutProtocolManager.o river-layout-v3-protocol.o main.o

river-layout-v3-protocol.h: protocol/river-layout-v3.xml
	$(WAYLAND_SCANNER) server-header protocol/river-layout-v3.xml $@

river-layout-v3-protocol.c: protocol/river-layout-v3.xml
	$(WAYLAND_SCANNER) public-code protocol/river-layout-v3.xml $@


river-layout-v3-protocol.o: river-layout-v3-protocol.h

protocol: river-layout-v3-protocol.o


all: protocol $(OBJS) 
	g++ -shared -fPIC -o riverLayoutPlugin.so -g $(OBJS) 

clean:
	rm ./riverLayoutPlugin.so
	rm $(OBJS)
	rm river-layout-v3-protocol.h
	rm river-layout-v3-protocol.c
