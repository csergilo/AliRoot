/**************************************************************************
 * Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
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

/* $Id$ */

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// 	************** Class for ZDC reconstruction      **************      //
//                  Author: Chiara.Oppedisano@to.infn.it		     //
//                                                                           //
// NOTATIONS ADOPTED TO IDENTIFY DETECTORS (used in different ages!):	     //
//   (ZN1,ZP1) or (ZNC, ZPC) or RIGHT refers to side C (RB26)		     //
//   (ZN2,ZP2) or (ZNA, ZPA) or LEFT refers to side A (RB24)		     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


#include <TH2F.h>
#include <TH1D.h>
#include <TAxis.h>
#include <TMap.h>

#include "AliRawReader.h"
#include "AliGRPObject.h"
#include "AliESDEvent.h"
#include "AliESDZDC.h"
#include "AliZDCDigit.h"
#include "AliZDCRawStream.h"
#include "AliZDCReco.h"
#include "AliZDCReconstructor.h"
#include "AliZDCPedestals.h"
#include "AliZDCEnCalib.h"
#include "AliZDCTowerCalib.h"
#include "AliZDCRecoParam.h"
#include "AliZDCRecoParampp.h"
#include "AliZDCRecoParamPbPb.h"


ClassImp(AliZDCReconstructor)
AliZDCRecoParam *AliZDCReconstructor::fRecoParam=0;  //reconstruction parameters

//_____________________________________________________________________________
AliZDCReconstructor:: AliZDCReconstructor() :
  fPedData(GetPedData()),
  fEnCalibData(GetEnCalibData()),
  fTowCalibData(GetTowCalibData()),
  fRecoMode(0),
  fBeamEnergy(0.),
  fNRun(0),
  fIsCalibrationMB(kFALSE),
  fPedSubMode(0),
  fRecoFlag(0x0)
{
  // **** Default constructor

}


//_____________________________________________________________________________
AliZDCReconstructor::~AliZDCReconstructor()
{
// destructor
   if(fRecoParam)    delete fRecoParam;
   if(fPedData)      delete fPedData;    
   if(fEnCalibData)  delete fEnCalibData;
   if(fTowCalibData) delete fTowCalibData;
}

//____________________________________________________________________________
void AliZDCReconstructor::Init()
{
  // Setting reconstruction mode
  // Getting beam type and beam energy from GRP calibration object
  
  if(fRecoMode==0 && fBeamEnergy==0.){
    // Initialization of the GRP entry 
    AliCDBEntry*  entry = AliCDBManager::Instance()->Get("GRP/GRP/Data");
    AliGRPObject* grpData = 0x0;
    if(entry){
      TMap* m = dynamic_cast<TMap*>(entry->GetObject());  // old GRP entry
      if(m){
        m->Print();
        grpData = new AliGRPObject();
        grpData->ReadValuesFromMap(m);
      }
      else{
        grpData = dynamic_cast<AliGRPObject*>(entry->GetObject());  // new GRP entry
      }
      entry->SetOwner(0);
      AliCDBManager::Instance()->UnloadFromCache("GRP/GRP/Data");
    }
    if(!grpData) AliError("No GRP entry found in OCDB!");
  
    TString runType = grpData->GetRunType();
    if(runType==AliGRPObject::GetInvalidString()){
      AliError("GRP/GRP/Data entry:  missing value for the run type ! Using UNKNOWN");
      runType = "UNKNOWN";
    }
    if((runType.CompareTo("CALIBRATION_MB")) == 0){
      fIsCalibrationMB = kTRUE;
      //
      fRecoParam = new AliZDCRecoParamPbPb();
      //
      TH2F* hZDCvsZEM = new TH2F("hZDCvsZEM","hZDCvsZEM",100,0.,10.,100,0.,1000.);
      hZDCvsZEM->SetXTitle("E_{ZEM} (TeV)"); hZDCvsZEM->SetYTitle("E_{ZDC} (TeV)");
      fRecoParam->SetZDCvsZEM(hZDCvsZEM);
      //
      TH2F* hZDCCvsZEM = new TH2F("hZDCCvsZEM","hZDCCvsZEM",100,0.,10.,100,0.,500.);
      hZDCCvsZEM->SetXTitle("E_{ZEM} (TeV)"); hZDCCvsZEM->SetYTitle("E_{ZDCC} (TeV)");
      fRecoParam->SetZDCCvsZEM(hZDCCvsZEM);
      //
      TH2F* hZDCAvsZEM = new TH2F("hZDCAvsZEM","hZDCAvsZEM",100,0.,10.,100,0.,500.);
      hZDCAvsZEM->SetXTitle("E_{ZEM} (TeV)"); hZDCAvsZEM->SetYTitle("E_{ZDCA} (TeV)"); 
      fRecoParam->SetZDCAvsZEM(hZDCAvsZEM);
    }
    
    TString beamType = grpData->GetBeamType();
    if(beamType==AliGRPObject::GetInvalidString()){
      AliError("GRP/GRP/Data entry:  missing value for the beam energy !");
      AliError("\t ZDC does not reconstruct event 4 UNKNOWN beam type\n");
      return;
    }
    if((beamType.CompareTo("p-p")) == 0){
      fRecoMode=1;
      fRecoParam = (AliZDCRecoParampp*) GetppRecoParamFromOCDB();
    }
    else if((beamType.CompareTo("A-A")) == 0){
      fRecoMode=2;
      if(fIsCalibrationMB == kFALSE) 
         fRecoParam = (AliZDCRecoParamPbPb*) GetPbPbRecoParamFromOCDB();
    }
    
    fBeamEnergy = grpData->GetBeamEnergy();
    if(fBeamEnergy==AliGRPObject::GetInvalidFloat()){
      AliError("GRP/GRP/Data entry:  missing value for the beam energy ! Using 0.");
      fBeamEnergy = 0.;
    }
    
    if(fIsCalibrationMB==kTRUE){
      AliInfo("\n ***** CALIBRATION_MB data -> building AliZDCRecoParamPbPb object *****");
    }
    else{ 
      AliInfo(Form("\n\n ***** ZDC reconstruction initialized for %s @ %1.3f GeV *****\n",beamType.Data(), fBeamEnergy));
    }
  }
  else{
    AliError(" ATTENTION!!!!!! No beam type nor beam energy has been set!!!!!!\n");
  }
  
}

