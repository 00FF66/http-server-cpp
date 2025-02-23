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
#include <thread>

struct StatusLine {
  std::string protocol;
  std::string status_code;
  std::string status;

  std::string to_str() {
    return protocol + " " + status_code + " " + status + "\r\n";
  }
};

struct HeaderData {
  std::string content_type;
  std::string content_length;
  
  std::string to_str() {
    return "Content-Type: " + content_type + 
    "\r\nContent-Length: " + content_length + "\r\n\r\n";
  }
};

struct Response {
  StatusLine status;
  HeaderData header;
  std::string response_body;

  std::string to_str() {
    return status.to_str() + header.to_str() + response_body;
  }
};

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

std::vector<std::string> ParseURL(std::string url) {
  std::vector<std::string> result;
  std::string url_str;

  for (int i = 0; i < url.length(); ++i) {
    if (url[i] == '/') {
      if (!url_str.empty()) {
        result.push_back(url_str);
        url_str.clear();
      }
    } else {
      url_str += url[i];
    }
  }
  if (!url_str.empty()) {
    result.push_back(url_str);
  }

  return result;
}

const char* ParseResponse(const std::string url) {
  StatusLine status_line;
  status_line.protocol = "HTTP/1.1";
  status_line.status = "OK";
  status_line.status_code = "200";

  HeaderData header_data;
  header_data.content_type = "text/plain";
  header_data.content_length = "0";
  
  Response response;
  response.status = status_line;
  response.header = header_data;
  if (url != "/") {
    response.status.status = "NOT FOUND";
    response.status.status_code = "404";
  }

  return response.to_str().c_str();
}

// TODO: temp method, create proper methods with structs later
std::string Response200WithIndex(std::vector<std::string> data, int index) {
  std::string body_content = data[index];
  return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(body_content.size()) + "\r\n\r\n" + body_content;
}

int OpenServerConnection() {
  // AF_INET - specify IPv4
  // SOCK_STREAM - defines TCP socket
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   throw std::runtime_error("Failed to create server socket\n");
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    throw std::runtime_error("setsockopt failed\n");
  }
  
  struct sockaddr_in server_addr;   // sockaddr_in - data type to store socket data
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY; // make it listen to any available IP
  // htons - convert unsigned int from machine byte order to network byte order
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    throw std::runtime_error("Failed to bind to port 4221\n");
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    throw std::runtime_error("listen failed\n");
  }

  return server_fd;
}

void ProcessRequest(const int client_fd) {
  
 
  std::cout << "Client connected on socket = " << client_fd << "\n";

  char buffer[1024] = {0};
  recv(client_fd, buffer, sizeof(buffer), 0);
  std::cout << "Client message:" << buffer << std::endl;
  
  std::vector<std::string> parsed_request = ParseRequestBuffer(buffer);
  std::vector<std::string> parse_request_target = ParseURL(parsed_request[1]);
  std::string response;
  if (parsed_request[1] == "/") {
    response = "HTTP/1.1 200 OK\r\n\r\n";
  } else if (parse_request_target[0] == "echo") {
    response = Response200WithIndex(parse_request_target, 1);
  } else if (parse_request_target[0] == "user-agent") {
    response = Response200WithIndex(parsed_request, 6);
  } else {
    response = "HTTP/1.1 404 Not Found\r\n\r\n";
  }

  ssize_t bytes_sent = send(client_fd, response.c_str(), response.size(), 0);

  if (bytes_sent > 0) {
    std::cout << "Sent " << bytes_sent << " bytes" << std::endl;
    std::cout << "Response sent: " << response << std::endl;
  } else {
    std::cout << "Error sending response";
  }

  close(client_fd);
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  int server_fd = OpenServerConnection();
  std::cout << "Server file descriptor = " << server_fd << std::endl;
  int client_fd;

  try {
    while (true) {
      struct sockaddr_in client_addr;
      const int client_addr_len = sizeof(client_addr);
      std::cout << "Waiting for a client to connect...\n";

      client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);      

      std::thread request_thread(ProcessRequest, client_fd);
      
      // Bad practice? Thread detached from main thread
      request_thread.detach();
    }
  } catch (...) {
    close(server_fd);
    throw;
  }
  close(server_fd);
  
  std::cout << "Server shutdown\n";

  return 0;
}
