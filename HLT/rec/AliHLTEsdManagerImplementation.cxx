// $Id$

//**************************************************************************
//* This file is property of and copyright by the ALICE HLT Project        * 
//* ALICE Experiment at CERN, All rights reserved.                         *
//*                                                                        *
//* Primary Authors: Matthias Richter <Matthias.Richter@ift.uib.no>        *
//*                  for The ALICE HLT Project.                            *
//*                                                                        *
//* Permission to use, copy, modify and distribute this software and its   *
//* documentation strictly for non-commercial purposes is hereby granted   *
//* without fee, provided that the above copyright notice appears in all   *
//* copies and that both the copyright notice and this permission notice   *
//* appear in the supporting documentation. The authors make no claims     *
//* about the suitability of this software for any purpose. It is          *
//* provided "as is" without express or implied warranty.                  *
//**************************************************************************

/** @file   AliHLTEsdManagerImplementation.cxx
    @author Matthias Richter
    @date   
    @brief  Manager for merging and writing of HLT ESDs
*/

#include "AliHLTEsdManagerImplementation.h"
#include "AliHLTComponent.h"
#include "AliESDEvent.h"
#include "AliHLTMessage.h"
#include "AliESDEvent.h"
#include "AliESDtrack.h"
#include "TFile.h"
#include "TTree.h"
#include "TClass.h"
#include "TObject.h"
#include "TObjectTable.h"
#include "TSystem.h"
#include "TChain.h"
#include "TList.h"

/** ROOT macro for the implementation of ROOT specific class methods */
ClassImp(AliHLTEsdManagerImplementation)

AliHLTEsdManagerImplementation::AliHLTEsdManagerImplementation()
  :
  fESDs(),
  fDirectory()
{
  // see header file for class documentation
  // or
  // refer to README to build package
  // or
  // visit http://web.ift.uib.no/~kjeks/doc/alice-hlt
}

AliHLTEsdManagerImplementation::~AliHLTEsdManagerImplementation()
{
  // see header file for class documentation
  for (unsigned int i=0; i<fESDs.size(); i++) {
    if (fESDs[i]) {
      delete fESDs[i];
    }
    fESDs[i]=NULL;
  }
}

AliHLTEsdManagerImplementation::AliHLTEsdListEntry* AliHLTEsdManagerImplementation::Find(AliHLTComponentDataType dt) const
{
  // see header file for class documentation
  AliHLTEsdListEntry* pEntry=NULL;
  for (unsigned int i=0; i<fESDs.size(); i++) {
    if (fESDs[i] && *(fESDs[i])==dt) {
      pEntry=const_cast<AliHLTEsdListEntry*>(fESDs[i]);
    }
  }
  return pEntry;
}