//_____________________________________________________________________________
void AliZDCReconstructor::Reconstruct(TTree* digitsTree, TTree* clustersTree) const
{
  // *** Local ZDC reconstruction for digits
  // Works on the current event
    
  // Retrieving calibration data  
  // Parameters for mean value pedestal subtraction
  int const kNch = 24;
  Float_t meanPed[2*kNch];    
  for(Int_t jj=0; jj<2*kNch; jj++) meanPed[jj] = fPedData->GetMeanPed(jj);
  // Parameters pedestal subtraction through correlation with out-of-time signals
  Float_t corrCoeff0[2*kNch], corrCoeff1[2*kNch];
  for(Int_t jj=0; jj<2*kNch; jj++){
     corrCoeff0[jj] =  fPedData->GetPedCorrCoeff0(jj);
     corrCoeff1[jj] =  fPedData->GetPedCorrCoeff1(jj);
  }

  // get digits
  AliZDCDigit digit;
  AliZDCDigit* pdigit = &digit;
  digitsTree->SetBranchAddress("ZDC", &pdigit);
  //printf("\n\t # of digits in tree: %d\n",(Int_t) digitsTree->GetEntries());

  // loop over digits
  Float_t tZN1Corr[10], tZP1Corr[10], tZN2Corr[10], tZP2Corr[10]; 
  Float_t dZEM1Corr[2], dZEM2Corr[2], sPMRef1[2], sPMRef2[2]; 
  for(Int_t i=0; i<10; i++){
     tZN1Corr[i] = tZP1Corr[i] = tZN2Corr[i] = tZP2Corr[i] = 0.;
     if(i<2) dZEM1Corr[i] = dZEM2Corr[i] = sPMRef1[i] = sPMRef2[i] = 0.;
  }  
  
  Int_t digNentries = digitsTree->GetEntries();
  Float_t ootDigi[kNch];
  // -- Reading out-of-time signals (last kNch entries) for current event
  if(fPedSubMode==1){
    for(Int_t iDigit=kNch; iDigit<digNentries; iDigit++){
       ootDigi[iDigit] = digitsTree->GetEntry(iDigit);
    }
  }
  
  for(Int_t iDigit=0; iDigit<(digNentries/2); iDigit++) {
   digitsTree->GetEntry(iDigit);
   if (!pdigit) continue;
   //  
   Int_t det = digit.GetSector(0);
   Int_t quad = digit.GetSector(1);
   Int_t pedindex = -1;
   Float_t ped2SubHg=0., ped2SubLg=0.;
   if(quad!=5){
     if(det==1)      pedindex = quad;
     else if(det==2) pedindex = quad+5;
     else if(det==3) pedindex = quad+9;
     else if(det==4) pedindex = quad+12;
     else if(det==5) pedindex = quad+17;
   }
   else pedindex = (det-1)/3+22;
   //
   if(fPedSubMode==0){
     ped2SubHg = meanPed[pedindex];
     ped2SubLg = meanPed[pedindex+kNch];
   }
   else if(fPedSubMode==1){
     ped2SubHg = corrCoeff1[pedindex]*ootDigi[pedindex]+corrCoeff0[pedindex];
     ped2SubLg = corrCoeff1[pedindex+kNch]*ootDigi[pedindex+kNch]+corrCoeff0[pedindex+kNch];
   }
      
   if(quad != 5){ // ZDC (not reference PTMs!)
    if(det == 1){ // *** ZNC
       tZN1Corr[quad] = (Float_t) (digit.GetADCValue(0)-ped2SubHg);
       tZN1Corr[quad+5] = (Float_t) (digit.GetADCValue(1)-ped2SubLg);
       if(tZN1Corr[quad]<0.) tZN1Corr[quad] = 0.;
       if(tZN1Corr[quad+5]<0.) tZN1Corr[quad+5] = 0.;
       // Ch. debug
       //printf("\t pedindex %d tZN1Corr[%d] = %1.0f tZN1Corr[%d] = %1.0f", 
       //	pedindex, quad, tZN1Corr[quad], quad+5, tZN1Corr[quad+5]);
    }
    else if(det == 2){ // *** ZP1
       tZP1Corr[quad] = (Float_t) (digit.GetADCValue(0)-ped2SubHg);
       tZP1Corr[quad+5] = (Float_t) (digit.GetADCValue(1)-ped2SubLg);
       if(tZP1Corr[quad]<0.) tZP1Corr[quad] = 0.;
       if(tZP1Corr[quad+5]<0.) tZP1Corr[quad+5] = 0.;
       // Ch. debug
       //printf("\t pedindex %d tZP1Corr[%d] = %1.0f tZP1Corr[%d] = %1.0f", 
       //	pedindex, quad, tZP1Corr[quad], quad+5, tZP1Corr[quad+5]);
    }
    else if(det == 3){
       if(quad == 1){	    // *** ZEM1  
         dZEM1Corr[0] += (Float_t) (digit.GetADCValue(0)-ped2SubHg); 
         dZEM1Corr[1] += (Float_t) (digit.GetADCValue(1)-ped2SubLg); 
         if(dZEM1Corr[0]<0.) dZEM1Corr[0] = 0.;
         if(dZEM1Corr[1]<0.) dZEM1Corr[1] = 0.;
         // Ch. debug
         //printf("\t pedindex %d tZEM1Corr[%d] = %1.0f tZEM1Corr[%d] = %1.0f", 
         //	pedindex, quad, tZEM1Corr[quad], quad+1, tZEM1Corr[quad+1]);
       }
       else if(quad == 2){  // *** ZEM2
         dZEM2Corr[0] += (Float_t) (digit.GetADCValue(0)-ped2SubHg); 
         dZEM2Corr[1] += (Float_t) (digit.GetADCValue(1)-ped2SubLg); 
         if(dZEM2Corr[0]<0.) dZEM2Corr[0] = 0.;
         if(dZEM2Corr[1]<0.) dZEM2Corr[1] = 0.;
         // Ch. debug
         //printf("\t pedindex %d tZEM2Corr[%d] = %1.0f tZEM2Corr[%d] = %1.0f", 
         //	pedindex, quad, tZEM2Corr[quad], quad+1, tZEM2Corr[quad+1]);
       }
    }
    else if(det == 4){  // *** ZN2
       tZN2Corr[quad] = (Float_t) (digit.GetADCValue(0)-ped2SubHg);
       tZN2Corr[quad+5] = (Float_t) (digit.GetADCValue(1)-ped2SubLg);
       if(tZN2Corr[quad]<0.) tZN2Corr[quad] = 0.;
       if(tZN2Corr[quad+5]<0.) tZN2Corr[quad+5] = 0.;
       // Ch. debug
       //printf("\t pedindex %d tZN2Corr[%d] = %1.0f tZN2Corr[%d] = %1.0f\n", 
       //	pedindex, quad, tZN2Corr[quad], quad+5, tZN2Corr[quad+5]);
    }
    else if(det == 5){  // *** ZP2 
       tZP2Corr[quad] = (Float_t) (digit.GetADCValue(0)-ped2SubHg);
       tZP2Corr[quad+5] = (Float_t) (digit.GetADCValue(1)-ped2SubLg);
       if(tZP2Corr[quad]<0.) tZP2Corr[quad] = 0.;
       if(tZP2Corr[quad+5]<0.) tZP2Corr[quad+5] = 0.;
       // Ch. debug
       //printf("\t pedindex %d tZP2Corr[%d] = %1.0f tZP2Corr[%d] = %1.0f\n", 
       //	pedindex, quad, tZP2Corr[quad], quad+5, tZP2Corr[quad+5]);
    }
   }
   else{ // Reference PMs
     if(det == 1){
       sPMRef1[0] = (Float_t) (digit.GetADCValue(0)-ped2SubHg);
       sPMRef1[1] = (Float_t) (digit.GetADCValue(1)-ped2SubLg);
       // Ch. debug
       if(sPMRef1[0]<0.) sPMRef1[0] = 0.;
       if(sPMRef2[1]<0.) sPMRef1[1] = 0.;
     }
     else if(det == 4){
       sPMRef2[0] = (Float_t) (digit.GetADCValue(0)-ped2SubHg);
       sPMRef2[1] = (Float_t) (digit.GetADCValue(1)-ped2SubLg);
       // Ch. debug
       if(sPMRef2[0]<0.) sPMRef2[0] = 0.;
       if(sPMRef2[1]<0.) sPMRef2[1] = 0.;
     }
   }

   // Ch. debug
   /*printf(" - AliZDCReconstructor -> digit #%d det %d quad %d pedHG %1.0f pedLG %1.0f\n",
   	 iDigit, det, quad, ped2SubHg, ped2SubLg);
   printf("   HGChain -> RawDig %d DigCorr %1.2f\n", digit.GetADCValue(0), digit.GetADCValue(0)-ped2SubHg); 
   printf("   LGChain -> RawDig %d DigCorr %1.2f\n", digit.GetADCValue(1), digit.GetADCValue(1)-ped2SubLg); 
   */
  }//digits loop
  
  // If CALIBRATION_MB run build the RecoParam object 
  if(fIsCalibrationMB){
    Float_t ZDCC=0., ZDCA=0., ZEM=0;
    ZEM += dZEM1Corr[0] + dZEM2Corr[0];
    for(Int_t jkl=0; jkl<5; jkl++){
       ZDCC += tZN1Corr[jkl] + tZP1Corr[jkl];
       ZDCA += tZN2Corr[jkl] + tZP2Corr[jkl];
    }
    // Using energies in TeV in fRecoParam object
    BuildRecoParam(fRecoParam->GethZDCvsZEM(), fRecoParam->GethZDCCvsZEM(), 
    		   fRecoParam->GethZDCAvsZEM(), ZDCC/1000., ZDCA/1000., ZEM/1000.);
  }
  // reconstruct the event
  if(fRecoMode==1)
    ReconstructEventpp(clustersTree, tZN1Corr, tZP1Corr, tZN2Corr, tZP2Corr, 
      dZEM1Corr, dZEM2Corr, sPMRef1, sPMRef2);
  else if(fRecoMode==2)
    ReconstructEventPbPb(clustersTree, tZN1Corr, tZP1Corr, tZN2Corr, tZP2Corr, 
      dZEM1Corr, dZEM2Corr, sPMRef1, sPMRef2);
}

