run:
	g++ -std=c++20 src/server.cpp -o server && ./server
clean:
	rm server