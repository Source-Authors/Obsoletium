// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"
#include "vcprojconvert.h"

#include <cstdio>
#include <cstdlib>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <comutil.h>  // _variant_t
#include <atlbase.h>  // CComPtr
#elif _LINUX
#include <ctime>  // needed by xercesc
#include <unistd.h>
#include <dirent.h>  // scandir()
#define _stat stat

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>

#include "valve_minmax_off.h"
#if defined(XERCES_NEW_IOSTREAMS)
#include <iostream>
#else
#include <iostream.h>
#endif

#include "valve_minmax_on.h"

#define IXMLDOMNode DOMNode
#define IXMLDOMNodeList DOMNodeList

#define _alloca alloca

XERCES_CPP_NAMESPACE_USE

class XStr {
 public:
  XStr(const char *const toTranscode) {
    // Call the private transcoding method
    fUnicodeForm = XMLString::transcode(toTranscode);
  }

  ~XStr() { XMLString::release(&fUnicodeForm); }

  const XMLCh *unicodeForm() const { return fUnicodeForm; }

 private:
  XMLCh *fUnicodeForm;
};

#define _bstr_t(str) XStr(str).unicodeForm()

#else
#error "Unsupported platform"
#endif

#include "tier0/platform.h"
#include "tier1/utlvector.h"

namespace se::vprojtomake {

// Purpose:  constructor
CVCProjConvert::CVCProjConvert()
#ifdef _WIN32
    : m_hrComInit{::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED |
                                                COINIT_DISABLE_OLE1DDE |
                                                COINIT_SPEED_OVER_MEMORY)}
#endif
{
#ifdef _WIN32
  if (FAILED(m_hrComInit)) {
    Warning("Unable to initialize COM: (hr) 0x%x.\n", m_hrComInit);
  }
#elif _LINUX
  try {
    XMLPlatformUtils::Initialize();
  } catch (const XMLException &toCatch) {
    char *message = XMLString::transcode(toCatch.getMessage());
    Error("Error during initialization! : %s\n", message);
    XMLString::release(&message);
  }
#endif

  m_bProjectLoaded = false;
}

CVCProjConvert::~CVCProjConvert() {
#ifdef _WIN32
  if (SUCCEEDED(m_hrComInit)) ::CoUninitialize();
#endif
}

