#ifndef SQ_INCLUDE_SQDBGSERVER_H_
#define SQ_INCLUDE_SQDBGSERVER_H_

constexpr inline int MAX_BP_PATH{512};
constexpr inline int MAX_MSG_LEN{2049};

#if defined(POSIX)
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tier0/threadtools.h"

using SOCKET = int;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#include "squirrel.h"

#include <set>
#include <string>
#include <vector>

using SOCKET = std::size_t;
using SQDBGString = std::basic_string<SQChar>;

inline bool dbg_less(const SQChar *x, const SQChar *y) {
  do {
    const int xl = *x == '\\' ? '/' : tolower(*x);
    const int yl = *y == '\\' ? '/' : tolower(*y);
    const int diff = xl - yl;

    if (diff != 0) return diff > 0;

    x++;
    y++;
  } while (*x != '\0' && *y != '\0');

  return false;
}

namespace sq::dbg {

struct BreakPoint {
  BreakPoint() : BreakPoint{0, nullptr} {}
  BreakPoint(SQInteger line, const SQChar *src) {
    _line = line;
    _src = src;
  }
  BreakPoint(const BreakPoint &bp) {
    _line = bp._line;
    _src = bp._src;
  }

  BreakPoint &operator=(const BreakPoint &bp) {
    _line = bp._line;
    _src = bp._src;
    return *this;
  }

  [[nodiscard]] inline bool operator<(const BreakPoint &bp) const {
    if (_line < bp._line) return true;

    if (_line == bp._line) {
      return dbg_less(_src.c_str(), bp._src.c_str());
    }

    return false;
  }

  [[nodiscard]] inline bool operator=(const BreakPoint &bp) const {
    return _line == bp._line && _src == bp._src;
  }

  SQInteger _line;
  SQDBGString _src;
};

struct Watch {
  Watch() { _id = 0; }
  Watch(int id, const SQChar *exp) {
    _id = id;
    _exp = exp;
  }
  Watch(const Watch &w) {
    _id = w._id;
    _exp = w._exp;
  }

  Watch &operator=(const Watch &w) {
    _id = w._id;
    _exp = w._exp;
    return *this;
  }

  bool operator<(const Watch &w) const { return _id < w._id; }
  bool operator==(const Watch &w) const { return _id == w._id; }

  int _id;
  SQDBGString _exp;
};

using BreakPointSet = std::set<BreakPoint>;
using BreakPointSetItor = BreakPointSet::iterator;

using WatchSet = std::set<Watch>;
using WatchSetItor = WatchSet::iterator;

using SQCharVec = std::vector<SQChar>;

struct SQDbgServer {
 public:
  enum class eDbgState {
    eDBG_Running,
    eDBG_StepOver,
    eDBG_StepInto,
    eDBG_StepReturn,
    eDBG_Suspended,
    eDBG_Disabled,
  };

  SQDbgServer(HSQUIRRELVM v);
  ~SQDbgServer();

  bool Init();
  bool IsConnected();

  bool ReadMsg();
  void BusyWait();
  void Hook(SQInteger type, SQInteger line, const SQChar *src, const SQChar *func);
  void ParseMsg(const char *msg);
  bool ParseBreakpoint(const char *msg, BreakPoint &out);
  bool ParseWatch(const char *msg, Watch &out);
  bool ParseRemoveWatch(const char *msg, int &id);
  void Terminated();
  //
  void BreakExecution();
  void Send(const SQChar *s, ...);
  void SendChunk(const SQChar *chunk);
  void Break(SQInteger line, const SQChar *src, const SQChar *type,
             const SQChar *error = nullptr);

  void SerializeState();
  // COMMANDS
  void AddBreakpoint(BreakPoint &bp);
  void AddWatch(Watch &w);
  void RemoveWatch(int id);
  void RemoveBreakpoint(BreakPoint &bp);

  //
  void SetErrorHandlers();

  // XML RELATED STUFF///////////////////////

  static constexpr inline int MAX_NESTING{10};

  struct XMLElementState {
    SQChar name[256];
    bool haschildren;
  };

  XMLElementState xmlstate[MAX_NESTING];
  int _xmlcurrentement;

  void BeginDocument() { _xmlcurrentement = -1; }
  void BeginElement(const SQChar *name);
  void Attribute(const SQChar *name, const SQChar *value);
  void EndElement(const SQChar *name);
  void EndDocument();

  const SQChar *escape_xml(const SQChar *x) const;
  //////////////////////////////////////////////

  HSQUIRRELVM _v;
  HSQOBJECT _debugroot;
  eDbgState _state;
  SOCKET _accept;
  SOCKET _endpoint;
  BreakPointSet _breakpoints;
  WatchSet _watches;
  int _recursionlevel;
  int _maxrecursion;
  int _nestedcalls;
  bool _ready;
  bool _autoupdate;
  HSQOBJECT _serializefunc;
  SQCharVec _scratchstring;
};

}  // namespace sq::dbg

#ifdef _WIN32
#define sqdbg_closesocket(x) closesocket((x))
#else
#define sqdbg_closesocket(x) close((x))
#endif

#endif  // !SQ_INCLUDE_SQDBGSERVER_H_
