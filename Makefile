CXXFLAGS=-I/Library/Frameworks/SDL.framework/Headers

all: hilbert

hilbert: SDLMain.o hilbert.o
	g++ $^ -framework SDL -framework Cocoa -o hilbert

hilbert.o: hilbert.cpp
	g++ -Wunused-variable -Wall $(CXXFLAGS) -c $< -o $@

SDLMain.o: SDLMain.m
	g++ $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o hilbert
