// Copyright Valve Corporation, All rights reserved.

#ifndef VPC_PROJECTGENERATOR_PS3_H_
#define VPC_PROJECTGENERATOR_PS3_H_

#include "projectgenerator_vcproj.h"

#define PROPERTYNAME(X, Y) X##_##Y ,

enum PS3Properties_e {
#include "projectgenerator_ps3.inc"
};

class CProjectGenerator_PS3 : public IVCProjWriter {
 public:
  CProjectGenerator_PS3();
  IBaseProjectGenerator *GetProjectGenerator() { return m_pVCProjGenerator; }

  virtual bool Save(const char *pOutputFilename);

 private:
  bool WriteToXML();
  bool WriteFolder(CProjectFolder *pFolder);
  bool WriteFile(CProjectFile *pFile);
  bool WriteConfiguration(CProjectConfiguration *pConfig);
  bool WritePreBuildEventTool(CPreBuildEventTool *pPreBuildEventTool);
  bool WriteCustomBuildTool(CCustomBuildTool *pCustomBuildTool);
  bool WriteSNCCompilerTool(CCompilerTool *pCompilerTool);
  bool WriteGCCCompilerTool(CCompilerTool *pCompilerTool);
  bool WriteSNCLinkerTool(CLinkerTool *pLinkerTool);
  bool WriteGCCLinkerTool(CLinkerTool *pLinkerTool);
  bool WritePreLinkEventTool(CPreLinkEventTool *pPreLinkEventTool);
  bool WriteLibrarianTool(CLibrarianTool *pLibrarianTool);
  bool WritePostBuildEventTool(CPostBuildEventTool *pPostBuildEventTool);
  const char *BoolStringToTrueFalseString(const char *pValue);

  CXMLWriter m_XMLWriter;
  CVCProjGenerator *m_pVCProjGenerator;
};

#endif  // VPC_PROJECTGENERATOR_PS3_H_
