// Copyright Valve Corporation, All rights reserved.
//
// Remote HLSL shaders compiler client

#include "winlite.h"
#include "com_ptr.h"

#include <comdef.h>
#include <conio.h>
#include <d3dcompiler.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <atomic>
#include <cassert>
#include <system_error>
#include <thread>
#include <vector>

namespace {

constexpr inline int kDefaultSendBufferSize{40000};
constexpr inline int kDefaultReceiveBufferSize{40000};

constexpr inline char kDefaultSharedPort[]{"20000"};

[[nodiscard]] std::error_code GetSystemError(int errc) noexcept {
  return std::error_code{errc, std::system_category()};
}

class ScopedWinsockInitializer {
 public:
  ScopedWinsockInitializer(unsigned short major, unsigned short minor) noexcept
      : rc_{GetSystemError(WSAStartup(MAKEWORD(minor, major), &wsaData_))} {
    if (LOBYTE(wsaData_.wVersion) != minor ||
        HIBYTE(wsaData_.wVersion) != major) {
      rc_ = GetSystemError(ERROR_SVHDX_VERSION_MISMATCH);
    }
  }
  ~ScopedWinsockInitializer() noexcept {
    if (!rc_.value() || rc_.value() == ERROR_SVHDX_VERSION_MISMATCH) {
      if (WSACleanup() == SOCKET_ERROR) {
        rc_ = GetSystemError(WSAGetLastError());

        fprintf(stderr, "WSACleanup failed: %s\n", rc_.message().c_str());

        exit(rc_.value());
      }
    }
  }

  ScopedWinsockInitializer(ScopedWinsockInitializer &) = delete;
  ScopedWinsockInitializer &operator=(ScopedWinsockInitializer &) = delete;

  [[nodiscard]] std::error_code error_code() const noexcept { return rc_; }

 private:
  WSADATA wsaData_;
  std::error_code rc_;
};

class ScopedAcceptSocket {
 public:
  ScopedAcceptSocket(SOCKET listen_socket) noexcept
      : s_{accept(listen_socket, nullptr, nullptr)},
        rc_{s_ != INVALID_SOCKET ? std::error_code{}
                                 : GetSystemError(WSAGetLastError())} {}
  ~ScopedAcceptSocket() noexcept {
    if (s_ != INVALID_SOCKET) {
      if (shutdown(s_, SD_SEND) == SOCKET_ERROR) {
        rc_ = GetSystemError(WSAGetLastError());

        fprintf(stderr, "shutdown failed: %s\n", rc_.message().c_str());

        exit(rc_.value());
      }

      if (closesocket(s_) == SOCKET_ERROR) {
        rc_ = GetSystemError(WSAGetLastError());

        fprintf(stderr, "closesocket failed: %s\n", rc_.message().c_str());

        exit(rc_.value());
      }
    }
  }

  [[nodiscard]] std::error_code error_code() const noexcept { return rc_; }

  template <int size>
  [[nodiscard]] std::error_code Receive(char (&buffer)[size], int flags,
                                        int &received_bytes) noexcept {
    received_bytes = recv(s_, buffer, size, flags);
    return received_bytes != SOCKET_ERROR ? std::error_code{}
                                          : GetSystemError(WSAGetLastError());
  }

  template <int max_size>
  [[nodiscard]] std::error_code Send(uint32_t (&buffer)[max_size], int size,
                                     int flags, int &send_bytes) noexcept {
    assert(size <= max_size);
    if (size > max_size) {
      return std::error_code{EINVAL, std::generic_category()};
    }

    send_bytes = send(s_, reinterpret_cast<const char *>(buffer), size, flags);
    return send_bytes != SOCKET_ERROR ? std::error_code{}
                                      : GetSystemError(WSAGetLastError());
  }

