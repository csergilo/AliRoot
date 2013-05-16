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

//Class to calculate the intrinsic efficiency of the detection elements of the
//MUON tracking chambers in function of the position in the detection element.
//WOrk on ESD only
//Author:  Nicolas LE BRIS - SUBATECH Nantes
// Modified by Matthieu LENHARDT - SUBATECH Nantes
// Modified by Antoine LARDEUX - SUBATECH Nantes

// ROOT includes
#include <TList.h>
#include <TH3F.h>
#include <TH2F.h>
#include <TObjArray.h>
#include <TGeoGlobalMagField.h>

// STEER includes
#include "AliESDEvent.h"
#include "AliESDMuonTrack.h"
#include "AliGeomManager.h"
#include "AliCDBManager.h"
#include "AliESDVZERO.h"

// ANALYSIS includes
#include "AliInputEventHandler.h"
#include "AliAnalysisManager.h"
#include "AliAnalysisTaskMuonTrackingEff.h"
#include "AliCentrality.h"

//MUON includes
#include "AliMUONCDB.h"
#include "AliMUONESDInterface.h"
#include "AliMUONGeometryTransformer.h"
#include "AliMUONTrack.h"
#include "AliMUONTrackParam.h"
#include "AliMUONTrackExtrap.h"
#include "AliMUONVCluster.h"
#include "AliMUONConstants.h"

//include MUON/mapping:
#include "AliMpDEManager.h"
#include "AliMpSegmentation.h"
#include "AliMpVSegmentation.h"
#include "AliMpPad.h"

ClassImp(AliAnalysisTaskMuonTrackingEff)

const Int_t AliAnalysisTaskMuonTrackingEff::fgkNbrOfDetectionElt[10] = {4, 4, 4, 4, 18, 18, 26, 26, 26, 26};
const Int_t AliAnalysisTaskMuonTrackingEff::fgkOffset = 100;

//________________________________________________________________________
AliAnalysisTaskMuonTrackingEff::AliAnalysisTaskMuonTrackingEff() :
  AliAnalysisTaskSE(),
  fOCDBLoaded(kFALSE),
  fOCDBpath(""),
  fMatchTrig(kFALSE),
  fApplyAccCut(kFALSE),
  fCurrentCentrality(0.),
  fTransformer(0x0),
  fDetEltTDHistList(0x0),
  fDetEltTTHistList(0x0),
  fDetEltSDHistList(0x0),
  fChamberTDHistList(0x0),
  fChamberTTHistList(0x0),
  fChamberSDHistList(0x0),
  fExtraHistList(0x0)
{
  /// Default constructor
}

//________________________________________________________________________
AliAnalysisTaskMuonTrackingEff::AliAnalysisTaskMuonTrackingEff(TString name) :
  AliAnalysisTaskSE(name),
  fOCDBLoaded(kFALSE),
  fOCDBpath("raw://"),
  fMatchTrig(kFALSE),
  fApplyAccCut(kFALSE),
  fCurrentCentrality(100.),
  fTransformer(0x0),
  fDetEltTDHistList(0x0),
  fDetEltTTHistList(0x0),
  fDetEltSDHistList(0x0),
  fChamberTDHistList(0x0),
  fChamberTTHistList(0x0),
  fChamberSDHistList(0x0),
  fExtraHistList(0x0)
{
  /// Constructor
  
  // Output slots 0 to 5 writes into a TClonesArray:
  DefineOutput(1, TList::Class());
  DefineOutput(2, TList::Class());
  DefineOutput(3, TList::Class());
  DefineOutput(4, TList::Class());
  DefineOutput(5, TList::Class());
  DefineOutput(6, TList::Class());
  DefineOutput(7, TList::Class());
}

//________________________________________________________________________
AliAnalysisTaskMuonTrackingEff::~AliAnalysisTaskMuonTrackingEff()
{
  /// Destructor
  if (!AliAnalysisManager::GetAnalysisManager()->IsProofMode()) {
    delete fDetEltTDHistList;
    delete fDetEltTTHistList;
    delete fDetEltSDHistList;
    delete fChamberTDHistList;
    delete fChamberTTHistList;
    delete fChamberSDHistList;
    delete fExtraHistList;
  }
  delete fTransformer;
}