int AliHLTEsdManagerImplementation::WriteESD(const AliHLTUInt8_t* pBuffer, AliHLTUInt32_t size,
			       AliHLTComponentDataType dt, AliESDEvent* tgtesd, int eventno)
{
  // see header file for class documentation
  if (!pBuffer && size<=0) return -EINVAL;
  int iResult=0;
  AliHLTUInt32_t firstWord=*((AliHLTUInt32_t*)pBuffer);
  if (firstWord==size-sizeof(AliHLTUInt32_t)) {
    HLTDebug("create object from block %s size %d", AliHLTComponent::DataType2Text(dt).c_str(), size);
    AliHLTMessage msg(const_cast<AliHLTUInt8_t*>(pBuffer), size);
    TClass* objclass=msg.GetClass();
    TObject* pObj=msg.ReadObject(objclass);
    if (pObj && objclass) {
      HLTDebug("object %p type %s created", pObj, objclass->GetName());
      AliESDEvent* pESD=dynamic_cast<AliESDEvent*>(pObj);
      TTree* pTree=NULL;
      if (!pESD) {
	pTree=dynamic_cast<TTree*>(pObj);
	if (pTree) {
	  pESD=new AliESDEvent;
	  pESD->CreateStdContent();
	  if (pTree->GetEntries()>0) {
	    if (pTree->GetEntries()>1) {
	      HLTWarning("only one entry allowed for ESD embedded into tree, data block %s contains tree with %d entires, taking first entry",
			 AliHLTComponent::DataType2Text(dt).c_str(), pTree->GetEntries());
	    }
	    pESD->ReadFromTree(pTree);
	    pTree->GetEvent(0);
	  }
	} else {
	  HLTWarning("tree of data block %s has no events, skipping data block", AliHLTComponent::DataType2Text(dt).c_str());
	}
      }
      if (pESD) {
	AliHLTEsdListEntry* entry=Find(dt);
	if (!entry) {
	  AliHLTEsdListEntry* newEntry=new AliHLTEsdListEntry(dt);
	  if (!fDirectory.IsNull()) {
	    newEntry->SetDirectory(fDirectory);
	  }
	  fESDs.push_back(newEntry);
	}
	if (tgtesd) {
#if 0 // later extension !defined(HAVE_NOT_ESD_NONSTD)
	  entry=Find(dt);
	  if (entry) {
	    entry->CopyNonEmptyObjects(tgtesd, pESD);
	  } else {
	    HLTError("internal mismatch, can not create list entry");
	    iResult=-ENOMEM;
	  }
#elif !defined(HAVE_NOT_ESD_COPY)
	  *tgtesd=*pESD;
#else //HAVE_NOT_ESD_COPY
	  static bool warningPrinted=false;
	  if (!warningPrinted) {
	    HLTWarning("old version of AliESDEvent does not provide assignment operator, skip merging to global hltEsd");
	  }
	  warningPrinted=true;
#endif //HAVE_NOT_ESD_COPY
	} else {
	entry=Find(dt);
	if (entry) {
	  entry->WriteESD(pESD, eventno);
	} else {
	  HLTError("internal mismatch, can not create list entry");
	  iResult=-ENOMEM;
	}
	}
      } else {
	HLTWarning("data block %s is not of class type AliESDEvent, ignoring ...", AliHLTComponent::DataType2Text(dt).c_str());
      }
      if (pTree) {
	// ESD has been created and must be cleaned up
	pESD->Reset();
	delete pESD;
	pESD=NULL;
      }
      delete pObj;
      pObj=NULL;
    } else {
    }
  }
  return iResult;
}

int AliHLTEsdManagerImplementation::PadESDs(int eventno)
{
  // see header file for class documentation
  int iResult=0;
  for (unsigned int i=0; i<fESDs.size(); i++) {
    if (fESDs[i]) {
      int res=fESDs[i]->WriteESD(NULL, eventno);
      if (res<0 && iResult>=0) iResult=res;
    }
  }
  return iResult;
}

void AliHLTEsdManagerImplementation::SetDirectory(const char* directory)
{
  // see header file for class documentation
  if (!directory) return;
  fDirectory=directory;
  for (unsigned int i=0; i<fESDs.size(); i++) {
    if (fESDs[i]) {
      fESDs[i]->SetDirectory(directory);
    }
  }
}

TString AliHLTEsdManagerImplementation::GetFileNames(AliHLTComponentDataType dt) const
{
  TString result;
  for (unsigned int i=0; i<fESDs.size(); i++) {
    if (fESDs[i] && *(fESDs[i])==dt) {
      if (!result.IsNull()) result+=" ";
      result+=fESDs[i]->GetFileName();
    }
  }
  return result;
}

TTree* AliHLTEsdManagerImplementation::EmbedIntoTree(AliESDEvent* pESD, const char* name, const char* title)
{
  // see header file for class documentation
  int iResult=0;
  TTree* pTree=new TTree(name, title);
  if (pTree) {
    pESD->WriteToTree(pTree);
    pTree->Fill();
    pTree->GetUserInfo()->Add(pESD);
  } else {
    iResult=-ENOMEM;
  }

  if (iResult<0) {
    pTree->GetUserInfo()->Clear();
    delete pTree;
  }

  return pTree;
}

AliHLTEsdManagerImplementation::AliHLTEsdListEntry::AliHLTEsdListEntry(AliHLTComponentDataType dt)
  :
  fName(),
  fDirectory(),
  fDt(dt),
  fpFile(NULL),
  fpTree(NULL),
  fpEsd(NULL),
  fPrefix()
{
  // see header file for class documentation
}