  ScopedAcceptSocket(ScopedAcceptSocket &&s) noexcept : s_{s.s_}, rc_{s.rc_} {
    s.s_ = INVALID_SOCKET;
    s.rc_ = {};
  }
  ScopedAcceptSocket &operator=(ScopedAcceptSocket &&s) noexcept {
    std::swap(s.s_, s_);
    std::swap(s.rc_, rc_);
    return *this;
  }

  ScopedAcceptSocket(ScopedAcceptSocket &) = delete;
  ScopedAcceptSocket &operator=(ScopedAcceptSocket &) = delete;

 private:
  SOCKET s_;
  std::error_code rc_;
};

class ScopedListenSocket {
 public:
  ScopedListenSocket(addrinfo *info, bool is_blocking, int backlog) noexcept
      : s_{socket(info->ai_family, info->ai_socktype, info->ai_protocol)},
        rc_{s_ != INVALID_SOCKET ? std::error_code{}
                                 : GetSystemError(WSAGetLastError())} {
    assert(info);

    if (!rc_) {
      // Setup the TCP listening socket
      rc_ = bind(s_, info->ai_addr, static_cast<int>(info->ai_addrlen)) !=
                    SOCKET_ERROR
                ? std::error_code{}
                : GetSystemError(WSAGetLastError());
    }

    if (!rc_) {
      rc_ = listen(s_, backlog) != SOCKET_ERROR
                ? std::error_code{}
                : GetSystemError(WSAGetLastError());
    }

    if (!rc_ && !is_blocking) {
      // Socket is blocking by default, so apply non-blocking only if requested.
      u_long mode = 1;
      rc_ = ioctlsocket(s_, FIONBIO, &mode) != SOCKET_ERROR
                ? std::error_code{}
                : GetSystemError(WSAGetLastError());
    }
  }
  ~ScopedListenSocket() noexcept {
    if (s_ != INVALID_SOCKET) {
      if (closesocket(s_) == SOCKET_ERROR) {
        rc_ = GetSystemError(WSAGetLastError());

        fprintf(stderr, "closesocket failed: %s\n", rc_.message().c_str());

        exit(rc_.value());
      }
    }
  }

  [[nodiscard]] std::error_code error_code() const noexcept { return rc_; }

  [[nodiscard]] ScopedAcceptSocket GetAcceptSocket() const noexcept {
    return ScopedAcceptSocket{s_};
  }

  ScopedListenSocket(ScopedListenSocket &) = delete;
  ScopedListenSocket &operator=(ScopedListenSocket &) = delete;

 private:
  SOCKET s_;
  std::error_code rc_;
};

class ScopedAddressInfo {
 public:
  ScopedAddressInfo(const char *port, int family, int socket_type,
                    IPPROTO proto, int flags) noexcept {
    assert(port);

    addrinfo hints = {};
    hints.ai_family = family;
    hints.ai_socktype = socket_type;
    hints.ai_protocol = proto;
    hints.ai_flags = flags;

    rc_ = GetSystemError(getaddrinfo(nullptr, port, &hints, &info_));
  }
  ~ScopedAddressInfo() noexcept {
    if (!rc_) freeaddrinfo(info_);
  }

  [[nodiscard]] std::error_code error_code() const noexcept { return rc_; }

  [[nodiscard]] ScopedListenSocket CreateListenSocket(
      bool is_blocking, int backlog) const noexcept {
    return ScopedListenSocket{info_, is_blocking, backlog};
  }

  ScopedAddressInfo(ScopedAddressInfo &) = delete;
  ScopedAddressInfo &operator=(ScopedAddressInfo &) = delete;

 private:
  addrinfo *info_;
  std::error_code rc_;
};

class ScopedSetIsServerExitFlag {
 public:
  explicit ScopedSetIsServerExitFlag(std::atomic_bool &is_server_exit) noexcept
      : is_server_exit_{is_server_exit} {
    assert(is_server_exit == false);
  }
  ~ScopedSetIsServerExitFlag() noexcept { is_server_exit_ = true; }

  ScopedSetIsServerExitFlag(ScopedSetIsServerExitFlag &) = delete;
  ScopedSetIsServerExitFlag &operator=(ScopedSetIsServerExitFlag &) = delete;