//________________________________________________________________________
void AliAnalysisTaskMuonTrackingEff::NotifyRun()
{
  /// Load the OCDB and the Geometry
  
  // Load it only once
  if (fOCDBLoaded) return;
  
  // OCDB
  AliCDBManager* man = AliCDBManager::Instance();
  if (man->IsDefaultStorageSet()) printf("EfficiencyTask: CDB default storage already set!\n");
  else man->SetDefaultStorage(fOCDBpath.Data());
  if (man->GetRun() > -1) printf("EfficiencyTask: run number already set!\n");
  else man->SetRun(fCurrentRunNumber);
  
  // Geometry
  if (!AliGeomManager::GetGeometry()) {
    AliGeomManager::LoadGeometry();
    if (!AliGeomManager::GetGeometry()) return;  
    if (!AliGeomManager::ApplyAlignObjsFromCDB("MUON")) return;
  }
  fTransformer = new AliMUONGeometryTransformer();
  fTransformer->LoadGeometryData();
  
  // Magnetic field for track extrapolation
  if (!TGeoGlobalMagField::Instance()->GetField()) {
    if (!AliMUONCDB::LoadField()) return;
  }
  
  // Mapping
  if (!AliMpSegmentation::Instance(kFALSE)) {
    if (!AliMUONCDB::LoadMapping(kTRUE)) return;
  }
  
  // RecoParam for refitting
  if (!AliMUONESDInterface::GetTracker()) {
    AliMUONRecoParam* recoParam = AliMUONCDB::LoadRecoParam();
    if (!recoParam) return;
    AliMUONESDInterface::ResetTracker(recoParam);
  }
  
  fOCDBLoaded = kTRUE;
}

