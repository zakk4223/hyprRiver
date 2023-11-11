PLUGIN_NAME=riverLayoutPlugin
INSTALL_LOCATION=${HOME}/.local/share/hyprload/plugins/bin

WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

CXXFLAGS=-shared -Wall -fPIC --no-gnu-unique -g `pkg-config --cflags pixman-1 libdrm hyprland` -std=c++2b 

OBJS=riverLayout.o RiverLayoutProtocolManager.o river-layout-v3-protocol.o main.o

river-layout-v3-protocol.h: protocol/river-layout-v3.xml
	$(WAYLAND_SCANNER) server-header protocol/river-layout-v3.xml $@

river-layout-v3-protocol.c: protocol/river-layout-v3.xml
	$(WAYLAND_SCANNER) public-code protocol/river-layout-v3.xml $@


river-layout-v3-protocol.o: river-layout-v3-protocol.h

protocol: river-layout-v3-protocol.o


all: protocol $(OBJS) 
	g++ -shared -fPIC -o $(PLUGIN_NAME).so -g $(OBJS) 

install: all
	mkdir -p ${INSTALL_LOCATION}
	cp $(PLUGIN_NAME).so ${INSTALL_LOCATION}

clean:
	rm ./$(PLUGIN_NAME).so
	rm $(OBJS)
	rm river-layout-v3-protocol.h
	rm river-layout-v3-protocol.c
