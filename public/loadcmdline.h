//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: loads additional command line options from a config file
//
// $NoKeywords: $
//=============================================================================//

#ifndef LOADCMDLINE_H
#define LOADCMDLINE_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Loads additional commandline arguments from a config file for an app.
// keyname: Name of the block containing the key/args pairs (ie map or model name)
// appname: Keyname for the commandline arguments to be loaded - typically the exe name.
//-----------------------------------------------------------------------------
void LoadCmdLineFromFile( int &argc, char **&argv, const char *keyname, const char *appname );

//-----------------------------------------------------------------------------
// Purpose: Cleans up any memory allocated for the new argv.  Pass in the app's
// argc and argv - this is safe even if no extra arguments were loaded.
//-----------------------------------------------------------------------------
void DeleteCmdLine( int argc, char **argv );

class ScopedCmdLine {
 public:
  ScopedCmdLine( int &argc, char **&argv, const char *keyname, const char *appname ) noexcept {
    LoadCmdLineFromFile(argc, argv, keyname, appname);
    m_argc = argc;
    m_argv = argv;
  }
  ~ScopedCmdLine() noexcept { DeleteCmdLine(m_argc, m_argv); }

  int argc() const noexcept { return m_argc; }
  char **argv() const noexcept { return m_argv; }

 private:
  int m_argc;
  char **m_argv;
};

#endif // LOADCMDLINE_H