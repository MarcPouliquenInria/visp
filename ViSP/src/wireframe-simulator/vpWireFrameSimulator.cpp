/****************************************************************************
 *
 * $Id$
 *
 * Copyright (C) 1998-2010 Inria. All rights reserved.
 *
 * This software was developed at:
 * IRISA/INRIA Rennes
 * Projet Lagadic
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * http://www.irisa.fr/lagadic
 *
 * This file is part of the ViSP toolkit.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE included in the packaging of this file.
 *
 * Licensees holding valid ViSP Professional Edition licenses may
 * use this file in accordance with the ViSP Commercial License
 * Agreement provided with the Software.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Contact visp@irisa.fr if any conditions of this licensing are
 * not clear to you.
 *
 * Description:
 * Wire frame simulator
 *
 * Authors:
 * Nicolas Melchior
 *
 *****************************************************************************/

/*!
  \file vpWireFrameSimulator.cpp
  \brief Implementation of a wire frame simulator.
*/

#include <visp/vpWireFrameSimulator.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <vector>

#include <visp/vpSimulatorException.h>
#include <visp/vpPoint.h>
#include <visp/vpCameraParameters.h>
#include <visp/vpMeterPixelConversion.h>
#include <visp/vpPoint.h>

#if defined(WIN32)
#define bcopy(b1,b2,len) (memmove((b2), (b1), (len)), (void) 0) 
#endif


//Inventor includes
#if defined(VISP_HAVE_COIN)
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/VRMLnodes/SoVRMLIndexedFaceSet.h>
#include <Inventor/VRMLnodes/SoVRMLIndexedLineSet.h>
#include <Inventor/VRMLnodes/SoVRMLCoordinate.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/misc/SoChildList.h>
#include <Inventor/actions/SoGetMatrixAction.h>
#include <Inventor/actions/SoGetPrimitiveCountAction.h>
#include <Inventor/actions/SoToVRML2Action.h>
#include <Inventor/VRMLnodes/SoVRMLGroup.h>
#include <Inventor/VRMLnodes/SoVRMLShape.h>
#endif

extern "C"{extern Point2i *point2i;}
extern "C"{extern Point2i *listpoint2i;}

typedef enum
{
  BND_MODEL,
  WRL_MODEL,
  UNKNOWN_MODEL
} Model_3D;

/*
  Get the extension of the file and return it
*/
Model_3D
getExtension(const char* file)
{
  std::string sfilename(file);

  int bnd = sfilename.find("bnd");
  int BND = sfilename.find("BND");
  int wrl = sfilename.find("wrl");
  int WRL = sfilename.find("WRL");
  
  int size = sfilename.size();

  if ((bnd>0 && bnd<size ) || (BND>0 && BND<size))
    return BND_MODEL;
  else if ((wrl>0 && wrl<size) || ( WRL>0 && WRL<size))
  {
#if defined(VISP_HAVE_COIN)
    return WRL_MODEL;
#else
    std::cout << "Coin not installed, cannot read VRML files" << std::endl;
    throw std::string("Coin not installed, cannot read VRML files");
#endif
  }
  else
  { 
    return UNKNOWN_MODEL;
  } 
}

/*
   Enable to initialize the scene
*/
void set_scene (const char* str, Bound_scene *sc, float factor)
{
  FILE  *fd;

  //if ((fd = fopen (str, 0)) == -1)
  if ((fd = fopen (str, "r")) == NULL)
  {
    char strerr[80];
    strcpy (strerr,"The file ");
    strcat (strerr,str);
    strcat (strerr," can not be opened");
    throw(vpException(vpSimulatorException::ioError,strerr)) ;
  }
  open_keyword (keyword_tbl);
  open_lex ();
  open_source (fd, str);
  malloc_Bound_scene (sc, str,(Index)BOUND_NBR);
  parser (sc);
  
  if (factor != 1)
  {
    for (int i = 0; i < sc->bound.nbr; i++)
    {
      for (int j = 0; j < sc->bound.ptr[i].point.nbr; j++)
      {
        sc->bound.ptr[i].point.ptr[j].x = sc->bound.ptr[i].point.ptr[j].x*factor;
        sc->bound.ptr[i].point.ptr[j].y = sc->bound.ptr[i].point.ptr[j].y*factor;
        sc->bound.ptr[i].point.ptr[j].z = sc->bound.ptr[i].point.ptr[j].z*factor;
      }
    }
  }
  
  close_source ();
  close_lex ();
  close_keyword ();
}

#if defined(VISP_HAVE_COIN)

typedef struct
{
  int nbPt;
  std::vector<vpPoint> pt;
  int nbIndex;
  std::vector<int> index;
} indexFaceSet;

void extractFaces(SoVRMLIndexedFaceSet*, indexFaceSet *ifs);
void ifsToBound (Bound*, vpList<indexFaceSet*> &);
void destroyIfs(vpList<indexFaceSet*> &);
  

void set_scene_wrl (const char* str, Bound_scene *sc, float factor)
{
  //Load the sceneGraph
  SoDB::init();
  SoInput in;
  SbBool ok = in.openFile(str);
  SoSeparator  *sceneGraph;
  SoVRMLGroup  *sceneGraphVRML2;
  
  if (!ok) {
    vpERROR_TRACE("can't open file \"%s\" \n Please check the Marker_Less.ini file", str);
    exit(1);
  }
  
  if(!in.isFileVRML2())
  {
    sceneGraph = SoDB::readAll(&in);
    if (sceneGraph == NULL) { /*return -1;*/ }
    sceneGraph->ref();

    SoToVRML2Action tovrml2;
    tovrml2.apply(sceneGraph);
    sceneGraphVRML2 =tovrml2.getVRML2SceneGraph();
    sceneGraphVRML2->ref();
    sceneGraph->unref();
  }
  else
  {
    sceneGraphVRML2	= SoDB::readAllVRML(&in);
    if (sceneGraphVRML2 == NULL) { /*return -1;*/ }
    sceneGraphVRML2->ref();
  }
  
  in.closeFile();

  int nbShapes = sceneGraphVRML2->getNumChildren();

  SoNode * child;
  
  malloc_Bound_scene (sc, str,(Index)BOUND_NBR);
  
  int iterShapes = 0;
  for (int i = 0; i < nbShapes; i++)
  {
    int nbFaces = 0;
    child = sceneGraphVRML2->getChild(i);
    if (child->getTypeId() == SoVRMLShape::getClassTypeId())
    {
      vpList<indexFaceSet*> ifs_list;
      SoChildList * child2list = child->getChildren();
      for (int j = 0; j < child2list->getLength(); j++)
      {
        if (((SoNode*)child2list->get(j))->getTypeId() == SoVRMLIndexedFaceSet::getClassTypeId())
        {
	  indexFaceSet *ifs = new indexFaceSet;
          SoVRMLIndexedFaceSet * face_set;
          face_set = (SoVRMLIndexedFaceSet*)child2list->get(j);
          extractFaces(face_set,ifs);
	  ifs_list.addRight(ifs);
	  nbFaces++;
        }
//         if (((SoNode*)child2list->get(j))->getTypeId() == SoVRMLIndexedLineSet::getClassTypeId())
//         {
//           std::cout << "> We found a line" << std::endl;
//           SoVRMLIndexedLineSet * line_set;
//           line_set = (SoVRMLIndexedLineSet*)child2list->get(j);
//           extractLines(line_set);
//         }
      }
      sc->bound.nbr++;
      ifsToBound (&(sc->bound.ptr[iterShapes]), ifs_list);
      destroyIfs(ifs_list);
      iterShapes++;
    }
  }
  
  if (factor != 1)
  {
    for (int i = 0; i < sc->bound.nbr; i++)
    {
      for (int j = 0; j < sc->bound.ptr[i].point.nbr; j++)
      {
        sc->bound.ptr[i].point.ptr[j].x = sc->bound.ptr[i].point.ptr[j].x*factor;
        sc->bound.ptr[i].point.ptr[j].y = sc->bound.ptr[i].point.ptr[j].y*factor;
        sc->bound.ptr[i].point.ptr[j].z = sc->bound.ptr[i].point.ptr[j].z*factor;
      }
    }
  }
}