//_____________________________________________________________________________
void AliZDCReconstructor::Reconstruct(AliRawReader* rawReader, TTree* clustersTree) 
{
  // *** ZDC raw data reconstruction
  // Works on the current event
  
  // Retrieving calibration data  
  // Parameters for pedestal subtraction
  int const kNch = 24;
  Float_t meanPed[2*kNch];    
  for(Int_t jj=0; jj<2*kNch; jj++) meanPed[jj] = fPedData->GetMeanPed(jj);
  // Parameters pedestal subtraction through correlation with out-of-time signals
  Float_t corrCoeff0[2*kNch], corrCoeff1[2*kNch];
  for(Int_t jj=0; jj<2*kNch; jj++){
     corrCoeff0[jj] =  fPedData->GetPedCorrCoeff0(jj);
     corrCoeff1[jj] =  fPedData->GetPedCorrCoeff1(jj);
  }

  Int_t adcZN1[5], adcZN1oot[5], adcZN1lg[5], adcZN1ootlg[5];
  Int_t adcZP1[5], adcZP1oot[5], adcZP1lg[5], adcZP1ootlg[5];
  Int_t adcZN2[5], adcZN2oot[5], adcZN2lg[5], adcZN2ootlg[5];
  Int_t adcZP2[5], adcZP2oot[5], adcZP2lg[5], adcZP2ootlg[5];
  Int_t adcZEM[2], adcZEMoot[2], adcZEMlg[2], adcZEMootlg[2];
  Int_t pmRef[2], pmRefoot[2], pmReflg[2], pmRefootlg[2];
  for(Int_t ich=0; ich<5; ich++){
    adcZN1[ich] = adcZN1oot[ich] = adcZN1lg[ich] = adcZN1ootlg[ich] = 0;
    adcZP1[ich] = adcZP1oot[ich] = adcZP1lg[ich] = adcZP1ootlg[ich] = 0;
    adcZN2[ich] = adcZN2oot[ich] = adcZN2lg[ich] = adcZN2ootlg[ich] = 0;
    adcZP2[ich] = adcZP2oot[ich] = adcZP2lg[ich] = adcZP2ootlg[ich] = 0;
    if(ich<2){
      adcZEM[ich] = adcZEMoot[ich] = adcZEMlg[ich] = adcZEMootlg[ich] = 0;
      pmRef[ich] = pmRefoot[ich] = pmReflg[ich] = pmRefootlg[ich] = 0;
    }
  }
  
  // loop over raw data
  Float_t tZN1Corr[10], tZP1Corr[10], tZN2Corr[10], tZP2Corr[10]; 
  Float_t dZEM1Corr[2], dZEM2Corr[2], sPMRef1[2], sPMRef2[2]; 
  for(Int_t i=0; i<10; i++){
     tZN1Corr[i] = tZP1Corr[i] = tZN2Corr[i] = tZP2Corr[i] = 0.;
     if(i<2) dZEM1Corr[i] = dZEM2Corr[i] = sPMRef1[i] = sPMRef2[i] = 0.;
  }  
  //
  rawReader->Reset();
  fNRun = (Int_t) rawReader->GetRunNumber();
  AliZDCRawStream rawData(rawReader);
  while(rawData.Next()){
   // Do
   Bool_t ch2process = kTRUE;
   //
   // Setting reco flags (part I)
   if((rawData.IsADCDataWord()) && (rawData.IsUnderflow() == kTRUE)){
     fRecoFlag = 0x1<< 8;
     ch2process = kFALSE;
   }
   if((rawData.IsADCDataWord()) && (rawData.IsOverflow() == kTRUE)){
     fRecoFlag = 0x1 << 7;
     ch2process = kFALSE;
   }
   if(rawData.GetNChannelsOn() < 48 ) fRecoFlag = 0x1 << 6;
   
   if((rawData.IsADCDataWord()) && (ch2process == kTRUE)){
     
     Int_t adcMod = rawData.GetADCModule();
     Int_t det = rawData.GetSector(0);
     Int_t quad = rawData.GetSector(1);
     Int_t gain = rawData.GetADCGain();
     Int_t pedindex=0;
     //
     // Mean pedestal value subtraction -------------------------------------------------------
     if(fPedSubMode == 0){
       // Not interested in o.o.t. signals (ADC modules 2, 3)
       if(adcMod == 2 || adcMod == 3) return;
       //
       if(quad != 5){ // ZDCs (not reference PTMs)
        if(det == 1){    
          pedindex = quad;
          if(gain == 0) tZN1Corr[quad]  += (Float_t) (rawData.GetADCValue()-meanPed[pedindex]); 
          else tZN1Corr[quad+5]  += (Float_t) (rawData.GetADCValue()-meanPed[pedindex+kNch]); 
        }
        else if(det == 2){ 
          pedindex = quad+5;
          if(gain == 0) tZP1Corr[quad]  += (Float_t) (rawData.GetADCValue()-meanPed[pedindex]); 
          else tZP1Corr[quad+5]  += (Float_t) (rawData.GetADCValue()-meanPed[pedindex+kNch]); 
        }
        else if(det == 3){ 
          pedindex = quad+9;
          if(quad==1){     
            if(gain == 0) dZEM1Corr[0] += (Float_t) (rawData.GetADCValue()-meanPed[pedindex]); 
            else dZEM1Corr[1] += (Float_t) (rawData.GetADCValue()-meanPed[pedindex+kNch]); 
          }
          else if(quad==2){ 
            if(gain == 0) dZEM2Corr[0] += (Float_t) (rawData.GetADCValue()-meanPed[pedindex]); 
            else dZEM2Corr[1] += (Float_t) (rawData.GetADCValue()-meanPed[pedindex+kNch]); 
          }
        }
        else if(det == 4){	 
          pedindex = quad+12;
          if(gain == 0) tZN2Corr[quad]  += (Float_t) (rawData.GetADCValue()-meanPed[pedindex]); 
          else tZN2Corr[quad+5]  += (Float_t) (rawData.GetADCValue()-meanPed[pedindex+kNch]); 
        }
        else if(det == 5){
          pedindex = quad+17;
          if(gain == 0) tZP2Corr[quad]  += (Float_t) (rawData.GetADCValue()-meanPed[pedindex]); 
          else tZP2Corr[quad+5]  += (Float_t) (rawData.GetADCValue()-meanPed[pedindex+kNch]); 
        }
       }
       else{ // reference PM
         pedindex = (det-1)/3 + 22;
         if(det == 1){
           if(gain==0) sPMRef1[0] += (Float_t) (rawData.GetADCValue()-meanPed[pedindex]);
	 else sPMRef1[1] += (Float_t) (rawData.GetADCValue()-meanPed[pedindex]);
         }
         else if(det == 4){
           if(gain==0) sPMRef2[0] += (Float_t) (rawData.GetADCValue()-meanPed[pedindex]);
      	   else sPMRef2[1] += (Float_t) (rawData.GetADCValue()-meanPed[pedindex]);
         }
       }
       // Ch. debug
       /*printf(" -> AliZDCReconstructor: det %d quad %d res %d -> Pedestal[%d] %1.0f\n", 
         det,quad,gain, pedindex, meanPed[pedindex]);
       printf(" -> AliZDCReconstructor: RawADC %1.0f ADCCorr %1.0f\n", 
         rawData.GetADCValue(), rawData.GetADCValue()-meanPed[pedindex]);*/
	 
     }// mean pedestal subtraction
     // Pedestal subtraction from correlation ------------------------------------------------
     else if(fPedSubMode == 1){
       // In time signals
       if(adcMod==0 || adcMod==1){
         if(quad != 5){ // signals from ZDCs
           if(det == 1){
	     if(gain==0) adcZN1[quad] = rawData.GetADCValue();
             else adcZN1lg[quad] = rawData.GetADCValue();
	   }
	   else if(det == 2){
	     if(gain==0) adcZP1[quad] = rawData.GetADCValue();
             else adcZP1lg[quad] = rawData.GetADCValue();
	   }
	   else if(det == 3){
	     if(gain==0) adcZEM[quad-1] = rawData.GetADCValue();
             else adcZEMlg[quad-1] = rawData.GetADCValue();
	   }
	   else if(det == 4){
	     if(gain==0) adcZN2[quad] = rawData.GetADCValue();
             else adcZN2lg[quad] = rawData.GetADCValue();
	   }
	   else if(det == 5){
	     if(gain==0) adcZP2[quad] = rawData.GetADCValue();
             else adcZP2lg[quad] = rawData.GetADCValue();
	   }
	 }
	 else{ // signals from reference PM
	    if(gain==0) pmRef[quad-1] = rawData.GetADCValue();
            else pmReflg[quad-1] = rawData.GetADCValue();
	 }
       }
       // Out-of-time pedestals
       else if(adcMod==2 || adcMod==3){
         if(quad != 5){ // signals from ZDCs
           if(det == 1){
	     if(gain==0) adcZN1oot[quad] = rawData.GetADCValue();
             else adcZN1ootlg[quad] = rawData.GetADCValue();
	   }
	   else if(det == 2){
	     if(gain==0) adcZP1oot[quad] = rawData.GetADCValue();
             else adcZP1ootlg[quad] = rawData.GetADCValue();
	   }
	   else if(det == 3){
	     if(gain==0) adcZEMoot[quad-1] = rawData.GetADCValue();
             else adcZEMootlg[quad-1] = rawData.GetADCValue();
	   }
	   else if(det == 4){
	     if(gain==0) adcZN2oot[quad] = rawData.GetADCValue();
             else adcZN2ootlg[quad] = rawData.GetADCValue();
	   }
	   else if(det == 5){
	     if(gain==0) adcZP2oot[quad] = rawData.GetADCValue();
             else adcZP2ootlg[quad] = rawData.GetADCValue();
	   }
	 }
	 else{ // signals from reference PM
	    if(gain==0) pmRefoot[quad-1] = rawData.GetADCValue();
            else pmRefootlg[quad-1] = rawData.GetADCValue();
	 }
       }
     } // pedestal subtraction from correlation
	// Ch. debug
        //printf("\t AliZDCReconstructor - det %d quad %d res %d -> Ped[%d] = %1.0f\n", 
        //  det,quad,gain, pedindex, meanPed[pedindex]);
   }//IsADCDataWord
  }//loop on raw data
  
  if(fPedSubMode==1){
    for(Int_t t=0; t<5; t++){
       tZN1Corr[t] = adcZN1[t] - (corrCoeff1[t]*adcZN1oot[t]+corrCoeff0[t]);
       tZN1Corr[t+5] = adcZN1lg[t] - (corrCoeff1[t+kNch]*adcZN1ootlg[t]+corrCoeff0[t+kNch]);
       //
       tZP1Corr[t] = adcZP1[t] - (corrCoeff1[t+5]*adcZP1oot[t]+corrCoeff0[t+5]);
       tZP1Corr[t+5] = adcZP1lg[t] - (corrCoeff1[t+5+kNch]*adcZP1ootlg[t]+corrCoeff0[t+5+kNch]);
       //
       tZN2Corr[t] = adcZN2[t] - (corrCoeff1[t+12]*adcZN2oot[t]+corrCoeff0[t+12]);
       tZN2Corr[t+5] = adcZN2lg[t] - (corrCoeff1[t+12+kNch]*adcZN2ootlg[t]+corrCoeff0[t+12+kNch]);
       //
       tZP2Corr[t] = adcZP2[t] - (corrCoeff1[t+17]*adcZP2oot[t]+corrCoeff0[t+17]);
       tZP2Corr[t+5] = adcZP2lg[t] - (corrCoeff1[t+17+kNch]*adcZP2ootlg[t]+corrCoeff0[t+17+kNch]);
       // 0---------0 Ch. debug 0---------0
/*       printf("\n\n ---------- Debug of pedestal subtraction from correlation ----------\n");
       printf("\tCorrCoeff0\tCorrCoeff1\n");
       printf(" ZN1 %d\t%1.0f\t%1.0f\n",t,corrCoeff0[t],corrCoeff1[t]);
       printf(" ZN1lg %d\t%1.0f\t%1.0f\n",t+kNch,corrCoeff0[t+kNch],corrCoeff1[t+kNch]);
       printf(" ZP1 %d\t%1.0f\t%1.0f\n",t+5,corrCoeff0[t+5],corrCoeff1[t+5]);
       printf(" ZP1lg %d\t%1.0f\t%1.0f\n",t+5+kNch,corrCoeff0[t+5+kNch],corrCoeff1[t+5+kNch]);
       printf(" ZN2 %d\t%1.0f\t%1.0f\n",t+12,corrCoeff0[t+12],corrCoeff1[t+12]);
       printf(" ZN2lg %d\t%1.0f\t%1.0f\n",t+12+kNch,corrCoeff0[t+12+kNch],corrCoeff1[t+12+kNch]);
       printf(" ZP2 %d\t%1.0f\t%1.0f\n",t+17,corrCoeff0[t+17],corrCoeff1[t+17]);
       printf(" ZP2lg %d\t%1.0f\t%1.0f\n",t+17+kNch,corrCoeff0[t+17+kNch],corrCoeff1[t+17+kNch]);
       
       printf("ZN1 ->  rawADC %d\tpedestal%1.2f\tcorrADC%1.2f\n",
       		adcZN1[t],(corrCoeff1[t]*adcZN1oot[t]+corrCoeff0[t]),tZN1Corr[t]);
       printf(" lg ->  rawADC %d\tpedestal%1.2f\tcorrADC%1.2f\n",
       		adcZN1lg[t],(corrCoeff1[t+kNch]*adcZN1ootlg[t]+corrCoeff0[t+kNch]),tZN1Corr[t+5]);
       //
       printf("ZP1 ->  rawADC %d\tpedestal%1.2f\tcorrADC%1.2f\n",
       		adcZP1[t],(corrCoeff1[t+5]*adcZP1oot[t]+corrCoeff0[t+5]),tZP1Corr[t]);
       printf(" lg ->  rawADC %d\tpedestal%1.2f\tcorrADC%1.2f\n",
       		adcZP1lg[t],(corrCoeff1[t+5+kNch]*adcZP1ootlg[t]+corrCoeff0[t+5+kNch]),tZP1Corr[t+5]);
       //
       printf("ZN2 ->  rawADC %d\tpedestal%1.2f\tcorrADC%1.2f\n",
       		adcZN2[t],(corrCoeff1[t+12]*adcZN2oot[t]+corrCoeff0[t+12]),tZN2Corr[t]);
       printf(" lg ->  rawADC %d\tpedestal%1.2f\tcorrADC%1.2f\n",
       		adcZN2lg[t],(corrCoeff1[t+12+kNch]*adcZN2ootlg[t]+corrCoeff0[t+12+kNch]),tZN2Corr[t+5]);
       //
       printf("ZP2 ->  rawADC %d\tpedestal%1.2f\tcorrADC%1.2f\n",
       		adcZP2[t],(corrCoeff1[t+17]*adcZP2oot[t]+corrCoeff0[t+17]),tZP2Corr[t]);
       printf(" lg ->  rawADC %d\tpedestal%1.2f\tcorrADC%1.2f\n",
       		adcZP2lg[t],(corrCoeff1[t+17+kNch]*adcZP2ootlg[t]+corrCoeff0[t+17+kNch]),tZP2Corr[t+5]);
*/
    }
    dZEM1Corr[0] = adcZEM[0]   - (corrCoeff1[9]*adcZEMoot[0]+corrCoeff0[9]);
    dZEM1Corr[1] = adcZEMlg[0] - (corrCoeff1[9+kNch]*adcZEMootlg[0]+corrCoeff0[9+kNch]);
    dZEM2Corr[0] = adcZEM[1]   - (corrCoeff1[10]*adcZEMoot[1]+corrCoeff0[10]);
    dZEM2Corr[1] = adcZEMlg[1] - (corrCoeff1[10+kNch]*adcZEMootlg[1]+corrCoeff0[10+kNch]);
    //
    sPMRef1[0] = pmRef[0]   - (corrCoeff1[22]*pmRefoot[0]+corrCoeff0[22]);
    sPMRef1[1] = pmReflg[0] - (corrCoeff1[22+kNch]*pmRefootlg[0]+corrCoeff0[22+kNch]);
    sPMRef2[0] = pmRef[0]   - (corrCoeff1[23]*pmRefoot[1]+corrCoeff0[23]);
    sPMRef2[1] = pmReflg[0] - (corrCoeff1[23+kNch]*pmRefootlg[1]+corrCoeff0[23+kNch]);
  }
  // Setting reco flags (part II)
  Float_t sumZNAhg, sumZPAhg, sumZNChg, sumZPChg;
  for(Int_t jj=0; jj<5; jj++){
    sumZNAhg += tZN2Corr[jj];
    sumZPAhg += tZP2Corr[jj];
    sumZNChg += tZN1Corr[jj];
    sumZPChg += tZP1Corr[jj];
  }
  if(sumZNAhg>0.)     fRecoFlag = 0x1;
  if(sumZPAhg>0.)     fRecoFlag = 0x1 << 1;
  if(dZEM1Corr[0]>0.) fRecoFlag = 0x1 << 2;
  if(dZEM2Corr[0]>0.) fRecoFlag = 0x1 << 3;
  if(sumZNChg>0.)     fRecoFlag = 0x1 << 4;
  if(sumZPChg>0.)     fRecoFlag = 0x1 << 5;
    
  // If CALIBRATION_MB run build the RecoParam object 
  if(fIsCalibrationMB){
    Float_t ZDCC=0., ZDCA=0., ZEM=0;
    ZEM += dZEM1Corr[0] + dZEM2Corr[0];
    for(Int_t jkl=0; jkl<5; jkl++){
       ZDCC += tZN1Corr[jkl] + tZP1Corr[jkl];
       ZDCA += tZN2Corr[jkl] + tZP2Corr[jkl];
    }
    BuildRecoParam(fRecoParam->GethZDCvsZEM(), fRecoParam->GethZDCCvsZEM(), 
    		   fRecoParam->GethZDCAvsZEM(), ZDCC/100., ZDCA/100., ZEM/100.);
  }
  // reconstruct the event
  else{
    if(fRecoMode==1) // p-p data
      ReconstructEventpp(clustersTree, tZN1Corr, tZP1Corr, tZN2Corr, tZP2Corr, 
        dZEM1Corr, dZEM2Corr, sPMRef1, sPMRef2);
    else if(fRecoMode==2) // Pb-Pb data
      ReconstructEventPbPb(clustersTree, tZN1Corr, tZP1Corr, tZN2Corr, tZP2Corr, 
        dZEM1Corr, dZEM2Corr, sPMRef1, sPMRef2);
  }
}

