# -g - create debug flags for debuging using gdb
# -lz - to link(-l) zlib.h(z) library
run:
	g++ -g -std=c++20 src/server.cpp -o server -lz && ./server --directory ./  
clean:
	rm server