void
extractFaces(SoVRMLIndexedFaceSet* face_set, indexFaceSet *ifs)
{
//   vpList<vpPoint> pointList;
//   pointList.kill();
  SoVRMLCoordinate *coord = (SoVRMLCoordinate *)(face_set->coord.getValue());
  int coordSize = coord->point.getNum();
  
  ifs->nbPt = coordSize;
  for (int i = 0; i < coordSize; i++)
  {
    SbVec3f point(0,0,0);
    point[0]=coord->point[i].getValue()[0];
    point[1]=coord->point[i].getValue()[1];
    point[2]=coord->point[i].getValue()[2];
    vpPoint pt;
    pt.setWorldCoordinates(point[0],point[1],point[2]);
    ifs->pt.push_back(pt);
  }
  
  SoMFInt32 indexList = face_set->coordIndex;
  int indexListSize = indexList.getNum();  
  
  ifs->nbIndex = indexListSize;
  for (int i = 0; i < indexListSize; i++)
  {
    int index = face_set->coordIndex[i];
    ifs->index.push_back(index);
  }
}

void ifsToBound (Bound* bptr, vpList<indexFaceSet*> &ifs_list)
{
  int nbPt = 0;
  ifs_list.front();
  for (int i = 0; i < ifs_list.nbElements(); i++)
  {
    indexFaceSet* ifs = ifs_list.value();
    nbPt += ifs->nbPt;
    ifs_list.next();
  }
  bptr->point.nbr = nbPt;
  bptr->point.ptr = (Point3f *) malloc (nbPt * sizeof (Point3f));
  
  ifs_list.front();
  int iter = 0;
  for (int i = 0; i < ifs_list.nbElements(); i++)
  {
    indexFaceSet* ifs = ifs_list.value();
    for (int j = 0; j < ifs->nbPt; j++)
    {
      bptr->point.ptr[iter].x = ifs->pt[j].get_oX();
      bptr->point.ptr[iter].y = ifs->pt[j].get_oY();
      bptr->point.ptr[iter].z = ifs->pt[j].get_oZ();
      iter++;
    }
    ifs_list.next();
  }
  
  int nbFace = 0;
  ifs_list.front();
  vpList<int> indSize;
  int indice = 0;
  for (int i = 0; i < ifs_list.nbElements(); i++)
  {
    indexFaceSet* ifs = ifs_list.value();
    for (int j = 0; j < ifs->nbIndex; j++)
    {
      if(ifs->index[j] == -1) 
      {
	nbFace++;
	indSize.addRight(indice);
	indice = 0;
      }
      else indice++;
    }
    ifs_list.next();
  }
  
  bptr->face.nbr = nbFace;
  bptr->face.ptr = (Face *) malloc (nbFace * sizeof (Face));
  
  
  indSize.front();
  for (int i = 0; i < indSize.nbElements(); i++)
  {
    bptr->face.ptr[i].vertex.nbr = indSize.value();
    bptr->face.ptr[i].vertex.ptr = (Index *) malloc (indSize.value() * sizeof (Index));
    indSize.next();
  }
  
  int offset = 0;
  ifs_list.front();
  indice = 0;
  for (int i = 0; i < ifs_list.nbElements(); i++)
  {
    indexFaceSet* ifs = ifs_list.value();
    iter = 0;
    for (int j = 0; j < ifs->nbIndex; j++)
    {
      if(ifs->index[j] != -1)
      {
	bptr->face.ptr[indice].vertex.ptr[iter] = ifs->index[j] + offset;
	iter++;
      }
      else
      {
	iter = 0;
	indice++;
      }
    }
    offset += ifs->nbPt;
  }
}

void destroyIfs(vpList<indexFaceSet*> &ifs_list)
{
  ifs_list.front();
  while (!ifs_list.outside())
  {
    indexFaceSet* ifs = ifs_list.value();
    delete ifs;
    ifs_list.next();
  }
  ifs_list.kill();
}
#else
void set_scene_wrl (const char* /*str*/, Bound_scene */*sc*/, float /*factor*/)
{
}
#endif


/*
  Convert the matrix format to deal with the one in the simulator
*/
void vp2jlc_matrix (const vpHomogeneousMatrix vpM, Matrix &jlcM)
{
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++) jlcM[j][i] = (float)vpM[i][j];
  }
}

/*
  Copy the scene corresponding to the registeresd parameters in the image.
*/
void
vpWireFrameSimulator::display_scene(Matrix mat, Bound_scene &sc, vpImage<vpRGBa> &I, vpColor color)
{
  extern Bound *clipping_Bound ();
  Bound *bp, *bend;
  Bound *clip; /* surface apres clipping */
  Byte b  = (Byte) *get_rfstack ();
  Matrix m;


  //bcopy ((char *) mat, (char *) m, sizeof (Matrix));
  memmove((char *) m, (char *) mat, sizeof (Matrix));
  View_to_Matrix (get_vwstack (), *(get_tmstack ()));
  postmult_matrix (m, *(get_tmstack ()));
  bp   = sc.bound.ptr;
  bend = bp + sc.bound.nbr;
  for (; bp < bend; bp++)
  {
    if ((clip = clipping_Bound (bp, m)) != NULL)
    {
      Face *fp   = clip->face.ptr;
      Face *fend = fp + clip->face.nbr;

      set_Bound_face_display (clip, b); //regarde si is_visible

      point_3D_2D (clip->point.ptr, clip->point.nbr,I.getWidth(),I.getHeight(),point2i);
      for (; fp < fend; fp++)
      {
        if (fp->is_visible)
	{
	  wireframe_Face (fp, point2i);
	  Point2i *pt = listpoint2i;
	  for (int i = 1; i < fp->vertex.nbr; i++)
	  {
	    vpDisplay::displayLine(I,vpImagePoint((pt)->y,(pt)->x),vpImagePoint((pt+1)->y,(pt+1)->x),color,1);
	    pt++;
	  }
	  if (fp->vertex.nbr > 2)
	  {
	    vpDisplay::displayLine(I,vpImagePoint((listpoint2i)->y,(listpoint2i)->x),vpImagePoint((pt)->y,(pt)->x),color,1);
	  }
	}
      }
    }
  }
}

/*
  Copy the scene corresponding to the registeresd parameters in the image.
*/
void
vpWireFrameSimulator::display_scene(Matrix mat, Bound_scene &sc, vpImage<unsigned char> &I, vpColor color)
{
  extern Bound *clipping_Bound ();
//  extern Point2i *point2i;
//  extern Point2i *listpoint2i;
  Bound *bp, *bend;
  Bound *clip; /* surface apres clipping */
  Byte b  = (Byte) *get_rfstack ();
  Matrix m;


  bcopy ((char *) mat, (char *) m, sizeof (Matrix));
  View_to_Matrix (get_vwstack (), *(get_tmstack ()));
  postmult_matrix (m, *(get_tmstack ()));
  bp   = sc.bound.ptr;
  bend = bp + sc.bound.nbr;
  for (; bp < bend; bp++)
  {
    if ((clip = clipping_Bound (bp, m)) != NULL)
    {
      Face *fp   = clip->face.ptr;
      Face *fend = fp + clip->face.nbr;

      set_Bound_face_display (clip, b); //regarde si is_visible

      point_3D_2D (clip->point.ptr, clip->point.nbr,I.getWidth(),I.getHeight(),point2i);
      for (; fp < fend; fp++)
      {
        if (fp->is_visible)
	{
	  wireframe_Face (fp, point2i);
	  Point2i *pt = listpoint2i;
	  for (int i = 1; i < fp->vertex.nbr; i++)
	  {
	    vpDisplay::displayLine(I,vpImagePoint((pt)->y,(pt)->x),vpImagePoint((pt+1)->y,(pt+1)->x),color,1);
	    pt++;
	  }
	  if (fp->vertex.nbr > 2)
	  {
	    vpDisplay::displayLine(I,vpImagePoint((listpoint2i)->y,(listpoint2i)->x),vpImagePoint((pt)->y,(pt)->x),color,1);
	  }
	}
      }
    }
  }
}

