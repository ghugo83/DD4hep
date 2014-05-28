// $Id: Geant4Converter.cpp 603 2013-06-13 21:15:14Z markus.frank $
//====================================================================
//  AIDA Detector description implementation for LCD
//--------------------------------------------------------------------
//
//  Author     : M.Frank
//
//====================================================================

// Framework include files
#include "DD4hep/Printout.h"
#include "DD4hep/InstanceCount.h"
#include "DDG4/Geant4RunAction.h"
#include "DDG4/Geant4OutputAction.h"

// Geant 4 includes
#include "G4HCofThisEvent.hh"
#include "G4Event.hh"

using namespace DD4hep::Simulation;
using namespace DD4hep;
using namespace std;

/// Standard constructor
Geant4OutputAction::Geant4OutputAction(Geant4Context* ctxt, const string& nam)
    : Geant4EventAction(ctxt, nam) {
  InstanceCount::increment(this);
  declareProperty("Output", m_output);
  context()->runAction().callAtBegin(this, &Geant4OutputAction::beginRun);
  context()->runAction().callAtEnd(this, &Geant4OutputAction::endRun);
}

/// Default destructor
Geant4OutputAction::~Geant4OutputAction() {
  InstanceCount::decrement(this);
}

/// begin-of-event callback
void Geant4OutputAction::begin(const G4Event* /* event */) {
}

/// End-of-event callback
void Geant4OutputAction::end(const G4Event* evt) {
  OutputContext < G4Event > ctxt(evt);
  G4HCofThisEvent* hce = evt->GetHCofThisEvent();
  if ( hce )  {
    int nCol = hce->GetNumberOfCollections();
    saveEvent(ctxt);
    for (int i = 0; i < nCol; ++i) {
      G4VHitsCollection* hc = hce->GetHC(i);
      saveCollection(ctxt, hc);
    }
    commit(ctxt);
    return;
  }
  printout(WARNING,"Geant4OutputAction",
	   "+++ The value of G4HCofThisEvent is NULL. No collections saved!");
}

/// Commit data at end of filling procedure
void Geant4OutputAction::commit(OutputContext<G4Event>& /* ctxt */) {
}

/// Callback to initialize the storing of the Geant4 information
void Geant4OutputAction::beginRun(const G4Run* /* run */) {
}

/// Callback to store the Geant4 run information
void Geant4OutputAction::endRun(const G4Run* /* run */) {
}

/// Callback to store the Geant4 run information
void Geant4OutputAction::saveRun(const G4Run* /* run */) {
}

/// Callback to store the Geant4 event
void Geant4OutputAction::saveEvent(OutputContext<G4Event>& /* ctxt */) {
}

/// Callback to store each Geant4 hit collection
void Geant4OutputAction::saveCollection(OutputContext<G4Event>& /* ctxt */, G4VHitsCollection* /* collection */) {
}

