// Copyright Valve Corporation, All rights reserved.

#ifndef SE_DEDICATED_CONSOLE_TEXT_CONSOLE_H_
#define SE_DEDICATED_CONSOLE_TEXT_CONSOLE_H_

namespace se::dedicated {

class CTextConsole {
 public:
  CTextConsole() : m_ConsoleVisible(true) {}
  virtual ~CTextConsole(){};

  virtual bool Init();
  virtual void ShutDown() = 0;
  virtual void Print(const char *pszMsg) = 0;

  virtual void SetTitle(const char *pszTitle) = 0;
  virtual void SetStatusLine(const char *pszStatus) = 0;
  virtual void UpdateStatus() = 0;

  // Must be provided by children
  virtual char *GetLine(int index, char *buf, size_t buflen) = 0;
  virtual int GetWidth() = 0;

  virtual void SetVisible(bool visible) { m_ConsoleVisible = visible; }
  virtual bool IsVisible() { return m_ConsoleVisible; }

 protected:
  bool m_ConsoleVisible;
};

}  // namespace se::dedicated

#endif  // !SE_DEDICATED_CONSOLE_TEXT_CONSOLE_H_