// vpImagePoint getCameraPosition(Matrix mat, vpImage<vpRGBa> &I)
// {
//   Matrix m;
// 
//   bcopy ((char *) mat, (char *) m, sizeof (Matrix));
//   View_to_Matrix (get_vwstack (), *(get_tmstack ()));
//   postmult_matrix (m, *(get_tmstack ()));
// 
//   Point3f p3[1];
//   Point2i p2[1];
//   Point4f p4[1];
// 
//   p3[0].x = 0;
//   p3[0].y = 0;
//   p3[0].z = 0;
// 
//   point_3D_4D (p3, 1, m, p4);
//   p3->x = p4->x / p4->w;
//   p3->y = p4->y / p4->w;
//   p3->z = p4->z / p4->w;
// 
//   int size = vpMath::minimum(I.getWidth(),I.getHeight());
//   point_3D_2D (p3, 1,size,size,p2);
// 
//   vpImagePoint iP((p2)->y,(p2)->x);
// 
//   return iP;
// }
// 
// vpImagePoint getCameraPosition(Matrix mat, vpImage<unsigned char> &I)
// {
//   Matrix m;
// 
//   bcopy ((char *) mat, (char *) m, sizeof (Matrix));
//   View_to_Matrix (get_vwstack (), *(get_tmstack ()));
//   postmult_matrix (m, *(get_tmstack ()));
// 
//   Point3f p3[1];
//   Point2i p2[1];
//   Point4f p4[1];
// 
//   p3[0].x = 0;//mat[3][0];
//   p3[0].y = 0;//mat[3][1];
//   p3[0].z = 0;//mat[3][2];
// 
//   point_3D_4D (p3, 1, m, p4);
//   p3->x = p4->x / p4->w;
//   p3->y = p4->y / p4->w;
//   p3->z = p4->z / p4->w;
// 
//   int size = vpMath::minimum(I.getWidth(),I.getHeight());
//   point_3D_2D (p3, 1,size,size,p2);
// 
//   vpImagePoint iP((p2)->y,(p2)->x);
// 
//   return iP;
// }


/*************************************************************************************************************/

/*!
  Basic constructor
*/
vpWireFrameSimulator::vpWireFrameSimulator()
{
  open_display();
  open_clipping();

  camColor = vpColor::green;
  camTrajColor = vpColor::green;
  curColor = vpColor::blue;
  desColor = vpColor::red;

  sceneInitialized = false;

  displayCameraTrajectory = true;
  cameraTrajectory.kill();
  poseList.kill();
  fMoList.kill();

  fMo.setIdentity();

  old_iPr = vpImagePoint(-1,-1);
  old_iPz = vpImagePoint(-1,-1);
  old_iPt = vpImagePoint(-1,-1);
  blockedr = false;
  blockedz = false;
  blockedt = false;
  blocked = false;

  nbrPtLimit = 1000;
  
  px_int = 1;
  py_int = 1;
  px_ext = 1;
  py_ext = 1;
  
  displayObject = false;
  displayDesiredObject = false;
  displayCamera = false;
  
  cameraFactor = 1.0;
  
  camTrajType = CT_LINE;
  
  extCamChanged = false;
  
  rotz.buildFrom(0,0,0,0,0,vpMath::rad(180));
}


/*!
  Basic destructor
*/
vpWireFrameSimulator::~vpWireFrameSimulator()
{
  if(sceneInitialized)
  {
    if(displayObject)
      free_Bound_scene (&(this->scene));
    if(displayCamera)
      free_Bound_scene (&(this->camera));
    if(displayDesiredObject)
      free_Bound_scene (&(this->desiredScene));
  }
  close_display ();
 // close_clipping ();

  cameraTrajectory.kill();
  poseList.kill();
  fMoList.kill();
}


/*!
  Initialize the simulator. It enables to choose the type of scene which will be used to display the object
  at the current position and at the desired position.
  
  It exists several default scenes you can use. Use the vpSceneObject and the vpSceneDesiredObject attributes to use them in this method. The corresponding files are stored in the "data" folder which is in the ViSP build directory.

  \param obj : Type of scene used to display the object at the current position.
  \param desiredObject : Type of scene used to display the object at the desired pose (in the internal view).
*/
void
vpWireFrameSimulator::initScene(vpSceneObject obj, vpSceneDesiredObject desiredObject)
{
  char name_cam[FILENAME_MAX];
  char name[FILENAME_MAX];

  object = obj;
  this->desiredObject = desiredObject;

  strcpy(name_cam,VISP_SCENES_DIR);
  if (desiredObject != D_TOOL) 
  {
    strcat(name_cam,"/camera.bnd");
    set_scene(name_cam,&camera,cameraFactor);
  }
  else
  {
    strcat(name_cam,"/tool.bnd");
    set_scene(name_cam,&(this->camera),1.0);
  }

  strcpy(name,VISP_SCENES_DIR);
  switch (obj)
  {
    case THREE_PTS : {strcat(name,"/3pts.bnd"); break; }
    case CUBE : { strcat(name, "/cube.bnd"); break; }
    case PLATE : { strcat(name, "/plate.bnd"); break; }
    case SMALL_PLATE : { strcat(name, "/plate_6cm.bnd"); break; }
    case RECTANGLE : { strcat(name, "/rectangle.bnd"); break; }
    case SQUARE_10CM : { strcat(name, "/square10cm.bnd"); break; }
    case DIAMOND : { strcat(name, "/diamond.bnd"); break; }
    case TRAPEZOID : { strcat(name, "/trapezoid.bnd"); break; }
    case THREE_LINES : { strcat(name, "/line.bnd"); break; }
    case ROAD : { strcat(name, "/road.bnd"); break; }
    case TIRE : { strcat(name, "/circles2.bnd"); break; }
    case PIPE : { strcat(name, "/pipe.bnd"); break; }
    case CIRCLE : { strcat(name, "/circle.bnd"); break; }
    case SPHERE : { strcat(name, "/sphere.bnd"); break; }
    case CYLINDER : { strcat(name, "/cylinder.bnd"); break; }
    case PLAN: { strcat(name, "/plan.bnd"); break; }
  }
  set_scene(name,&(this->scene),1.0);

  switch (desiredObject)
  {
    case D_STANDARD : { break; }
    case D_CIRCLE : { 
      strcpy(name,VISP_SCENES_DIR);
      strcat(name, "/cercle_sq2.bnd");
      break; }
    case D_TOOL : { 
      strcpy(name,VISP_SCENES_DIR);
      strcat(name, "/tool.bnd");
      break; }
  }
  set_scene(name,&(this->desiredScene),1.0);

  if (obj == PIPE) load_rfstack(IS_INSIDE);
  else add_rfstack(IS_BACK);

  add_vwstack ("start","depth", 0.0, 100.0);
  add_vwstack ("start","window", -0.1,0.1,-0.1,0.1);
  add_vwstack ("start","type", PERSPECTIVE);

  sceneInitialized = true;
  displayObject = true;
  displayDesiredObject = true;
  displayCamera = true;
}