AliHLTEsdManagerImplementation::AliHLTEsdListEntry::~AliHLTEsdListEntry()
{
  // see header file for class documentation
  if (fpEsd) delete fpEsd;
  fpEsd=NULL;

  if (fpTree) delete fpTree;
  fpTree=NULL;

  if (fpFile) {
    fpFile->Close();
    delete fpFile;
  }
  fpFile=NULL;
}

bool AliHLTEsdManagerImplementation::AliHLTEsdListEntry::operator==(AliHLTComponentDataType dt) const
{
  // see header file for class documentation
  return fDt==dt;
}

int AliHLTEsdManagerImplementation::AliHLTEsdListEntry::WriteESD(AliESDEvent* pSrcESD, int eventno)
{
  // see header file for class documentation
  int iResult=0;
#ifndef HAVE_NOT_ESD_COPY
  if (fName.IsNull()) {
    // this is the first event, create the file name
    fName="";
    if (!fDirectory.IsNull()) {
      fName+=fDirectory; fName+="/";
    }
    fName+="Ali"; fName+=GetPrefix();
    if (fDt!=kAliHLTDataTypeESDObject &&
	fDt!=kAliHLTDataTypeESDTree) {

      HLTWarning("non-standard ESD type %s", AliHLTComponent::DataType2Text(fDt).c_str());
      TString id;
      id.Insert(0, fDt.fID, kAliHLTComponentDataTypefIDsize);
      id.Remove(TString::kTrailing, ' ');
      id.ToUpper();
      fName+="_"; fName+=id; fName+=".root";
    } else {
      fName+="ESDs.root";
    }

    fpFile=new TFile(fName, "RECREATE");
    fpTree=new TTree("esdTree", "Tree with HLT ESD objects");
    fpEsd=new AliESDEvent;
    if (fpEsd) {
      fpEsd->CreateStdContent();
      if (fpTree) {
	fpEsd->WriteToTree(fpTree);
      }
    }
  }

  if (fpFile && fpTree && fpEsd) {
    // synchronize and add empty events
    fpEsd->Reset();
    int nofCurrentEvents=fpTree->GetEntries();
    if (nofCurrentEvents<eventno) {
      iResult=1; // indicate tree to be written
      HLTDebug("adding %d empty events to file %s", eventno-nofCurrentEvents, fName.Data());
      for (int i=nofCurrentEvents; i<eventno; i++) {
	fpTree->Fill();
      }
    }

    if (iResult>=0 && pSrcESD) {
      *fpEsd=*pSrcESD;
      fpTree->Fill();
      iResult=1; // indicate tree to be written
    }

    if (iResult>0) {
      fpFile->cd();
      fpTree->GetUserInfo()->Add(fpEsd);
      fpTree->Write(fpTree->GetName(),TObject::kOverwrite);
      fpTree->GetUserInfo()->Clear();
    }
  }
#else //HAVE_NOT_ESD_COPY
  // this is the old workaround, necessary for older AliRoot versions
  // version<=v4-12-Release

  // we need to copy the ESD, I did not find an approptiate
  // method, the workaround is to save the ESD in a temporary
  // tree, read the content back into the ESD structure
  // used for filling.
  // Unfortunately the following code crashes at the second event.
  // The expert on the ESD (Christian Klein Boesig) does not have
  // a solution either. It seems to be a problem in ROOT.
  //  TTree* dummy=new TTree("dummy","dummy");
  //  dummy->SetDirectory(0);
  //  pESD->WriteToTree(dummy);
  //  dummy->Fill();
  //  dummy->GetUserInfo()->Add(pESD);
  //  fpEsd->ReadFromTree(dummy);
  //  dummy->GetEvent(0);
  //  fpEsd->WriteToTree(fpTree);
  //  fpTree->Fill();
  //  dummy->GetUserInfo()->Clear();
  //  delete dummy;
  //
  // The only way is via TChain, which is working on files only at the
  // time of writing.
  // We use temporary files for the new event to be copied into the
  // existing tree.
  //
  if (fName.IsNull()) {
    // this is the first event, create the file on disk and write ESD
    TString origin;
    origin.Insert(0, fDt.fOrigin, kAliHLTComponentDataTypefOriginSize);
    origin.Remove(TString::kTrailing, ' ');
    origin.ToUpper();
    fName="";
    if (!fDirectory.IsNull()) {
      fName+=fDirectory; fName+="/";
    }
    fName+="AliHLT"; fName+=origin;
    if (fDt!=kAliHLTDataTypeESDObject &&
	fDt!=kAliHLTDataTypeESDTree) {

      HLTWarning("non-standard ESD type %s", AliHLTComponent::DataType2Text(fDt).c_str());
      TString id;
      id.Insert(0, fDt.fID, kAliHLTComponentDataTypefIDsize);
      id.Remove(TString::kTrailing, ' ');
      id.ToUpper();
      fName+="_"; fName+=id; fName+=".root";
    } else {
      fName+="ESDs.root";
    }

    if (!gSystem->AccessPathName(fName)) {
      // file exists, delete
      TString shellcmd="rm -f ";
      shellcmd+=fName;
      gSystem->Exec(shellcmd);
    }
  }

  TChain chain("esdTree");
  TList cleanup;
  cleanup.SetOwner();

  int nofCurrentEvents=0;
  if (iResult>=0) {
    if (!gSystem->AccessPathName(fName)) {
      // these are the other events, use the target file and temporary files to merge
      // with TChain
      chain.Add(fName);

      if (eventno>=0) {
	TFile file(fName);
	if (!file.IsZombie()) {
	  TTree* pSrcTree;
	  file.GetObject("esdTree", pSrcTree);
	  if (pSrcTree) {
	    nofCurrentEvents=pSrcTree->GetEntries();
	  }
	  file.Close();
	}
      }
    }
  }

  // synchronize and add empty events
  if (nofCurrentEvents<eventno) {
    iResult=1; // indicate files to merge
    TTree* pTgtTree=new TTree("esdTree", "Tree with HLT ESD objects");
    if (pTgtTree) {
      pTgtTree->SetDirectory(0);
      AliESDEvent* pTmpESD=new AliESDEvent;
      if (pTmpESD) {
	TString tmpfilename;
	FILE* pTmpFile=gSystem->TempFileName(tmpfilename);
	if (pTmpFile) {
	  fclose(pTmpFile);
	  pTmpFile=NULL;
	  cleanup.Add(new TObjString(tmpfilename));
	  TFile emptyevents(tmpfilename, "RECREATE");
	  if (!emptyevents.IsZombie()) {
	    pTmpESD->CreateStdContent();
	    pTmpESD->WriteToTree(pTgtTree);
	    HLTDebug("adding %d empty events to file %s", eventno-nofCurrentEvents, fName.Data());
	    for (int i=nofCurrentEvents; i<eventno; i++) {
	      pTgtTree->Fill();
	    }
	    pTgtTree->GetUserInfo()->Add(pTmpESD);
	    emptyevents.cd();
	    pTgtTree->Write();
	    emptyevents.Close();
	    chain.Add(tmpfilename);
	    pTgtTree->GetUserInfo()->Clear();
	  }
	}
	delete pTmpESD;
      } else {
	iResult=-ENOMEM;
      }
      delete pTgtTree;
    } else {
      iResult=-ENOMEM;
    }
  }

  if (iResult>=0 && pSrcESD) {
    // add the new event to the chain
    iResult=1; // indicate files to merge
    TString tmpfilename=WriteTempFile(pSrcESD);
    if (!tmpfilename.IsNull()) {
      chain.Add(tmpfilename);
      cleanup.Add(new TObjString(tmpfilename));
    }
  }

  if (iResult>0) {
    // build temporary file name for chain output
    TString tgtName;
    FILE* pTmpFile=gSystem->TempFileName(tgtName);
    if (pTmpFile) {
      fclose(pTmpFile);
      pTmpFile=NULL;

      // there have been problems with the memory consumption when using
      // TChain::Merge
      // but using a separate loop soemtimes crashes in AliESDEvent::ReadFromTree
      // since this is for backward compatiblity only, we take the TChain::Merge
      chain.Merge(tgtName);
//       TFile tgtFile(tgtName, "RECREATE");
//       TTree* pTgtTree=new TTree("esdTree", "Tree with HLT ESD objects");
//       AliESDEvent* pTgtESD=new AliESDEvent;
//       if (pTgtTree && pTgtESD) {
// 	pTgtESD->ReadFromTree(&chain);
// 	pTgtESD->WriteToTree(pTgtTree);

// 	int nofEvents=chain.GetEntries();
// 	for (int event=0; event<nofEvents; event++) {
// 	  chain.GetEntry(event);
// 	  pTgtTree->Fill();
// 	}

// 	pTgtTree->GetUserInfo()->Add(pTgtESD);
// 	tgtFile.cd();
// 	pTgtTree->Write();
// 	pTgtTree->GetUserInfo()->Clear();
//       } else {
// 	iResult=-ENOMEM;
//       }

//       if (pTgtTree) delete pTgtTree;
//       if (pTgtESD) delete pTgtESD;
//       tgtFile.Close();

      // rename the merged file to the original file
      TString shellcmd="mv ";
      shellcmd+=tgtName + " " + fName;
      if (gSystem->Exec(shellcmd)==0) {
	HLTDebug("renaming %s to %s", tgtName.Data(), fName.Data());
      } else {
	HLTError("can not rename temporary file %s to %s", tgtName.Data(), fName.Data());
      }
    } else {
      HLTError("can not get temporary file name from system");
      iResult=-EBADF;
    }
  }

  // delete temporary files
  // the list objects are cleaned up by the TList destructor as the
  // list is owner
  TIter entry(&cleanup);
  while (TObject* pObj=entry.Next()) {
    if (dynamic_cast<TObjString*>(pObj)) {
      TString shellcmd="rm -f ";
      shellcmd+=(dynamic_cast<TObjString*>(pObj))->GetString();
      gSystem->Exec(shellcmd);
    }
  }
#endif //HAVE_NOT_ESD_COPY

  return iResult;
}

