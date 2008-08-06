// Filename: maxToEggConverter.h
// Created by Corey Revilla and Ken Strickland (6/22/03)
// from mayaToEggConverter.cxx created by drose (10Nov99)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) Carnegie Mellon University.  All rights reserved.
//
// All use of this software is subject to the terms of the revised BSD
// license.  You should have received a copy of this license along
// with this source code in a file named "LICENSE."
//
////////////////////////////////////////////////////////////////////

#ifndef __maxToEggConverter__H
#define __maxToEggConverter__H

#pragma conform(forScope, off)

#include "pandatoolbase.h"

/* 3ds Max Includes, with bonus(!) memory protection
 */

#ifdef MAX5
//Disable the "Too many actual parameters in istdplug.h" warning in Max5
#pragma warning(push)
#pragma warning(disable: 4002)
#include "max_pre_include.h"
#endif
#include "Max.h"
#include "iparamb2.h"
#include "iparamm2.h"
#include "istdplug.h"
#include "iskin.h"
#include "maxResource.h"
#include "stdmat.h"
#include "phyexp.h"
#include "surf_api.h"
#ifdef MAX5
#include "max_post_include.h"
#pragma warning(pop)
#endif


/* Panda Includes
 */
#include "eggCoordinateSystem.h"
#include "eggGroup.h"
#include "eggPolygon.h"
#include "eggTextureCollection.h"
#include "eggTexture.h"
#include "eggVertex.h"
#include "eggVertexPool.h"
#include "eggNurbsCurve.h"
#include "pandatoolbase.h"
#include "somethingToEgg.h"
#include "somethingToEggConverter.h"
#include "eggXfmSAnim.h"

/* Local Includes
 */
#include "maxNodeTree.h"

/* Error-Reporting Includes
 */
#include "maxLogger.h"
#define MTEC Logger::ST_MAP_ME_TO_APP_SPECIFIC_SYSTEM4

/* Helpful Defintions and Casts
 */
#define null 0
#define PHYSIQUE_CLASSID Class_ID(PHYSIQUE_CLASS_ID_A, PHYSIQUE_CLASS_ID_B)

/* External Helper Functions for UI
 */
// *** Figure out why this is causing link errors
//DWORD WINAPI ProgressBarFunction(LPVOID arg);

////////////////////////////////////////////////////////////////////
//       Class : MaxToEggConverter
// Description : This class supervises the construction of an EggData
//               structure from a Max model
////////////////////////////////////////////////////////////////////
class MaxToEggConverter : public SomethingToEggConverter {
 public:
  MaxToEggConverter(const string &program_name = "");
  MaxToEggConverter(const MaxToEggConverter &copy);
  virtual ~MaxToEggConverter();

  virtual SomethingToEggConverter *make_copy();

  virtual string get_name() const;
  virtual string get_extension() const;

  virtual bool convert_file(const Filename &filename);
  bool convert_max(bool from_selection);

  //Sets the interface to 3dsMax.
  void setMaxInterface(Interface *pInterface);

  void set_selection_list(ULONG *list, int len) {
    _selection_list = list; _selection_len = len; }
  void set_double_sided(bool flag) {double_sided = flag;}

 private:
  double _current_frame;
  ULONG *_selection_list; int _selection_len;

  bool convert_flip(double start_frame, double end_frame, 
                    double frame_inc, double output_frame_rate);

  bool convert_char_model();
  bool convert_char_chan(double start_frame, double end_frame, 
                         double frame_inc, double output_frame_rate);
  bool convert_hierarchy(EggGroupNode *egg_root);
  bool process_model_node(MaxNodeDesc *node_desc);

  void get_transform(INode *max_node, EggGroup *egg_group);
  LMatrix4d get_object_transform(INode *max_node);
  void get_joint_transform(INode *max_node, EggGroup *egg_group);
  void get_joint_transform(INode *max_node, INode *parent_node, 
                           EggGroup *egg_group);

  // *** Leaving out these functions til there use/support in Max is determined
  /*
  void make_nurbs_surface(const MDagPath &dag_path, 
                          MFnNurbsSurface &surface,
                          EggGroup *group);
  EggNurbsCurve *make_trim_curve(const MFnNurbsCurve &curve,
                                 const string &nurbs_name,
                                 EggGroupNode *egg_group,
                                 int trim_curve_index);
 */
  bool make_nurbs_curve(NURBSCVCurve *curve, const string &name,
                        TimeValue time, EggGroup *egg_group);
  void make_polyset(INode *max_node,
                    Mesh *mesh,
                    EggGroup *egg_group,
                    Shader *default_shader = NULL);
  /*
  void make_locator(const MDagPath &dag_path, const MFnDagNode &dag_node,
                    EggGroup *egg_group);
  */

  //Gets the vertex normal for a given face and vertex. Go figure.
  Point3 get_max_vertex_normal(Mesh *mesh, int faceNo, int vertNo);
  
  void get_vertex_weights(INode *max_node, EggVertexPool *vpool);
  /*
  void set_shader_attributes(EggPrimitive &primitive,
                             const MayaShader &shader);
  */
  void set_material_attributes(EggPrimitive &primitive, INode *max_node);

  void set_material_attributes(EggPrimitive &primitive, Mtl *maxMaterial, Face *face);

  void apply_texture_properties(EggTexture &tex, 
                                StdMat *maxMaterial);
  /*
  bool compare_texture_properties(EggTexture &tex, 
                                  const MayaShaderColorDef &color_def);
  */

  bool reparent_decals(EggGroupNode *egg_parent);

  string _program_name;
  bool _from_selection;

  MaxNodeTree _tree;
  
  int _cur_tref;
  bool double_sided;

 public:
  //MayaShaders _shaders;
  EggTextureCollection _textures;
  Interface *maxInterface;
  
  bool _polygon_output;
  double _polygon_tolerance;

  enum TransformType {
    TT_invalid,
    TT_all,
    TT_model,
    TT_dcs,
    TT_none,
  };
  TransformType _transform_type;

  static TransformType string_transform_type(const string &arg);

  Modifier* FindSkinModifier (INode* node, const Class_ID &type);
};


#endif