//________________________________________________________________________
void AliAnalysisTaskMuonTrackingEff::UserCreateOutputObjects()
{
  /// Define efficiency histograms
  
  fDetEltTDHistList  = new TList();
  fDetEltTDHistList->SetOwner();
  fDetEltTTHistList  = new TList();
  fDetEltTTHistList->SetOwner();
  fDetEltSDHistList  = new TList();
  fDetEltSDHistList->SetOwner();
  fChamberTDHistList = new TList();
  fChamberTDHistList->SetOwner();
  fChamberTTHistList = new TList();
  fChamberTTHistList->SetOwner();
  fChamberSDHistList = new TList();
  fChamberSDHistList->SetOwner();
  fExtraHistList = new TList();
  fExtraHistList->SetOwner();
  
  TH2F *h2;
  TH3F *h3;
  TString histNameTD;
  TString histNameTT;
  TString histNameSD;
  TString histTitle;
  Int_t nCentBins = 22;
  Double_t centRange[2] = {-5., 105.};
  Int_t iDEGlobal = 0;

  
  for (Int_t iCh = 0; iCh < 10; iCh++)
  {
    // histograms per chamber
    histTitle.Form("ChamberNbr %d", iCh+1);
    histNameTD.Form("TD_ChamberNbr%d", iCh+1);
    histNameTT.Form("TT_ChamberNbr%d",iCh+1);
    histNameSD.Form("SD_ChamberNbr%d", iCh+1);
    h2 = new TH2F(histNameTD, histTitle, fgkNbrOfDetectionElt[iCh], 0., fgkNbrOfDetectionElt[iCh], nCentBins, centRange[0], centRange[1]);
    fChamberTDHistList->AddAt(h2, iCh);
    h2 = new TH2F(histNameTT, histTitle, fgkNbrOfDetectionElt[iCh], 0., fgkNbrOfDetectionElt[iCh], nCentBins, centRange[0], centRange[1]);
    fChamberTTHistList->AddAt(h2, iCh);
    h2 = new TH2F(histNameSD, histTitle, fgkNbrOfDetectionElt[iCh], 0., fgkNbrOfDetectionElt[iCh], nCentBins, centRange[0], centRange[1]);
    fChamberSDHistList->AddAt(h2, iCh);
    
    // histograms per DE
    for (Int_t iDE = 0; iDE < fgkNbrOfDetectionElt[iCh]; iDE++)
    {
      Int_t deId = FromLocalId2DetElt(iCh, iDE);
      histTitle.Form("detEltNbr %d",deId);
      histNameTD.Form("TD_detEltNbr%d",deId);
      histNameTT.Form("TT_detEltNbr%d",deId);
      histNameSD.Form("SD_detEltNbr%d",deId);
      if(iCh < 4)
      {// chambers 1 -> 4
	h3 = new TH3F(histNameTD, histTitle, 12, -10.0 , 110.0, 12, -10.0, 110.0, nCentBins, centRange[0], centRange[1]);
	fDetEltTDHistList->AddAt(h3, iDEGlobal);
	h3 = new TH3F(histNameTT, histTitle, 12, -10.0 , 110.0, 12, -10.0, 110.0, nCentBins, centRange[0], centRange[1]);
	fDetEltTTHistList->AddAt(h3, iDEGlobal);
	h3 = new TH3F(histNameSD, histTitle, 12, -10.0 , 110.0, 12, -10.0, 110.0, nCentBins, centRange[0], centRange[1]);
	fDetEltSDHistList->AddAt(h3, iDEGlobal);
      }
      else 
      {// chambers 5 -> 10
	h3 = new TH3F(histNameTD, histTitle, 28, -140.0, 140.0, 8, -40.0, 40.0, nCentBins, centRange[0], centRange[1]);
	fDetEltTDHistList->AddAt(h3, iDEGlobal);	  
	h3 = new TH3F(histNameTT, histTitle, 28, -140.0, 140.0, 8, -40.0, 40.0, nCentBins, centRange[0], centRange[1]);
	fDetEltTTHistList->AddAt(h3, iDEGlobal);
	h3 = new TH3F(histNameSD, histTitle, 28, -140.0, 140.0, 8, -40.0, 40.0, nCentBins, centRange[0], centRange[1]);
	fDetEltSDHistList->AddAt(h3, iDEGlobal);
      }
      iDEGlobal++;
    }
  }
  
  // global histograms per chamber
  h2 = new TH2F("TD_Chambers 11", "Chambers 11", 10, 0.5, 10.5, nCentBins, centRange[0], centRange[1]);
  fChamberTDHistList->AddAt(h2, 10);
  h2 = new TH2F("TT_Chambers 11", "Chambers 11", 10, 0.5, 10.5, nCentBins, centRange[0], centRange[1]);
  fChamberTTHistList->AddAt(h2, 10);
  h2 = new TH2F("SD_Chambers 11", "Chambers 11", 10, 0.5, 10.5, nCentBins, centRange[0], centRange[1]);
  fChamberSDHistList->AddAt(h2, 10);


  //Extra histograms
  TH1F *fHistPt = new TH1F("fHistPt", "Pt distribution", 100, 0.0, 50);
  fExtraHistList->AddAt(fHistPt,0);

  
  // post the output data at least once
  PostData(1, fDetEltTDHistList);  
  PostData(2, fDetEltTTHistList); 
  PostData(3, fDetEltSDHistList);
  PostData(4, fChamberTDHistList);
  PostData(5, fChamberTTHistList);
  PostData(6, fChamberSDHistList);
  PostData(7, fExtraHistList);
}

//________________________________________________________________________
void AliAnalysisTaskMuonTrackingEff::UserExec(Option_t *)
{
  /// Main event loop
  
  // check the OCDB has been loaded properly
  if (!fOCDBLoaded) return;
  
  // get the current event
  AliESDEvent* esd = dynamic_cast<AliESDEvent*>(InputEvent());
  if (!esd) return;
  
  // get the centrality
  fCurrentCentrality = esd->GetCentrality()->GetCentralityPercentileUnchecked("V0M");
  
  // loop over tracks
  AliMUONTrack track;
  Int_t nTracks = (Int_t)esd->GetNumberOfMuonTracks();
  for (Int_t iTrack = 0; iTrack < nTracks; iTrack++)
  {
    AliESDMuonTrack* esdTrack = esd->GetMuonTrack(iTrack);
    
    if(!esdTrack->ContainTrackerData()) continue;
    
    if(fMatchTrig && !esdTrack->ContainTriggerData()) continue;
    
    Double_t thetaTrackAbsEnd = TMath::ATan(esdTrack->GetRAtAbsorberEnd()/505.) * TMath::RadToDeg();
    Double_t eta = esdTrack->Eta();
    if(fApplyAccCut && !(thetaTrackAbsEnd >= 2. && thetaTrackAbsEnd <= 9. && eta >= -4. && eta <= -2.5)) continue;

    ((TH1F*) fExtraHistList->At(0))->Fill(esdTrack->Pt());
    
    AliMUONESDInterface::ESDToMUON(*esdTrack, track);
    
    TrackParamLoop(track.GetTrackParamAtCluster());
  }
  
  // post the output data:
  PostData(1, fDetEltTDHistList);  
  PostData(2, fDetEltTTHistList);  
  PostData(3, fDetEltSDHistList);  
  PostData(4, fChamberTDHistList);
  PostData(5, fChamberTTHistList);
  PostData(6, fChamberSDHistList);
  PostData(7, fExtraHistList);
}
    