//_____________________________________________________________________________
void AliZDCReconstructor::ReconstructEventpp(TTree *clustersTree, Float_t* corrADCZN1, 
	Float_t* corrADCZP1, Float_t* corrADCZN2, Float_t* corrADCZP2,
	Float_t* corrADCZEM1, Float_t* corrADCZEM2, Float_t* sPMRef1, Float_t* sPMRef2) const
{
  // ****************** Reconstruct one event ******************
  				
  // ******	Retrieving calibration data 
  // --- Equalization coefficients ---------------------------------------------
  Float_t equalCoeffZN1[5], equalCoeffZP1[5], equalCoeffZN2[5], equalCoeffZP2[5];
  for(Int_t ji=0; ji<5; ji++){
     equalCoeffZN1[ji] = fTowCalibData->GetZN1EqualCoeff(ji);
     equalCoeffZP1[ji] = fTowCalibData->GetZP1EqualCoeff(ji); 
     equalCoeffZN2[ji] = fTowCalibData->GetZN2EqualCoeff(ji); 
     equalCoeffZP2[ji] = fTowCalibData->GetZP2EqualCoeff(ji); 
  }
  // --- Energy calibration factors ------------------------------------
  Float_t calibEne[4];
  // **** Energy calibration coefficient set to 1 
  // **** (no trivial way to calibrate in p-p runs)
  for(Int_t ij=0; ij<4; ij++) calibEne[ij] = fEnCalibData->GetEnCalib(ij);
  
  // ******	Equalization of detector responses
  Float_t equalTowZN1[10], equalTowZN2[10], equalTowZP1[10], equalTowZP2[10];
  for(Int_t gi=0; gi<10; gi++){
     equalTowZN1[gi] = corrADCZN1[gi]*equalCoeffZN1[gi];
     equalTowZP1[gi] = corrADCZP1[gi]*equalCoeffZP1[gi];
     equalTowZN2[gi] = corrADCZN2[gi]*equalCoeffZN2[gi];
     equalTowZP2[gi] = corrADCZP2[gi]*equalCoeffZP2[gi];
  }
  
  // ******	Summed response for hadronic calorimeter (SUMMED and then CALIBRATED!)
  Float_t calibSumZN1[]={0,0}, calibSumZN2[]={0,0}, calibSumZP1[]={0,0}, calibSumZP2[]={0,0};
  for(Int_t gi=0; gi<5; gi++){
       calibSumZN1[0] += equalTowZN1[gi];
       calibSumZP1[0] += equalTowZP1[gi];
       calibSumZN2[0] += equalTowZN2[gi];
       calibSumZP2[0] += equalTowZP2[gi];
       //
       calibSumZN1[1] += equalTowZN1[gi+5];
       calibSumZP1[1] += equalTowZP1[gi+5];
       calibSumZN2[1] += equalTowZN2[gi+5];
       calibSumZP2[1] += equalTowZP2[gi+5];
  }
  // High gain chain
  calibSumZN1[0] = calibSumZN1[0]*calibEne[0]/8.;
  calibSumZP1[0] = calibSumZP1[0]*calibEne[1]/8.;
  calibSumZN2[0] = calibSumZN2[0]*calibEne[2]/8.;
  calibSumZP2[0] = calibSumZP2[0]*calibEne[3]/8.;
  // Low gain chain
  calibSumZN1[1] = calibSumZN1[1]*calibEne[0];
  calibSumZP1[1] = calibSumZP1[1]*calibEne[1];
  calibSumZN2[1] = calibSumZN2[1]*calibEne[2];
  calibSumZP2[1] = calibSumZP2[1]*calibEne[3];
  
  // ******	Energy calibration of detector responses
  Float_t calibTowZN1[10], calibTowZN2[10], calibTowZP1[10], calibTowZP2[10];
  for(Int_t gi=0; gi<5; gi++){
     // High gain chain
     calibTowZN1[gi] = equalTowZN1[gi]*calibEne[0]/8.;
     calibTowZP1[gi] = equalTowZP1[gi]*calibEne[1]/8.;
     calibTowZN2[gi] = equalTowZN2[gi]*calibEne[2]/8.;
     calibTowZP2[gi] = equalTowZP2[gi]*calibEne[3]/8.;
     // Low gain chain
     calibTowZN1[gi+5] = equalTowZN1[gi+5]*calibEne[0];
     calibTowZP1[gi+5] = equalTowZP1[gi+5]*calibEne[1];
     calibTowZN2[gi+5] = equalTowZN2[gi+5]*calibEne[2];
     calibTowZP2[gi+5] = equalTowZP2[gi+5]*calibEne[3];
  }
  //
  Float_t sumZEM[]={0,0}, calibZEM1[]={0,0}, calibZEM2[]={0,0};
  calibZEM1[0] = corrADCZEM1[0]*calibEne[5]/8.;
  calibZEM1[1] = corrADCZEM1[1]*calibEne[5];
  calibZEM2[0] = corrADCZEM2[0]*calibEne[5]/8.;
  calibZEM2[1] = corrADCZEM2[1]*calibEne[5];
  for(Int_t k=0; k<2; k++) sumZEM[k] = calibZEM1[k] + calibZEM2[k];
  
  //  ******	No. of spectator and participants nucleons
  //  Variables calculated to comply with ESD structure
  //  *** N.B. -> They have a meaning only in Pb-Pb!!!!!!!!!!!!
  Int_t nDetSpecNLeft=0, nDetSpecPLeft=0, nDetSpecNRight=0, nDetSpecPRight=0;
  Int_t nGenSpec=0, nGenSpecLeft=0, nGenSpecRight=0;
  Int_t nPart=0, nPartTotLeft=0, nPartTotRight=0;
  Double_t impPar=0., impPar1=0., impPar2=0.;
  
  // create the output tree
  AliZDCReco reco(calibSumZN1, calibSumZP1, calibSumZN2, calibSumZP2, 
  		  calibTowZN1, calibTowZP1, calibTowZN2, calibTowZP2, 
		  calibZEM1, calibZEM2, sPMRef1, sPMRef2,
		  nDetSpecNLeft, nDetSpecPLeft, nDetSpecNRight, nDetSpecPRight, 
		  nGenSpec, nGenSpecLeft, nGenSpecRight, 
		  nPart, nPartTotLeft, nPartTotRight, 
		  impPar, impPar1, impPar2);
		  
  AliZDCReco* preco = &reco;
  const Int_t kBufferSize = 4000;
  clustersTree->Branch("ZDC", "AliZDCReco", &preco, kBufferSize);

  // write the output tree
  clustersTree->Fill();
}