 private:
  std::atomic_bool &is_server_exit_;
};

class ComErrorCategory : public std::error_category {
 public:
  ComErrorCategory() noexcept = default;

  [[nodiscard]] const char *name() const noexcept override { return "com"; }

  [[nodiscard]] std::string message(int error_value) const override {
    const _com_error com_error{static_cast<HRESULT>(error_value)};
    return std::string{com_error.ErrorMessage()};
  }

  ComErrorCategory(ComErrorCategory &) = delete;
  ComErrorCategory &operator=(ComErrorCategory &) = delete;
};

[[nodiscard]] inline ComErrorCategory &com_error_category() {
  static ComErrorCategory com_error_category;
  return com_error_category;
}

[[nodiscard]] inline std::error_code get_com_error_code(
    const HRESULT result) noexcept {
  return std::error_code{result, com_error_category()};
}

class ScopedThreadName {
 public:
  ScopedThreadName(HANDLE thread, const wchar_t *new_thread_name) noexcept
      : thread_{thread},
        old_thread_name_{nullptr},
        rc_{get_com_error_code(TransformGetSetNameResult(
            GetThreadDescription(thread, &old_thread_name_)))} {
    if (!rc_) {
      rc_ = get_com_error_code(TransformGetSetNameResult(
          SetThreadDescription(thread, new_thread_name)));
    }
  }
  ~ScopedThreadName() noexcept {
    if (!rc_ && old_thread_name_) {
      rc_ = get_com_error_code(TransformGetSetNameResult(
          SetThreadDescription(thread_, old_thread_name_)));

      if (rc_) {
        fprintf(stderr,
                "SetThreadDescription failed to restore old thread name %ls.\n",
                old_thread_name_);
      }
    }

    LocalFree(old_thread_name_);
  }

  [[nodiscard]] std::error_code error_code() const noexcept { return rc_; }

  ScopedThreadName(ScopedThreadName &) = delete;
  ScopedThreadName &operator=(ScopedThreadName &) = delete;

 private:
  HANDLE thread_;
  wchar_t *old_thread_name_;
  std::error_code rc_;

