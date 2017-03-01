#ifndef _OCRTRANSLATEENGINE_H
#define _OCRTRANSLATEENGINE_H

/*
 * Author: Sriram Aananthakrishnan, 2017 *
 */

#include "ocrObjectInfo.h"
#include <string>

/**********************
 * TranslateException *
 **********************/
class TranslateException : public std::exception {
  std::string m_what;
 public:
  TranslateException(std::string what);
  virtual const char* what() const throw();
  ~TranslateException() throw();
};

// We will do some book keeping for the generated AST fragments
// during the translation phase.
// Useful in accessing them at various points of translation
// Motivation : We will need the GUIDs of datablock, edts and events
// at various steps of generating the AST for OCR objects
/*****************
 * OcrDbkAstInfo *
 *****************/
class OcrDbkAstInfo {
  std::string m_dbkname;
  // SgSymbol represents the concept of a name in ROSE
  // With a symbol we can access a variable's declaration, scope and build SgVarRefExp
  //! SgSymbol associated with the ocrGuid of the datablock
  SgVariableSymbol* m_ocrGuidSymbol;
  //! SgSymbol associated with the ptr of the datablock
  SgVariableSymbol* m_ptrSymbol;
 public:
  OcrDbkAstInfo(std::string dbkname, SgVariableSymbol* ocrGuidSymbol, SgVariableSymbol* ptrSymbol);
  SgVariableSymbol* getOcrGuidSymbol() const;
  SgVariableSymbol* getPtrSymbol() const;
  std::string str() const;
};
typedef boost::shared_ptr<OcrDbkAstInfo> OcrDbkAstInfoPtr;

/*****************
 * OcrEdtAstInfo *
 *****************/
class OcrEdtAstInfo {
  std::string m_edtname;
  SgType* m_depElemType;
  SgFunctionDeclaration* m_edtDecl;
  SgVariableSymbol* m_edtTemplateGuid;
  SgVariableSymbol* m_depElemStructSymbol;
  SgVariableSymbol* m_edtGuid;
 public:
  OcrEdtAstInfo(std::string edtname, SgType* depElemType, SgFunctionDeclaration* edtDecl);
  SgFunctionDeclaration* getEdtFunctionDeclaration() const;
  SgType* getDepElemStructType() const;
  SgVariableSymbol* getEdtTemplateGuid() const;
  SgVariableSymbol* getDepElemStructSymbol() const;
  SgVariableSymbol* getEdtGuid() const;
  void setEdtTemplateGuid(SgVariableSymbol* edtTemplateGuid);
  void setDepElemStructSymbol(SgVariableSymbol* depElemStructSymbol);
  void setEdtGuid(SgVariableSymbol* edtGuid);
  std::string str() const;
};
typedef boost::shared_ptr<OcrEdtAstInfo> OcrEdtAstInfoPtr;

/*****************
 * OcrEvtAstInfo *
 *****************/
class OcrEvtAstInfo {
  std::string m_evtname;
  SgVariableSymbol* m_evtGuid;
 public:
  OcrEvtAstInfo(std::string evtname, SgVariableSymbol* evtGuid);
  SgVariableSymbol* getEvtGuid() const;
};
typedef boost::shared_ptr<OcrEvtAstInfo> OcrEvtAstInfoPtr;

/*********************
 * OcrAstInfoManager *
 *********************/