TString AliHLTEsdManagerImplementation::AliHLTEsdListEntry::WriteTempFile(AliESDEvent* pESD) const
{
  // see header file for class documentation
  int iResult=0;
  TString tmpfilename;
  FILE* pTmpFile=gSystem->TempFileName(tmpfilename);
  if (pTmpFile) {
    fclose(pTmpFile);
    pTmpFile=NULL;

    TFile file(tmpfilename, "RECREATE");
    if (!file.IsZombie()) {
      TTree* pTree=AliHLTEsdManagerImplementation::EmbedIntoTree(pESD);
      if (pTree) {
	file.cd();
	if (pTree->Write()>0) {
	} else {
	  HLTError("can not write esd tree to temporary file %s", tmpfilename.Data());
	}

	pTree->GetUserInfo()->Clear();
	delete pTree;
      } else {
	iResult=-ENOMEM;
      }
      file.Close();
    } else {
      HLTError("can not open file %s", tmpfilename.Data());
      iResult=-EBADF;
    }
  } else {
    HLTError("can not get temporary file name from system");
    iResult=-EBADF;
  }

  if (iResult<0) {
    if (gSystem->AccessPathName(tmpfilename)==0) {
      TString shellcmd="rm -f ";
      shellcmd+=tmpfilename;
      gSystem->Exec(shellcmd);
    }
    tmpfilename="";
  }
  return tmpfilename;
}