//________________________________________________________________________
void AliAnalysisTaskMuonTrackingEff::Terminate(Option_t *)
{
  /// final plots
}

//________________________________________________________________________
void AliAnalysisTaskMuonTrackingEff::TrackParamLoop(const TObjArray* trackParams)
{
  /// Loop on all the track params and fill the histos
  
  Bool_t trackFilter[10];
  memset(trackFilter, kFALSE, 10*sizeof(Bool_t));
  Bool_t chamberResponse[10];
  memset(chamberResponse, kFALSE, 10*sizeof(Bool_t));
  
  // check if the chamber responds
  Int_t nTrackParams = (Int_t) trackParams->GetEntriesFast();
  for (Int_t iTrackParam = 0; iTrackParam < nTrackParams; ++iTrackParam)
  { 
    Int_t chamberId = static_cast<AliMUONTrackParam*>(trackParams->UncheckedAt(iTrackParam))->GetClusterPtr()->GetChamberId();
    trackFilter[chamberId] = kTRUE;
    chamberResponse[chamberId] = kTRUE;
  }
  
  // To make sure the calculation of the efficiency of a given chamber (DE) is not biased by the tracking algorithm
  // we must make sure the track would have been reconstructed whatever this chamber (DE) has responded or not.
  // If the track is valid for a given chamber, the following code set trackFilter[chamberId] to kTRUE.
  for (Int_t station = 0; station < 4; ++station)
  {
    Int_t filter;
    Int_t ch1 = 2*station;
    Int_t ch2 = 2*station + 1;
    Int_t ch3 = 2*station + 2;
    Int_t ch4 = 2*station + 3;
    if (station < 3 )
    {
      filter           = trackFilter[ch1];
      trackFilter[ch1] = trackFilter[ch2];
      trackFilter[ch2] = filter;
    }
    else
    {
      if (chamberResponse[ch3] && chamberResponse[ch4])
      {
	filter           = trackFilter[ch1];
	trackFilter[ch1] = trackFilter[ch2];
	trackFilter[ch2] = filter;
      }
      else
      {
	trackFilter[ch1] = kFALSE;
	trackFilter[ch2] = kFALSE;
      }
      
      if (chamberResponse[ch1] && chamberResponse[ch2])
      {
	filter           = trackFilter[ch3];
	trackFilter[ch3] = trackFilter[ch4];
	trackFilter[ch4] = filter;
      }
      else
      {
	trackFilter[ch3] = kFALSE;
	trackFilter[ch4] = kFALSE;
      }
    }
  }
  
  // loop over track parameters
  Int_t oldChamber = -1;
  for (Int_t iTrackParam = 0; iTrackParam < nTrackParams; ++iTrackParam)
  {
    AliMUONTrackParam* trackParam = static_cast<AliMUONTrackParam*>(trackParams->UncheckedAt(iTrackParam));
    AliMUONVCluster* cluster = trackParam->GetClusterPtr();
    
    Int_t newChamber = cluster->GetChamberId();
    
    Int_t detElt = cluster->GetDetElemId();
    
    ///track position in the global coordinate system
    Double_t posXG = trackParam->GetNonBendingCoor(); 
    Double_t posYG = trackParam->GetBendingCoor(); 
    Double_t posZG = trackParam->GetZ(); 
    
    ///track position in the coordinate system of the DE
    Double_t posXL, posYL, posZL;
    fTransformer->Global2Local(detElt, posXG, posYG, posZG, posXL, posYL, posZL);
    
    // fill histograms if the track is valid for this chamber
    if(trackFilter[newChamber])
    {
      
      // fill histograms of the cluster positions on the detection element of the TRACKS DETECTED (TD)
      FillTDHistos(newChamber, detElt, posXL, posYL);
      
      // fill histograms of the cluster positions on the detection element of ALL THE TRACKS (TT)
      FillTTHistos(newChamber, detElt, posXL, posYL);
      
    } else {

     FillSDHistos(newChamber, detElt, posXL, posYL);
    
    }
    
    // look for missing cluster(s) if any
    if (newChamber != oldChamber) 
    {
      if (newChamber > oldChamber + 1)
      {
	Int_t nbrMissChamber = newChamber - (oldChamber + 1);
	
	// find the DE(s) that should have been fired and fill the corresponding histograms
	FindAndFillMissedDetElt(trackParam, trackFilter, oldChamber+1, nbrMissChamber);
      }
      
      // in case the last chamber has not responded
      if ( iTrackParam == nTrackParams-1 && newChamber != 9) FindAndFillMissedDetElt(trackParam, trackFilter, 9, 1);
    }
    
    oldChamber = newChamber; 
  } 
}

