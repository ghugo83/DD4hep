// $Id:$
//====================================================================
//  AIDA Detector description implementation for LCD
//--------------------------------------------------------------------
//
//  Author     : M.Frank
//
//====================================================================

// Framework includes
#include "DD4hep/Volumes.h"
#include "DD4hep/FieldTypes.h"
#include "XML/DocumentHandler.h"
#include "LCDDConverter.h"

// ROOT includes
#include "TROOT.h"
#include "TColor.h"
#include "TGeoShape.h"
#include "TGeoEltu.h"
#include "TGeoCone.h"
#include "TGeoParaboloid.h"
#include "TGeoPara.h"
#include "TGeoPcon.h"
#include "TGeoPgon.h"
#include "TGeoSphere.h"
#include "TGeoTorus.h"
#include "TGeoTube.h"
#include "TGeoTrd1.h"
#include "TGeoTrd2.h"
#include "TGeoArb8.h"
#include "TGeoMatrix.h"
#include "TGeoBoolNode.h"
#include "TGeoCompositeShape.h"
#include "TGeoShapeAssembly.h"
#include "TGeoNode.h"
#include "TClass.h"
#include "TMath.h"
#include "Reflex/PluginService.h"
#include <iostream>
#include <iomanip>

using namespace DD4hep::Geometry;
using namespace DD4hep;
using namespace std;

namespace {
  typedef Position XYZRotation;
  XYZRotation getXYZangles(const Double_t* r)   {
    static Double_t rad = DEGREE_2_RAD;
    Double_t cosb = sqrt(r[0]*r[0] + r[1]*r[1]);
    if (cosb > 0.00001) {
      return XYZRotation(atan2(r[5], r[8])*rad, atan2(-r[2], cosb)*rad, atan2(r[1], r[0])*rad);
    }
    return XYZRotation(atan2(-r[7], r[4])*rad,atan2(-r[2], cosb)*rad,0);
  }
}


void LCDDConverter::GeometryInfo::check(const string& name, const TNamed* n,map<string,const TNamed*>& m) const {
  map<string,const TNamed*>::const_iterator i=m.find(name);
  if ( i != m.end() ) {
    const char* isa = n ? n->IsA()->GetName() : (*i).second ? (*i).second->IsA()->GetName() : "Unknown";
    cout << isa << "(position):  duplicate entry with name:" << name 
	 << " " << (void*)n << " " << (void*)(*i).second << endl;
  }
  m.insert(make_pair(name,n));
}

/// Initializing Constructor
LCDDConverter::LCDDConverter( LCDD& lcdd ) : m_lcdd(lcdd) {
  m_checkOverlaps = true;
}

/// Dump element in GDML format to output stream
xml_h LCDDConverter::handleElement(const string& name, const TGeoElement* element) const {
  GeometryInfo& geo = data();
  xml_h e = geo.xmlElements[element];
  if ( !e ) {
    xml_elt_t atom(geo.doc,_X(atom));
    geo.doc_materials.append(e=xml_elt_t(geo.doc,_X(element)));
    e.append(atom);
    e.setAttr(_A(name),element->GetName());
    e.setAttr(_A(formula),element->GetName());
    e.setAttr(_A(Z),element->Z());
    atom.setAttr(_A(type),"A");
    atom.setAttr(_A(unit),"g/mol");
    atom.setAttr(_A(value),element->A() /* *(g/mole) */);
    geo.xmlElements[element] = e;
  }
  return e;
}

/// Dump material in GDML format to output stream
xml_h LCDDConverter::handleMaterial(const string& name, const TGeoMedium* medium) const {
  GeometryInfo& geo = data();
  xml_h mat = geo.xmlMaterials[medium];
  if ( !mat ) {
    xml_h obj;
    TGeoMaterial* m = medium->GetMaterial();
    double        d = m->GetDensity(); //*(gram/cm3);
    mat = xml_elt_t(geo.doc,_X(material));
    mat.setAttr(_A(name),medium->GetName());
    mat.append(obj=xml_elt_t(geo.doc,_X(D)));
    obj.setAttr(_A(value),m->GetDensity()  /*  *(g/cm3)  */);
    obj.setAttr(_A(unit),"g/cm3");
    obj.setAttr(_A(type),"density");

    geo.checkMaterial(name,medium);

    if ( m->IsMixture() ) {
      TGeoMixture* mix=(TGeoMixture*)m;
      const double *wmix = mix->GetWmixt();
      double sum = 0e0;
      for (int i=0, n=mix->GetNelements(); i < n; i++) {
	TGeoElement *elt = mix->GetElement(i);
	handleElement(elt->GetName(),elt);
	sum += wmix[i];
      }
      for (int i=0, n=mix->GetNelements(); i < n; i++) {
	TGeoElement *elt = mix->GetElement(i);
	string formula = elt->GetTitle()+string("_elm");
	mat.append(obj=xml_elt_t(geo.doc,_X(fraction)));
	obj.setAttr(_A(n),wmix[i]/sum);
	obj.setAttr(_A(ref),elt->GetName());
      }
    }
    else {
      TGeoElement *elt = m->GetElement(0);
      xml_elt_t atom(geo.doc,_X(atom));
      handleElement(elt->GetName(),elt);
      mat.append(atom);
      mat.setAttr(_A(Z),m->GetZ());
      atom.setAttr(_A(type),"A");
      atom.setAttr(_A(unit),"g/mol");
      atom.setAttr(_A(value),m->GetA()  /*  *(g/mole)  */);
    }
    geo.doc_materials.append(mat);
    geo.xmlMaterials[medium] = mat;
  }
  return mat;
}