void AliHLTEsdManagerImplementation::AliHLTEsdListEntry::SetDirectory(const char* directory)
{
  // see header file for class documentation
  if (!directory) return;
  if (!fName.IsNull()) {
    HLTWarning("ESD entry already in writing mode (%s), ignoring directory", fName.Data());
    return;
  }
  fDirectory=directory;
}

void AliHLTEsdManagerImplementation::AliHLTEsdListEntry::Delete()
{
  // see header file for class documentation
  if (fName.IsNull()) return;
  if (gSystem->AccessPathName(fName)!=0) return;

  TString shellcmd="rm -f ";
  shellcmd+=fName;
  gSystem->Exec(shellcmd);
  fName="";
}

const char* AliHLTEsdManagerImplementation::AliHLTEsdListEntry::GetFileName() const
{
  // see header file for class documentation
  return fName.Data();
}

const char* AliHLTEsdManagerImplementation::AliHLTEsdListEntry::GetPrefix()
{
  // see header file for class documentation
  if (fPrefix.IsNull()) {
    fPrefix.Insert(0, fDt.fOrigin, kAliHLTComponentDataTypefOriginSize);
    fPrefix.Remove(TString::kTrailing, ' ');
    fPrefix.ToUpper();
    if (!fPrefix.Contains("HLT")) {
      fPrefix.Insert(0, "HLT");
    }
  }
  return fPrefix.Data();
}

