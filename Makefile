run:
	g++ -g -std=c++20 src/server.cpp -o server && ./server --directory ./
clean:
	rm server