  static [[nodiscard]] HRESULT TransformGetSetNameResult(HRESULT hr) noexcept {
    if (hr == 0x10000000) {
      // GetThreadDescription returns this code for empty thread name, assume it
      // is ok.
      return S_OK;
    }

    return hr;
  }
};

[[nodiscard]] std::error_code GetMultibyteAsWide(const char *mb,
                                                 std::wstring &result) {
  assert(mb);

  const size_t required_size{mbstowcs(nullptr, mb, 0)};
  result.resize(required_size + 1);

  const size_t converted_size{mbstowcs(result.data(), mb, required_size + 1)};
  return converted_size != std::numeric_limits<size_t>::max()
             ? std::error_code{}
             : std::error_code{EINVAL, std::generic_category()};
}

struct ShaderCompileClientArgs {
  // Text in command file from game.
  char receive_buffer[kDefaultReceiveBufferSize];
  // Error string or binary shader blob in reply.
  uint32_t send_buffer[kDefaultSendBufferSize];
  // Base path to search shaders for.
  char path_base[MAX_PATH];
  // Default port to use for accepting shader compilation requests.
  char default_port[8];
  // Set by main thread when compiler should exit.
  std::atomic_bool should_exit;
  // Set by compiler thread when it is exit.
  std::atomic_bool is_server_exit;
  // Exit code for the app.  Set by compiler thread.
  std::error_code exit_code;
  // Should compiler disassemble compiled shaders?
  bool should_disassemble_shaders;
  // Should compiler optimize shaders during compilation?
  bool should_optimize_shaders;
};

[[nodiscard]] ScopedAcceptSocket AcceptConnection(
    const ScopedListenSocket &scoped_listen_socket,
    const std::atomic_bool &should_exit, bool is_blocking,
    std::error_code &rc) noexcept {
  do {
    // Accept a client socket
    ScopedAcceptSocket scoped_accept_client_socket{
        scoped_listen_socket.GetAcceptSocket()};
    rc = scoped_accept_client_socket.error_code();
    if (rc && (is_blocking || rc.value() != WSAEWOULDBLOCK)) {
      break;
    }

    if (is_blocking || !rc) {
      return scoped_accept_client_socket;
    }

    using namespace std::chrono_literals;

    // Ok, fast wait to be responsive.
    std::this_thread::sleep_for(10ms);
  } while (!should_exit);

  // App exited, return success.
  rc = std::error_code{0, std::generic_category()};
  return ScopedAcceptSocket{INVALID_SOCKET};
}

template <int buffer_size>
[[nodiscard]] int Receive(ScopedAcceptSocket &scoped_accept_socket,
                          char (&receive_buffer)[buffer_size],
                          const std::atomic_bool &should_exit, bool is_blocking,
                          std::error_code &rc) noexcept {
  do {
    int received_bytes{-1};

    rc = scoped_accept_socket.Receive(receive_buffer, 0, received_bytes);
    if (rc && (is_blocking || rc.value() != WSAEWOULDBLOCK)) {
      return -1;
    }

    if (is_blocking || !rc) return received_bytes;

    using namespace std::chrono_literals;

    // Ok, fast wait to be responsive.
    std::this_thread::sleep_for(10ms);
  } while (!should_exit);

  // App exited, return success.
  rc = std::error_code{0, std::generic_category()};
  return -1;
}

template <int buffer_size>
[[nodiscard]] int Send(ScopedAcceptSocket &scoped_accept_socket,
                       uint32_t (&send_buffer)[buffer_size], int send_size,
                       const std::atomic_bool &should_exit, bool is_blocking,
                       std::error_code &rc) noexcept {
  do {
    int send_bytes{-1};

    rc = scoped_accept_socket.Send(send_buffer, send_size, 0, send_bytes);
    if (rc && (is_blocking || rc.value() != WSAEWOULDBLOCK)) {
      return -1;
    }
    if (send_bytes != send_size) {
      return send_bytes;
    }

    if (is_blocking || !rc) return send_bytes;

    using namespace std::chrono_literals;

    // Ok, fast wait to be responsive.
    std::this_thread::sleep_for(10ms);
  } while (!should_exit);

  // App exited, return success.
  rc = std::error_code{0, std::generic_category()};
  return -1;
}

// This guy just spins and compiles when a command comes in from the game.
void ShaderCompileClient(ShaderCompileClientArgs &args) {
  const ScopedThreadName scoped_thread_name{GetCurrentThread(),
                                            L"Rshc_ClientThread"};
  std::error_code rc{scoped_thread_name.error_code()};
  if (rc) {
    args.exit_code = rc;
    fprintf(stderr, "Unable to set compile client thread name\n");
    return;
  }

  constexpr unsigned short kWinsockMajor{2}, kWinsockMinor{2};

  // Signal main thread child exited.
  const ScopedSetIsServerExitFlag scoped_set_is_server_exit_flag{
      args.is_server_exit};

  const ScopedWinsockInitializer scoped_winsock_initializer{kWinsockMajor,
                                                            kWinsockMinor};
  rc = scoped_winsock_initializer.error_code();
  if (rc) {
    args.exit_code = rc;
    fprintf(stderr, "WSAStartup v %hu.%hu failed with error: %s\n",
            kWinsockMajor, kWinsockMinor, rc.message().c_str());
    return;
  }

  const ScopedAddressInfo scoped_address_info{
      args.default_port, AF_INET, SOCK_STREAM, IPPROTO_TCP, AI_PASSIVE};
  rc = scoped_address_info.error_code();
  if (rc) {
    args.exit_code = rc;
    fprintf(stderr, "getaddrinfo failed: %s\n", rc.message().c_str());
    return;
  }

  constexpr bool is_blocking{false};

  const ScopedListenSocket scoped_listen_socket{
      scoped_address_info.CreateListenSocket(is_blocking, SOMAXCONN)};
  // Create a SOCKET for connecting to server
  rc = scoped_listen_socket.error_code();
  if (rc) {
    args.exit_code = rc;
    fprintf(stderr, "Create listen socket failed: %s\n", rc.message().c_str());
    return;
  }

  printf("Waiting for initial connection...\n");

  ScopedAcceptSocket scoped_accept_socket{AcceptConnection(
      scoped_listen_socket, args.should_exit, is_blocking, rc)};
  if (rc) {
    args.exit_code = rc;
    fprintf(stderr, "Create accept socket failed: %s\n", rc.message().c_str());
    return;
  }

  if (!rc && !args.should_exit) {
    // First connection
    printf("Game connected\n");
  }

  while (!args.should_exit) {
    const int received_bytes{Receive(scoped_accept_socket, args.receive_buffer,
                                     args.should_exit, is_blocking, rc)};

    if (received_bytes > 0) {
      const char *shader_file_name{strtok(args.receive_buffer, "\n")};
      char full_file_name[MAX_PATH];

      // If we took in a path on the commandline, we concatenate the incoming
      // filenames with it
      if (args.path_base[0]) {
        strncpy_s(full_file_name, args.path_base, MAX_PATH);
        strncat_s(full_file_name, shader_file_name, MAX_PATH);

        shader_file_name = full_file_name;
      }

      const char *shader_model{strtok(nullptr, "\n")};
      int send_buffer_size = 0;

      {
        char *num_macroses_ptr{strtok(nullptr, "\n")};
        unsigned long num_macroses{strtoul(num_macroses_ptr, nullptr, 10)};

        // Read macros from the command file
        std::vector<D3D_SHADER_MACRO> macros;
        macros.reserve(num_macroses);

        // The last one is the (null) one, so don't bother reading it
        // Start from 1 as num_macroses may be 0.
        for (unsigned long i = 1; i < num_macroses; i++) {
          // Allocate and populate strings
          const char *name{strtok(nullptr, "\n")};
          const char *definition{strtok(nullptr, "\n")};

          macros.emplace_back(D3D_SHADER_MACRO{name, definition});
        }

        // Null macro at the end
        macros.emplace_back(D3D_SHADER_MACRO{nullptr, nullptr});

        std::wstring wide_shader_file_name;
        rc = GetMultibyteAsWide(shader_file_name, wide_shader_file_name);
        if (rc) {
          args.exit_code = rc;
          fprintf(stderr,
                  "Unable to convert %s shader file name to UTF-16: %s\n",
                  shader_file_name, rc.message().c_str());
          return;
        }

        se::win::com::com_ptr<ID3DBlob> shader, errors;

        constexpr unsigned kCommonFlags{D3DCOMPILE_WARNINGS_ARE_ERRORS |
                                        D3DCOMPILE_ENABLE_STRICTNESS |
                                        D3DCOMPILE_AVOID_FLOW_CONTROL};

        constexpr unsigned kDebugOnlyFlags{D3DCOMPILE_DEBUG |
                                           D3DCOMPILE_SKIP_OPTIMIZATION |
                                           D3DCOMPILE_DEBUG_NAME_FOR_SOURCE};

        constexpr unsigned kReleaseOnlyFlags{D3DCOMPILE_OPTIMIZATION_LEVEL3};

        const unsigned flags1{kCommonFlags |
                              (args.should_optimize_shaders ? kReleaseOnlyFlags
                                                            : kDebugOnlyFlags)};

        printf("Compiling shader...\n");
        printf("Filename: %s\n", shader_file_name);
        printf("Shader model: %s\n", shader_model);
        printf("Macros: ");

        for (unsigned long i = 0; i < num_macroses - 1; i++)
          printf("  %s\n", macros[i].Name);

        printf("\n");

        // TODO(dimhotepus): Migrate to
        // https://github.com/microsoft/DirectXShaderCompiler.
        HRESULT hr{D3DCompileFromFile(
            wide_shader_file_name.data(), macros.data(), nullptr, "main",
            shader_model, flags1, 0, &shader, &errors)};
        if (FAILED(hr)) {
          args.send_buffer[0] = '\0';

          fprintf(stderr, "%s compilation error 0x%.8lx!\n", shader_file_name,
                  hr);

          if (errors) {
            // Null-terminated string
            memcpy(args.send_buffer + 1, errors->GetBufferPointer(),
                   errors->GetBufferSize());
            fprintf(stderr, "%s\n",
                    reinterpret_cast<const char *>(args.send_buffer + 1));

            // account for uint32_t at front of the buffer
            send_buffer_size =
                static_cast<int>(errors->GetBufferSize() + sizeof(uint32_t));
          } else {
            reinterpret_cast<uint8_t *>(args.send_buffer + 1)[0] = '?';
            reinterpret_cast<uint8_t *>(args.send_buffer + 1)[1] = '\0';

            // account for uint32_t at front of the buffer
            send_buffer_size = static_cast<int>(2 + sizeof(uint32_t));
          }
        } else {
          // Success!
          args.send_buffer[0] = static_cast<uint32_t>(shader->GetBufferSize());
          memcpy(args.send_buffer + 1, shader->GetBufferPointer(),
                 shader->GetBufferSize());

          // account for uint32_t at front of the buffer
          send_buffer_size =
              static_cast<int>(shader->GetBufferSize() + sizeof(uint32_t));

          printf("%s compiled successfullly.\n", shader_file_name);

          if (args.should_disassemble_shaders) {
            printf("Filename: %s\n", shader_file_name);
            printf("Shader model: %s\n", shader_model);
            printf("Macros: ");

            for (unsigned long i = 0; i < num_macroses - 1; i++)
              printf("  %s\n", macros[i].Name);

            printf("\n");

            se::win::com::com_ptr<ID3DBlob> disassembly{nullptr};
            hr = D3DDisassemble(
                shader->GetBufferPointer(), shader->GetBufferSize(),
                D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING, "", &disassembly);
            if (FAILED(hr)) {
              fprintf(stderr, "Disassembly error 0x%.8lx: %s\n", hr,
                      _com_error{hr}.ErrorMessage());
            } else {
              printf(
                  "Disassembled shader:\n%s\n",
                  static_cast<const char *>(disassembly->GetBufferPointer()));
            }
          }
        }

        // Purge the macro buffer
        macros.clear();
      }

      // Send the compiled shader back to the game
      const int send_bytes{Send(scoped_accept_socket, args.send_buffer,
                                send_buffer_size, args.should_exit, is_blocking,
                                rc)};
      rc = scoped_accept_socket.error_code();
      if (rc && (is_blocking || rc.value() != WSAEWOULDBLOCK)) {
        args.exit_code = rc;
        fprintf(stderr, "send failed: %s\n", rc.message().c_str());
        return;
      }
      if (send_bytes != send_buffer_size) {
        args.exit_code = std::error_code{ERANGE, std::generic_category()};
        fprintf(stderr,
                "send failed. Send only %d bytes, but expected %d: %s\n",
                send_bytes, send_buffer_size, rc.message().c_str());
        return;
      }
    } else {
      if (rc && (is_blocking || rc.value() != WSAEWOULDBLOCK)) {
        args.exit_code = rc;
        fprintf(stderr, "Receive from accept socket failed: %s\n",
                rc.message().c_str());
        return;
      }

      // If the connection has been gracefully closed, the Receive return value
      // is zero.  We had a game talking to us but it went away.
      printf("Game went away, waiting for new connection...\n");

      // Block again waiting to accept a connection
      scoped_accept_socket = AcceptConnection(
          scoped_listen_socket, args.should_exit, is_blocking, rc);
      if (rc) {
        args.exit_code = rc;
        fprintf(stderr, "Recreate accept socket failed: %s\n",
                rc.message().c_str());
        return;
      }

      printf("Game connected\n");
    }
  }
}

template <size_t size>
void AppendPathSeparator(char (&path)[size]) {
  const size_t len{strlen(path)};

  // Windows only.
  // If we don't have a path separator at the end of the path, put one there
  if (len != 0 && path[len - 1] != '\\') {
    strncat_s(path, "\\", size);
  }
}

void PrintfUsage(const char *argv0) {
  fprintf(
      stderr,
      "Usage:\n%s --base-path "
      "U:\\piston\\staging\\src\\materialsystem\\stdshaders\\ "
      "[--no-logo] [--should-disassemble-shaders] [--no-optimize-shaders]\n",
      argv0);
}

}  // namespace

