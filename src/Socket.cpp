#include "Socket.h"

#include <mutex>

#include "Message.h"

Log *Socket::debug = nullptr;

int Socket::sockAddrSize = sizeof(sockaddr_in);
bool Socket::_winsockInitialized = false;
WSADATA Socket::_wsa;
std::map<SOCKET, int> Socket::_refCounts;

Socket::Socket() : _lingerTime(0) {
  if (!_winsockInitialized) initWinsock();

  _raw = socket(AF_INET, SOCK_STREAM, 0);
  addRef();
}

Socket::Socket(SOCKET &rawSocket, const std::string &ip)
    : _raw(rawSocket), _ip(ip), _lingerTime(0) {
  addRef();
  rawSocket = INVALID_SOCKET;
}

Socket::Socket(const Socket &rhs)
    : _raw(rhs._raw), _lingerTime(rhs._lingerTime), _ip(rhs._ip) {
  if (_raw == INVALID_SOCKET) return;
  addRef();
}

Socket Socket::Empty() {
  SOCKET invalidSocket = INVALID_SOCKET;
  return {invalidSocket, {}};
}

Socket::~Socket() { close(); }

const Socket &Socket::operator=(const Socket &rhs) {
  if (this == &rhs) return *this;

  close();

  _raw = rhs._raw;
  _ip = rhs._ip;
  addRef();
  _lingerTime = rhs._lingerTime;
  return *this;
}

void Socket::addRef() {
  if (_refCounts.find(_raw) != _refCounts.end())
    ++_refCounts[_raw];
  else
    _refCounts[_raw] = 1;
}

void Socket::bind(sockaddr_in &socketAddr) {
  if (!valid()) return;

  _isBound =
      ::bind(_raw, (sockaddr *)&socketAddr, sockAddrSize) != SOCKET_ERROR;
  if (!_isBound)
    *debug << Color::CHAT_ERROR << "Error binding socket: " << WSAGetLastError()
           << Log::endl;
}

void Socket::listen() {
  if (!valid()) return;

  ::listen(_raw, 3);
}

void Socket::sendMessage(const Message &msg, const Socket &destSocket) const {
  if (!_winsockInitialized) return;
  auto msgString = msg.compile();

  static std::mutex mutex;
  mutex.lock();

  auto result =
      send(destSocket.getRaw(), msgString.c_str(), (int)msgString.length(), 0);
  if (result < 0 && debug)
    *debug << Color::CHAT_ERROR << "Failed to send command \"" << msg
           << "\" to socket " << destSocket.getRaw() << Log::endl;

  mutex.unlock();
}

void Socket::sendMessage(const Message &msg) const { sendMessage(msg, *this); }

void Socket::initWinsock() {
  if (WSAStartup(MAKEWORD(2, 2), &_wsa) == 0) _winsockInitialized = true;
}

void Socket::delayClosing(ms_t lingerTime) { _lingerTime = lingerTime; }

int Socket::closeRawAfterDelay(void *data) {
  ms_t *const p = static_cast<ms_t *>(data);
  const SOCKET s = static_cast<SOCKET>(p[0]);
  const ms_t delay = p[1];
  delete[] p;

  SDL_Delay(delay);
  closesocket(s);

  return 0;
}

void Socket::close() {
  if (valid()) {
    --_refCounts[_raw];
    if (_refCounts[_raw] == 0) {
      if (_lingerTime > 0) {
        ms_t *const args = new ms_t[2];
        args[0] = static_cast<ms_t>(_raw);
        args[1] = _lingerTime;
        SDL_CreateThread(closeRawAfterDelay, "Closing socket",
                         static_cast<void *>(args));
      } else {
        closesocket(_raw);
      }
      _refCounts.erase(_raw);
      if (_refCounts.empty()) {
        WSACleanup();
        _winsockInitialized = false;
      }
    }
  }
}