/*!
  Initialize the simulator. It enables to choose the type of scene which will be used to display the object
  at the current position and at the desired position.
  
  Here you can use the scene you want. You have to set the path to a .bnd or a .wrl file which is a 3D model file.

  \param obj : Path to the scene file you want to use.
  \param desiredObject : Path to the scene file you want to use.
*/
void
vpWireFrameSimulator::initScene(const char* obj, const char* desiredObject)
{
  char name_cam[FILENAME_MAX];
  char name[FILENAME_MAX];

  object = THREE_PTS;
  this->desiredObject = D_STANDARD;
  
  strcpy(name_cam,VISP_SCENES_DIR);
  strcat(name_cam,"/camera.bnd");
  set_scene(name_cam,&camera,cameraFactor);

  strcpy(name,obj);
  Model_3D model;
  model = getExtension(obj);
  if (model == BND_MODEL)
    set_scene(name,&(this->scene),1.0);
  else if (model == WRL_MODEL)
    set_scene_wrl(name,&(this->scene),1.0);
  else if (model == UNKNOWN_MODEL)
  {
    vpERROR_TRACE("Unknown file extension for the 3D model");
  }

  strcpy(name,desiredObject);  
  model = getExtension(desiredObject);
  if (model == BND_MODEL)
    set_scene(name,&(this->desiredScene),1.0);
  else if (model == WRL_MODEL)
    set_scene_wrl(name,&(this->desiredScene),1.0);
  else if (model == UNKNOWN_MODEL)
  {
    vpERROR_TRACE("Unknown file extension for the 3D model");
  }

  add_rfstack(IS_BACK);

  add_vwstack ("start","depth", 0.0, 100.0);
  add_vwstack ("start","window", -0.1,0.1,-0.1,0.1);
  add_vwstack ("start","type", PERSPECTIVE);

  sceneInitialized = true;
  displayObject = true;
  displayDesiredObject = true;
  displayCamera = true;
}


/*!
  Initialize the simulator. It enables to choose the type of object which will be used to display the object
  at the current position. The object at the desired position is not displayed.
  
  It exists several default scenes you can use. Use the vpSceneObject attributes to use them in this method. The corresponding files are stored in the "data" folder which is in the ViSP build directory.

  \param obj : Type of scene used to display the object at the current position.
*/
void
vpWireFrameSimulator::initScene(vpSceneObject obj)
{
  char name_cam[FILENAME_MAX];
  char name[FILENAME_MAX];

  object = obj;

  strcpy(name_cam,VISP_SCENES_DIR);

  strcpy(name,VISP_SCENES_DIR);
  switch (obj)
  {
    case THREE_PTS : {strcat(name,"/3pts.bnd"); break; }
    case CUBE : { strcat(name, "/cube.bnd"); break; }
    case PLATE : { strcat(name, "/plate.bnd"); break; }
    case SMALL_PLATE : { strcat(name, "/plate_6cm.bnd"); break; }
    case RECTANGLE : { strcat(name, "/rectangle.bnd"); break; }
    case SQUARE_10CM : { strcat(name, "/square10cm.bnd"); break; }
    case DIAMOND : { strcat(name, "/diamond.bnd"); break; }
    case TRAPEZOID : { strcat(name, "/trapezoid.bnd"); break; }
    case THREE_LINES : { strcat(name, "/line.bnd"); break; }
    case ROAD : { strcat(name, "/road.bnd"); break; }
    case TIRE : { strcat(name, "/circles2.bnd"); break; }
    case PIPE : { strcat(name, "/pipe.bnd"); break; }
    case CIRCLE : { strcat(name, "/circle.bnd"); break; }
    case SPHERE : { strcat(name, "/sphere.bnd"); break; }
    case CYLINDER : { strcat(name, "/cylinder.bnd"); break; }
    case PLAN: { strcat(name, "/plan.bnd"); break; }
  }
  set_scene(name,&(this->scene),1.0);

  if (obj == PIPE) load_rfstack(IS_INSIDE);
  else add_rfstack(IS_BACK);

  add_vwstack ("start","depth", 0.0, 100.0);
  add_vwstack ("start","window", -0.1,0.1,-0.1,0.1);
  add_vwstack ("start","type", PERSPECTIVE);

  sceneInitialized = true;
  displayObject = true;
  displayCamera = true;
}

/*!
  Initialize the simulator. It enables to choose the type of scene which will be used to display the object
  at the current position. The object at the desired position is not displayed.
  
  Here you can use the scene you want. You have to set the path to a .bnd or a .wrl file which is a 3D model file.

  \param obj : Path to the scene file you want to use.
*/
void
vpWireFrameSimulator::initScene(const char* obj)
{
  char name_cam[FILENAME_MAX];
  char name[FILENAME_MAX];

  object = THREE_PTS;
  
  strcpy(name_cam,VISP_SCENES_DIR);
  strcat(name_cam,"/camera.bnd");
  set_scene(name_cam,&camera,cameraFactor);

  strcpy(name,obj);
  Model_3D model;
  model = getExtension(obj);
  if (model == BND_MODEL)
    set_scene(name,&(this->scene),1.0);
  else if (model == WRL_MODEL)
    set_scene_wrl(name,&(this->scene),1.0);
  else if (model == UNKNOWN_MODEL)
  {
    vpERROR_TRACE("Unknown file extension for the 3D model");
  }

  add_rfstack(IS_BACK);

  add_vwstack ("start","depth", 0.0, 100.0);
  add_vwstack ("start","window", -0.1,0.1,-0.1,0.1);
  add_vwstack ("start","type", PERSPECTIVE);

  sceneInitialized = true;
  displayObject = true;
  displayCamera = true;
}



