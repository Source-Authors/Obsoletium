/*
   See copyright notice in sqrdbg.h
*/

#include "sqrdbg.h"

#include "sqdbgserver.h"
#include "serialize_state_nut.h"

#include <squirrel.h>

#ifndef POSIX
#include <winsock.h>
using socklen_t = int;
#endif

#include "tier0/basetypes.h"

#if defined(VSCRIPT_DLL_EXPORT) || defined(VSQUIRREL_TEST)
#include "tier0/memdbgon.h"
#endif

namespace sq::dbg {

HSQREMOTEDBG sq_rdbg_init(HSQUIRRELVM v, unsigned short port,
                          SQBool autoupdate) {
#ifdef _WIN32
  WSADATA wsadata;
  if (WSAStartup(MAKEWORD(2, 0), &wsadata) != 0) {
    return nullptr;
  }
#endif

  auto *rdbg = new SQDbgServer(v);
  rdbg->_autoupdate = autoupdate ? true : false;
  rdbg->_accept = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in bindaddr;
  BitwiseClear(bindaddr);
  bindaddr.sin_family = AF_INET;
  bindaddr.sin_port = htons(port);
  bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(rdbg->_accept, (sockaddr *)&bindaddr, sizeof(bindaddr)) ==
      SOCKET_ERROR) {
    delete rdbg;
    sq_throwerror(v, _SC("failed to bind the socket"));
    return nullptr;
  }

  if (!rdbg->Init()) {
    delete rdbg;
    sq_throwerror(v, _SC("failed to initialize the debugger"));
    return nullptr;
  }

  return rdbg;
}

SQRESULT sq_rdbg_waitforconnections(HSQREMOTEDBG rdbg) {
  const SQRESULT rc{sq_compilebuffer(
      rdbg->_v, (const SQChar *)g_Script_serialize_state,
      (SQInteger)scstrlen((const SQChar *)g_Script_serialize_state),
      _SC("SERIALIZE_STATE"), SQFalse)};
  if (SQ_FAILED(rc)) {
    sq_throwerror(rdbg->_v, _SC("error compiling the serialization function"));
    return rc;
  }

  sq_getstackobj(rdbg->_v, -1, &rdbg->_serializefunc);
  sq_addref(rdbg->_v, &rdbg->_serializefunc);
  sq_pop(rdbg->_v, 1);

  if (listen(rdbg->_accept, 0) == SOCKET_ERROR) {
    return sq_throwerror(rdbg->_v, _SC("error on listen(socket)"));
  }

  sockaddr_in cliaddr;
  int addrlen = sizeof(cliaddr);
  rdbg->_endpoint =
      accept(rdbg->_accept, (sockaddr *)&cliaddr, (socklen_t *)&addrlen);

  // do not accept any other connection
  sqdbg_closesocket(rdbg->_accept);
  rdbg->_accept = INVALID_SOCKET;

  if (rdbg->_endpoint == INVALID_SOCKET) {
    return sq_throwerror(rdbg->_v, _SC("error accept(socket)"));
  }

  while (!rdbg->_ready) {
    sq_rdbg_update(rdbg);
  }

  return SQ_OK;
}

SQRESULT sq_rdbg_update(HSQREMOTEDBG rdbg) {
#ifdef _WIN32
  TIMEVAL time;
#else
  struct timeval time;
#endif

  time.tv_sec = 0;
  time.tv_usec = 0;
  fd_set read_flags;
  FD_ZERO(&read_flags);
  FD_SET(rdbg->_endpoint, &read_flags);
  select(NULL /*ignored*/, &read_flags, NULL, NULL, &time);

  if (FD_ISSET(rdbg->_endpoint, &read_flags)) {
    int size = 0;
    char c, prev = '\0';

    char temp[1024];
    BitwiseClear(temp);

    int res;
    FD_CLR(rdbg->_endpoint, &read_flags);
    while ((res = recv(rdbg->_endpoint, &c, 1, 0)) > 0) {
      if (c == '\n') break;

      if (c != '\r') {
        temp[size] = c;
        prev = c;
        size++;
      }
    }

    switch (res) {
      case 0:
        return sq_throwerror(rdbg->_v, _SC("disconnected"));
      case SOCKET_ERROR:
        return sq_throwerror(rdbg->_v, _SC("socket error"));
    }

    temp[size] = '\0';
    temp[size + 1] = '\0';

    rdbg->ParseMsg(temp);
  }

  return SQ_OK;
}

SQInteger debug_hook(HSQUIRRELVM v) {
  intp event_type, line;
  const SQChar *src, *func;

  sq_getinteger(v, 2, &event_type);
  sq_getstring(v, 3, &src);
  sq_getinteger(v, 4, &line);
  sq_getstring(v, 5, &func);

  SQUserPointer up;
  sq_getuserpointer(v, -1, &up);

  HSQREMOTEDBG rdbg = (HSQREMOTEDBG)up;
  rdbg->Hook(event_type, line, src, func);

  if (rdbg->_autoupdate) {
    if (SQ_FAILED(sq_rdbg_update(rdbg)))
      return sq_throwerror(v, _SC("socket failed"));
  }

  return 0;
}

SQInteger error_handler(HSQUIRRELVM v) {
  const SQChar *fn = _SC("unknown");
  const SQChar *src = _SC("unknown");

  SQUserPointer up;
  sq_getuserpointer(v, -1, &up);
  HSQREMOTEDBG rdbg = (HSQREMOTEDBG)up;

  SQStackInfos si;
  if (SQ_SUCCEEDED(sq_stackinfos(v, 1, &si))) {
    if (si.funcname) fn = si.funcname;
    if (si.source) src = si.source;

    scprintf(_SC("*FUNCTION [%s] %s line [%zd]\n"), fn, src, si.line);
  }

  if (sq_gettop(v) >= 1) {
    const SQChar *sErr = nullptr;

    if (SQ_SUCCEEDED(sq_getstring(v, 2, &sErr))) {
      scprintf(_SC("\nAN ERROR HAS OCCURED [%s]\n"), sErr);

      rdbg->Break(si.line, src, _SC("error"), sErr);
    } else {
      scprintf(_SC("\nAN ERROR HAS OCCURED [unknown]\n"));

      rdbg->Break(si.line, src, _SC("error"), _SC("unknown"));
    }
  }

  rdbg->BreakExecution();
  return 0;
}

SQRESULT sq_rdbg_shutdown(HSQREMOTEDBG *rdbg) {
  if (rdbg) {
    delete *rdbg;
    rdbg = nullptr;
  }

#ifdef _WIN32
  ::WSACleanup();
#endif

  return SQ_OK;
}

}  // namespace sq::dbg