/// Dump solid in GDML format to output stream
xml_h LCDDConverter::handleSolid(const string& name, const TGeoShape* shape)   const   {
  GeometryInfo& geo = data();
  xml_h solid(geo.xmlSolids[shape]);
  if ( !solid && shape ) {
    xml_h zplane(0);
    geo.checkShape(name,shape);
    if ( shape->IsA() == TGeoBBox::Class() ) {
      const TGeoBBox* s = (const TGeoBBox*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(box)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(x),s->GetDX()*CM_2_MM);
      solid.setAttr(_A(y),s->GetDY()*CM_2_MM);
      solid.setAttr(_A(z),s->GetDZ()*CM_2_MM);
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoTube::Class() ) {
      const TGeoTube* s = (const TGeoTube*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(tube)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(rmin),s->GetRmin()*CM_2_MM);
      solid.setAttr(_A(rmax),s->GetRmax()*CM_2_MM);
      solid.setAttr(_A(z),s->GetDz()*CM_2_MM);
      solid.setAttr(_A(startphi),0e0);
      solid.setAttr(_A(deltaphi),2*M_PI);
      solid.setAttr(_A(aunit),"rad");
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoEltu::Class() ) {
      const TGeoEltu* s = (const TGeoEltu*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(eltube)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(dx),s->GetA()*CM_2_MM);
      solid.setAttr(_A(dy),s->GetB()*CM_2_MM);
      solid.setAttr(_A(dz),s->GetDz()*CM_2_MM);
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoTubeSeg::Class() ) {
      const TGeoTubeSeg* s = (const TGeoTubeSeg*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(tube)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(rmin),s->GetRmin()*CM_2_MM);
      solid.setAttr(_A(rmax),s->GetRmax()*CM_2_MM);
      solid.setAttr(_A(z),s->GetDz()*CM_2_MM);
      solid.setAttr(_A(startphi),s->GetPhi1()*DEGREE_2_RAD);
      solid.setAttr(_A(deltaphi),s->GetPhi2()*DEGREE_2_RAD);
      solid.setAttr(_A(aunit),"rad");
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoTrd1::Class() ) {
      const TGeoTrd1* s = (const TGeoTrd1*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(trd)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(x1),s->GetDx1()*CM_2_MM);
      solid.setAttr(_A(x2),s->GetDx2()*CM_2_MM);
      solid.setAttr(_A(y1),s->GetDy()*CM_2_MM);
      solid.setAttr(_A(y2),s->GetDy()*CM_2_MM);
      solid.setAttr(_A(z), s->GetDz()*CM_2_MM);
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoTrd2::Class() ) {
      const TGeoTrd2* s = (const TGeoTrd2*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(trd)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(x1),s->GetDx1()*CM_2_MM);
      solid.setAttr(_A(x2),s->GetDx2()*CM_2_MM);
      solid.setAttr(_A(y1),s->GetDy1()*CM_2_MM);
      solid.setAttr(_A(y2),s->GetDy2()*CM_2_MM);
      solid.setAttr(_A(z), s->GetDz()*CM_2_MM);
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoTrap::Class() ) {
      const TGeoTrap* s = (const TGeoTrap*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(trap)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(x1),s->GetBl1()*CM_2_MM);
      solid.setAttr(_A(x2),s->GetTl1()*CM_2_MM);
      solid.setAttr(_A(x3),s->GetBl2()*CM_2_MM);
      solid.setAttr(_A(x4),s->GetTl2()*CM_2_MM);
      solid.setAttr(_A(y1),s->GetH1()*CM_2_MM);
      solid.setAttr(_A(y2),s->GetH2()*CM_2_MM);
      solid.setAttr(_A(z),s->GetDz()*CM_2_MM);
      solid.setAttr(_A(alpha1),s->GetAlpha1()*DEGREE_2_RAD);
      solid.setAttr(_A(alpha2),s->GetAlpha2()*DEGREE_2_RAD);
      solid.setAttr(_A(theta),s->GetTheta()*DEGREE_2_RAD);
      solid.setAttr(_A(phi),s->GetPhi()*DEGREE_2_RAD);
      solid.setAttr(_A(aunit),"rad");
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoPara::Class() ) {
      const TGeoPara* s = (const TGeoPara*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(para)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(x),s->GetX()*CM_2_MM);
      solid.setAttr(_A(y),s->GetY()*CM_2_MM);
      solid.setAttr(_A(z),s->GetZ()*CM_2_MM);
      solid.setAttr(_A(alpha),s->GetAlpha()*DEGREE_2_RAD);
      solid.setAttr(_A(theta),s->GetTheta()*DEGREE_2_RAD);
      solid.setAttr(_A(phi),s->GetPhi()*DEGREE_2_RAD);
      solid.setAttr(_A(aunit),"rad");
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoPgon::Class() ) {
      const TGeoPgon* s = (const TGeoPgon*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(polyhedra)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(startphi),s->GetPhi1()*DEGREE_2_RAD);
      solid.setAttr(_A(deltaphi),s->GetDphi()*DEGREE_2_RAD);
      solid.setAttr(_A(aunit),"rad");
      solid.setAttr(_A(lunit),"mm");
      for( size_t i=0; i<s->GetNz(); ++i )  {
	zplane = xml_elt_t(geo.doc,_X(zplane));
	zplane.setAttr(_A(rmin),s->GetRmin(i)*CM_2_MM);
	zplane.setAttr(_A(rmax),s->GetRmax(i)*CM_2_MM);
	zplane.setAttr(_A(z),s->GetZ(i)*CM_2_MM);
	solid.append(zplane);
      }
    }
    else if ( shape->IsA() == TGeoPcon::Class() ) {
      const TGeoPcon* s = (const TGeoPcon*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(polycone)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(startphi),s->GetPhi1()*DEGREE_2_RAD);
      solid.setAttr(_A(deltaphi),s->GetDphi()*DEGREE_2_RAD);
      solid.setAttr(_A(aunit),"rad");
      solid.setAttr(_A(lunit),"mm");
      for( size_t i=0; i<s->GetNz(); ++i )  {
	zplane = xml_elt_t(geo.doc,_X(zplane));
	zplane.setAttr(_A(rmin),s->GetRmin(i)*CM_2_MM);
	zplane.setAttr(_A(rmax),s->GetRmax(i)*CM_2_MM);
	zplane.setAttr(_A(z),s->GetZ(i)*CM_2_MM);
	solid.append(zplane);
      }
    }
    else if ( shape->IsA() == TGeoCone::Class() ) {
      const TGeoCone* s = (const TGeoCone*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(cone)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(z),s->GetDz()*CM_2_MM);
      solid.setAttr(_A(rmin1),s->GetRmin1()*CM_2_MM);
      solid.setAttr(_A(rmax1),s->GetRmax1()*CM_2_MM);
      solid.setAttr(_A(rmin2),s->GetRmin2()*CM_2_MM);
      solid.setAttr(_A(rmax2),s->GetRmax2()*CM_2_MM);
      solid.setAttr(_A(z),s->GetDz()*CM_2_MM);
      solid.setAttr(_A(startphi),0e0);
      solid.setAttr(_A(deltaphi),2*M_PI);
      solid.setAttr(_A(aunit),"rad");
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoConeSeg::Class() ) {
      const TGeoConeSeg* s = (const TGeoConeSeg*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(cone)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(startphi),s->GetPhi1()*DEGREE_2_RAD);
      solid.setAttr(_A(deltaphi),(s->GetPhi2()-s->GetPhi1())*DEGREE_2_RAD);
      solid.setAttr(_A(aunit),"rad");
      solid.setAttr(_A(lunit),"mm");
      zplane = xml_elt_t(geo.doc,_X(zplane));
      zplane.setAttr(_A(rmin),s->GetRmin1()*CM_2_MM);
      zplane.setAttr(_A(rmax),s->GetRmax1()*CM_2_MM);
      zplane.setAttr(_A(z),s->GetDz()*CM_2_MM);
      solid.append(zplane);
      zplane = xml_elt_t(geo.doc,_X(zplane));
      zplane.setAttr(_A(rmin),s->GetRmin2()*CM_2_MM);
      zplane.setAttr(_A(rmax),s->GetRmax2()*CM_2_MM);
      zplane.setAttr(_A(z),s->GetDz()*CM_2_MM);
      solid.append(zplane);
    }
    else if ( shape->IsA() == TGeoParaboloid::Class() ) {
      const TGeoParaboloid* s = (const TGeoParaboloid*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(paraboloid)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(rlo),s->GetRlo()*CM_2_MM);
      solid.setAttr(_A(rhi),s->GetRhi()*CM_2_MM);
      solid.setAttr(_A(dz),s->GetDz()*CM_2_MM);
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoSphere::Class() ) {
      const TGeoSphere* s = (const TGeoSphere*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(sphere)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(rmin),s->GetRmin()*CM_2_MM);
      solid.setAttr(_A(rmax),s->GetRmax()*CM_2_MM);
      solid.setAttr(_A(startphi),s->GetPhi1()*DEGREE_2_RAD);
      solid.setAttr(_A(deltaphi),(s->GetPhi2()-s->GetPhi1())*DEGREE_2_RAD);
      solid.setAttr(_A(starttheta),s->GetTheta1()*DEGREE_2_RAD);
      solid.setAttr(_A(deltatheta),(s->GetTheta2()-s->GetTheta1())*DEGREE_2_RAD);
      solid.setAttr(_A(aunit),"rad");
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoTorus::Class() ) {
      const TGeoTorus* s = (const TGeoTorus*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(torus)));
      solid.setAttr(_A(name),Unicode(name));
      solid.setAttr(_A(rtor),s->GetR());
      solid.setAttr(_A(rmin),s->GetRmin());
      solid.setAttr(_A(rmax),s->GetRmax());
      solid.setAttr(_A(startphi),s->GetPhi1()*DEGREE_2_RAD);
      solid.setAttr(_A(deltaphi),s->GetDphi()*DEGREE_2_RAD);
      solid.setAttr(_A(aunit),"rad");
      solid.setAttr(_A(lunit),"mm");
    }
    else if ( shape->IsA() == TGeoShapeAssembly::Class() )  {
      TGeoShapeAssembly* s = (TGeoShapeAssembly*)shape;
      geo.doc_solids.append(solid = xml_elt_t(geo.doc,_X(assembly)));
      solid.setAttr(_A(name),Unicode(name));
    }
    else if ( shape->IsA() == TGeoCompositeShape::Class() ) {
      const TGeoCompositeShape* s = (const TGeoCompositeShape*)shape;
      const TGeoBoolNode* boolean = s->GetBoolNode();
      TGeoBoolNode::EGeoBoolType oper = boolean->GetBooleanOperator();
      TGeoMatrix* m       = boolean->GetRightMatrix();
      xml_h    left    = handleSolid(name+"_left", boolean->GetLeftShape());
      xml_h    right   = handleSolid(name+"_right",boolean->GetRightShape());
      xml_h first(0), second(0);
      if ( !left )   {
	throw runtime_error("G4Converter: No left Geant4 Solid present for composite shape:"+name);
      }
      if ( !right )   {
	throw runtime_error("G4Converter: No right Geant4 Solid present for composite shape:"+name);
      }

      if (      oper == TGeoBoolNode::kGeoSubtraction )
	solid = xml_elt_t(geo.doc,_X(subtraction));
      else if ( oper == TGeoBoolNode::kGeoUnion )
	solid = xml_elt_t(geo.doc,_X(union));
      else if ( oper == TGeoBoolNode::kGeoIntersection )
	solid = xml_elt_t(geo.doc,_X(intersection));

      xml_h obj;
      solid.append(first=xml_elt_t(geo.doc,_X(first)));
      solid.setAttr(_A(name),Unicode(name));
      first.setAttr(_A(ref),name+"_left");
      XYZRotation    rot = getXYZangles(boolean->GetLeftMatrix()->Inverse().GetRotationMatrix());
      const double  *tr  = boolean->GetLeftMatrix()->GetTranslation();

      if ((tr[0] != 0.0) || (tr[1] != 0.0) || (tr[2] != 0.0)) {
	first.append(obj=xml_elt_t(geo.doc,_X(firstposition)));
	obj.setAttr(_A(x),tr[0]*CM_2_MM);
	obj.setAttr(_A(y),tr[1]*CM_2_MM);
	obj.setAttr(_A(z),tr[2]*CM_2_MM);
      }
      if ((rot.X() != 0.0) || (rot.Y() != 0.0) || (rot.Z() != 0.0)) {
	first.append(obj=xml_elt_t(geo.doc,_X(firstrotation)));
	obj.setAttr(_A(x),rot.X());
	obj.setAttr(_A(y),rot.Y());
	obj.setAttr(_A(z),rot.Z());
      }

      rot = getXYZangles(boolean->GetRightMatrix()->Inverse().GetRotationMatrix());
      tr  = boolean->GetRightMatrix()->GetTranslation();
      solid.append(second=xml_elt_t(geo.doc,_X(second)));
      second.setAttr(_A(ref),name+"_right");
      if ((tr[0] != 0.0) || (tr[1] != 0.0) || (tr[2] != 0.0)) {
	first.append(obj=xml_elt_t(geo.doc,_X(position)));
	obj.setAttr(_A(x),tr[0]*CM_2_MM);
	obj.setAttr(_A(y),tr[1]*CM_2_MM);
	obj.setAttr(_A(z),tr[2]*CM_2_MM);
      }
      if ((rot.X() != 0.0) || (rot.Y() != 0.0) || (rot.Z() != 0.0)) {
	first.append(obj=xml_elt_t(geo.doc,_X(rotation)));
	obj.setAttr(_A(x),rot.X());
	obj.setAttr(_A(y),rot.Y());
	obj.setAttr(_A(z),rot.Z());
      }
    }
    if ( !solid ) {
      string err = "Failed to handle unknown solid shape:" + 
	name + " of type " + string(shape->IsA()->GetName());
      throw runtime_error(err);
    }
    data().xmlSolids[shape] = solid;
  }
  return solid;
}