//_____________________________________________________________________________
void AliZDCReconstructor::ReconstructEventPbPb(TTree *clustersTree, 
	Float_t* corrADCZN1, Float_t* corrADCZP1, Float_t* corrADCZN2, Float_t* corrADCZP2,
	Float_t* corrADCZEM1, Float_t* corrADCZEM2, Float_t* sPMRef1, Float_t* sPMRef2) const
{
  // ****************** Reconstruct one event ******************

  // ******	Retrieving calibration data 
  // --- Equalization coefficients ---------------------------------------------
  Float_t equalCoeffZN1[5], equalCoeffZP1[5], equalCoeffZN2[5], equalCoeffZP2[5];
  for(Int_t ji=0; ji<5; ji++){
     equalCoeffZN1[ji] = fTowCalibData->GetZN1EqualCoeff(ji);
     equalCoeffZP1[ji] = fTowCalibData->GetZP1EqualCoeff(ji); 
     equalCoeffZN2[ji] = fTowCalibData->GetZN2EqualCoeff(ji); 
     equalCoeffZP2[ji] = fTowCalibData->GetZP2EqualCoeff(ji); 
  }
  // --- Energy calibration factors ------------------------------------
  Float_t valFromOCDB[6], calibEne[6];
  for(Int_t ij=0; ij<6; ij++){
    valFromOCDB[ij] = fEnCalibData->GetEnCalib(ij);
    if(ij<4){
      if(valFromOCDB[ij]!=0) calibEne[ij] = fBeamEnergy/valFromOCDB[ij];
      else AliWarning(" Value from OCDB for E calibration = 0 !!!\n");
    } 
    else calibEne[ij] = valFromOCDB[ij];
  }
  
  // ******	Equalization of detector responses
  Float_t equalTowZN1[10], equalTowZN2[10], equalTowZP1[10], equalTowZP2[10];
  for(Int_t gi=0; gi<10; gi++){
     equalTowZN1[gi] = corrADCZN1[gi]*equalCoeffZN1[gi];
     equalTowZP1[gi] = corrADCZP1[gi]*equalCoeffZP1[gi];
     equalTowZN2[gi] = corrADCZN2[gi]*equalCoeffZN2[gi];
     equalTowZP2[gi] = corrADCZP2[gi]*equalCoeffZP2[gi];
  }
  
  // ******	Summed response for hadronic calorimeter (SUMMED and then CALIBRATED!)
  Float_t calibSumZN1[]={0,0}, calibSumZN2[]={0,0}, calibSumZP1[]={0,0}, calibSumZP2[]={0,0};
  for(Int_t gi=0; gi<5; gi++){
       calibSumZN1[0] += equalTowZN1[gi];
       calibSumZP1[0] += equalTowZP1[gi];
       calibSumZN2[0] += equalTowZN2[gi];
       calibSumZP2[0] += equalTowZP2[gi];
       //
       calibSumZN1[1] += equalTowZN1[gi+5];
       calibSumZP1[1] += equalTowZP1[gi+5];
       calibSumZN2[1] += equalTowZN2[gi+5];
       calibSumZP2[1] += equalTowZP2[gi+5];
  }
  // High gain chain
  calibSumZN1[0] = calibSumZN1[0]*calibEne[0]/8.;
  calibSumZP1[0] = calibSumZP1[0]*calibEne[1]/8.;
  calibSumZN2[0] = calibSumZN2[0]*calibEne[2]/8.;
  calibSumZP2[0] = calibSumZP2[0]*calibEne[3]/8.;
  // Low gain chain
  calibSumZN1[1] = calibSumZN1[1]*calibEne[0];
  calibSumZP1[1] = calibSumZP1[1]*calibEne[1];
  calibSumZN2[1] = calibSumZN2[1]*calibEne[2];
  calibSumZP2[1] = calibSumZP2[1]*calibEne[3];
  //
  Float_t sumZEM[]={0,0}, calibZEM1[]={0,0}, calibZEM2[]={0,0};
  calibZEM1[0] = corrADCZEM1[0]*calibEne[5]/8.;
  calibZEM1[1] = corrADCZEM1[1]*calibEne[5];
  calibZEM2[0] = corrADCZEM2[0]*calibEne[5]/8.;
  calibZEM2[1] = corrADCZEM2[1]*calibEne[5];
  for(Int_t k=0; k<2; k++) sumZEM[k] = calibZEM1[k] + calibZEM2[k];
    
  // ******	Energy calibration of detector responses
  Float_t calibTowZN1[10], calibTowZN2[10], calibTowZP1[10], calibTowZP2[10];
  for(Int_t gi=0; gi<5; gi++){
     // High gain chain
     calibTowZN1[gi] = equalTowZN1[gi]*calibEne[0]/8.;
     calibTowZP1[gi] = equalTowZP1[gi]*calibEne[1]/8.;
     calibTowZN2[gi] = equalTowZN2[gi]*calibEne[2]/8.;
     calibTowZP2[gi] = equalTowZP2[gi]*calibEne[3]/8.;
     // Low gain chain
     calibTowZN1[gi+5] = equalTowZN1[gi+5]*calibEne[0];
     calibTowZP1[gi+5] = equalTowZP1[gi+5]*calibEne[1];
     calibTowZN2[gi+5] = equalTowZN2[gi+5]*calibEne[2];
     calibTowZP2[gi+5] = equalTowZP2[gi+5]*calibEne[3];
  }
  
  //  ******	Number of detected spectator nucleons
  Int_t nDetSpecNLeft=0, nDetSpecPLeft=0, nDetSpecNRight=0, nDetSpecPRight=0;
  if(fBeamEnergy!=0){
    nDetSpecNLeft = (Int_t) (calibSumZN1[0]/fBeamEnergy);
    nDetSpecPLeft = (Int_t) (calibSumZP1[0]/fBeamEnergy);
    nDetSpecNRight = (Int_t) (calibSumZN2[0]/fBeamEnergy);
    nDetSpecPRight = (Int_t) (calibSumZP2[0]/fBeamEnergy);
  }
  else AliWarning(" ATTENTION!!! fBeamEnergy=0 -> N_spec will be ZERO!!! \n");
  /*printf("\n\t AliZDCReconstructor -> nDetSpecNLeft %d, nDetSpecPLeft %d,"
    " nDetSpecNRight %d, nDetSpecPRight %d\n",nDetSpecNLeft, nDetSpecPLeft, 
    nDetSpecNRight, nDetSpecPRight);*/
  
  if(fIsCalibrationMB == kFALSE){
    // ******	Reconstruction parameters ------------------ 
    // Ch. debug
    //fRecoParam->Print("");
    //
    TH2F *hZDCvsZEM  = fRecoParam->GethZDCvsZEM();
    TH2F *hZDCCvsZEM = fRecoParam->GethZDCCvsZEM();
    TH2F *hZDCAvsZEM = fRecoParam->GethZDCAvsZEM();
    TH1D *hNpartDist = fRecoParam->GethNpartDist();
    TH1D *hbDist = fRecoParam->GethbDist();    
    Float_t ClkCenter = fRecoParam->GetClkCenter();
    //
    Double_t xHighEdge = hZDCvsZEM->GetXaxis()->GetXmax();
    Double_t origin = xHighEdge*ClkCenter;
    // Ch. debug
    printf("\n\n  xHighEdge %1.2f, origin %1.4f \n", xHighEdge, origin);
    //
    // ====> Summed ZDC info (sideA+side C)
    TF1 *line = new TF1("line","[0]*x+[1]",0.,xHighEdge);
    Float_t y = (calibSumZN1[0]+calibSumZP1[0]+calibSumZN2[0]+calibSumZP2[0])/1000.;
    Float_t x = (calibZEM1[0]+calibZEM2[0])/1000.;
    line->SetParameter(0, y/(x-origin));
    line->SetParameter(1, -origin*y/(x-origin));
    // Ch. debug
    printf("  ***************** Summed ZDC info (sideA+side C) \n");
    printf("  E_{ZEM} %1.4f, E_{ZDC} %1.2f, TF1: %1.2f*x + %1.2f   ", x, y,y/(x-origin),-origin*y/(x-origin));
    //
    Double_t countPerc=0;
    Double_t xBinCenter=0, yBinCenter=0;
    for(Int_t nbinx=1; nbinx<=hZDCvsZEM->GetNbinsX(); nbinx++){
      for(Int_t nbiny=1; nbiny<=hZDCvsZEM->GetNbinsY(); nbiny++){
         xBinCenter = hZDCvsZEM->GetXaxis()->GetBinCenter(nbinx);
         yBinCenter = hZDCvsZEM->GetYaxis()->GetBinCenter(nbiny);
         //
	 if(line->GetParameter(0)>0){
           if(yBinCenter < (line->GetParameter(0)*xBinCenter + line->GetParameter(1))){
             countPerc += hZDCvsZEM->GetBinContent(nbinx,nbiny);
             // Ch. debug
             /*printf(" xBinCenter  %1.3f, yBinCenter %1.0f,  countPerc %1.0f\n", 
	 	xBinCenter, yBinCenter, countPerc);*/
           }
   	 }
   	 else{
   	   if(yBinCenter > (line->GetParameter(0)*xBinCenter + line->GetParameter(1))){
   	     countPerc += hZDCvsZEM->GetBinContent(nbinx,nbiny);
             // Ch. debug
             /*printf(" xBinCenter  %1.3f, yBinCenter %1.0f,  countPerc %1.0f\n", 
	 	xBinCenter, yBinCenter, countPerc);*/
   	   }
   	 }
      }
    }
    //
    Double_t xSecPerc = 0.;
    if(hZDCvsZEM->GetEntries()!=0){ 
      xSecPerc = countPerc/hZDCvsZEM->GetEntries();
    }
    else{
      AliWarning("  Histogram hZDCvsZEM from OCDB has no entries!!!");
    }
    // Ch. debug
    //printf("  xSecPerc %1.4f  \n", xSecPerc);

    // ====> side C
    TF1 *lineC = new TF1("lineC","[0]*x+[1]",0.,xHighEdge);
    Float_t yC = (calibSumZN1[0]+calibSumZP1[0])/1000.;
    lineC->SetParameter(0, yC/(x-origin));
    lineC->SetParameter(1, -origin*yC/(x-origin));
    // Ch. debug
    //printf("  ***************** Side C \n");
    //printf("  E_{ZEM} %1.4f, E_{ZDCC} %1.2f, TF1: %1.2f*x + %1.2f   ", x, yC,yC/(x-origin),-origin*yC/(x-origin));
    //
    Double_t countPercC=0;
    Double_t xBinCenterC=0, yBinCenterC=0;
    for(Int_t nbinx=1; nbinx<=hZDCCvsZEM->GetNbinsX(); nbinx++){
      for(Int_t nbiny=1; nbiny<=hZDCCvsZEM->GetNbinsY(); nbiny++){
    	 xBinCenterC = hZDCCvsZEM->GetXaxis()->GetBinCenter(nbinx);
    	 yBinCenterC = hZDCCvsZEM->GetYaxis()->GetBinCenter(nbiny);
    	 if(lineC->GetParameter(0)>0){
           if(yBinCenterC < (lineC->GetParameter(0)*xBinCenterC + lineC->GetParameter(1))){
             countPercC += hZDCCvsZEM->GetBinContent(nbinx,nbiny);
           }
    	 }
    	 else{
    	   if(yBinCenterC > (lineC->GetParameter(0)*xBinCenterC + lineC->GetParameter(1))){
    	     countPercC += hZDCCvsZEM->GetBinContent(nbinx,nbiny);
    	   }
    	 }
      }
    }
    //
    Double_t xSecPercC = 0.;
    if(hZDCCvsZEM->GetEntries()!=0){ 
      xSecPercC = countPercC/hZDCCvsZEM->GetEntries();
    }
    else{
      AliWarning("  Histogram hZDCCvsZEM from OCDB has no entries!!!");
    }
    // Ch. debug
    //printf("  xSecPercC %1.4f  \n", xSecPercC);
    
    // ====> side A
    TF1 *lineA = new TF1("lineA","[0]*x+[1]",0.,xHighEdge);
    Float_t yA = (calibSumZN2[0]+calibSumZP2[0])/1000.;
    lineA->SetParameter(0, yA/(x-origin));
    lineA->SetParameter(1, -origin*yA/(x-origin));
    //
    // Ch. debug
    //printf("  ***************** Side A \n");
    //printf("  E_{ZEM} %1.4f, E_{ZDCA} %1.2f, TF1: %1.2f*x + %1.2f   ", x, yA,yA/(x-origin),-origin*yA/(x-origin));
    //
    Double_t countPercA=0;
    Double_t xBinCenterA=0, yBinCenterA=0;
    for(Int_t nbinx=1; nbinx<=hZDCAvsZEM->GetNbinsX(); nbinx++){
      for(Int_t nbiny=1; nbiny<=hZDCAvsZEM->GetNbinsY(); nbiny++){
    	 xBinCenterA = hZDCAvsZEM->GetXaxis()->GetBinCenter(nbinx);
    	 yBinCenterA = hZDCAvsZEM->GetYaxis()->GetBinCenter(nbiny);
    	 if(lineA->GetParameter(0)>0){
           if(yBinCenterA < (lineA->GetParameter(0)*xBinCenterA + lineA->GetParameter(1))){
             countPercA += hZDCAvsZEM->GetBinContent(nbinx,nbiny);
           }
   	 }
   	 else{
   	   if(yBinCenterA > (lineA->GetParameter(0)*xBinCenterA + lineA->GetParameter(1))){
   	     countPercA += hZDCAvsZEM->GetBinContent(nbinx,nbiny);
   	   }
   	 }
      }
    }
    //
    Double_t xSecPercA = 0.;
    if(hZDCAvsZEM->GetEntries()!=0){ 
      xSecPercA = countPercA/hZDCAvsZEM->GetEntries();
    }
    else{
      AliWarning("  Histogram hZDCAvsZEM from OCDB has no entries!!!");
    }
    // Ch. debug
    //printf("  xSecPercA %1.4f  \n", xSecPercA);
    
    //  ******    Number of participants (from E_ZDC vs. E_ZEM correlation)
    Int_t nPart=0, nPartC=0, nPartA=0;
    Double_t nPartFrac=0., nPartFracC=0., nPartFracA=0.;
    for(Int_t npbin=1; npbin<hNpartDist->GetNbinsX(); npbin++){
      nPartFrac += (hNpartDist->GetBinContent(npbin))/(hNpartDist->GetEntries());
      if((1.-nPartFrac) < xSecPerc){
    	nPart = (Int_t) hNpartDist->GetBinLowEdge(npbin);
        // Ch. debug
        //printf("  ***************** Summed ZDC info (sideA+side C) \n");
        //printf("  nPartFrac %1.4f, nPart %d\n", nPartFrac, nPart);
    	break;
      }
    }
    if(nPart<0) nPart=0;
    //
    for(Int_t npbin=1; npbin<hNpartDist->GetNbinsX(); npbin++){
      nPartFracC += (hNpartDist->GetBinContent(npbin))/(hNpartDist->GetEntries());
      if((1.-nPartFracC) < xSecPercC){
    	nPartC = (Int_t) hNpartDist->GetBinLowEdge(npbin);
        // Ch. debug
        //printf("  ***************** Side C \n");
        //printf("  nPartFracC %1.4f, nPartC %d\n", nPartFracC, nPartC);
    	break;
    }
    }
    if(nPartC<0) nPartC=0;
    //
    for(Int_t npbin=1; npbin<hNpartDist->GetNbinsX(); npbin++){
      nPartFracA += (hNpartDist->GetBinContent(npbin))/(hNpartDist->GetEntries());
      if((1.-nPartFracA) < xSecPercA){
    	nPartA = (Int_t) hNpartDist->GetBinLowEdge(npbin);
        // Ch. debug
        //printf("  ***************** Side A \n");
        //printf("  nPartFracA %1.4f, nPartA %d\n\n", nPartFracA, nPartA);
        break;
      }
    }
    if(nPartA<0) nPartA=0;
    
    //  ******    Impact parameter (from E_ZDC vs. E_ZEM correlation)
    Float_t b=0, bC=0, bA=0;
    Double_t bFrac=0., bFracC=0., bFracA=0.;
    for(Int_t ibbin=1; ibbin<hbDist->GetNbinsX(); ibbin++){
      bFrac += (hbDist->GetBinContent(ibbin))/(hbDist->GetEntries());
      if((1.-bFrac) < xSecPerc){
   	b = hbDist->GetBinLowEdge(ibbin);
   	break;
      }
    }
    //
    for(Int_t ibbin=1; ibbin<hbDist->GetNbinsX(); ibbin++){
      bFracC += (hbDist->GetBinContent(ibbin))/(hbDist->GetEntries());
      if((1.-bFracC) < xSecPercC){
   	bC = hbDist->GetBinLowEdge(ibbin);
   	break;
      }
    }
    //
    for(Int_t ibbin=1; ibbin<hbDist->GetNbinsX(); ibbin++){
      bFracA += (hbDist->GetBinContent(ibbin))/(hNpartDist->GetEntries());
      if((1.-bFracA) < xSecPercA){
   	bA = hbDist->GetBinLowEdge(ibbin);
   	break;
      }
    }

    //  ******	Number of spectator nucleons 
    Int_t nGenSpec=0, nGenSpecC=0, nGenSpecA=0;
    //
    nGenSpec = 416 - nPart;
    nGenSpecC = 416 - nPartC;
    nGenSpecA = 416 - nPartA;
    if(nGenSpec>416) nGenSpec=416; if(nGenSpec<0) nGenSpec=0;
    if(nGenSpecC>416) nGenSpecC=416; if(nGenSpecC<0) nGenSpecC=0;
    if(nGenSpecA>416) nGenSpecA=416; if(nGenSpecA<0) nGenSpecA=0;
  
    //  Ch. debug
    /*printf("\n\t AliZDCReconstructor -> calibSumZN1[0] %1.0f, calibSumZP1[0] %1.0f,"
        "  calibSumZN2[0] %1.0f, calibSumZP2[0] %1.0f, corrADCZEMHG %1.0f\n", 
        calibSumZN1[0],calibSumZP1[0],calibSumZN2[0],calibSumZP2[0],corrADCZEMHG);
    printf("\t AliZDCReconstructor -> nGenSpecLeft %d nGenSpecRight %d\n", 
        nGenSpecLeft, nGenSpecRight);
    printf("\t AliZDCReconstructor ->  NpartL %d,  NpartR %d,  b %1.2f fm\n\n",nPartTotLeft, nPartTotRight, impPar);
    */
  
    // create the output tree
    AliZDCReco reco(calibSumZN1, calibSumZP1, calibSumZN2, calibSumZP2, 
  	  	    calibTowZN1, calibTowZP1, calibTowZN2, calibTowZP2, 
		    calibZEM1, calibZEM2, sPMRef1, sPMRef2,
		    nDetSpecNLeft, nDetSpecPLeft, nDetSpecNRight, nDetSpecPRight, 
		    nGenSpec, nGenSpecA, nGenSpecC, 
		    nPart, nPartA, nPartC, b, bA, bC);
		  
    AliZDCReco* preco = &reco;
    const Int_t kBufferSize = 4000;
    clustersTree->Branch("ZDC", "AliZDCReco", &preco, kBufferSize);
    // write the output tree
    clustersTree->Fill();
  
    delete lineC;  delete lineA;
  } // ONLY IF fIsCalibrationMB==kFALSE
  else{
    // create the output tree
    AliZDCReco reco(calibSumZN1, calibSumZP1, calibSumZN2, calibSumZP2, 
  	  	    calibTowZN1, calibTowZP1, calibTowZN2, calibTowZP2, 
		    calibZEM1, calibZEM2, sPMRef1, sPMRef2,
		    nDetSpecNLeft, nDetSpecPLeft, nDetSpecNRight, nDetSpecPRight, 
		    0, 0, 0, 
		    0, 0, 0, 0., 0., 0.);
		  
    AliZDCReco* preco = &reco;
    const Int_t kBufferSize = 4000;
    clustersTree->Branch("ZDC", "AliZDCReco", &preco, kBufferSize);
    // write the output tree
    clustersTree->Fill();
  }
}

