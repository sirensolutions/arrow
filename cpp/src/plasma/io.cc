// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "plasma/io.h"

#include <cstdint>
#include <sstream>

#include "arrow/status.h"

#include "plasma/common.h"

using arrow::Status;

/* Number of times we try connecting to a socket. */
#define NUM_CONNECT_ATTEMPTS 50
#define CONNECT_TIMEOUT_MS 100

namespace plasma {

Status WriteBytes(int fd, uint8_t* cursor, size_t length) {
  ssize_t nbytes = 0;
  size_t bytesleft = length;
  size_t offset = 0;
  while (bytesleft > 0) {
    /* While we haven't written the whole message, write to the file descriptor,
     * advance the cursor, and decrease the amount left to write. */
    nbytes = write(fd, cursor + offset, bytesleft);
    if (nbytes < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        continue;
      }
      return Status::IOError(std::string(strerror(errno)));
    } else if (nbytes == 0) {
      return Status::IOError("Encountered unexpected EOF");
    }
    ARROW_CHECK(nbytes > 0);
    bytesleft -= nbytes;
    offset += nbytes;
  }

  return Status::OK();
}

Status WriteMessage(int fd, int64_t type, int64_t length, uint8_t* bytes) {
  int64_t version = PLASMA_PROTOCOL_VERSION;
  RETURN_NOT_OK(WriteBytes(fd, reinterpret_cast<uint8_t*>(&version), sizeof(version)));
  RETURN_NOT_OK(WriteBytes(fd, reinterpret_cast<uint8_t*>(&type), sizeof(type)));
  RETURN_NOT_OK(WriteBytes(fd, reinterpret_cast<uint8_t*>(&length), sizeof(length)));
  return WriteBytes(fd, bytes, length * sizeof(char));
}

Status ReadBytes(int fd, uint8_t* cursor, size_t length) {
  ssize_t nbytes = 0;
  /* Termination condition: EOF or read 'length' bytes total. */
  size_t bytesleft = length;
  size_t offset = 0;
  while (bytesleft > 0) {
    nbytes = read(fd, cursor + offset, bytesleft);
    if (nbytes < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        continue;
      }
      return Status::IOError(std::string(strerror(errno)));
    } else if (0 == nbytes) {
      return Status::IOError("Encountered unexpected EOF");
    }
    ARROW_CHECK(nbytes > 0);
    bytesleft -= nbytes;
    offset += nbytes;
  }

  return Status::OK();
}

Status ReadMessage(int fd, int64_t* type, std::vector<uint8_t>* buffer) {
  int64_t version;
  RETURN_NOT_OK_ELSE(ReadBytes(fd, reinterpret_cast<uint8_t*>(&version), sizeof(version)),
                     *type = DISCONNECT_CLIENT);
  ARROW_CHECK(version == PLASMA_PROTOCOL_VERSION) << "version = " << version;
  RETURN_NOT_OK_ELSE(ReadBytes(fd, reinterpret_cast<uint8_t*>(type), sizeof(*type)),
                     *type = DISCONNECT_CLIENT);
  int64_t length_temp;
  RETURN_NOT_OK_ELSE(
      ReadBytes(fd, reinterpret_cast<uint8_t*>(&length_temp), sizeof(length_temp)),
      *type = DISCONNECT_CLIENT);
  // The length must be read as an int64_t, but it should be used as a size_t.
  size_t length = static_cast<size_t>(length_temp);
  if (length > buffer->size()) {
    buffer->resize(length);
  }
  RETURN_NOT_OK_ELSE(ReadBytes(fd, buffer->data(), length), *type = DISCONNECT_CLIENT);
  return Status::OK();
}