int AliHLTEsdManagerImplementation::AliHLTEsdListEntry::CopyNonEmptyObjects(AliESDEvent* pTgt, AliESDEvent* pSrc)
{
  // see header file for class documentation
  int iResult=0;
  if (!pTgt || !pSrc) return -EINVAL;

  const char* defaultPrefix="HLT";
  TIter next(pSrc->GetList());
  TObject* pSrcObject=NULL;
  TString name;
  while ((pSrcObject=next())) {
    if(!pSrcObject->InheritsFrom("TCollection")){
      // simple objects
    } else if(pSrcObject->InheritsFrom("TClonesArray")){
      TClonesArray* pTClA=dynamic_cast<TClonesArray*>(pSrcObject);
      if (pTClA!=NULL && pTClA->GetEntriesFast()>0) {
	bool bMakeNewName=true;
	name=defaultPrefix;
	name+=pTClA->GetName();
	TObject* pTgtObject=pTgt->GetList()->FindObject(name);
	TClonesArray* pTgtArray=NULL;
	if (bMakeNewName=(pTgtObject!=NULL) && pTgtObject->InheritsFrom("TClonesArray")){
	  pTgtArray=dynamic_cast<TClonesArray*>(pTgtObject);
	  if (pTgtArray) {
	    TString classType=pTClA->Class()->GetName();
	    if (classType.CompareTo(pTgtArray->Class()->GetName())==0) {
	      if (pTgtArray->GetEntries()==0) {
		bMakeNewName=false;
	      } else {
		HLTWarning("TClonesArray \"%s\"  in target ESD %p is already filled with %d entries",
			   name.Data(), pTgt, pTgtArray->GetEntries());
	      }
	    } else {
	      HLTWarning("TClonesArray \"%s\" exists in target ESD %p, but describes incompatible class type %s instead of %s",
			 name.Data(), pTgt, pTgtArray->GetClass()->GetName(), pTClA->GetClass()->GetName());
	    }
	  } else {
	    HLTError("internal error: dynamic cast failed for object %s %p", pTgtObject->GetName(), pTgtObject);
	  }
	} else if (pTgtObject) {
	  HLTWarning("object \"%s\" does already exist in target ESD %p, but is %s rather than TClonesArray",
		     name.Data(), pTgt, pTgtObject->Class()->GetName());
	  // TODO: temporary solution, think about a general naming scheme and add
	  // the name as property of AliHLTEsdListEntry
	}

	if (bMakeNewName) {
	  pTgtArray=NULL;
	  int count=1;
	  while (pTgt->GetList()->FindObject(name)) {
	    name.Form("%sESD_%s", GetPrefix(), pTClA->GetName());
	    if (count++>1) {
	      name+=Form("%d", count);
	    }
	  }
	  HLTWarning("adding new TClonesArray \"%s\" because of conflicts", name.Data());
	}

	if (pTgtArray) {
	  pTgtArray->ExpandCreate(pTClA->GetEntries());
	  HLTInfo("Expanding TClonesArray \"%s\" to %d elements", pTgtArray->GetClass()->GetName(), pTClA->GetEntries());
	} else {
	  pTgtArray=new TClonesArray(pTClA->GetClass(), pTClA->GetEntries());
	  pTgtArray->ExpandCreate(pTClA->GetEntries());
	  pTgtArray->SetName(name);
	  pTgt->AddObject(pTgtArray);
	  HLTInfo("Adding TClonesArray \"%s\" with %d elements to ESD %p", name.Data(), pTClA->GetEntries(), pTgt);
	}

	if (pTgtArray) {
	  for(int i=0; i<pTClA->GetEntriesFast(); ++i){
	    (*pTClA)[i]->Copy(*((*pTgtArray)[i]));
	  }
	} else {
	  iResult=-ENOMEM;
	}
      }
    }
  }
  return iResult;
}