/// Convert the Position into the corresponding Xml object(s).
xml_h LCDDConverter::handlePosition(const std::string& name, const TGeoMatrix* trafo)   const {
  GeometryInfo& geo = data();
  xml_h pos = geo.xmlPositions[trafo];
  if ( !pos ) {
    static xml_h identity;
    const double* tr = trafo->GetTranslation();
    if ( tr[0] != 0 || tr[1] != 0 || tr[2] != 0 )   {
      geo.checkPosition(name,trafo);
      geo.doc_define.append(pos=xml_elt_t(geo.doc,_X(position)));
      pos.setAttr(_A(name),name);
      pos.setAttr(_A(x),tr[0]);
      pos.setAttr(_A(y),tr[1]);
      pos.setAttr(_A(z),tr[2]);
    }
    else if ( identity )  {
      pos = identity;
    }
    else {
      geo.doc_define.append(identity=xml_elt_t(geo.doc,_X(position)));
      identity.setAttr(_A(name),"identity_pos");
      identity.setAttr(_A(x),0);
      identity.setAttr(_A(y),0);
      identity.setAttr(_A(z),0);
      pos = identity;
      geo.checkPosition("identity_pos",0);
    }
    geo.xmlPositions[trafo] = pos;
  }
  return pos;
}

/// Convert the Rotation into the corresponding Xml object(s).
xml_h LCDDConverter::handleRotation(const std::string& name, const TGeoMatrix* trafo)   const {
  GeometryInfo& geo = data();
  xml_h rot = geo.xmlRotations[trafo];
  if ( !rot ) {
    static xml_h identity;
    XYZRotation r = getXYZangles(trafo->Inverse().GetRotationMatrix());
    if ( r.X() != 0 || r.Y() != 0 || r.Z() != 0 )   {
      geo.checkRotation(name,trafo);
      geo.doc_define.append(rot=xml_elt_t(geo.doc,_X(rotation)));
      rot.setAttr(_A(name),name);
      rot.setAttr(_A(x),r.X());
      rot.setAttr(_A(y),r.Y());
      rot.setAttr(_A(z),r.Z());
    }
    else if ( identity )  {
      rot = identity;
    }
    else {
      geo.doc_define.append(identity=xml_elt_t(geo.doc,_X(rotation)));
      identity.setAttr(_A(name),"identity_rot");
      identity.setAttr(_A(x),0);
      identity.setAttr(_A(y),0);
      identity.setAttr(_A(z),0);
      rot = identity;
      geo.checkRotation("identity_rot",0);
    }
    geo.xmlRotations[trafo] = rot;
  }
  return rot;
}