//________________________________________________________________________
void AliAnalysisTaskMuonTrackingEff::FindAndFillMissedDetElt(const AliMUONTrackParam* trackParam,
							     const Bool_t* trackFilter,
							     Int_t firstMissCh, Int_t nbrMissCh)
{
  /// Find which detection elements should have been hit but were missed, and fill the TT histos appropriately
  
  // copy track parameters for extrapolation
  AliMUONTrackParam extrapTrackParam(*trackParam);
  
  // loop over missing chambers
  for (Int_t iCh = 0; iCh < nbrMissCh; ++iCh)
  {
    Int_t chamber = firstMissCh + iCh;
    
    // skip this chamber if the track is not valid for it
    if(!trackFilter[chamber]) continue;
    
    Int_t nbrOfDetElt =  AliMpDEManager::GetNofDEInChamber(chamber, kTRUE);
    
    Double_t pos1[6] = {0, 0, 0, 0, 0, 0};
    Double_t pos2[6] = {0, 0, 0, 0, 0, 0};
    Double_t posMiss[2] = {0, 0};
    
    // track position at the chamber z
    pos1[2] = AliMUONConstants::DefaultChamberZ(chamber);
    AliMUONTrackExtrap::ExtrapToZ(&extrapTrackParam, pos1[2]);
    pos1[0] = extrapTrackParam.GetNonBendingCoor();
    pos1[1] = extrapTrackParam.GetBendingCoor();
    
    // track position at the chamber z + dz (where dz = distance between the 2 chamber in the station)
    pos2[2] = AliMUONConstants::DefaultChamberZ(chamber) + AliMUONConstants::DzCh();
    AliMUONTrackExtrap::ExtrapToZ(&extrapTrackParam, pos2[2]);
    pos2[0] = extrapTrackParam.GetNonBendingCoor();
    pos2[1] = extrapTrackParam.GetBendingCoor();
    
    // loop over all the detection element of the chamber
    for (Int_t iDE = 0; iDE < nbrOfDetElt; iDE++)
    {
      Int_t deId = (chamber + 1)*fgkOffset + iDE;
      
      // track positions (at chamber z and chamber z + dz) in the local coordinate system of the DE
      fTransformer->Global2Local(deId, pos1[0], pos1[1], pos1[2], pos1[3], pos1[4], pos1[5]);
      fTransformer->Global2Local(deId, pos2[0], pos2[1], pos2[2], pos2[3], pos2[4], pos2[5]);
      
      // track position at z=0 in the local coordinate system of the DE
      CoordinatesOfMissingCluster(pos1[3], pos1[4], pos1[5], pos2[3], pos2[4], pos2[5], posMiss[0], posMiss[1]);
      
      // check if the track cross this DE and fill the corresponding histogram
      if (CoordinatesInDetElt(deId, posMiss[0], posMiss[1])) FillTTHistos(chamber, deId, posMiss[0], posMiss[1]);
    }
  }
}

