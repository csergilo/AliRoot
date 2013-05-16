//---------------------------------------------------------
// PARAMETERS TO BE SET BY HAND


// const Int_t runs[]={ 139173, 139172, 139110, 139107, 139042, 139038, 138973, 138796, 138795, 138742, 138740, 138737, 138736, 138732, 138731, 138730, 138666, 138662, 138653, 138652, 138638, 138637, 138624, 138621, 138620, 138583, 138582, 138579, 138578, 138534, 138533, 138469, 138442, 138439, 138438, 138396, 138275, 138225, 138201, 138200, 138197, 138192, 138190, 138154, 138153, 138151, 138150, 138126, 138125, 137848, 137847, 137844, 137843, 137752, 137751, 137748, 137724, 137722, 137718, 137704, 137693, 137692, 137691, 137689, 137686, 137685, 137639, 137638, 137609, 137608, 137595, 137549, 137546, 137544, 137541, 137539, 137531, 137530, 137443, 137441, 137440, 137439, 137434, 137432, 137431, 137430, 137370, 137366, 137365, 137243, 137236, 137235, 137232, 137230, 137165, 137163, 137162, 137161, 137137, 137136, 137135, 137133, 137132, 137125, 137124, 137045, 137042, 136879, 136854, 136851, -1 };

// const Int_t runs[]={
//   137135, 137161, 137162, 137163, 137165, 137230, 137232, 137235, 137236, 137243, 137365, 137366, 137430, 137431, 137432, 137434, 137439, 137440, 137441, 137443, 137530, 137531, 137539, 137541, 137544, 137546, 137549, 137595, 137638, 137639, 137685, 137691, 137692, 137693, 137704, 137718, 137722, 137724, 137748, 137751, 137752, 137843, 138125, 138126, 138190, 138192, 138200, 138201, 138275, 138359, 138364, 138396, 138438, 138439, 138442, 138469, 138534, 138578, 138579, 138582, 138583, 138621, 138624, 138638, 138653, 138662, 138666, 138730, 138731, 138740, 138830, 138836, 138837, 138870, 138871, 138872, 138924, 138972, 138976, 138977, 138978, 138979, 138980, 138982, 138983, 139024, 139025, 139028, 139029, 139030, 139031, 139034, 139036, 139037, 139038, 139042, 139105, 139107, 139172, 139173, 139309, 139310, 139314, 139328, 139329, 139360, 139437, 139438, 139440, 139441, 139465, 139503, 139505, 139507, 139510, -1};

const Int_t runs[]={
  154808,
  154796,
  154793,
  154789,
  154787,
  154786,
  154783,
  154780,
  154773,
  154763,
  154570,
  154495,
  154485,
  154483,
  154482,
  154480,
  154478,
  154448,
  154385,
  154383,
  154382,
  154289,
  154286,
  154283,
  154281,
  154273,
  154270,
  154269,
  154266,
  154264,
  154261,
  154257,
  154252,
  154222,
  154221,
  154220,
  154219,
  154211,
  154207,
  154151,
  154143,
  154141,
  154136,
  154132,
  154126,
  154130,
  153738,
  153733,
  153728,
  153727,
  153726,
  153725,
  153718,
  153709,
  153702,
  153594,
  153591,
  153589,
  153571,
  153570,
  153566,
  153560,
  153558,
  153552,
  153548,
  153544,
  153542,
  153541,
  153539,
  153536,
  153533,
  153373,
  153371,
  153369,
  153363,
  153362,
  -1
};



// check number of processed QA trains for this runs. Try all. 

//const Int_t QAcycle[]={41, 40, 36, 34, 33, 32, 31, -1};
const Int_t QAcycle[]={69,-1};


TString location="/alice/data/2011/LHC11c";
//  TString output="ESDs/pass1_4plus/QA";
TString output="ESDs/pass1/QA";
Bool_t localMode = 1; // if true, read local copy
  

// event stat files are saved locally, with the following suffix. If
// localMode above is true, this local copies are processed rather
// than the originals on alien
const char * localSuffix = "pass1";
//const char * localPath   = "./cache_41/";
const char * localPath   = "./files/";
//const char * localPath   = "./QA_LHC10h_43/";

// See twiki and update this list, if needed
TString knownProblems = "";
  
// "137161, 137162, 137163, 137165, 138190, 138200, 138201, 138396, 138439, 138469, 138579, 137443, 137230, 137232, 137443, 137531, 137546, 138150, 138154, 138197, 138438, 139024, 139031";
const char * trigger1 = "CEMC7-B-NOPF-ALLNOTRD";//rowCMBAC 
const char * trigger2 = "CMUSH7-B-NOPF-MUON";//rowCMBACS2
const char * trigger3 = "CINT7-B-NOPF-ALLNOTRD";//rowC0SMH

const char *period="LHC11c pass1, Cycle QA69";