/// Dump logical volume in GDML format to output stream
xml_h LCDDConverter::handleVolume(const string& name, const TGeoVolume* volume)   const   {
  GeometryInfo& geo = data();
  xml_h vol = geo.xmlVolumes[volume];
  if ( !vol ) {
    const TGeoVolume*  v  = volume;
    Volume            _v  = Ref_t(v);
    string            n   = v->GetName();
    TGeoMedium*       m   = v->GetMedium();
    TGeoShape*        s   = v->GetShape();
    xml_ref_t        sol = handleSolid(s->GetName(),s);

    if ( !sol )
      throw runtime_error("G4Converter: No Geant4 Solid present for volume:"+n);
    else if ( !m && v->IsA() != TGeoVolumeAssembly::Class() )
      throw runtime_error("G4Converter: No Geant4 material present for volume:"+n);

    geo.checkVolume(name,volume);
    geo.doc_structure.append(vol=xml_elt_t(geo.doc,_X(volume)));
    vol.setAttr(_A(name),n);
    vol.setRef(_X(solidref),sol.name());
    if ( m )   {
      xml_ref_t med = handleMaterial(m->GetName(),m);
      vol.setRef(_X(materialref),med.name());
    }
    if ( dynamic_cast<const Volume::Object*>(volume) ) {
      Region            reg = _v.region();
      LimitSet          lim = _v.limitSet();
      VisAttr           vis = _v.visAttributes();
      SensitiveDetector det = _v.sensitiveDetector();
      if ( vis.isValid() )   {
	xml_ref_t data = handleVis(vis.name(),vis.ptr());
	vol.setRef(_X(visref),data.name());
      }
      if ( lim.isValid() )   {
	xml_ref_t data = handleLimitSet(lim.name(),lim.ptr());
	vol.setRef(_X(limitsetref),data.name());
      }
      if ( reg.isValid() )   {
	xml_ref_t data = handleRegion(reg.name(),reg.ptr());
	vol.setRef(_X(regionref),data.name());
      }
      if ( det.isValid() )   {
	xml_ref_t data = handleSensitive(det.name(),det.ptr());
	vol.setRef(_X(sdref),data.name());
      }
    }
    geo.xmlVolumes[v] = vol;
  }
  return vol;
}

