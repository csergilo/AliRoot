/*************************************************************************
* Copyright(c) 1998-2008, ALICE Experiment at CERN, All rights reserved. *
*                                                                        *
* Author: The ALICE Off-line Project.                                    *
* Contributors are mentioned in the code where appropriate.              *
*                                                                        *
* Permission to use, copy, modify and distribute this software and its   *
* documentation strictly for non-commercial purposes is hereby granted   *
* without fee, provided that the above copyright notice appears in all   *
* copies and that both the copyright notice and this permission notice   *
* appear in the supporting documentation. The authors make no claims     *
* about the suitability of this software for any purpose. It is          *
* provided "as is" without express or implied warranty.                  * 
**************************************************************************/

#include "Riostream.h" //needed as include
#include "TChain.h"
#include "TTree.h"
#include "TFile.h" //needed as include
#include "TList.h"


class AliAnalysisTask;
#include "AliAnalysisManager.h"

#include "AliESDEvent.h"
#include "AliESDInputHandler.h"

#include "AliAODEvent.h"
#include "AliAODInputHandler.h"

#include "AliMCEventHandler.h"
#include "AliMCEvent.h"

#include "AliAnalysisTaskScalarProduct.h"
#include "AliFlowEventSimpleMaker.h"
#include "AliFlowAnalysisWithScalarProduct.h"

// AliAnalysisTaskScalarProduct:
//
// analysis task for Scalar Product Method
//
// Author: Naomi van der Kolk (kolk@nikhef.nl)

ClassImp(AliAnalysisTaskScalarProduct)

//________________________________________________________________________
AliAnalysisTaskScalarProduct::AliAnalysisTaskScalarProduct(const char *name) : 
  AliAnalysisTask(name, ""), 
  fESD(0),
  fAOD(0),
  fSP(0),
  fEventMaker(0),
  fAnalysisType("ESD")
{
  // Constructor
  cout<<"AliAnalysisTaskScalarProduct::AliAnalysisTaskScalarProduct(const char *name)"<<endl;

  // Define input and output slots here
  // Input slot #0 works with a TChain
  DefineInput(0, TChain::Class());
  // Output slot #0 writes into a TList container
  DefineOutput(0, TList::Class());  
}

//________________________________________________________________________
void AliAnalysisTaskScalarProduct::ConnectInputData(Option_t *) 
{
  // Connect ESD or AOD here
  // Called once
  cout<<"AliAnalysisTaskScalarProduct::ConnectInputData(Option_t *)"<<endl;

  TTree* tree = dynamic_cast<TTree*> (GetInputData(0));
  if (!tree) {
    Printf("ERROR: Could not read chain from input slot 0");
  } else {
    // Disable all branches and enable only the needed ones
    if (fAnalysisType == "MC") {
      cout<<"!!!!!reading MC kinematics only"<<endl;
      // we want to process only MC
      tree->SetBranchStatus("*", kFALSE);

      AliESDInputHandler *esdH = dynamic_cast<AliESDInputHandler*> (AliAnalysisManager::GetAnalysisManager()->GetInputEventHandler());

      if (!esdH) {
	Printf("ERROR: Could not get ESDInputHandler");
      } else {
	fESD = esdH->GetEvent();
      }
    }
    else if (fAnalysisType == "ESD") {
      cout<<"!!!!!reading the ESD only"<<endl;
      tree->SetBranchStatus("*", kFALSE);
      tree->SetBranchStatus("Tracks.*", kTRUE);

      AliESDInputHandler *esdH = dynamic_cast<AliESDInputHandler*> (AliAnalysisManager::GetAnalysisManager()->GetInputEventHandler());

      if (!esdH) {
	Printf("ERROR: Could not get ESDInputHandler");
      } else
	fESD = esdH->GetEvent();
    }
    else if (fAnalysisType == "AOD") {
      cout<<"!!!!!reading the AOD only"<<endl;
      AliAODInputHandler *aodH = dynamic_cast<AliAODInputHandler*> (AliAnalysisManager::GetAnalysisManager()->GetInputEventHandler());

      if (!aodH) {
	Printf("ERROR: Could not get AODInputHandler");
      }
      else {
	fAOD = aodH->GetEvent();
      }
    }
    else {
      Printf("!!!!!Wrong analysis type: Only ESD, AOD and MC types are allowed!");
      exit(1);
      
    }
  }
}

//________________________________________________________________________
void AliAnalysisTaskScalarProduct::CreateOutputObjects() 
{
  // Called once
  cout<<"AliAnalysisTaskScalarProduct::CreateOutputObjects()"<<endl;

  if (!(fAnalysisType == "AOD" || fAnalysisType == "ESD" || fAnalysisType == "MC")) {
    cout<<"WRONG ANALYSIS TYPE! only ESD, AOD and MC are allowed."<<endl;
    exit(1);
  }

  //event maker
  fEventMaker = new AliFlowEventSimpleMaker();
  //Analyser
  fSP  = new AliFlowAnalysisWithScalarProduct() ;

  //output file
  TString outputName = "outputFromScalarProductAnalysis" ;
  outputName += fAnalysisType.Data() ;
  outputName += ".root" ;
  fSP->SetHistFileName( outputName.Data() );
    
  fSP-> Init();


}

//________________________________________________________________________
void AliAnalysisTaskScalarProduct::Exec(Option_t *) 
{
  // Main loop
  // Called for each event

  
  // Process MC truth, therefore we receive the AliAnalysisManager and ask it for the AliMCEventHandler
  // This handler can return the current MC event

  AliMCEventHandler* eventHandler = dynamic_cast<AliMCEventHandler*> (AliAnalysisManager::GetAnalysisManager()->GetMCtruthEventHandler());
  if (!eventHandler) {
    Printf("ERROR: Could not retrieve MC event handler");
    return;
  }

  AliMCEvent* mcEvent = eventHandler->MCEvent();
  if (!mcEvent) {
    Printf("ERROR: Could not retrieve MC event");
    return;
  }

  Printf("MC particles: %d", mcEvent->GetNumberOfTracks());
      
  if (fAnalysisType == "MC") {
    // analysis 
    AliFlowEventSimple* fEvent = fEventMaker->FillTracks(mcEvent);
    fSP->Make(fEvent);

    delete fEvent;
  }
  else if (fAnalysisType == "ESD") {
    if (!fESD) {
      Printf("ERROR: fESD not available");
      return;
    }
    Printf("There are %d tracks in this event", fESD->GetNumberOfTracks());
    
    // analysis 
    AliFlowEventSimple* fEvent = fEventMaker->FillTracks(fESD);
    fSP->Make(fEvent);
    delete fEvent;
  }
  else if (fAnalysisType == "AOD") {
    if (!fAOD) {
      Printf("ERROR: fAOD not available");
      return;
    }
    Printf("There are %d tracks in this event", fAOD->GetNumberOfTracks());

    // analysis 
    AliFlowEventSimple* fEvent = fEventMaker->FillTracks(fAOD);
    fSP->Make(fEvent);
    delete fEvent;
  }

}      

//________________________________________________________________________
void AliAnalysisTaskScalarProduct::Terminate(Option_t *) 
{
  // Called once at the end of the query
  fSP->Finish();
  PostData(0,fSP->GetHistFile());

  delete fSP;
  delete fEventMaker;
}
