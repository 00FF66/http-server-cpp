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
#include <thread>
#include <fstream>
#include <sstream>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <cctype>

std::mutex file_lock;

struct StatusLine {
  std::string protocol;
  std::string status_code;
  std::string status;

  std::string to_str() const {
    return protocol + " " + status_code + " " + status + "\r\n";
  }
};

struct HeaderData {
  std::unordered_map<std::string, std::string> headers;

  std::string to_str() const {
    std::string response_str = "";

    auto getHeader = [this](const std::string& key) {
      auto it = headers.find(key);
      return (it != headers.end()) ? it->second : "";
    };

    if (getHeader("Content-Encoding") != "") {
      response_str += "Content-Encoding: " + getHeader("Content-Encoding") + "\r\n";
    }
    if (getHeader("Content-Type") != "") {
      response_str += "Content-Type: " + getHeader("Content-Type") + "\r\n";
    }
    if (getHeader("Content-Length") != "") {
      response_str += "Content-Length: " + getHeader("Content-Length") + "\r\n";
    }
    return response_str + "\r\n";
  }
};

struct Response {
  StatusLine status;
  HeaderData header;
  std::string body;

  std::string to_str() const {
    return status.to_str() + header.to_str() + body;
  }
};

struct Request {
  std::string method;
  std::vector<std::string> url;  // Parsed URL components
  std::string url_str;          // Original URL string
  std::unordered_map<std::string, std::string> headers;
  std::string body;

  std::string to_str() const {
    auto getHeader = [this](const std::string& key) {
      auto it = headers.find(key);
      return (it != headers.end()) ? it->second : "";
    };

  return method + " " + url_str + " " + getHeader("protocol") + 
          "\r\nHost: " + getHeader("Host") + 
          "\r\nUser-Agent: " + getHeader("User-Agent") + 
          "\r\nAccept: " + getHeader("Accept") + 
          "\r\nAccept-Encoding: " + getHeader("Accept-Encoding") + "\r\n";
  }
};

std::string trim(const std::string& str) {
  auto start = std::find_if(str.begin(), str.end(), [](unsigned char ch) {
      return !std::isspace(ch);
  });

  auto end = std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
      return !std::isspace(ch);
  }).base();

  return (start < end) ? std::string(start, end) : "";
}