/// Dump logical volume in GDML format to output stream
void LCDDConverter::collectVolume(const string& name, const TGeoVolume* volume)   const   {
  Volume v = Ref_t(volume);
  if ( dynamic_cast<const Volume::Object*>(volume) ) {
    GeometryInfo&     geo = data();
    Region            reg = v.region();
    LimitSet          lim = v.limitSet();
    SensitiveDetector det = v.sensitiveDetector();
    if ( lim.isValid() ) geo.limits.insert(lim.ptr());
    if ( reg.isValid() ) geo.regions.insert(reg.ptr());
    if ( det.isValid() ) geo.sensitives.insert(det.ptr());
  }
  else {
    cout << "LCDDConverter::collectVolume: Skip volume:" << volume->GetName() << endl;
  }
}

void LCDDConverter::checkVolumes(const string& name, const TGeoVolume* volume) const {
  NameSet::const_iterator i=m_checkNames.find(name);
  if ( i != m_checkNames.end() ) {
    Volume v = Ref_t(volume);
    cout << "checkVolumes: Volume " << name << " ";
    if ( dynamic_cast<const Volume::Object*>(volume) ) {
      SensitiveDetector s = v.sensitiveDetector();
      VisAttr vis = v.visAttributes();
      if ( s.isValid() ) {
	cout << "of " << s.name() << " ";
      }
      else if ( vis.isValid() ) {
	cout << "with VisAttrs " << vis.name() << " ";
      }
    }
    cout << "has duplicate entries." << endl;
    return;
  }
  m_checkNames.insert(name);
}

/// Dump volume placement in GDML format to output stream
xml_h LCDDConverter::handlePlacement(const string& name, const TGeoNode* node) const {
  GeometryInfo& geo = data();
  xml_h place    = geo.xmlPlacements[node];
  if ( !place )   {
    TGeoMatrix*  t = node->GetMatrix();
    TGeoVolume*  v = node->GetVolume();
    xml_ref_t vol = xml_h(geo.xmlVolumes[v]);
    xml_h   mot = geo.xmlVolumes[node->GetMotherVolume()];

    place = xml_elt_t(geo.doc,_X(physvol));
    if ( mot ) { // Beware of top level volume!
      mot.append(place);
    }
    place.setRef(_X(volumeref),vol.name());
    if ( t )  {
      char text[32];
      ::sprintf(text,"_%p_pos",node);
      xml_ref_t pos = handlePosition(name+text,t);
      ::sprintf(text,"_%p_rot",node);
      xml_ref_t rot = handleRotation(name+text,t);
      place.setRef(_X(positionref),pos.name());
      place.setRef(_X(rotationref),rot.name());
    }
    if ( dynamic_cast<const PlacedVolume::Object*>(node) ) {
      PlacedVolume p = Ref_t(node);
      const PlacedVolume::VolIDs& ids = p.volIDs();
      for(PlacedVolume::VolIDs::const_iterator i=ids.begin(); i!=ids.end(); ++i) {
	xml_h pvid = xml_elt_t(geo.doc,"physvolid");
	pvid.setAttr(_A(field_name),(*i).first);
	pvid.setAttr(_A(value),(*i).second);
	place.append(pvid);
      }
    }
    geo.xmlPlacements[node] = place;
  }
  else {
    cout << "Attempt to DOUBLE-place physical volume:" << name << " No:" << node->GetNumber() << endl;    
  }
  return place;
}

