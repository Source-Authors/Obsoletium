// Copyright Valve Corporation, All rights reserved.

#ifndef VPC_PROJECTGENERATOR_WIN32_H_
#define VPC_PROJECTGENERATOR_WIN32_H_

#include "projectgenerator_vcproj.h"

#define PROPERTYNAME(X, Y) X##_##Y,

enum Win32Properties_e {
#include "projectgenerator_win32.inc"
};

class CProjectGenerator_Win32 : public IVCProjWriter {
 public:
  CProjectGenerator_Win32();
  IBaseProjectGenerator *GetProjectGenerator() { return m_pVCProjGenerator; }

  virtual bool Save(const char *pOutputFilename);

 private:
  bool WriteToXML();

  bool WriteFolder(CProjectFolder *pFolder);
  bool WriteFile(CProjectFile *pFile);
  bool WriteConfiguration(CProjectConfiguration *pConfig);
  bool WriteProperty(const PropertyState_t *pPropertyState,
                     const char *pOutputName = NULL, const char *pValue = NULL);
  bool WriteTool(const char *pToolName, const CProjectTool *pProjectTool);
  bool WriteNULLTool(const char *pToolName,
                     const CProjectConfiguration *pConfig);

  CXMLWriter m_XMLWriter;
  CVCProjGenerator *m_pVCProjGenerator;
};

#endif  // VPC_PROJECTGENERATOR_WIN32_H_
