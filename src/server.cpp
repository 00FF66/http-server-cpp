#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <string>

// comment 

// TODO: refactor to parsing to object instead of vector
std::vector<std::string> ParseRequestBuffer(const char* buffer) {
  std::vector<std::string> result;
  std::string word;

  for (int i = 0; buffer[i] != '\0'; ++i) {
    if (buffer[i] == ' ' || buffer[i] == '\r' || buffer[i] == '\n') {
      if (!word.empty()) {
        result.push_back(word);
        word.clear();
      }
    } else {
      word += buffer[i];
    }
  }
  if (!word.empty()) {
    result.push_back(word);
  }

  return result;
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  // AF_INET - specify IPv4
  // SOCK_STREAM - defines TCP socket
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  

  struct sockaddr_in server_addr;   // sockaddr_in - data type to store socket data
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY; // make it listen to any available IP
  // htons - convert unsigned int from machine byte order to network byte order
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for a client to connect...\n";
  
  int rcvsocket = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
  std::cout << "Client connected\n";

  char buffer[1024] = {0};
  recv(rcvsocket, buffer, sizeof(buffer), 0);
  std::cout << "Client message:" << buffer << std::endl;
  
  std::vector<std::string> parsed_request = ParseRequestBuffer(buffer);
  const char* response_200 = "HTTP/1.1 200 OK\r\n\r\n";
  const char* response_404 = "HTTP/1.1 404 Not Found\r\n\r\n";
  const char* response = response_200;
  if (parsed_request[1] != "/") {
    response = response_404;
  }

  ssize_t bytes_send = send(rcvsocket, response, strlen(response), 0);

  if (bytes_send > 0) {
    std::cout << "Response sent: " << response << std::endl;
  } else {
    std::cout << "Error sending response";
  }


  close(rcvsocket);
  close(server_fd);

  return 0;
}