/// Convert the geometry type region into the corresponding Geant4 object(s).
xml_h LCDDConverter::handleRegion(const std::string& name, const TNamed* region)   const  {
  GeometryInfo& geo = data();  
  xml_h reg = geo.xmlRegions[region];
  if ( !reg )   {
    Region r = Ref_t(region);
    geo.doc_regions.append(reg=xml_elt_t(geo.doc,_X(region)));
    reg.setAttr(_A(name),  r.name());
    reg.setAttr(_A(cut),   r.cut());
    reg.setAttr(_A(eunit), r.energyUnit());
    reg.setAttr(_A(lunit), r.lengthUnit());
    reg.setAttr(_A(store_secondaries),r.storeSecondaries());
    geo.xmlRegions[region] = reg;
  }
  return reg;
}

/// Convert the geometry type LimitSet into the corresponding Geant4 object(s)
xml_h LCDDConverter::handleLimitSet(const std::string& name, const TNamed* limitset)    const  {
  GeometryInfo& geo = data();  
  xml_h xml = geo.xmlLimits[limitset];
  if ( !xml )   {
    LimitSet lim = Ref_t(limitset);
    geo.doc_limits.append(xml=xml_elt_t(geo.doc,_X(limitset)));
    xml.setAttr(_A(name),lim.name());
    const LimitSet::Object& obj = lim.limits();
    for(LimitSet::Object::const_iterator i=obj.begin(); i!=obj.end(); ++i) {
      xml_h x = xml_elt_t(geo.doc,_X(limit));
      const Limit& l = *i;
      xml.append(x);
      x.setAttr(_A(name),l.name);
      x.setAttr(_A(unit),l.unit);
      x.setAttr(_A(value),l.value);
      x.setAttr(_A(particles),l.particles);
    }
    geo.xmlLimits[limitset] = xml;
  }
  return xml;
}

/// Convert the geometry type SensitiveDetector into the corresponding Geant4 object(s).
xml_h LCDDConverter::handleSensitive(const string& name, const TNamed* sens_det)   const  {
  GeometryInfo& geo = data();
  xml_h sensdet = geo.xmlSensDets[sens_det];
  if ( !sensdet )   {
    SensitiveDetector sd = Ref_t(sens_det);
    string type = sd.type(), name = sd.name();
    geo.doc_detectors.append(sensdet = xml_elt_t(geo.doc,Unicode(type)));
    sensdet.setAttr(_A(name),sd.name());
    sensdet.setAttr(_A(ecut),sd.energyCutoff());
    sensdet.setAttr(_A(eunit),"MeV");
    sensdet.setAttr(_A(verbose),sd.verbose());
    sensdet.setAttr(_A(hits_collection),sd.hitsCollection());
    if ( sd.combineHits() ) sensdet.setAttr(_A(combine_hits),sd.combineHits());
    Readout ro = sd.readout();
    if ( ro.isValid() ) {
      xml_ref_t ref = handleIdSpec(ro.idSpec().name(),ro.idSpec().ptr());
      sensdet.setRef(_A(idspecref),ref.name());
    }
    geo.xmlSensDets[sens_det] = sensdet;
  }
  return sensdet;
}

/// Convert the geometry id dictionary entry to the corresponding Xml object(s).
xml_h LCDDConverter::handleIdSpec(const std::string& name, const TNamed* id_spec) const {
  GeometryInfo& geo = data();
  xml_h id = geo.xmlIdSpecs[id_spec];
  if ( !id )   {
    IDDescriptor desc = Ref_t(id_spec);
    geo.doc_idDict.append(id=xml_elt_t(geo.doc,_X(idspec)));
    id.setAttr(_A(name),name);
    const IDDescriptor::FieldMap& m = desc.fields();
    for(IDDescriptor::FieldMap::const_iterator i=m.begin(); i!=m.end(); ++i) {
      xml_h idfield = xml_elt_t(geo.doc,"idfield");
      const IDDescriptor::Field& f = (*i).second;
      idfield.setAttr(_A(signed),f.second<0 ? true : false);
      idfield.setAttr(_A(label),(*i).first);
      idfield.setAttr(_A(length),f.second<0 ? -f.second : f.second);
      idfield.setAttr(_A(start),f.first);
      id.append(idfield);
    }
    geo.xmlIdSpecs[id_spec] = id;
  }
  return id;
}

/// Convert the geometry visualisation attributes to the corresponding Geant4 object(s).
xml_h LCDDConverter::handleVis(const string& name, const TNamed* v) const  {
  GeometryInfo& geo = data();
  xml_h vis = geo.xmlVis[v];
  if ( !vis )   {
    float   r=0, g=0, b=0;
    VisAttr attr  = Ref_t(v);
    int     style = attr.lineStyle();
    int     draw  = attr.drawingStyle();

    geo.doc_display.append(vis=xml_elt_t(geo.doc,_X(vis)));
    vis.setAttr(_A(name),attr.name());
    vis.setAttr(_A(visible),attr.visible());
    vis.setAttr(_A(show_daughters),attr.showDaughters());
    if ( style == VisAttr::SOLID )
      vis.setAttr(_A(line_style),"unbroken");
    else if ( style == VisAttr::DASHED )
      vis.setAttr(_A(line_style),"broken");
    if ( draw == VisAttr::SOLID )
      vis.setAttr(_A(line_style),"solid");
    else if ( draw == VisAttr::WIREFRAME )
      vis.setAttr(_A(line_style),"wireframe");

    xml_h col = xml_elt_t(geo.doc,_X(color));
    col.setAttr(_A(alpha),attr.alpha());
    col.setAttr(_A(R),r);
    col.setAttr(_A(G),g);
    col.setAttr(_A(B),b);
    vis.append(col);
    geo.xmlVis[v] = vis;
  }
  return vis;
}