// Purpose: load up a project and parse it
bool CVCProjConvert::LoadProject(const char *project) {
#ifdef _WIN32
  CComPtr<IXMLDOMDocument> pXMLDoc;
  HRESULT hr{::CoCreateInstance(CLSID_DOMDocument, nullptr,
                                CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument,
                                (void **)&pXMLDoc)};
  if (FAILED(hr)) {
    Warning("Can't create XML document instance: (hr) 0x%x.\n", hr);
    return false;
  }

  hr = pXMLDoc->put_async(VARIANT_BOOL(FALSE));
  if (FAILED(hr)) {
    Warning("Can't set async mode for XML document: (hr) 0x%x.\n", hr);
    return false;
  }

  VARIANT_BOOL vtbool;
  _variant_t bstrProject(project);

  hr = pXMLDoc->load(bstrProject, &vtbool);
  if (FAILED(hr) || vtbool == VARIANT_FALSE) {
    Warning(
        "Can't open XML document for Visual Studio Project '%s'. Project is "
        "%s: (hr) 0x%x.\n",
        project, vtbool == VARIANT_FALSE ? "invalid" : "valid", hr);
    return false;
  }
#elif _LINUX
  XercesDOMParser *parser = new XercesDOMParser();
  parser->setValidationScheme(XercesDOMParser::Val_Always);  // optional.
  parser->setDoNamespaces(true);                             // optional

  ErrorHandler *errHandler = (ErrorHandler *)new HandlerBase();
  parser->setErrorHandler(errHandler);

  try {
    parser->parse(project);
  } catch (const XMLException &toCatch) {
    char *message = XMLString::transcode(toCatch.getMessage());
    Error("Exception message is: %s\n", message);
    XMLString::release(&message);
    return;
  } catch (const DOMException &toCatch) {
    char *message = XMLString::transcode(toCatch.msg);
    Error("Exception message is: %s\n", message);
    XMLString::release(&message);
    return;
  } catch (...) {
    Error("Unexpected Exception \n");
    return;
  }

  DOMDocument *pXMLDoc = parser->getDocument();
#endif

  if (!ExtractProjectName(pXMLDoc) || !m_Name.IsValid()) {
    Warning("Can't extract Visual Studio Project '%s' name.\n",
            project);
    return false;
  }

  char baseDir[MAX_PATH];
  Q_ExtractFilePath(project, baseDir, sizeof(baseDir));
  Q_StripTrailingSlash(baseDir);
  m_BaseDir = baseDir;

  if (!ExtractConfigurations(pXMLDoc) || m_Configurations.Count() == 0) {
    Warning(
        "Can't find any configurations to load in Visual Studio Project "
        "'%s'.\n",
        project);
    return false;
  }

  if (!ExtractFiles(pXMLDoc)) {
    Warning("Can't extract files from Visual Studio Project '%s'.\n", project);
    return false;
  }

#if _LINUX
  delete pXMLDoc;
  delete errHandler;
#endif

  m_bProjectLoaded = true;
  return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of different configurations loaded
//-----------------------------------------------------------------------------
intp CVCProjConvert::GetNumConfigurations() const {
  Assert(m_bProjectLoaded);
  return m_Configurations.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns the index of a config with this name, -1 on err
//-----------------------------------------------------------------------------
intp CVCProjConvert::FindConfiguration(CUtlSymbol name) {
  if (!name.IsValid()) return -1;

  for (intp i = 0; i < m_Configurations.Count(); i++) {
    if (m_Configurations[i].GetName() == name) return i;
  }

  return -1;
}

//-----------------------------------------------------------------------------
// Purpose: extracts the value of the xml attrib "attribName"
//-----------------------------------------------------------------------------
CUtlSymbol CVCProjConvert::GetXMLAttribValue(IXMLDOMElement *p,
                                             const char *attribName) {
  if (!p) return CUtlSymbol();

#ifdef _WIN32
  _variant_t vtValue = {};
  HRESULT hr{p->getAttribute(_bstr_t(attribName), &vtValue)};
  // element not found
  if (FAILED(hr) || vtValue.vt == VT_NULL) return CUtlSymbol();

  Assert(vtValue.vt == VT_BSTR);
  CUtlSymbol name(static_cast<char *>(_bstr_t(vtValue.bstrVal)));
#elif _LINUX
  const XMLCh *xAttrib = XMLString::transcode(attribName);
  const XMLCh *value = p->getAttribute(xAttrib);
  // element not found
  if (value == NULL) return CUtlSymbol();

  char *transValue = XMLString::transcode(value);
  CUtlSymbol name(transValue);
  XMLString::release(&xAttrib);
  XMLString::release(&transValue);
#endif

  return name;
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of this node
//-----------------------------------------------------------------------------
CUtlSymbol CVCProjConvert::GetXMLNodeName(IXMLDOMElement *p) {
  CUtlSymbol name;
  if (!p) return name;

#ifdef _WIN32
  BSTR bstrName;
  HRESULT hr = p->get_nodeName(&bstrName);
  if (FAILED(hr)) return name;

  name = static_cast<char *>(_bstr_t(bstrName));
  return name;
#elif _LINUX
  Assert(0);
  Error("Function CVCProjConvert::GetXMLNodeName not implemented\n");
  return name;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: returns the config object at this index
//-----------------------------------------------------------------------------
CVCProjConvert::CConfiguration &CVCProjConvert::GetConfiguration(intp i) {
  Assert(m_bProjectLoaded);
  Assert(m_Configurations.IsValidIndex(i));
  return m_Configurations[i];
}

//-----------------------------------------------------------------------------
// Purpose: extracts the project name from the loaded vcxproj
//-----------------------------------------------------------------------------
bool CVCProjConvert::ExtractProjectName(IXMLDOMDocument *pDoc) {
#ifdef _WIN32
  CComPtr<IXMLDOMNodeList> pProj;
  CComPtr<IXMLDOMNode> pNode;
  CComQIPtr<IXMLDOMElement> pElem(pNode);

  HRESULT hr{
      pDoc->getElementsByTagName(_bstr_t("VisualStudioProject"), &pProj)};
  if (FAILED(hr)) return false;

  long len = 0;
  hr = pProj->get_length(&len);
  if (FAILED(hr)) return false;

  Assert(len == 1);
  if (len == 1) {
    hr = pProj->get_item(0, &pNode);
    if (FAILED(hr)) return false;
  }

  m_Name = GetXMLAttribValue(pElem, "Name");
  return true;
#elif _LINUX
  DOMNodeList *nodes =
      pDoc->getElementsByTagName(_bstr_t("VisualStudioProject"));
  if (nodes) {
    int len = nodes->getLength();
    if (len == 1) {
      DOMNode *node = nodes->item(0);
      if (node) {
        m_Name = GetXMLAttribValue(node, "Name");
        return true;
      }
    }
  }
  return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: extracts the list of configuration names from the vcxproj
//-----------------------------------------------------------------------------
bool CVCProjConvert::ExtractConfigurations(IXMLDOMDocument *pDoc) {
  m_Configurations.RemoveAll();

  if (!pDoc) return false;

#ifdef _WIN32
  CComPtr<IXMLDOMNodeList> pConfigs;
  HRESULT hr = pDoc->getElementsByTagName(_bstr_t("Configuration"), &pConfigs);
  if (FAILED(hr)) return false;

  long len = 0;
  hr = pConfigs->get_length(&len);
  if (FAILED(hr)) return false;

  CComPtr<IXMLDOMNode> pNode;
  for (int i = 0; i < len; i++) {
    hr = pConfigs->get_item(i, &pNode);
    if (FAILED(hr)) continue;

    CComQIPtr<IXMLDOMElement> pElem(pNode);
    CUtlSymbol configName = GetXMLAttribValue(pElem, "Name");
    if (!configName.IsValid()) continue;

    intp newIndex = m_Configurations.AddToTail();
    CConfiguration &config = m_Configurations[newIndex];
    config.SetName(configName);

    if (!ExtractIncludes(pElem, config)) return false;
  }
#elif _LINUX
  DOMNodeList *nodes = pDoc->getElementsByTagName(_bstr_t("Configuration"));
  if (nodes) {
    int len = nodes->getLength();
    for (int i = 0; i < len; i++) {
      DOMNode *node = nodes->item(i);
      if (node) {
        CUtlSymbol configName = GetXMLAttribValue(node, "Name");
        if (configName.IsValid()) {
          int newIndex = m_Configurations.AddToTail();
          CConfiguration &config = m_Configurations[newIndex];
          config.SetName(configName);
          ExtractIncludes(node, config);
        }
      }
    }
  }
#endif
  return true;
}

//-----------------------------------------------------------------------------
// Purpose: extracts the list of defines and includes used for this config
//-----------------------------------------------------------------------------
bool CVCProjConvert::ExtractIncludes(IXMLDOMElement *pDoc,
                                     CConfiguration &config) {
  config.ResetDefines();
  config.ResetIncludes();

  if (!pDoc) return false;

#ifdef _WIN32
  CComPtr<IXMLDOMNodeList> pTools;
  HRESULT hr = pDoc->getElementsByTagName(_bstr_t("Tool"), &pTools);
  if (FAILED(hr)) return false;

  long len = 0;
  hr = pTools->get_length(&len);
  if (FAILED(hr)) return false;

  CComPtr<IXMLDOMNode> pNode;
  for (int i = 0; i < len; i++) {
    hr = pTools->get_item(i, &pNode);
    if (FAILED(hr)) return false;

    CComQIPtr<IXMLDOMElement> pElem(pNode);
    CUtlSymbol toolName = GetXMLAttribValue(pElem, "Name");
    if (toolName == "VCCLCompilerTool") {
      CUtlSymbol defines = GetXMLAttribValue(pElem, "PreprocessorDefinitions");
      char *str = (char *)_alloca(Q_strlen(defines.String()) + 1);
      Assert(str);

      Q_strcpy(str, defines.String());

      // now tokenize the string on the ";" char
      char *delim = strchr(str, ';');
      char *curpos = str;

      while (delim) {
        *delim = 0;
        delim++;

        if (Q_stricmp(curpos, "WIN32") && Q_stricmp(curpos, "_WIN32") &&
            Q_stricmp(curpos, "_WIN64") && Q_stricmp(curpos, "_WINDOWS") &&
            Q_stricmp(curpos, "WINDOWS"))  // don't add WIN32/64 defines
        {
          config.AddDefine(curpos);
        }

        curpos = delim;
        delim = strchr(delim, ';');
      }

      if (Q_stricmp(curpos, "WIN32") && Q_stricmp(curpos, "_WIN32") &&
          Q_stricmp(curpos, "_WIN64") && Q_stricmp(curpos, "_WINDOWS") &&
          Q_stricmp(curpos, "WINDOWS"))  // don't add WIN32/64 defines
      {
        config.AddDefine(curpos);
      }

      CUtlSymbol includes =
          GetXMLAttribValue(pElem, "AdditionalIncludeDirectories");
      char *str2 = (char *)_alloca(Q_strlen(includes.String()) + 1);
      Assert(str2);
      Q_strcpy(str2, includes.String());

      // now tokenize the string on the ";" char
      delim = strchr(str2, ',');
      curpos = str2;

      while (delim) {
        *delim = 0;
        delim++;
        config.AddInclude(curpos);
        curpos = delim;
        delim = strchr(delim, ',');
      }

      config.AddInclude(curpos);
    }
  }
#elif _LINUX
  DOMNodeList *nodes = pDoc->getElementsByTagName(_bstr_t("Tool"));
  if (nodes) {
    int len = nodes->getLength();
    for (int i = 0; i < len; i++) {
      DOMNode *node = nodes->item(i);
      if (node) {
        CUtlSymbol toolName = GetXMLAttribValue(node, "Name");
        if (toolName == "VCCLCompilerTool") {
          CUtlSymbol defines =
              GetXMLAttribValue(node, "PreprocessorDefinitions");
          char *str = (char *)_alloca(Q_strlen(defines.String()) + 1);
          Assert(str);
          Q_strcpy(str, defines.String());
          // now tokenize the string on the ";" char
          char *delim = strchr(str, ';');
          char *curpos = str;
          while (delim) {
            *delim = 0;
            delim++;
            if (Q_stricmp(curpos, "WIN32") && Q_stricmp(curpos, "_WIN32") &&
                Q_stricmp(curpos, "_WINDOWS") &&
                Q_stricmp(curpos, "WINDOWS"))  // don't add WIN32 defines
            {
              config.AddDefine(curpos);
            }
            curpos = delim;
            delim = strchr(delim, ';');
          }
          if (Q_stricmp(curpos, "WIN32") && Q_stricmp(curpos, "_WIN32") &&
              Q_stricmp(curpos, "_WINDOWS") &&
              Q_stricmp(curpos, "WINDOWS"))  // don't add WIN32 defines
          {
            config.AddDefine(curpos);
          }

          CUtlSymbol includes =
              GetXMLAttribValue(node, "AdditionalIncludeDirectories");
          char *str2 = (char *)_alloca(Q_strlen(includes.String()) + 1);
          Assert(str2);
          Q_strcpy(str2, includes.String());
          // now tokenize the string on the ";" char
          char token = ',';
          delim = strchr(str2, token);
          if (!delim) {
            token = ';';
            delim = strchr(str2, token);
          }
          curpos = str2;
          while (delim) {
            *delim = 0;
            delim++;
            Q_FixSlashes(curpos);
            char fullPath[MAX_PATH];
            Q_snprintf(fullPath, sizeof(fullPath), "%s/%s", m_BaseDir.String(),
                       curpos);
            Q_StripTrailingSlash(fullPath);
            config.AddInclude(fullPath);
            curpos = delim;
            delim = strchr(delim, token);
          }
          Q_FixSlashes(curpos);
          Q_strlower(curpos);
          char fullPath[MAX_PATH];
          Q_snprintf(fullPath, sizeof(fullPath), "%s/%s", m_BaseDir.String(),
                     curpos);
          Q_StripTrailingSlash(fullPath);
          config.AddInclude(fullPath);
        }
      }
    }
  }
#endif
  return true;
}

//-----------------------------------------------------------------------------
// Purpose: walks a particular files config entry and removes an files not valid
// for this config
//-----------------------------------------------------------------------------
bool CVCProjConvert::IterateFileConfigurations(IXMLDOMElement *pFile,
                                               CUtlSymbol fileName) {
#ifdef _WIN32
  CComPtr<IXMLDOMNodeList> pConfigs;
  HRESULT hr =
      pFile->getElementsByTagName(_bstr_t("FileConfiguration"), &pConfigs);
  if (FAILED(hr)) return false;

  long len = 0;
  hr = pConfigs->get_length(&len);
  if (FAILED(hr)) return false;

  CComPtr<IXMLDOMNode> pNode;
  for (int i = 0; i < len; i++) {
    hr = pConfigs->get_item(i, &pNode);
    if (FAILED(hr)) return false;

    CComQIPtr<IXMLDOMElement> pElem(pNode);
    CUtlSymbol configName = GetXMLAttribValue(pElem, "Name");
    CUtlSymbol excluded = GetXMLAttribValue(pElem, "ExcludedFromBuild");

    if (configName.IsValid() && excluded.IsValid()) {
      intp index = FindConfiguration(configName);

      if (index > 0 && excluded == "TRUE") {
        m_Configurations[index].RemoveFile(fileName);
      }
    }
  }  // for

#elif _LINUX
  DOMNodeList *nodes =
      pFile->getElementsByTagName(_bstr_t("FileConfiguration"));
  if (nodes) {
    int len = nodes->getLength();
    for (int i = 0; i < len; i++) {
      DOMNode *node = nodes->item(i);
      if (node) {
        CUtlSymbol configName = GetXMLAttribValue(node, "Name");
        CUtlSymbol excluded = GetXMLAttribValue(node, "ExcludedFromBuild");
        if (configName.IsValid() && excluded.IsValid()) {
          intp index = FindConfiguration(configName);
          if (index >= 0 && excluded == "TRUE") {
            m_Configurations[index].RemoveFile(fileName);
          }
        }
      }

    }  // for
  }  // if
#endif

  return true;
}

//-----------------------------------------------------------------------------
// Purpose: walks the file elements in the vcxproj and inserts them into configs
//-----------------------------------------------------------------------------
bool CVCProjConvert::ExtractFiles(IXMLDOMDocument *pDoc) {
  if (!pDoc) return false;

  Assert(m_Configurations.Count());  // some configs must be loaded first

#ifdef _WIN32
  CComPtr<IXMLDOMNodeList> pFiles;
  CComPtr<IXMLDOMNode> pNode;
  CComQIPtr<IXMLDOMElement> pElem(pNode);

  HRESULT hr{pDoc->getElementsByTagName(_bstr_t("File"), &pFiles)};
  if (FAILED(hr)) return false;

  long len = 0;

  hr = pFiles->get_length(&len);
  if (FAILED(hr)) return false;

  for (long i = 0; i < len; i++) {
    hr = pFiles->get_item(i, &pNode);
    if (FAILED(hr)) return false;

    CUtlSymbol fileName = GetXMLAttribValue(pElem, "RelativePath");
    if (!fileName.IsValid()) return false;

    CConfiguration::FileType_e type = GetFileType(fileName.String());
    CConfiguration::CFileEntry fileEntry(fileName.String(), type);

    // add the file to all configs
    for (auto &c : m_Configurations) c.InsertFile(fileEntry);

    // now remove the excluded ones
    if (!IterateFileConfigurations(pElem, fileName)) return false;
  }

  return true;
#elif _LINUX
  DOMNodeList *nodes = pDoc->getElementsByTagName(_bstr_t("File"));
  if (nodes) {
    int len = nodes->getLength();
    for (int i = 0; i < len; i++) {
      DOMNode *node = nodes->item(i);
      if (node) {
        CUtlSymbol fileName = GetXMLAttribValue(node, "RelativePath");
        if (fileName.IsValid()) {
          char fixedFileName[MAX_PATH];
          Q_strncpy(fixedFileName, fileName.String(), sizeof(fixedFileName));
          if (fixedFileName[0] == '.' && fixedFileName[1] == '\\') {
            Q_memmove(fixedFileName, fixedFileName + 2,
                      sizeof(fixedFileName) - 2);
          }

          Q_FixSlashes(fixedFileName);
          FindFileCaseInsensitive(fixedFileName, sizeof(fixedFileName));
          CConfiguration::FileType_e type = GetFileType(fileName.String());
          CConfiguration::CFileEntry fileEntry(fixedFileName, type);
          for (int i = 0; i < m_Configurations.Count();
               i++)  // add the file to all configs
          {
            CConfiguration &config = m_Configurations[i];
            config.InsertFile(fileEntry);
          }
          IterateFileConfigurations(
              node, fixedFileName);  // now remove the excluded ones
        }
      }
    }  // for
  }
  return true;
#endif
}

#ifdef _LINUX
static char fileName[MAX_PATH];
int CheckName(const struct dirent *dir) {
  return !strcasecmp(dir->d_name, fileName);
}

const char *findFileInDirCaseInsensitive(const char *file) {
  const char *dirSep = strrchr(file, '/');
  if (!dirSep) {
    dirSep = strrchr(file, '\\');
    if (!dirSep) {
      return NULL;
    }
  }

  char *dirName = static_cast<char *>(alloca((dirSep - file) + 1));
  if (!dirName) return NULL;

  Q_strncpy(dirName, file, dirSep - file);
  dirName[dirSep - file] = '\0';

  struct dirent **namelist;
  int n;

  Q_strncpy(fileName, dirSep + 1, MAX_PATH);

  n = scandir(dirName, &namelist, CheckName, alphasort);

  if (n > 0) {
    while (n > 1) {
      free(namelist[n]);  // free the malloc'd strings
      n--;
    }

    Q_snprintf(fileName, sizeof(fileName), "%s/%s", dirName,
               namelist[0]->d_name);
    return fileName;
  } else {
    // last ditch attempt, just return the lower case version!
    Q_strncpy(fileName, file, sizeof(fileName));
    Q_strlower(fileName);
    return fileName;
  }
}

void CVCProjConvert::FindFileCaseInsensitive(
    char *fileName, [[maybe_unused]] int fileNameSize) {
  char filePath[MAX_PATH];
  V_sprintf_safe(filePath, "%s/%s", m_BaseDir.String(), fileName);

  // found the filename directly
  struct _stat buf;
  if (_stat(filePath, &buf) == 0) return;

  const char *realName = findFileInDirCaseInsensitive(filePath);
  if (realName) {
    Q_strncpy(fileName, realName + strlen(m_BaseDir.String()) + 1,
              fileNameSize);
  }
}

#endif

// Purpose: extracts the generic type of a file being loaded
CVCProjConvert::CConfiguration::FileType_e CVCProjConvert::GetFileType(
    const char *fileName) {
  CConfiguration::FileType_e type = CConfiguration::FILE_TYPE_UNKNOWN_E;

  char ext[10];
  Q_ExtractFileExtension(fileName, ext, sizeof(ext));

  if (!Q_stricmp(ext, "lib")) {
    type = CConfiguration::FILE_LIBRARY;
  } else if (!Q_stricmp(ext, "h")) {
    type = CConfiguration::FILE_HEADER;
  } else if (!Q_stricmp(ext, "hh")) {
    type = CConfiguration::FILE_HEADER;
  } else if (!Q_stricmp(ext, "hpp")) {
    type = CConfiguration::FILE_HEADER;
  } else if (!Q_stricmp(ext, "cpp")) {
    type = CConfiguration::FILE_SOURCE;
  } else if (!Q_stricmp(ext, "cxx")) {
    type = CConfiguration::FILE_SOURCE;
  } else if (!Q_stricmp(ext, "c")) {
    type = CConfiguration::FILE_SOURCE;
  } else if (!Q_stricmp(ext, "cc")) {
    type = CConfiguration::FILE_SOURCE;
  }

  return type;
}

}  // namespace se::vprojtomake