/*!
  Get the internal view ie the view of the camera.

  \param I : The image where the internal view is displayed.
  
  \warning : The objects are displayed thanks to overlays. The image I is not modified.
*/
void
vpWireFrameSimulator::getInternalImage(vpImage<vpRGBa> &I)
{
  if (!sceneInitialized)
    throw(vpException(vpSimulatorException::notInitializedError,"The scene has to be initialized")) ;

  double u;
  double v;
  if(px_int != 1 && py_int != 1)
  {
    u = (double)I.getWidth()/(2*px_int);
    v = (double)I.getHeight()/(2*py_int);
  }
  else
  {
    u = (double)I.getWidth()/(vpMath::minimum(I.getWidth(),I.getHeight()));
    v = (double)I.getHeight()/(vpMath::minimum(I.getWidth(),I.getHeight()));
  }

  float o44c[4][4],o44cd[4][4],x,y,z;
  Matrix id = IDENTITY_MATRIX;

  vp2jlc_matrix(cMo.inverse(),o44c);
  vp2jlc_matrix(cdMo.inverse(),o44cd);

  add_vwstack ("start","cop", o44c[3][0],o44c[3][1],o44c[3][2]);
  x = o44c[2][0] + o44c[3][0];
  y = o44c[2][1] + o44c[3][1];
  z = o44c[2][2] + o44c[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", o44c[2][0],o44c[2][1],o44c[2][2]);
  add_vwstack ("start","vup", o44c[1][0],o44c[1][1],o44c[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);
  if (displayObject)
    display_scene(id,this->scene,I, curColor);


  add_vwstack ("start","cop", o44cd[3][0],o44cd[3][1],o44cd[3][2]);
  x = o44cd[2][0] + o44cd[3][0];
  y = o44cd[2][1] + o44cd[3][1];
  z = o44cd[2][2] + o44cd[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", o44cd[2][0],o44cd[2][1],o44cd[2][2]);
  add_vwstack ("start","vup", o44cd[1][0],o44cd[1][1],o44cd[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);
  if (displayDesiredObject)
  {
    if (desiredObject == D_TOOL) display_scene(o44cd,desiredScene,I, vpColor::red);
    else display_scene(id,desiredScene,I, desColor);
  }

}


/*!
  Get the external view. It corresponds to the view of the scene from a reference frame you have to set.

  \param I : The image where the external view is displayed.
  
  \warning : The objects are displayed thanks to overlays. The image I is not modified.
*/

void
vpWireFrameSimulator::getExternalImage(vpImage<vpRGBa> &I)
{
  bool changed = false;
  vpHomogeneousMatrix displacement = navigation(I,changed);

  if (displacement[2][3] != 0 /*|| rotation[0][3] != 0 || rotation[1][3] != 0*/)
      camMf2 = camMf2*displacement;

  f2Mf = camMf2.inverse()*camMf;

  camMf = camMf2* displacement * f2Mf;

  double u;
  double v;
  if(px_ext != 1 && py_ext != 1)
  {
    u = (double)I.getWidth()/(2*px_ext);
    v = (double)I.getHeight()/(2*py_ext);
  }
  else
  {
    u = (double)I.getWidth()/(vpMath::minimum(I.getWidth(),I.getHeight()));
    v = (double)I.getHeight()/(vpMath::minimum(I.getWidth(),I.getHeight()));
  }

  float w44o[4][4],w44cext[4][4],w44c[4][4],x,y,z;

  vp2jlc_matrix(camMf.inverse(),w44cext);
  vp2jlc_matrix(fMo*cMo.inverse(),w44c);
  vp2jlc_matrix(fMo,w44o);


  add_vwstack ("start","cop", w44cext[3][0],w44cext[3][1],w44cext[3][2]);
  x = w44cext[2][0] + w44cext[3][0];
  y = w44cext[2][1] + w44cext[3][1];
  z = w44cext[2][2] + w44cext[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", w44cext[2][0],w44cext[2][1],w44cext[2][2]);
  add_vwstack ("start","vup", w44cext[1][0],w44cext[1][1],w44cext[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);
  if ((object == CUBE) || (object == SPHERE))
  {
    add_vwstack ("start","type", PERSPECTIVE);
  }
  
  if (displayObject)
    display_scene(w44o,this->scene,I, curColor);

  if (displayCamera)
    display_scene(w44c,camera, I, camColor);

  if (displayCameraTrajectory)
  {
    vpImagePoint iP;
    vpImagePoint iP_1;
    poseList.end();
    poseList.addRight(cMo);
    fMoList.end();
    fMoList.addRight(fMo);
  
    int iter = 0;

    if (changed || extCamChanged)
    {
      cameraTrajectory.kill();
      poseList.front();
      fMoList.front();
      while (!poseList.outside() && !fMoList.outside())
      {
        iP = projectCameraTrajectory(I, poseList.value(),fMoList.value());
        cameraTrajectory.addRight(iP);
	if (camTrajType == CT_LINE)
	{
          if (iter != 0) vpDisplay::displayLine(I,iP_1,iP,camTrajColor);
	}
	else if (camTrajType == CT_POINT)
	  vpDisplay::displayPoint(I,iP,camTrajColor);
        poseList.next();
        fMoList.next();
        iter++;
        iP_1 = iP;
      }
      extCamChanged = false;
    }
    else
    {
      iP = projectCameraTrajectory(I, poseList.value(),fMoList.value());
      cameraTrajectory.end();
      cameraTrajectory.addRight(iP);
      cameraTrajectory.front();
      while (!cameraTrajectory.outside())
      {
	if (camTrajType == CT_LINE)
	{
          if (iter != 0) vpDisplay::displayLine(I,iP_1,cameraTrajectory.value(),camTrajColor);
	}
	else if (camTrajType == CT_POINT)
	  vpDisplay::displayPoint(I,cameraTrajectory.value(),camTrajColor);
        iter++;
        iP_1 = cameraTrajectory.value();
        cameraTrajectory.next();
      }
    }

    if (poseList.nbElement() > nbrPtLimit)
    {
      poseList.front();
      poseList.suppress();
    }
    if (fMoList.nbElement() > nbrPtLimit)
    {
      fMoList.front();
      fMoList.suppress();
    }
    if (cameraTrajectory.nbElement() > nbrPtLimit)
    {
      cameraTrajectory.front();
      cameraTrajectory.suppress();
    }
  }
}


/*!
  Get an external view. The point of view is set thanks to the pose between the camera camMf and the fixed world frame.

  \param I : The image where the external view is displayed.
  \param camMf : The pose between the point of view and the fixed world frame.
  
  \warning : The objects are displayed thanks to overlays. The image I is not modified.
*/
void
vpWireFrameSimulator::getExternalImage(vpImage<vpRGBa> &I, vpHomogeneousMatrix camMf)
{
  float w44o[4][4],w44cext[4][4],w44c[4][4],x,y,z;
  
  vpHomogeneousMatrix camMft = rotz * camMf;

  double u;
  double v;
  if(px_ext != 1 && py_ext != 1)
  {
    u = (double)I.getWidth()/(2*px_ext);
    v = (double)I.getHeight()/(2*py_ext);
  }
  else
  {
    u = (double)I.getWidth()/(vpMath::minimum(I.getWidth(),I.getHeight()));
    v = (double)I.getHeight()/(vpMath::minimum(I.getWidth(),I.getHeight()));
  }

  vp2jlc_matrix(camMft.inverse(),w44cext);
  vp2jlc_matrix(fMo*cMo.inverse(),w44c);
  vp2jlc_matrix(fMo,w44o);

  add_vwstack ("start","cop", w44cext[3][0],w44cext[3][1],w44cext[3][2]);
  x = w44cext[2][0] + w44cext[3][0];
  y = w44cext[2][1] + w44cext[3][1];
  z = w44cext[2][2] + w44cext[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", w44cext[2][0],w44cext[2][1],w44cext[2][2]);
  add_vwstack ("start","vup", w44cext[1][0],w44cext[1][1],w44cext[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);
  
  if (displayObject)
    display_scene(w44o,this->scene,I, curColor);
  if (displayCamera)
    display_scene(w44c,camera, I, camColor);
}


/*!
  Get the internal view ie the view of the camera.

  \param I : The image where the internal view is displayed.
  
  \warning : The objects are displayed thanks to overlays. The image I is not modified.
*/
void
vpWireFrameSimulator::getInternalImage(vpImage<unsigned char> &I)
{
  if (!sceneInitialized)
    throw(vpException(vpSimulatorException::notInitializedError,"The scene has to be initialized")) ;

  double u;
  double v;
  if(px_int != 1 && py_int != 1)
  {
    u = (double)I.getWidth()/(2*px_int);
    v = (double)I.getHeight()/(2*py_int);
  }
  else
  {
    u = (double)I.getWidth()/(vpMath::minimum(I.getWidth(),I.getHeight()));
    v = (double)I.getHeight()/(vpMath::minimum(I.getWidth(),I.getHeight()));
  }

  float o44c[4][4],o44cd[4][4],x,y,z;
  Matrix id = IDENTITY_MATRIX;

  vp2jlc_matrix(cMo.inverse(),o44c);
  vp2jlc_matrix(cdMo.inverse(),o44cd);

  add_vwstack ("start","cop", o44c[3][0],o44c[3][1],o44c[3][2]);
  x = o44c[2][0] + o44c[3][0];
  y = o44c[2][1] + o44c[3][1];
  z = o44c[2][2] + o44c[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", o44c[2][0],o44c[2][1],o44c[2][2]);
  add_vwstack ("start","vup", o44c[1][0],o44c[1][1],o44c[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);
  if (displayObject)
    display_scene(id,this->scene,I, curColor);


  add_vwstack ("start","cop", o44cd[3][0],o44cd[3][1],o44cd[3][2]);
  x = o44cd[2][0] + o44cd[3][0];
  y = o44cd[2][1] + o44cd[3][1];
  z = o44cd[2][2] + o44cd[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", o44cd[2][0],o44cd[2][1],o44cd[2][2]);
  add_vwstack ("start","vup", o44cd[1][0],o44cd[1][1],o44cd[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);
  if (displayDesiredObject)
  {
    if (desiredObject == D_TOOL) display_scene(o44cd,desiredScene,I, vpColor::red);
    else display_scene(id,desiredScene,I, desColor);
  }

}


/*!
  Get the external view. It corresponds to the view of the scene from a reference frame you have to set.

  \param I : The image where the external view is displayed.
  
  \warning : The objects are displayed thanks to overlays. The image I is not modified.
*/

void
vpWireFrameSimulator::getExternalImage(vpImage<unsigned char> &I)
{
  bool changed = false;
  vpHomogeneousMatrix displacement = navigation(I,changed);

  if (displacement[2][3] != 0 /*|| rotation[0][3] != 0 || rotation[1][3] != 0*/)
      camMf2 = camMf2*displacement;

  f2Mf = camMf2.inverse()*camMf;

  camMf = camMf2* displacement * f2Mf;

  double u;
  double v;
  if(px_ext != 1 && py_ext != 1)
  {
    u = (double)I.getWidth()/(2*px_ext);
    v = (double)I.getHeight()/(2*py_ext);
  }
  else
  {
    u = (double)I.getWidth()/(vpMath::minimum(I.getWidth(),I.getHeight()));
    v = (double)I.getHeight()/(vpMath::minimum(I.getWidth(),I.getHeight()));
  }

  float w44o[4][4],w44cext[4][4],w44c[4][4],x,y,z;

  vp2jlc_matrix(camMf.inverse(),w44cext);
  vp2jlc_matrix(fMo*cMo.inverse(),w44c);
  vp2jlc_matrix(fMo,w44o);


  add_vwstack ("start","cop", w44cext[3][0],w44cext[3][1],w44cext[3][2]);
  x = w44cext[2][0] + w44cext[3][0];
  y = w44cext[2][1] + w44cext[3][1];
  z = w44cext[2][2] + w44cext[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", w44cext[2][0],w44cext[2][1],w44cext[2][2]);
  add_vwstack ("start","vup", w44cext[1][0],w44cext[1][1],w44cext[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);
  if ((object == CUBE) || (object == SPHERE))
  {
    add_vwstack ("start","type", PERSPECTIVE);
  }
  if (displayObject)
    display_scene(w44o,this->scene,I, curColor);

  if (displayCamera)
    display_scene(w44c,camera, I, camColor);

  if (displayCameraTrajectory)
  {
    vpImagePoint iP;
    vpImagePoint iP_1;
    poseList.end();
    poseList.addRight(cMo);
    fMoList.end();
    fMoList.addRight(fMo);
  
    int iter = 0;

    if (changed || extCamChanged)
    {
      cameraTrajectory.kill();
      poseList.front();
      fMoList.front();
      while (!poseList.outside() && !fMoList.outside())
      {
        iP = projectCameraTrajectory(I, poseList.value(),fMoList.value());
        cameraTrajectory.addRight(iP);
        //vpDisplay::displayPoint(I,cameraTrajectory.value(),vpColor::green);
        if (camTrajType == CT_LINE)
	{
          if (iter != 0) vpDisplay::displayLine(I,iP_1,iP,camTrajColor);
	}
	else if (camTrajType == CT_POINT)
	  vpDisplay::displayPoint(I,iP,camTrajColor);
        poseList.next();
        fMoList.next();
        iter++;
        iP_1 = iP;
      }
      extCamChanged = false;
    }
    else
    {
      iP = projectCameraTrajectory(I, poseList.value(),fMoList.value());
      cameraTrajectory.end();
      cameraTrajectory.addRight(iP);
      cameraTrajectory.front();
      while (!cameraTrajectory.outside())
      {
        if (camTrajType == CT_LINE)
	{
          if (iter != 0) vpDisplay::displayLine(I,iP_1,cameraTrajectory.value(),camTrajColor);
	}
	else if (camTrajType == CT_POINT)
	  vpDisplay::displayPoint(I,cameraTrajectory.value(),camTrajColor);
        iter++;
        iP_1 = cameraTrajectory.value();
        cameraTrajectory.next();
      }
    }

    if (poseList.nbElement() > nbrPtLimit)
    {
      poseList.front();
      poseList.suppress();
    }
    if (fMoList.nbElement() > nbrPtLimit)
    {
      fMoList.front();
      fMoList.suppress();
    }
    if (cameraTrajectory.nbElement() > nbrPtLimit)
    {
      cameraTrajectory.front();
      cameraTrajectory.suppress();
    }
  }
}


/*!
  Get an external view. The point of view is set thanks to the pose between the camera camMf and the fixed world frame.

  \param I : The image where the external view is displayed.
  \param camMf : The pose between the point of view and the fixed world frame.
  
  \warning : The objects are displayed thanks to overlays. The image I is not modified.
*/
void
vpWireFrameSimulator::getExternalImage(vpImage<unsigned char> &I, vpHomogeneousMatrix camMf)
{
  float w44o[4][4],w44cext[4][4],w44c[4][4],x,y,z;

  vpHomogeneousMatrix camMft = rotz * camMf;
  
  double u;
  double v;
  if(px_ext != 1 && py_ext != 1)
  {
    u = (double)I.getWidth()/(2*px_ext);
    v = (double)I.getHeight()/(2*py_ext);
  }
  else
  {
    u = (double)I.getWidth()/(vpMath::minimum(I.getWidth(),I.getHeight()));
    v = (double)I.getHeight()/(vpMath::minimum(I.getWidth(),I.getHeight()));
  }

  vp2jlc_matrix(camMft.inverse(),w44cext);
  vp2jlc_matrix(fMo*cMo.inverse(),w44c);
  vp2jlc_matrix(fMo,w44o);

  add_vwstack ("start","cop", w44cext[3][0],w44cext[3][1],w44cext[3][2]);
  x = w44cext[2][0] + w44cext[3][0];
  y = w44cext[2][1] + w44cext[3][1];
  z = w44cext[2][2] + w44cext[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", w44cext[2][0],w44cext[2][1],w44cext[2][2]);
  add_vwstack ("start","vup", w44cext[1][0],w44cext[1][1],w44cext[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);
  
  if (displayObject)
    display_scene(w44o,this->scene,I, curColor);
  if (displayCamera)
    display_scene(w44c,camera, I, camColor);
}

/*!
  Display a trajectory thanks to a list of homogeneous matrices which give the position of the camera relative to the object and the position of the object relative to the world refrence frame. The trajectory is projected into the view of an external camera whose position is given in parameter.
  
  The two lists must have the same size of homogeneous matrices must have the same size.
  
  \param I : The image where the trajectory is displayed.
  \param list_cMo : The homogeneous matrices list containing the position of the camera relative to the object.
  \param list_fMo : The homogeneous matrices list containing the position of the object relative to the world reference frame.
  \param cMf : A homogeneous matrix which gives the position of the external camera (used to project the trajectory) relative to the world refrence frame.
*/
void
vpWireFrameSimulator::displayTrajectory (vpImage<unsigned char> &I, vpList<vpHomogeneousMatrix> &list_cMo, vpList<vpHomogeneousMatrix> &list_fMo, vpHomogeneousMatrix cMf)
{
  if (list_cMo.nbElements() != list_fMo.nbElements())
    throw(vpException(vpException::dimensionError ,"The two lists must have the same size")) ;
  
  list_cMo.front();
  list_fMo.front();
  vpImagePoint iP;
  vpImagePoint iP_1;
  int iter = 0;

  while (!list_cMo.outside() && !list_fMo.outside())
  {
    iP = projectCameraTrajectory(I, rotz * list_cMo.value(), list_fMo.value(), rotz * cMf);
    if (camTrajType == CT_LINE)
    {
      if (iter != 0) vpDisplay::displayLine(I,iP_1,iP,camTrajColor);
    }
    else if (camTrajType == CT_POINT)
      vpDisplay::displayPoint(I,iP,camTrajColor);
    list_cMo.next();
    list_fMo.next();
    iter++;
    iP_1 = iP;
  }
}

/*!
  Display a trajectory thanks to a list of homogeneous matrices which give the position of the camera relative to the object and the position of the object relative to the world refrence frame. The trajectory is projected into the view of an external camera whose position is given in parameter.
  
  The two lists must have the same size of homogeneous matrices must have the same size.
  
  \param I : The image where the trajectory is displayed.
  \param list_cMo : The homogeneous matrices list containing the position of the camera relative to the object.
  \param list_fMo : The homogeneous matrices list containing the position of the object relative to the world reference frame.
  \param cMf : A homogeneous matrix which gives the position of the external camera (used to project the trajectory) relative to the world refrence frame.
*/
void
vpWireFrameSimulator::displayTrajectory (vpImage<vpRGBa> &I, vpList<vpHomogeneousMatrix> &list_cMo, vpList<vpHomogeneousMatrix> &list_fMo, vpHomogeneousMatrix cMf)
{
  if (list_cMo.nbElements() != list_fMo.nbElements())
    throw(vpException(vpException::dimensionError ,"The two lists must have the same size")) ;
  
  list_cMo.front();
  list_fMo.front();
  vpImagePoint iP;
  vpImagePoint iP_1;
  int iter = 0;

  while (!list_cMo.outside() && !list_fMo.outside())
  {
    iP = projectCameraTrajectory(I, rotz * list_cMo.value(), list_fMo.value(), rotz * cMf);
    if (camTrajType == CT_LINE)
    {
      if (iter != 0) vpDisplay::displayLine(I,iP_1,iP,camTrajColor);
    }
    else if (camTrajType == CT_POINT)
      vpDisplay::displayPoint(I,iP,camTrajColor);
    list_cMo.next();
    list_fMo.next();
    iter++;
    iP_1 = iP;
  }
}

/*!
  Enables to change the external camera position.
*/
vpHomogeneousMatrix
vpWireFrameSimulator::navigation(vpImage<vpRGBa> &I, bool &changed)
{
  double width = vpMath::minimum(I.getWidth(),I.getHeight());
  vpImagePoint iP;
  vpImagePoint trash;
  bool clicked = false;
  bool clickedUp = false;
  vpMouseButton::vpMouseButtonType b = vpMouseButton::button1;

  vpHomogeneousMatrix mov(0,0,0,0,0,0);
  changed = false;
  
  if(!blocked) vpDisplay::getClickUp(I,trash, b,false);

  if(!blocked)clicked = vpDisplay::getClick(I,trash,b,false);

  if(blocked)clickedUp = vpDisplay::getClickUp(I,trash, b,false);

  if(clicked)
  {
    if (b == vpMouseButton::button1) blockedr = true;
    if (b == vpMouseButton::button2) blockedz = true;
    if (b == vpMouseButton::button3) blockedt = true;
    blocked = true;
  }
  if(clickedUp)
  {
    if (b == vpMouseButton::button1)
    {
      old_iPr = vpImagePoint(-1,-1);
      blockedr = false;
    }
    if (b == vpMouseButton::button2)
    {
      old_iPz = vpImagePoint(-1,-1);
      blockedz = false;
    }
    if (b == vpMouseButton::button3)
    {
      old_iPt = vpImagePoint(-1,-1);
      blockedt = false;
    }
    if (!(blockedr || blockedz || blockedt))
    {
      blocked = false;
      while (vpDisplay::getClick(I,trash,b,false)) {};
    }
  }
  
//   std::cout << "clicked : " << clicked << std::endl;
//   std::cout << "clickedUp : " << clickedUp << std::endl;
//   std::cout << "blockedr : " << blockedr << std::endl;
//   std::cout << "blockedz : " << blockedz << std::endl;
//   std::cout << "blockedt : " << blockedt << std::endl;
//   std::cout << "blocked : " << blocked << std::endl;
  
  vpDisplay::getPointerPosition(I,iP);
  
  //std::cout << "point : " << iP << std::endl;

  double anglei = 0;
  double anglej = 0;

  if (old_iPr != vpImagePoint(-1,-1) && blockedr)
  {
    double diffi = iP.get_i() - old_iPr.get_i();
    double diffj = iP.get_j() - old_iPr.get_j();
    //cout << "delta :" << diffj << endl;;
    anglei = diffi*360/width;
    anglej = diffj*360/width;
    mov.buildFrom(0,0,0,vpMath::rad(-anglei),vpMath::rad(anglej),0);
    changed = true;
  }

  if (blockedr) old_iPr = iP;

  if (old_iPz != vpImagePoint(-1,-1) && blockedz)
  {
    double diffi = iP.get_i() - old_iPz.get_i();
    mov.buildFrom(0,0,diffi*0.01,0,0,0);
    changed = true;
  }

  if (blockedz) old_iPz = iP;

  if (old_iPt != vpImagePoint(-1,-1) && blockedt)
  {
    double diffi = iP.get_i() - old_iPt.get_i();
    double diffj = iP.get_j() - old_iPt.get_j();
    mov.buildFrom(diffj*0.01,diffi*0.01,0,0,0,0);
    changed = true;
  }

  if (blockedt) old_iPt = iP;

  return mov;
}


/*!
  Enables to change the external camera position.
*/
vpHomogeneousMatrix
vpWireFrameSimulator::navigation(vpImage<unsigned char> &I, bool &changed)
{
  double width = vpMath::minimum(I.getWidth(),I.getHeight());
  vpImagePoint iP;
  vpImagePoint trash;
  bool clicked = false;
  bool clickedUp = false;
  vpMouseButton::vpMouseButtonType b = vpMouseButton::button1;

  vpHomogeneousMatrix mov(0,0,0,0,0,0);
  changed = false;

  if(!blocked) vpDisplay::getClickUp(I,trash, b,false);
  
  if(!blocked)clicked = vpDisplay::getClick(I,trash,b,false);

  if(blocked)clickedUp = vpDisplay::getClickUp(I,trash, b,false);

  if(clicked)
  {
    if (b == vpMouseButton::button1) blockedr = true;
    if (b == vpMouseButton::button2) blockedz = true;
    if (b == vpMouseButton::button3) blockedt = true;
    blocked = true;
  }
  if(clickedUp)
  {
    if (b == vpMouseButton::button1)
    {
      old_iPr = vpImagePoint(-1,-1);
      blockedr = false;
    }
    if (b == vpMouseButton::button2)
    {
      old_iPz = vpImagePoint(-1,-1);
      blockedz = false;
    }
    if (b == vpMouseButton::button3)
    {
      old_iPt = vpImagePoint(-1,-1);
      blockedt = false;
    }
    if (!(blockedr || blockedz || blockedt))
    {
      blocked = false;
      while (vpDisplay::getClick(I,trash,b,false)) {};
    }
  }
  
  vpDisplay::getPointerPosition(I,iP);
  
  //std::cout << "point : " << iP << std::endl;

  double anglei = 0;
  double anglej = 0;

  if (old_iPr != vpImagePoint(-1,-1) && blockedr)
  {
    double diffi = iP.get_i() - old_iPr.get_i();
    double diffj = iP.get_j() - old_iPr.get_j();
    //cout << "delta :" << diffj << endl;;
    anglei = diffi*360/width;
    anglej = diffj*360/width;
    mov.buildFrom(0,0,0,vpMath::rad(-anglei),vpMath::rad(anglej),0);
    changed = true;
  }

  if (blockedr) old_iPr = iP;

  if (old_iPz != vpImagePoint(-1,-1) && blockedz)
  {
    double diffi = iP.get_i() - old_iPz.get_i();
    mov.buildFrom(0,0,diffi*0.01,0,0,0);
    changed = true;
  }

  if (blockedz) old_iPz = iP;

  if (old_iPt != vpImagePoint(-1,-1) && blockedt)
  {
    double diffi = iP.get_i() - old_iPt.get_i();
    double diffj = iP.get_j() - old_iPt.get_j();
    mov.buildFrom(diffj*0.01,diffi*0.01,0,0,0,0);
    changed = true;
  }

  if (blockedt) old_iPt = iP;

  return mov;
}

/*!
  Project the center of the internal camera into the external camera view.
*/
vpImagePoint
vpWireFrameSimulator::projectCameraTrajectory (vpImage<vpRGBa> &I, vpHomogeneousMatrix cMo, vpHomogeneousMatrix fMo)
{
  vpPoint point;
  point.setWorldCoordinates(0,0,0);

  point.track(rotz*(camMf*fMo*cMo.inverse())) ;

  vpImagePoint iP;

  vpMeterPixelConversion::convertPoint ( getExternalCameraParameters(I), point.get_x(), point.get_y(),iP );

  return iP;
}

/*!
  Project the center of the internal camera into the external camera view.
*/
vpImagePoint
vpWireFrameSimulator::projectCameraTrajectory (vpImage<unsigned char> &I, vpHomogeneousMatrix cMo, vpHomogeneousMatrix fMo)
{
  vpPoint point;
  point.setWorldCoordinates(0,0,0);

  point.track(rotz*(camMf*fMo*cMo.inverse())) ;

  vpImagePoint iP;

  vpMeterPixelConversion::convertPoint ( getExternalCameraParameters(I), point.get_x(), point.get_y(),iP );

  return iP;
}

/*!
  Project the center of the internal camera into the external camera view.
*/
vpImagePoint
vpWireFrameSimulator::projectCameraTrajectory (vpImage<vpRGBa> &I, vpHomogeneousMatrix cMo, vpHomogeneousMatrix fMo, vpHomogeneousMatrix cMf)
{
  vpPoint point;
  point.setWorldCoordinates(0,0,0);

  point.track(rotz*(cMf*fMo*cMo.inverse())) ;

  vpImagePoint iP;

  vpMeterPixelConversion::convertPoint ( getExternalCameraParameters(I), point.get_x(), point.get_y(),iP );

  return iP;
}

/*!
  Project the center of the internal camera into the external camera view.
*/
vpImagePoint
vpWireFrameSimulator::projectCameraTrajectory (vpImage<unsigned char> &I, vpHomogeneousMatrix cMo, vpHomogeneousMatrix fMo, vpHomogeneousMatrix cMf)
{
  vpPoint point;
  point.setWorldCoordinates(0,0,0);

  point.track(rotz*(cMf*fMo*cMo.inverse())) ;

  vpImagePoint iP;

  vpMeterPixelConversion::convertPoint ( getExternalCameraParameters(I), point.get_x(), point.get_y(),iP );

  return iP;
}

/*!
*************************************************
*/
void
vpWireFrameSimulator::projectObjectInternal(vpImage<vpRGBa> &I, Bound_scene &object, vpHomogeneousMatrix cMobject)
{
  if (!sceneInitialized)
    throw(vpException(vpSimulatorException::notInitializedError,"The scene has to be initialized")) ;

  double u;
  double v;
  if(px_int != 1 && py_int != 1)
  {
    u = (double)I.getWidth()/(2*px_int);
    v = (double)I.getHeight()/(2*py_int);
  }
  else
  {
    u = (double)I.getWidth()/(vpMath::minimum(I.getWidth(),I.getHeight()));
    v = (double)I.getHeight()/(vpMath::minimum(I.getWidth(),I.getHeight()));
  }

  float o44c[4][4],x,y,z;
  Matrix id = IDENTITY_MATRIX;

  vp2jlc_matrix(cMobject.inverse(),o44c);

  add_vwstack ("start","cop", o44c[3][0],o44c[3][1],o44c[3][2]);
  x = o44c[2][0] + o44c[3][0];
  y = o44c[2][1] + o44c[3][1];
  z = o44c[2][2] + o44c[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", o44c[2][0],o44c[2][1],o44c[2][2]);
  add_vwstack ("start","vup", o44c[1][0],o44c[1][1],o44c[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);

  display_scene(id,object,I, curColor);
}

/*!
*************************************************
*/
void
vpWireFrameSimulator::projectObjectInternal(vpImage<unsigned char> &I, Bound_scene &object, vpHomogeneousMatrix cMobject)
{
  if (!sceneInitialized)
    throw(vpException(vpSimulatorException::notInitializedError,"The scene has to be initialized")) ;

  double u;
  double v;
  if(px_int != 1 && py_int != 1)
  {
    u = (double)I.getWidth()/(2*px_int);
    v = (double)I.getHeight()/(2*py_int);
  }
  else
  {
    u = (double)I.getWidth()/(vpMath::minimum(I.getWidth(),I.getHeight()));
    v = (double)I.getHeight()/(vpMath::minimum(I.getWidth(),I.getHeight()));
  }

  float o44c[4][4],x,y,z;
  Matrix id = IDENTITY_MATRIX;

  vp2jlc_matrix(cMobject.inverse(),o44c);

  add_vwstack ("start","cop", o44c[3][0],o44c[3][1],o44c[3][2]);
  x = o44c[2][0] + o44c[3][0];
  y = o44c[2][1] + o44c[3][1];
  z = o44c[2][2] + o44c[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", o44c[2][0],o44c[2][1],o44c[2][2]);
  add_vwstack ("start","vup", o44c[1][0],o44c[1][1],o44c[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);

  display_scene(id,object,I, curColor);
}


/*!
*************************************************
*/
void
vpWireFrameSimulator::projectObjectExternal(vpImage<vpRGBa> &I, Bound_scene &object, vpHomogeneousMatrix fMobject, vpHomogeneousMatrix camMf)
{
  float w44o[4][4],w44cext[4][4],x,y,z;

  vpHomogeneousMatrix camMft = rotz * camMf;
  
  double u;
  double v;
  if(px_ext != 1 && py_ext != 1)
  {
    u = (double)I.getWidth()/(2*px_ext);
    v = (double)I.getHeight()/(2*py_ext);
  }
  else
  {
    u = (double)I.getWidth()/(vpMath::minimum(I.getWidth(),I.getHeight()));
    v = (double)I.getHeight()/(vpMath::minimum(I.getWidth(),I.getHeight()));
  }

  vp2jlc_matrix(camMft.inverse(),w44cext);
  vp2jlc_matrix(fMobject,w44o);

  add_vwstack ("start","cop", w44cext[3][0],w44cext[3][1],w44cext[3][2]);
  x = w44cext[2][0] + w44cext[3][0];
  y = w44cext[2][1] + w44cext[3][1];
  z = w44cext[2][2] + w44cext[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", w44cext[2][0],w44cext[2][1],w44cext[2][2]);
  add_vwstack ("start","vup", w44cext[1][0],w44cext[1][1],w44cext[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);
  
  if (displayObject)
    display_scene(w44o,object,I, curColor);
}

/*!
*************************************************
*/
void
vpWireFrameSimulator::projectObjectExternal(vpImage<unsigned char> &I, Bound_scene &object, vpHomogeneousMatrix fMobject, vpHomogeneousMatrix camMf)
{
  float w44o[4][4],w44cext[4][4],x,y,z;

  vpHomogeneousMatrix camMft = rotz * camMf;
  
  double u;
  double v;
  if(px_ext != 1 && py_ext != 1)
  {
    u = (double)I.getWidth()/(2*px_ext);
    v = (double)I.getHeight()/(2*py_ext);
  }
  else
  {
    u = (double)I.getWidth()/(vpMath::minimum(I.getWidth(),I.getHeight()));
    v = (double)I.getHeight()/(vpMath::minimum(I.getWidth(),I.getHeight()));
  }

  vp2jlc_matrix(camMft.inverse(),w44cext);
  vp2jlc_matrix(fMobject,w44o);

  add_vwstack ("start","cop", w44cext[3][0],w44cext[3][1],w44cext[3][2]);
  x = w44cext[2][0] + w44cext[3][0];
  y = w44cext[2][1] + w44cext[3][1];
  z = w44cext[2][2] + w44cext[3][2];
  add_vwstack ("start","vrp", x,y,z);
  add_vwstack ("start","vpn", w44cext[2][0],w44cext[2][1],w44cext[2][2]);
  add_vwstack ("start","vup", w44cext[1][0],w44cext[1][1],w44cext[1][2]);
  add_vwstack ("start","window", -u, u, -v, v);
  
  if (displayObject)
    display_scene(w44o,object,I, curColor);
}

