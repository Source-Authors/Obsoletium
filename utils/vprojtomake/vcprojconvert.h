// Copyright Valve Corporation, All rights reserved.

#ifndef SE_UTILS_VPROJTOMAKE_VCPROJCONVERT_H_
#define SE_UTILS_VPROJTOMAKE_VCPROJCONVERT_H_

#include "tier1/utlvector.h"
#include "tier1/utlsymbol.h"

struct IXMLDOMDocument;
struct IXMLDOMElement;

namespace se::vprojtomake {

class CVCProjConvert {
 public:
  CVCProjConvert();
  ~CVCProjConvert();

  bool LoadProject(const char *project);
  intp GetNumConfigurations() const;

  CUtlSymbol &GetName() { return m_Name; }
  CUtlSymbol &GetBaseDir() { return m_BaseDir; }

  class CConfiguration {
   public:
    CConfiguration() = default;
    ~CConfiguration() = default;

    enum FileType_e {
      FILE_SOURCE,
      FILE_HEADER,
      FILE_LIBRARY,
      FILE_TYPE_UNKNOWN_E
    };

    class CFileEntry {
     public:
      CFileEntry(CUtlSymbol name, FileType_e type) {
        m_Name = name;
        m_Type = type;
      }
      ~CFileEntry() = default;

      const char *GetName() { return m_Name.String(); }
      FileType_e GetType() { return m_Type; }
      bool operator==(const CFileEntry other) const {
        return m_Name == other.m_Name;
      }

     private:
      FileType_e m_Type;
      CUtlSymbol m_Name;
    };

    void InsertFile(CFileEntry file) { m_Files.AddToTail(file); }
    void RemoveFile(CUtlSymbol file) {
      m_Files.FindAndRemove(CFileEntry(file, FILE_TYPE_UNKNOWN_E));
    }  // file type doesn't matter on remove
    void SetName(CUtlSymbol name) { m_Name = name; }

    intp GetNumFileNames() const { return m_Files.Count(); }
    const char *GetFileName(intp i) { return m_Files[i].GetName(); }
    FileType_e GetFileType(intp i) { return m_Files[i].GetType(); }
    CUtlSymbol &GetName() { return m_Name; }

    void ResetDefines() { m_Defines.RemoveAll(); }
    void AddDefine(CUtlSymbol define) { m_Defines.AddToTail(define); }
    intp GetNumDefines() const { return m_Defines.Count(); }
    const char *GetDefine(intp i) { return m_Defines[i].String(); }

    void ResetIncludes() { m_Includes.RemoveAll(); }
    void AddInclude(CUtlSymbol include) { m_Includes.AddToTail(include); }
    intp GetNumIncludes() const { return m_Includes.Count(); }
    const char *GetInclude(intp i) { return m_Includes[i].String(); }

   private:
    CUtlSymbol m_Name;
    CUtlVector<CUtlSymbol> m_Defines;
    CUtlVector<CUtlSymbol> m_Includes;
    CUtlVector<CFileEntry> m_Files;
  };

  CConfiguration &GetConfiguration(intp i);
  intp FindConfiguration(CUtlSymbol name);

 private:
  bool ExtractFiles(IXMLDOMDocument *pDoc);
  bool ExtractConfigurations(IXMLDOMDocument *pDoc);
  bool ExtractProjectName(IXMLDOMDocument *pDoc);
  bool ExtractIncludes(IXMLDOMElement *pDoc, CConfiguration &config);
  bool IterateFileConfigurations(IXMLDOMElement *pFile, CUtlSymbol fileName);

  // helper funcs
  CUtlSymbol GetXMLNodeName(IXMLDOMElement *p);
  CUtlSymbol GetXMLAttribValue(IXMLDOMElement *p, const char *attribName);
  CConfiguration::FileType_e GetFileType(const char *fileName);

#ifdef _LINUX
  void FindFileCaseInsensitive(char *file, int fileNameSize);
#endif

  // data
  CUtlVector<CConfiguration> m_Configurations;
  CUtlSymbol m_Name;
  CUtlSymbol m_BaseDir;

#ifdef _WIN32
  long m_hrComInit;
#endif

  bool m_bProjectLoaded;
};

}  // namespace se::vprojtomake

#endif  // !SE_UTILS_VPROJTOMAKE_VCPROJCONVERT_H_