//_____________________________________________________________________________
void AliZDCReconstructor::BuildRecoParam(TH2F* hCorr, TH2F* hCorrC, TH2F* hCorrA,
     			Float_t ZDCC, Float_t ZDCA, Float_t ZEM) const
{
  // Calculate RecoParam object for Pb-Pb data
  hCorr->Fill(ZDCC+ZDCA, ZEM);
  hCorrC->Fill(ZDCC, ZEM);
  hCorrA->Fill(ZDCA, ZEM);
  //
  /*TH1D*	hNpartDist;
  TH1D*	hbDist;    
  Float_t clkCenter;*/   
 
}

//_____________________________________________________________________________
void AliZDCReconstructor::FillZDCintoESD(TTree *clustersTree, AliESDEvent* esd) const
{
  // fill energies and number of participants to the ESD

  if(fIsCalibrationMB==kTRUE) WritePbPbRecoParamInOCDB();
  
  AliZDCReco reco;
  AliZDCReco* preco = &reco;
  clustersTree->SetBranchAddress("ZDC", &preco);

  clustersTree->GetEntry(0);
  //
  AliESDZDC * esdzdc = esd->GetESDZDC();
  Float_t tZN1Ene[5], tZN2Ene[5], tZP1Ene[5], tZP2Ene[5];
  Float_t tZN1EneLR[5], tZN2EneLR[5], tZP1EneLR[5], tZP2EneLR[5];
  for(Int_t i=0; i<5; i++){
     tZN1Ene[i] = reco.GetZN1HREnTow(i);
     tZN2Ene[i] = reco.GetZN2HREnTow(i);
     tZP1Ene[i] = reco.GetZP1HREnTow(i);
     tZP2Ene[i] = reco.GetZP2HREnTow(i);
     //
     tZN1EneLR[i] = reco.GetZN1LREnTow(i);
     tZN2EneLR[i] = reco.GetZN2LREnTow(i);
     tZP1EneLR[i] = reco.GetZP1LREnTow(i);
     tZP2EneLR[i] = reco.GetZP2LREnTow(i);
  }
  //
  esdzdc->SetZN1TowerEnergy(tZN1Ene);
  esdzdc->SetZN2TowerEnergy(tZN2Ene);
  esdzdc->SetZP1TowerEnergy(tZP1Ene);
  esdzdc->SetZP2TowerEnergy(tZP2Ene);
  //
  esdzdc->SetZN1TowerEnergyLR(tZN1EneLR);
  esdzdc->SetZN2TowerEnergyLR(tZN2EneLR);
  esdzdc->SetZP1TowerEnergyLR(tZP1EneLR);
  esdzdc->SetZP2TowerEnergyLR(tZP2EneLR);
  // 
  Int_t nPart  = reco.GetNParticipants();
  Int_t nPartA = reco.GetNPartSideA();
  Int_t nPartC = reco.GetNPartSideC();
  Double_t b  = reco.GetImpParameter();
  Double_t bA = reco.GetImpParSideA();
  Double_t bC = reco.GetImpParSideC();
 //
  esd->SetZDC(reco.GetZN1HREnergy(), reco.GetZP1HREnergy(), reco.GetZEM1HRsignal(), 
  	      reco.GetZEM2HRsignal(), reco.GetZN2HREnergy(), reco.GetZP2HREnergy(), 
	      nPart, nPartA, nPartC, b, bA, bC, fRecoFlag);
  
}