int bind_ipc_sock(const std::string& pathname, bool shall_listen) {
  struct sockaddr_un socket_address;
  int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    ARROW_LOG(ERROR) << "socket() failed for pathname " << pathname;
    return -1;
  }
  /* Tell the system to allow the port to be reused. */
  int on = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&on),
                 sizeof(on)) < 0) {
    ARROW_LOG(ERROR) << "setsockopt failed for pathname " << pathname;
    close(socket_fd);
    return -1;
  }

  unlink(pathname.c_str());
  memset(&socket_address, 0, sizeof(socket_address));
  socket_address.sun_family = AF_UNIX;
  if (pathname.size() + 1 > sizeof(socket_address.sun_path)) {
    ARROW_LOG(ERROR) << "Socket pathname is too long.";
    close(socket_fd);
    return -1;
  }
  strncpy(socket_address.sun_path, pathname.c_str(), pathname.size() + 1);

  if (bind(socket_fd, reinterpret_cast<struct sockaddr*>(&socket_address),
           sizeof(socket_address)) != 0) {
    ARROW_LOG(ERROR) << "Bind failed for pathname " << pathname;
    close(socket_fd);
    return -1;
  }
  if (shall_listen && listen(socket_fd, 128) == -1) {
    ARROW_LOG(ERROR) << "Could not listen to socket " << pathname;
    close(socket_fd);
    return -1;
  }
  return socket_fd;
}

Status ConnectIpcSocketRetry(const std::string& pathname, int num_retries,
                             int64_t timeout, int* fd) {
  /* Pick the default values if the user did not specify. */
  if (num_retries < 0) {
    num_retries = NUM_CONNECT_ATTEMPTS;
  }
  if (timeout < 0) {
    timeout = CONNECT_TIMEOUT_MS;
  }
  *fd = connect_ipc_sock(pathname);
  while (*fd < 0 && num_retries > 0) {
    ARROW_LOG(ERROR) << "Connection to IPC socket failed for pathname " << pathname
                     << ", retrying " << num_retries << " more times";
    /* Sleep for timeout milliseconds. */
    usleep(static_cast<int>(timeout * 1000));
    *fd = connect_ipc_sock(pathname);
    --num_retries;
  }
  /* If we could not connect to the socket, exit. */
  if (*fd == -1) {
    std::stringstream ss;
    ss << "Could not connect to socket " << pathname;
    return Status::IOError(ss.str());
  }
  return Status::OK();
}

int connect_ipc_sock(const std::string& pathname) {
  struct sockaddr_un socket_address;
  int socket_fd;

  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    ARROW_LOG(ERROR) << "socket() failed for pathname " << pathname;
    return -1;
  }

  memset(&socket_address, 0, sizeof(socket_address));
  socket_address.sun_family = AF_UNIX;
  if (pathname.size() + 1 > sizeof(socket_address.sun_path)) {
    ARROW_LOG(ERROR) << "Socket pathname is too long.";
    return -1;
  }
  strncpy(socket_address.sun_path, pathname.c_str(), pathname.size() + 1);

  if (connect(socket_fd, reinterpret_cast<struct sockaddr*>(&socket_address),
              sizeof(socket_address)) != 0) {
    close(socket_fd);
    return -1;
  }

  return socket_fd;
}

int AcceptClient(int socket_fd) {
  int client_fd = accept(socket_fd, NULL, NULL);
  if (client_fd < 0) {
    ARROW_LOG(ERROR) << "Error reading from socket.";
    return -1;
  }
  return client_fd;
}

uint8_t* read_message_async(int sock) {
  int64_t size;
  Status s = ReadBytes(sock, reinterpret_cast<uint8_t*>(&size), sizeof(int64_t));
  if (!s.ok()) {
    /* The other side has closed the socket. */
    ARROW_LOG(DEBUG) << "Socket has been closed, or some other error has occurred.";
    close(sock);
    return NULL;
  }
  uint8_t* message = reinterpret_cast<uint8_t*>(malloc(size));
  s = ReadBytes(sock, message, size);
  if (!s.ok()) {
    free(message);
    /* The other side has closed the socket. */
    ARROW_LOG(DEBUG) << "Socket has been closed, or some other error has occurred.";
    close(sock);
    return NULL;
  }
  return message;
}

}  // namespace plasma