std::vector<std::string> ParseURL(std::string& url) {
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

bool contains(const std::vector<std::string>& vec, const std::string& target) {
  return std::find(vec.begin(), vec.end(), target) != vec.end();
}

std::vector<std::string> ParseStringToVectorString(const std::string& str, char delimiter) {
  std::vector<std::string> result;
  std::stringstream ss(str);
  std::string item;

  while (std::getline(ss, item, delimiter)) {
      result.push_back(trim(item));
  }

  return result;
}

Request ParseRequest(const std::string buffer) {
  Request request;
  // used to process string stream line by line and word by word instead of char by char
  std::istringstream requestStream(buffer);
  std::string line;
  
  // Process the request line
  std::getline(requestStream, line);
  std::istringstream lineStream(line);
  lineStream >> request.method >> request.url_str >> request.headers["protocol"];
  
  // Parse request URL once it's extracted
  request.url = ParseURL(request.url_str);
  
  // Process headers
  while (std::getline(requestStream, line) && line != "\r") {
    auto colonPos = line.find(':');
    if (colonPos != std::string::npos) {
      std::string headerName = line.substr(0, colonPos);
      std::string headerValue = line.substr(colonPos + 2); // Skip the colon and space
      request.headers[headerName] = trim(headerValue);
      
    }
  }
  
  // Process body
  while (std::getline(requestStream, line)) {
    request.body += line + "\n";
  }

  request.body = trim(request.body);
  
  return request;
}

std::string PrepareResponse(const std::string& status, const std::string& status_code, const std::string& content_type, const std::string& body, const std::string& encoding) {
  StatusLine status_line;
  status_line.protocol = "HTTP/1.1";

  status_line.status = status;
  status_line.status_code = status_code;

  HeaderData header_data;
  std::vector<std::string> encoding_vec = ParseStringToVectorString(encoding, ',');
  if (encoding != "" && contains(encoding_vec, "gzip")) {
    header_data.headers["Content-Encoding"] = "gzip"; // change to list of supported encodings
  }
  header_data.headers["Content-Type"] = content_type;
  if (body.size() > 0) {
    header_data.headers["Content-Length"] = std::to_string(body.size());
  }
  
  Response response;
  response.status = status_line;
  response.header = header_data;
  response.body = body;

  return response.to_str();
}

void CreateFile(Request request, const std::string &directory) {
  // std::cout << "FIle body: " << request.body << std::endl;
  // std::cout << "FIle size: " << std::to_string(request.body.size()) << std::endl;
  // Open file in write mode
  std::ofstream file(directory + request.url[1]);
    
  // Check if file opened successfully
  if (!file) {
      std::cerr << "Error opening file: " << directory + request.url[1] << std::endl;
      // throw
      return;
  }

  // Write string to file
  file << request.body;

  // Close file
  file.close();
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

std::string ReadFileStream(std::string filename) {
  std::lock_guard<std::mutex> lock(file_lock);
  std::ifstream input(filename);
  // efficient way to read an entire file in one go
  std::stringstream buffer;
  if (!input) {
    std::cout << "File can't be opened\n";
    return "";
  }
  
  if (input.good()) {
    buffer << input.rdbuf();
  }
  input.close();

  return buffer.str();
}

void ProcessRequest(const int client_fd, std::string directory) {

  std::cout << "Client connected on socket = " << client_fd << "\n";

  // each thread gets its own buffer
  thread_local char buffer[1024] = {0};
  recv(client_fd, buffer, sizeof(buffer), 0);
  std::cout << "Client message:" << buffer << std::endl;
  
  Request parsed_request = ParseRequest(std::string(buffer));
  std::string response;
  if (parsed_request.url_str == "/" || parsed_request.url_str == "") {
    response = "HTTP/1.1 200 OK\r\n\r\n";
  } else if (parsed_request.url[0] == "echo") {
    std::string body = parsed_request.url[1];
    response = PrepareResponse("OK", "200", "text/plain", std::move(body), parsed_request.headers["Accept-Encoding"]);
  } else if (parsed_request.url[0] == "user-agent") {
    response = PrepareResponse("OK", "200", "text/plain", std::move(parsed_request.headers["User-Agent"]), parsed_request.headers["Accept-Encoding"]);
  } else if (parsed_request.url[0] == "files") {
    if (parsed_request.method == "GET") {
      std::string body = ReadFileStream(directory+parsed_request.url[1]);
      if (body.length() > 0) {
        response = PrepareResponse("OK", "200", "application/octet-stream", std::move(body), parsed_request.headers["Accept-Encoding"]);
      } else {
        response = "HTTP/1.1 404 Not Found\r\n\r\n";
      }
    } else if (parsed_request.method == "POST") {
      CreateFile(parsed_request, directory);
      response = "HTTP/1.1 201 Created\r\n\r\n";
    } else {
      response = "HTTP/1.1 404 Not Found\r\n\r\n";
    }
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

  // all odd is program options
  // std::cout << "Program options: " << argv[1] << std::endl;
  // all even is option params
  // std::cout << "Directory path = " << argv[2] << std::endl;

  std::string directory = argv[2];
  
  int server_fd = OpenServerConnection();
  std::cout << "Server file descriptor = " << server_fd << std::endl;
  int client_fd;

  try {
    while (true) {
      struct sockaddr_in client_addr;
      const int client_addr_len = sizeof(client_addr);
      std::cout << "Waiting for a client to connect...\n";

      client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);      

      std::thread request_thread(ProcessRequest, client_fd, directory);
      
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
