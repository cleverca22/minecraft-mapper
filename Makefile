all: biome-thingy chunk-save-logger

biome-thingy: biome-thingy.o nbt.o utils.o chunk.o zooms.o image.o event_loop.o region.o inotify.o
	$(CXX) -o $@ $^ -Wall -lz -lpng -g

chunk-save-logger: chunk-save-logger.o inotify.o region.o
	$(CXX) -o $@ $^ -Wall -g

%.o: %.cpp
	$(CXX) -o $@ $< -Wall -c -std=c++20 -g -O3 -fno-inline-functions

%.deps.o: %.cpp
	$(CXX) -o $@ -M $<

biome-thingy.o: biome-thingy.cpp nbt.h utils.h chunk.h section.h zooms.h image.h event_loop.h region.h
chunk-save-logger.o: chunk-save-logger.cpp inotify.h region.h
chunk.o: chunk.cpp chunk.h section.h
event_loop.o: event_loop.cpp event_loop.h region.h inotify.h
image.o: image.cpp image.h
inotify.o: inotify.cpp
nbt.o: nbt.cpp nbt.h
region.o: region.cpp region.h
utils.o: utils.cpp utils.h image.h
zooms.o: zooms.cpp zooms.h utils.h image.h