/// Convert the electric or magnetic fields into the corresponding Xml object(s).
xml_h LCDDConverter::handleField(const std::string& name, const TNamed* f)   const {
  GeometryInfo& geo = data();
  xml_h field = geo.xmlFields[f];
  if ( !field ) {
    Ref_t  fld(f);
    string type = f->GetTitle();
    field=xml_elt_t(geo.doc,_X(field));
    field.setAttr(_A(name),f->GetName());
    field.setAttr(_A(type),type);
    fld = ROOT::Reflex::PluginService::Create<TNamed*>(type+"_Convert2LCDD",&m_lcdd,&field,&fld);
    cout << (fld.isValid() ? "Converted" : "FAILED    to convert ")
	 << " electromagnetic field:" << f->GetName() << " of type " << f->GetTitle() << endl;
    if ( !fld.isValid() ) {
      throw runtime_error("Failed to locate plugin to convert electromagnetic field:"+
			  string(f->GetName())+" of type "+string(f->GetTitle()));
    }
    geo.doc_fields.append(field);
  }
  return field;
}

/// Handle the geant 4 specific properties
void LCDDConverter::handleProperties(LCDD::Properties& prp)   const {
  map<string,string> processors;
  static int s_idd = 9999999;
  string id;
  for(LCDD::Properties::const_iterator i=prp.begin(); i!=prp.end(); ++i) {
    const string& nam = (*i).first;
    const LCDD::PropertyValues& vals = (*i).second;
    if ( nam.substr(0,6) == "geant4" ) {
      LCDD::PropertyValues::const_iterator id_it = vals.find("id");
      if ( id_it != vals.end() )  {
	id= (*id_it).second;
      }
      else {
	char txt[32];
	::sprintf(txt,"%d",++s_idd);
	id = txt;
      }
      processors.insert(make_pair(id,nam));
    }
  }
  for(map<string,string>::const_iterator i=processors.begin(); i!=processors.end(); ++i) {
    const LCDDConverter* ptr = this;
    string nam = (*i).second;
    const LCDD::PropertyValues& vals = prp[nam];
    string type = vals.find("type")->second;
    string tag  = type + "_Geant4_action";
    long result = ROOT::Reflex::PluginService::Create<long>(tag,&m_lcdd,ptr,&vals);
    if ( 0 == result ) {
      throw runtime_error("Failed to locate plugin to interprete files of type"
			  " \""+tag+"\" - no factory:"+type);
    }
    result = *(long*)result;
    if ( result != 1 ) {
      throw runtime_error("Failed to invoke the plugin "+tag+" of type "+type);
    }
    cout << "+++++ Executed Successfully Geant4 setup module *" << type << "* ." << endl;
  }
}

/// Add header information in LCDD format
void LCDDConverter::handleHeader()   const   {
  GeometryInfo& geo = data();
  Header hdr = m_lcdd.header();
  xml_h obj;
  geo.doc_header.append(obj=xml_elt_t(geo.doc,"detector"));
  obj.setAttr(_A(name),hdr.name());
  geo.doc_header.append(obj=xml_elt_t(geo.doc,"generator"));
  obj.setAttr(_A(name),"LCDDConverter");
  obj.setAttr(_A(version),hdr.version());
  obj.setAttr(_A(file),hdr.url());
  obj.setAttr(_A(checksum),m_lcdd.constantAsString("compact_checksum"));
  geo.doc_header.append(obj=xml_elt_t(geo.doc,"author"));
  obj.setAttr(_A(name),hdr.author());
  geo.doc_header.append(obj=xml_elt_t(geo.doc,"comment"));
  obj.setText(hdr.comment());
}

template <typename O, typename C, typename F> void handle(const O* o, const C& c, F pmf)  {
  for(typename C::const_iterator i=c.begin(); i != c.end(); ++i) {
    string n = (*i)->GetName();
    (o->*pmf)(n,*i);
  }
}

template <typename O, typename C, typename F> void handleMap(const O* o, const C& c, F pmf)  {
  for(typename C::const_iterator i=c.begin(); i != c.end(); ++i)
    (o->*pmf)((*i).first, (*i).second);
}

template <typename O, typename C, typename F> void handleRMap(const O* o, const C& c, F pmf)  {
  for(typename C::const_reverse_iterator i=c.rbegin(); i != c.rend(); ++i)
    handle(o, (*i).second, pmf);
}