//_____________________________________________________________________________
AliCDBStorage* AliZDCReconstructor::SetStorage(const char *uri) 
{
  // Setting the storage

  Bool_t deleteManager = kFALSE;
  
  AliCDBManager *manager = AliCDBManager::Instance();
  AliCDBStorage *defstorage = manager->GetDefaultStorage();
  
  if(!defstorage || !(defstorage->Contains("ZDC"))){ 
     AliWarning("No default storage set or default storage doesn't contain ZDC!");
     manager->SetDefaultStorage(uri);
     deleteManager = kTRUE;
  }
 
  AliCDBStorage *storage = manager->GetDefaultStorage();

  if(deleteManager){
    AliCDBManager::Instance()->UnsetDefaultStorage();
    defstorage = 0;   // the storage is killed by AliCDBManager::Instance()->Destroy()
  }

  return storage; 
}

//_____________________________________________________________________________
AliZDCPedestals* AliZDCReconstructor::GetPedData() const
{

  // Getting pedestal calibration object for ZDC set

  AliCDBEntry  *entry = AliCDBManager::Instance()->Get("ZDC/Calib/Pedestals");
  if(!entry) AliFatal("No calibration data loaded!");  

  AliZDCPedestals *calibdata = dynamic_cast<AliZDCPedestals*>  (entry->GetObject());
  if(!calibdata)  AliFatal("Wrong calibration object in calibration  file!");

  return calibdata;
}