//________________________________________________________________________
void AliAnalysisTaskMuonTrackingEff::CoordinatesOfMissingCluster(Double_t x1, Double_t y1, Double_t z1,
								 Double_t x2, Double_t y2, Double_t z2,
								 Double_t& x, Double_t& y) const
{
  /// Compute the coordinates of the missing cluster. They are defined by the intersection between
  /// the straigth line joining two extrapolated points (1 and 2) and the detection element plane.
  /// In the local coordinates, this means Z=0 in the parametric equation of the line.
  Double_t t = - z1 / (z2 - z1);
  x = t * (x2 - x1) + x1;
  y = t * (y2 - y1) + y1;
}

//________________________________________________________________________
Bool_t AliAnalysisTaskMuonTrackingEff::CoordinatesInDetElt(Int_t DeId, Double_t x, Double_t y) const
{
  /// Return kTRUE if the coordinates are in the Detection Element.
  /// This is done by checking if a pad correspond to the (x, y) position.
  const AliMpVSegmentation* seg1 = AliMpSegmentation::Instance()->GetMpSegmentation(DeId, AliMp::kCath0);
  const AliMpVSegmentation* seg2 = AliMpSegmentation::Instance()->GetMpSegmentation(DeId, AliMp::kCath1);
  if (!seg1 || !seg2) return kFALSE;
  AliMpPad pad1 = seg1->PadByPosition(x, y, kFALSE);
  AliMpPad pad2 = seg2->PadByPosition(x, y, kFALSE);
  return (pad1.IsValid() && pad2.IsValid());
}

//________________________________________________________________________
void AliAnalysisTaskMuonTrackingEff::FillTDHistos(Int_t chamber, Int_t detElt, Double_t posXL, Double_t posYL)
{
  /// Fill the histo for detected tracks
  ((TH3F*) fDetEltTDHistList->At(FromDetElt2iDet(chamber, detElt)))->Fill(posXL, posYL, fCurrentCentrality);
  ((TH2F*) fChamberTDHistList->At(chamber))->Fill(FromDetElt2LocalId(chamber, detElt), fCurrentCentrality);
  ((TH2F*) fChamberTDHistList->At(10))->Fill(chamber+1, fCurrentCentrality);
}

//________________________________________________________________________
void AliAnalysisTaskMuonTrackingEff::FillTTHistos(Int_t chamber, Int_t detElt, Double_t posXL, Double_t posYL)
{
  /// Fill the histo for all tracks
  ((TH3F*) fDetEltTTHistList->At(FromDetElt2iDet(chamber, detElt))) -> Fill(posXL, posYL, fCurrentCentrality);
  ((TH2F*) fChamberTTHistList->At(chamber))->Fill(FromDetElt2LocalId(chamber, detElt), fCurrentCentrality);
  ((TH2F*) fChamberTTHistList->At(10))->Fill(chamber+1, fCurrentCentrality);
}

//________________________________________________________________________
void AliAnalysisTaskMuonTrackingEff::FillSDHistos(Int_t chamber, Int_t detElt, Double_t posXL, Double_t posYL)
{
  /// Fill the histo for single detected tracks
  ((TH3F*) fDetEltSDHistList->At(FromDetElt2iDet(chamber, detElt))) -> Fill(posXL, posYL, fCurrentCentrality);
  ((TH2F*) fChamberSDHistList->At(chamber))->Fill(FromDetElt2LocalId(chamber, detElt), fCurrentCentrality);
  ((TH2F*) fChamberSDHistList->At(10))->Fill(chamber+1, fCurrentCentrality);
}

//________________________________________________________________________
Int_t AliAnalysisTaskMuonTrackingEff::FromDetElt2iDet(Int_t chamber, Int_t detElt) const
{
  /// Connexion between the detection element Id and its position in the list of histograms
  Int_t iDet = FromDetElt2LocalId(chamber, detElt);
  for (Int_t iCh = chamber-1; iCh >=0; iCh--) iDet += fgkNbrOfDetectionElt[iCh];
  return iDet;
}

//________________________________________________________________________
Int_t AliAnalysisTaskMuonTrackingEff::FromDetElt2LocalId(Int_t chamber, Int_t detElt) const
{
  /// Connexion between the detection element Id and its number in the chamber
  return detElt - fgkOffset*(chamber+1);    
}

//________________________________________________________________________
Int_t AliAnalysisTaskMuonTrackingEff::FromLocalId2DetElt(Int_t chamber, Int_t iDet) const
{
  /// Connexion between the number of the detection element in the chamber and its Id
  return iDet + fgkOffset*(chamber+1);    
}