/// Create geometry conversion
void LCDDConverter::create(DetElement top) {
  LCDD& lcdd = m_lcdd;
  GeometryInfo& geo = *(m_dataPtr=new GeometryInfo);
  m_data->clear();
  collect(top,geo);
  m_checkOverlaps = false;

  #define ns_location "http://www.lcsim.org.schemas/lcdd/1.0"

  const char empty_lcdd[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\">\n"
    "<!--                                                               \n"
    "      +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
    "      ++++   Linear collider detector description LCDD in C++  ++++\n"
    "      ++++   DD4hep Detector description generator.            ++++\n"
    "      ++++                                                     ++++\n"
    "      ++++                              M.Frank CERN/LHCb      ++++\n"
    "      +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
    "-->\n"
    "<lcdd "
    "xmlns:lcdd=\"" ns_location "\"\n"
    "xmlns:xs=\"http://www/w3.org/2001/XMLSchema-instance\"\n"
    ">\n"
    "</lcdd>\0\0";

  XML::DocumentHandler docH;
  xml_elt_t elt(0);
  geo.doc = docH.parse(empty_lcdd,sizeof(empty_lcdd));
  //doc->setXmlStandalone(true);
  //doc->setStrictErrorChecking(true);
  geo.doc_root = geo.doc.root();

  //Box worldSolid(doc,_X(world_box));
  geo.doc_root.append(geo.doc_header    = xml_elt_t(geo.doc,_X(header)));
  geo.doc_root.append(geo.doc_idDict    = xml_elt_t(geo.doc,_X(iddict)));
  geo.doc_root.append(geo.doc_detectors = xml_elt_t(geo.doc,_X(sensitive_detectors)));
  geo.doc_root.append(geo.doc_limits    = xml_elt_t(geo.doc,_X(limits)));
  geo.doc_root.append(geo.doc_regions   = xml_elt_t(geo.doc,_X(regions)));
  geo.doc_root.append(geo.doc_display   = xml_elt_t(geo.doc,_X(display)));
  geo.doc_root.append(geo.doc_gdml      = xml_elt_t(geo.doc,_X(gdml)));
  geo.doc_root.append(geo.doc_fields    = xml_elt_t(geo.doc,_X(fields)));
  //elt = xml_elt_t();

  geo.doc_gdml.append(geo.doc_define    = xml_elt_t(geo.doc,_X(define)));
  geo.doc_gdml.append(geo.doc_materials = xml_elt_t(geo.doc,_X(materials)));
  geo.doc_gdml.append(geo.doc_solids    = xml_elt_t(geo.doc,_X(solids)));
  geo.doc_gdml.append(geo.doc_structure = xml_elt_t(geo.doc,_X(structure)));
  geo.doc_gdml.append(geo.doc_setup     = xml_elt_t(geo.doc,_X(setup)));
  elt = xml_elt_t(geo.doc,_X(world));
  elt.setAttr(_A(ref),lcdd.worldVolume().name());
  geo.doc_setup.append(elt);


  // Ensure that all required materials are present in the Geant4 material table
  const LCDD::HandleMap& mat = lcdd.materials();
  for(LCDD::HandleMap::const_iterator i=mat.begin(); i!=mat.end(); ++i)
    geo.materials.insert(dynamic_cast<TGeoMedium*>((*i).second.ptr()));

  const LCDD::HandleMap& fld = lcdd.fields();
  for(LCDD::HandleMap::const_iterator i=fld.begin(); i!=fld.end(); ++i)
    geo.fields.insert((*i).second.ptr());

  handleHeader();
  // Start creating the objects for materials, solids and log volumes.
  handle(this, geo.materials, &LCDDConverter::handleMaterial);
  cout << "++ Handled " << geo.materials.size() << " materials." << endl;

  handle(this, geo.volumes,   &LCDDConverter::collectVolume);
  cout << "++ Handled " << geo.volumes.size() << " volumes." << endl;

  handle(this, geo.solids,    &LCDDConverter::handleSolid);
  cout << "++ Handled " << geo.solids.size() << " solids." << endl;

  handle(this, geo.vis,       &LCDDConverter::handleVis);
  cout << "++ Handled " << geo.solids.size() << " visualization attributes." << endl;

  handle(this, geo.sensitives, &LCDDConverter::handleSensitive);
  cout << "++ Handled " << geo.sensitives.size() << " sensitive detectors." << endl;

  handle(this, geo.limits, &LCDDConverter::handleLimitSet);
  cout << "++ Handled " << geo.limits.size() << " limit sets." << endl;

  handle(this, geo.regions, &LCDDConverter::handleRegion);
  cout << "++ Handled " << geo.regions.size() << " regions." << endl;

  handle(this, geo.volumes,   &LCDDConverter::handleVolume);
  cout << "++ Handled " << geo.volumes.size() << " volumes." << endl;

  handle(this, geo.fields,   &LCDDConverter::handleField);
  cout << "++ Handled " << geo.fields.size() << " fields." << endl;

  // Now place all this stuff appropriately
  handleRMap(this, *m_data, &LCDDConverter::handlePlacement);

  m_checkNames.clear();
  handle(this, geo.volumes,   &LCDDConverter::checkVolumes);

#if 0
  //==================== Fields
  handleProperties(m_lcdd.properties());
#endif
}

LCDDConverter::GeometryInfo::GeometryInfo()
  : doc(0), doc_root(0), doc_header(0), doc_idDict(0), doc_detectors(0), doc_limits(0), doc_regions(0),
    doc_display(0), doc_gdml(0), doc_fields(0), doc_define(0), doc_materials(0),
    doc_solids(0), doc_structure(0),doc_setup(0)
{
}

static long create_translator(LCDD& lcdd, int argc, char** argv)   {
  LCDDConverter wr(lcdd);
  FILE* file = argc>0 ? ::fopen(argv[0],"w") : stdout;
  wr.create(lcdd.world());
  LCDDConverter::GeometryInfo& geo = wr.data();
  if ( !file ) {
    cout << "Failed to open output file:" << argv[0] << endl;
    return 0;
  }
  geo.doc->Print(file);
  if ( argc>0 ) ::fclose(file);
  return 1;
}

DECLARE_APPLY(DD4hepGeometry2LCDD,create_translator);