//_____________________________________________________________________________
AliZDCEnCalib* AliZDCReconstructor::GetEnCalibData() const
{

  // Getting energy and equalization calibration object for ZDC set

  AliCDBEntry  *entry = AliCDBManager::Instance()->Get("ZDC/Calib/EnCalib");
  if(!entry) AliFatal("No calibration data loaded!");  

  AliZDCEnCalib *calibdata = dynamic_cast<AliZDCEnCalib*> (entry->GetObject());
  if(!calibdata)  AliFatal("Wrong calibration object in calibration  file!");

  return calibdata;
}

//_____________________________________________________________________________
AliZDCTowerCalib* AliZDCReconstructor::GetTowCalibData() const
{

  // Getting energy and equalization calibration object for ZDC set

  AliCDBEntry  *entry = AliCDBManager::Instance()->Get("ZDC/Calib/TowCalib");
  if(!entry) AliFatal("No calibration data loaded!");  

  AliZDCTowerCalib *calibdata = dynamic_cast<AliZDCTowerCalib*> (entry->GetObject());
  if(!calibdata)  AliFatal("Wrong calibration object in calibration  file!");

  return calibdata;
}

//_____________________________________________________________________________
AliZDCRecoParampp* AliZDCReconstructor::GetppRecoParamFromOCDB() const
{

  // Getting reconstruction parameters from OCDB

  AliCDBEntry  *entry = AliCDBManager::Instance()->Get("ZDC/Calib/RecoParampp");
  if(!entry) AliFatal("No RecoParam data found in OCDB!");  
  
  AliZDCRecoParampp *param = dynamic_cast<AliZDCRecoParampp*> (entry->GetObject());
  if(!param)  AliFatal("No RecoParam object in OCDB entry!");
  
  return param;

}

//_____________________________________________________________________________
AliZDCRecoParamPbPb* AliZDCReconstructor::GetPbPbRecoParamFromOCDB() const
{

  // Getting reconstruction parameters from OCDB

  AliCDBEntry  *entry = AliCDBManager::Instance()->Get("ZDC/Calib/RecoParamPbPb");
  if(!entry) AliFatal("No RecoParam data found in OCDB!");  
  
  AliZDCRecoParamPbPb *param = dynamic_cast<AliZDCRecoParamPbPb*> (entry->GetObject());
  if(!param)  AliFatal("No RecoParam object in OCDB entry!");
  
  return param;

}

//_____________________________________________________________________________
void AliZDCReconstructor::WritePbPbRecoParamInOCDB() const
{

  // Writing Pb-Pb reconstruction parameters from OCDB

  AliCDBManager *man = AliCDBManager::Instance();
  AliCDBMetaData *md= new AliCDBMetaData();
  md->SetResponsible("Chiara Oppedisano");
  md->SetComment("ZDC Pb-Pb reconstruction parameters");
  md->SetObjectClassName("AliZDCRecoParamPbPb");
  AliCDBId id("ZDC/Calib/RecoParamPbPb",fNRun,AliCDBRunRange::Infinity());
  man->Put(fRecoParam, id, md);

}