typedef std::map<std::string, OcrDbkAstInfoPtr> OcrDbkAstInfoMap;
typedef std::pair<std::string, OcrDbkAstInfoPtr> OcrDbkAstInfoMapElem;
typedef std::map<std::string, OcrEdtAstInfoPtr> OcrEdtAstInfoMap;
typedef std::pair<std::string, OcrEdtAstInfoPtr> OcrEdtAstInfoMapElem;
typedef std::map<std::string, OcrEvtAstInfoPtr> OcrEvtAstInfoMap;
typedef std::pair<std::string, OcrEvtAstInfoPtr> OcrEvtAstInfoMapElem;
class OcrAstInfoManager {
  OcrDbkAstInfoMap m_ocrDbkAstInfoMap;
  OcrEdtAstInfoMap m_ocrEdtAstInfoMap;
  OcrEvtAstInfoMap m_ocrEvtAstInfoMap;
 public:
  OcrAstInfoManager();
  bool regOcrDbkAstInfo(std::string dbkname, SgVariableSymbol* ocrGuidSymbol, SgVariableSymbol* ptrSymbol);
  bool regOcrEdtAstInfo(std::string edtName, SgType* depElemType, SgFunctionDeclaration* edtDecl);
  bool regOcrEvtAstInfo(std::string edtName, SgVariableSymbol* evtGuid);
  OcrDbkAstInfoPtr getOcrDbkAstInfo(std::string dbkname);
  OcrEdtAstInfoPtr getOcrEdtAstInfo(std::string edtname);
  OcrEvtAstInfoPtr getOcrEvtAstInfo(std::string evtname);
  std::string ocrEvtAstInfoMap2Str() const;
};

/*********************
 * Utility Functions *
 *********************/
SgVariableSymbol* GetVariableSymbol(SgVariableDeclaration* vdecl, std::string vname);

/************************
 * DepElemVarRefExpPass *
 ************************/
//! Top-down AST traversal which replaces the sub-tree (SgVarRefExp)
//! of any depElem with the AST (SgArrowExp(SgVarRefExp, SgVarRefExp))
//! example: x => depElem->x
//! Traversal done in post-order to avoid visiting the transformed AST
class DepElemVarRefExpPass : public AstSimpleProcessing {
  SgScopeStatement* m_scope;
  SgName m_depElemStructName;
  // SgVariableSymbols picked from the task annotation
  // The scope of these symbols is the same as the scope of task annotation
  // Since we just cut-paste the AST from the task annotation to the EDT function
  // these variable symbols are the same as gathered from the task annotation
  // We can directly use the pointers for comparison when replacing the SgVarRefExp
  // of the dependent elements
  std::set<SgVariableSymbol*> m_varSymbolSet;
 public:
  DepElemVarRefExpPass(SgScopeStatement* root, SgName depElemStructName, std::list<SgVarRefExp*> depElems);
  void visit(SgNode* sgn);
  void atTraversalEnd();
};

/*****************
 * OcrTranslator *
 *****************/
//! Driver for OCR Translation
class OcrTranslator {
  SgProject* m_project;
  const OcrObjectManager& m_ocrObjectManager;
  OcrAstInfoManager m_ocrAstInfoManager;
 private:
  void insertOcrHeaderFiles();
  void outlineEdt(std::string, OcrEdtContextPtr edt);
  //! datablock translation involves building the following AST fragments
  //! 1. AST for variable declaration of ocrGuid for the datablock
  //! 2. AST for variable declaration of the pointer (u64*) for the datablock
  //! 3. Replace malloc calls with ocrDbCreate API call
  //! 4. Bookkeeping SgSymbols associated with the datablock
  void translateDbk(std::string name, OcrDbkContextPtr dbkContext);
  void setupEdtEvtCreate(std::string edtname, OcrEdtContextPtr edtContext);
  void setupEdtTemplate(std::string edtname, OcrEdtContextPtr edtContext);
  void setupEdtDepElems(std::string edtname, OcrEdtContextPtr edtContext);
  void setupEdtCreate(std::string edtname, OcrEdtContextPtr edtContext);
  void setupEdtDepDbks(std::string edtname, OcrEdtContextPtr edtContext);
  void setupEdtDepEvts(std::string edtname, OcrEdtContextPtr edtContext);
  void setupEdtEvtsSatisfy(std::string edtname, OcrEdtContextPtr edtContext);
 public:
  OcrTranslator(SgProject* project, const OcrObjectManager& ocrObjectManager);
  void outlineEdts();
  void setupEdts();
  void translateDbks();
  // main driver function
  void translate();
};

#endif
