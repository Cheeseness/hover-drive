CXXFLAGS:=$(CXXFLAGS) -std=c++11 `sdl2-config --cflags`
LDFLAGS:=$(LDFLAGS) `sdl2-config --libs` -lSDL2_ttf -lSDL2_mixer -lGLEW -lGL -lGLU -lm 

drive: drive.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) drive.cpp -o drive 

clean:
	rm -f drive

.PHONY: clean
