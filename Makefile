all: biome-thingy chunk-save-logger

biome-thingy: biome-thingy.o nbt.o utils.o chunk.o zooms.o image.o event_loop.o region.o inotify.o
	$(CXX) -o $@ $^ -Wall -lz -lpng -g

chunk-save-logger: chunk-save-logger.o inotify.o region.o
	$(CXX) -o $@ $^ -Wall -g
%.o: %.cpp
	$(CXX) -o $@ $< -Wall -c -g -std=c++20

%.deps.o: %.cpp
	$(CXX) -o $@ -M $<

biome-thingy.o: biome-thingy.cpp nbt.h utils.h chunk.h section.h zooms.h image.h event_loop.h

chunk-save-logger.o: chunk-save-logger.cpp inotify.h region.h

nbt.o: nbt.cpp nbt.h
utils.o: utils.cpp utils.h image.h
chunk.o: chunk.cpp chunk.h section.h
zooms.o: zooms.cpp zooms.h utils.h image.h
image.o: image.cpp image.h
event_loop.o: event_loop.cpp event_loop.h region.h inotify.h
region.o: region.cpp region.h
inotify.o: inotify.cpp