int main(int argc, char *argv[]) {
  const ScopedThreadName scoped_thread_name{GetCurrentThread(),
                                            L"Rshc_MainThread"};
  std::error_code rc{scoped_thread_name.error_code()};
  if (rc) {
    fprintf(stderr, "%s error: Unable to set compile client thread name: %s\n",
            argv[0], rc.message().c_str());
    return rc.value();
  }

  if (argc < 3) {
    PrintfUsage(argv[0]);
    return 1;
  }

  ShaderCompileClientArgs args = {};
  // Optimization on by default to allow miss command line flags.
  args.should_optimize_shaders = true;

  // If Path specified on the commandline, utility expects that the remote
  // machine is going to send base filenames only.  It also means utility
  // strcats this path onto the filename from the worker.
  //
  // For example, if you have shader source on your Windows machine, you can use
  // something like this:
  //
  //   U:\piston\staging\src\materialsystem\stdshaders

  bool show_logo{true};
  for (int i{1}; i < argc; ++i) {
    if (!strcmp(argv[i], "--base-path")) {
      if (i + 1 < argc || strstr(argv[i + 1], "--") == argv[i + 1]) {
        strcpy_s(args.path_base, argv[i + 1]);
        ++i;
      } else {
        PrintfUsage(argv[0]);
        return 1;
      }
    } else if (!strcmp(argv[i], "--no-logo")) {
      show_logo = false;
    } else if (!strcmp(argv[i], "--should-disassemble-shaders")) {
      args.should_disassemble_shaders = true;
    } else if (!strcmp(argv[i], "--no-optimize-shaders")) {
      args.should_optimize_shaders = false;
    } else {
      fprintf(stderr, "%s error: Unknown command line arg '%s'.", argv[0],
              argv[i]);
      PrintfUsage(argv[0]);
      return 1;
    }
  }

  strcpy_s(args.default_port, kDefaultSharedPort);
  AppendPathSeparator(args.path_base);

  if (show_logo) {
    printf(
        "Remote HLSL shader "
#ifdef _WIN64
        "x64"
#else
        "x86"
#endif
        " compiler client from " __DATE__
        ".\nCopyright Valve Corporation, All rights reserved.\n\n");
  }

  printf(
      "Remote HLSL shader "
#ifdef _WIN64
      "x64"
#else
      "x86"
#endif "
      " compiler client is listening on port %s.\nPress ESC to exit.\n\n",
      args.default_port);

  // Kick off compile server thread.
  std::thread remote_compile_thread{ShaderCompileClient, std::ref(args)};

  constexpr int kEscapeCode{27};

  // Spin until escape.
  while (_getch() != kEscapeCode) {
    using namespace std::chrono_literals;

    // Long wait to reduce CPU wall time.
    std::this_thread::sleep_for(10ms);
  }

  printf("Exiting...\n");

  args.should_exit = true;
  remote_compile_thread.join();

  // Spin until compile thread set exit code.
  while (!args.is_server_exit) {
    using namespace std::chrono_literals;

    // Fast spin to reduce app exit time.
    std::this_thread::yield();
  }

  return args.exit_code.value();
}
