#include "TMRNativeTopology.h"
#include <stdio.h>

/*
  Create a vertex from a point
*/
TMRVertexFromPoint::TMRVertexFromPoint( TMRPoint p ){
  pt = p;
}

/*
  Read out the point
*/
int TMRVertexFromPoint::evalPoint( TMRPoint *p ){
  *p = pt;
  return 0;
}

/*
  Create a vertex from curve
*/
TMRVertexFromEdge::TMRVertexFromEdge( TMREdge *_edge, 
                                      double _t ){
  t = _t;
  edge = _edge;
  edge->incref();
  setAttribute(edge->getAttribute());
}

/*
  Evaluate the vertex based on a node location
*/
TMRVertexFromEdge::TMRVertexFromEdge( TMREdge *_edge,
                                      TMRPoint p ){
  edge = _edge;
  edge->incref();
  setAttribute(edge->getAttribute());
  
  // Determine the parametric location of p using the initial
  // position
  edge->invEvalPoint(p, &t);
}

/*
  Free the object
*/
TMRVertexFromEdge::~TMRVertexFromEdge(){
  edge->decref();
}

/*
  Evaluate the point
*/
int TMRVertexFromEdge::evalPoint( TMRPoint *p ){
  return edge->evalPoint(t, p);
}

/*
  Retrieve the underlying curve object
*/
TMREdge* TMRVertexFromEdge::getEdge(){
  return edge;
}

/*
  Get the underlying parametric point
*/
int TMRVertexFromEdge::getParamOnEdge( TMREdge *_edge, 
                                       double *_t ){
  int fail = 0;
  if (edge == _edge){
    *_t = t;
    return fail;
  }
  return TMRVertex::getParamOnEdge(_edge, _t);
}

/*
  Get the underlying parametric point (if any)
*/
int TMRVertexFromEdge::getParamsOnFace( TMRFace *face,
                                        double *u, double *v ){
  return edge->getParamsOnFace(face, t, 1, u, v);
}

/*
  Determine the vertex location based on a surface location
*/
TMRVertexFromFace::TMRVertexFromFace( TMRFace *_face, 
                                      double _u, double _v ){
  face = _face;
  face->incref();
  setAttribute(face->getAttribute());
  u = _u;
  v = _v;
}

/*
  First determine the parametric vertex locations by projecting
  the point onto the surface.
*/
TMRVertexFromFace::TMRVertexFromFace( TMRFace *_face, 
                                      TMRPoint p ){
  face = _face;
  face->incref();
  setAttribute(face->getAttribute());
  face->invEvalPoint(p, &u, &v);
}

/*
  Free the data
*/
TMRVertexFromFace::~TMRVertexFromFace(){
  face->decref();
}

/*
  Evaluate the point on the surface
*/
int TMRVertexFromFace::evalPoint( TMRPoint *p ){
  return face->evalPoint(u, v, p);
}

/*
  Get the underlying parametric point (if any)
*/
int TMRVertexFromFace::getParamsOnFace( TMRFace *_face,
                                        double *_u, double *_v ){
  if (face == _face){
    *_u = u;
    *_v = v;
    return 1;
  }
  return TMRVertex::getParamsOnFace(face, _u, _v);
}

/*
  Create the curve parametrized on the surface
*/
TMREdgeFromFace::TMREdgeFromFace( TMRFace *_face, 
                                  TMRPcurve *_pcurve ){
  face = _face;
  face->incref();
  setAttribute(face->getAttribute());
  pcurve = _pcurve;
  pcurve->incref();
}

/*
  Destroy the curve
*/
TMREdgeFromFace::~TMREdgeFromFace(){
  face->decref();
  pcurve->decref();
}

/*
  Get the parameter range for this curve
*/
void TMREdgeFromFace::getRange( double *tmin, double *tmax ){
  pcurve->getRange(tmin, tmax);
}  
  
/*
  Given the parametric point, evaluate the x,y,z location
*/
int TMREdgeFromFace::evalPoint( double t, TMRPoint *X ){
  double u, v;
  int fail = pcurve->evalPoint(t, &u, &v);
  fail = fail || face->evalPoint(u, v, X);
  return fail;
}

/*
  Parametrize the curve on the given surface
*/
int TMREdgeFromFace::getParamsOnFace( TMRFace *surf, 
                                      double t, int dir, 
                                      double *u, double *v ){
  if (surf == face){
    return pcurve->evalPoint(t, u, v);
  }

  TMRPoint p;
  int fail = evalPoint(t, &p);
  fail = fail || surf->invEvalPoint(p, u, v);
  return fail;
}

/*
  Given the point, find the parametric location
*/
int TMREdgeFromFace::invEvalPoint( TMRPoint X, double *t ){
  *t = 0.0;
  int fail = 1;
  return fail;
}

/*
  Given the parametric point, evaluate the derivative 
*/
int TMREdgeFromFace::evalDeriv( double t, TMRPoint *Xt ){
  double u, v, ut, vt;
  pcurve->evalPoint(t, &u, &v);
  pcurve->evalDeriv(t, &ut, &vt);
  TMRPoint Xu, Xv;
  face->evalDeriv(u, v, &Xu, &Xv);
  Xt->x = ut*Xu.x + vt*Xv.x;
  Xt->y = ut*Xu.y + vt*Xv.y;
  Xt->z = ut*Xu.z + vt*Xv.z;
}

/*
  Split/segment the curve 
*/
TMRSplitEdge::TMRSplitEdge( TMREdge *_edge, 
                            double _t1, double _t2 ){
  edge = _edge;
  edge->incref();
  setAttribute(edge->getAttribute());

  // Set the parameter values
  t1 = _t1;
  t2 = _t2;

  // Check the range
  double tmin, tmax;
  edge->getRange(&tmin, &tmax);
  if (t1 < tmin){ t1 = tmin; }
  else if (t1 > tmax){ t1 = tmax; }
  if (t2 > tmax){ t2 = tmax; }
  else if (t2 < tmin){ t2 = tmin; }
}

/*
  Split the curve to the nearest points
*/
TMRSplitEdge::TMRSplitEdge( TMREdge *_edge, 
                            TMRPoint *p1, TMRPoint *p2 ){
  edge = _edge;
  edge->incref();
  setAttribute(edge->getAttribute());

  // Perform the inverse evaluation
  edge->invEvalPoint(*p1, &t1);
  edge->invEvalPoint(*p2, &t2);

  // Check the range
  double tmin, tmax;
  edge->getRange(&tmin, &tmax);
  if (t1 < tmin){ t1 = tmin; }
  else if (t1 > tmax){ t1 = tmax; }
  if (t2 > tmax){ t2 = tmax; }
  else if (t2 < tmin){ t2 = tmin; }
}

/*
  Split the curve and check if the two vertices are evalauted from
  this curve using a parametric location. Otherwise do the same as
  before.
*/
TMRSplitEdge::TMRSplitEdge( TMREdge *_edge, 
                            TMRVertex *v1, TMRVertex *v2 ){
  edge = _edge;
  edge->incref();
  setAttribute(edge->getAttribute());

  // Get the parameters for this curve for the point v1/v2
  v1->getParamOnEdge(edge, &t1);
  v2->getParamOnEdge(edge, &t2);

  // Set the vertices for this curve
  setVertices(v1, v2);

  // Check the range
  double tmin, tmax;
  edge->getRange(&tmin, &tmax);
  if (t1 < tmin){ t1 = tmin; }
  else if (t1 > tmax){ t1 = tmax; }
  if (t2 > tmax){ t2 = tmax; }
  else if (t2 < tmin){ t2 = tmin; } 
}

/*
  Decrease the reference count
*/
TMRSplitEdge::TMRSplitEdge(){
  edge->decref();
}

/*
  Get the parameter range
*/
void TMRSplitEdge::getRange( double *tmin, double *tmax ){
  *tmin = 0.0;
  *tmax = 1.0;
}

/*
  Evaluate the point
*/
int TMRSplitEdge::evalPoint( double t, TMRPoint *X ){
  int fail = 1;
  if (t < 0.0){ return fail; }
  if (t > 1.0){ return fail; }
  t = (1.0 - t)*t1 + t*t2;
  return edge->evalPoint(t, X);
}

/*
  Get the parameter on the split curve
*/
int TMRSplitEdge::getParamsOnFace( TMRFace *face, double t, 
                                   int dir, double *u, double *v ){
  int fail = 1;
  if (t < 0.0){ return fail; }
  if (t > 1.0){ return fail; }
  t = (1.0 - t)*t1 + t*t2;
  return edge->getParamsOnFace(face, t, dir, u, v);
}

/*
  Create a parametric TFI
  
  The transfinite interpolation is performed in the parameter space
  and all points are obtained directly from the surface object
  itself.
*/
TMRParametricTFIFace::TMRParametricTFIFace( TMRFace *_face, 
                                            TMREdge *_edges[], 
                                            const int _dir[],
                                            TMRVertex *verts[] ){
  face = _face;
  face->incref();
  setAttribute(face->getAttribute());

  for ( int k = 0; k < 4; k++ ){
    // Retrieve the parametric curves on the surface
    edges[k] = _edges[k];
    edges[k]->incref();
    dir[k] = _dir[k];
  
    double tmin = 0.0, tmax = 0.0;
    edges[k]->getRange(&tmin, &tmax);
    if (tmin != 0.0 || tmax != 1.0){
      fprintf(stderr, 
              "TMRParametricTFIFace error: All edges must have t in [0, 1]\n");
    }

    // Reparametrize the vertices on the surface
    if (dir[k] > 0){
      edges[k]->getParamsOnFace(face, 0.0, dir[k],
                                &vupt[k], &vvpt[k]);
    }
    else {
      edges[k]->getParamsOnFace(face, 1.0, dir[k],
                                &vupt[k], &vvpt[k]);
    }
  }

  // Set the vertices for this surface
  if (dir[0] > 0){
    edges[0]->setVertices(verts[0], verts[1]); 
  }
  else {
    edges[0]->setVertices(verts[1], verts[0]);
  }

  if (dir[1] > 0){  
    edges[1]->setVertices(verts[1], verts[2]);
  }
  else {
    edges[1]->setVertices(verts[2], verts[1]);
  }

  if (dir[2] > 0){
    edges[2]->setVertices(verts[2], verts[3]);
  }
  else {
    edges[2]->setVertices(verts[3], verts[2]);
  }

  if (dir[3] > 0){
    edges[3]->setVertices(verts[3], verts[0]);
  }
  else {
    edges[3]->setVertices(verts[0], verts[3]);
  }

  // Set the curve segment into the curve
  TMREdgeLoop *loop = new TMREdgeLoop(4, edges, dir);
  addEdgeLoop(loop);
}

/*
  Destroy the parametric TFI object
*/  
TMRParametricTFIFace::~TMRParametricTFIFace(){
  face->decref();
  for ( int k = 0; k < 4; k++ ){
    edges[k]->decref();
  }
}

/*
  The range must always be between [0,1] for all curves
*/
void TMRParametricTFIFace::getRange( double *umin, double *vmin,
                                     double *umax, double *vmax ){
  *umin = 0.0;
  *vmin = 0.0;
  *umax = 1.0;
  *vmax = 1.0;
}

/*
  Evaluate the surface at the specified parametric point (u,v)

  This code uses a transfinite interpolation to obtain the 
  parametric surface coordinates (us(u,v), vs(u,v)) in terms 
  of the TFI parameter coordinates (u,v)
*/
int TMRParametricTFIFace::evalPoint( double u, double v, 
                                     TMRPoint *X ){
  // Evaluate the curves along the v-direction
  int fail = 0;
  double cupt[4], cvpt[4];
  double params[4] = {u, v, 1.0-u, 1.0-v};

  for ( int k = 0; k < 4; k++ ){
    if (dir[k] > 0){
      fail = fail || 
        edges[k]->getParamsOnFace(face, params[k], dir[k],
                                  &cupt[k], &cvpt[k]);
    }
    else {
      fail = fail || 
        edges[k]->getParamsOnFace(face, 1.0-params[k], dir[k],
                                  &cupt[k], &cvpt[k]);
    }
  }
    
  // Compute the parametric coordinates
  double us, vs;
  us = (1.0-u)*cupt[3] + u*cupt[1] + (1.0-v)*cupt[0] + v*cupt[2]
    - ((1.0-u)*(1.0-v)*vupt[0] + u*(1.0-v)*vupt[1] + 
       u*v*vupt[2] + v*(1.0-u)*vupt[3]);

  vs = (1.0-u)*cvpt[3] + u*cvpt[1] + (1.0-v)*cvpt[0] + v*cvpt[2]
    - ((1.0-u)*(1.0-v)*vvpt[0] + u*(1.0-v)*vvpt[1] + 
       u*v*vvpt[2] + v*(1.0-u)*vvpt[3]);

  fail = fail || face->evalPoint(us, vs, X);
  return fail;
} 

/*  
  Inverse evaluation: This is not yet implemented
*/
int TMRParametricTFIFace::invEvalPoint( TMRPoint p, 
                                        double *u, double *v ){
  *u = 0.0; 
  *v = 0.0;
  int fail = 1;
  return fail;
}

/*
  Derivative evaluation: This is not yet implemented
*/
int TMRParametricTFIFace::evalDeriv( double u, double v, 
                                     TMRPoint *Xu, TMRPoint *Xv ){

  // Evaluate the curves along the v-direction
  int fail = 1;
  Xu->zero();
  Xv->zero();

  return fail;
}

/*
  Create the TFI volume
*/
TMRTFIVolume::TMRTFIVolume( TMRFace *_faces[], const int _orient[],
                            TMREdge *_edges[], const int _dir[],
                            TMRVertex *_verts[] ):
TMRVolume(6, faces, _orient){
  for ( int i = 0; i < 6; i++ ){
    faces[i] = _faces[i];
    faces[i]->incref();
    orient[i] = _orient[i];
  }
  for ( int i = 0; i < 12; i++ ){
    edges[i] = _edges[i];
    edges[i]->incref();
    edge_dir[i] = _dir[i];
  }
  for ( int i = 0; i < 8; i++ ){
    verts[i] = _verts[i];
    verts[i]->incref();
    verts[i]->evalPoint(&c[i]);
  }
}

/*
  Destroy the TFI volume object
*/
TMRTFIVolume::~TMRTFIVolume(){
  for ( int i = 0; i < 6; i++ ){
    faces[i]->decref();
  }
  for ( int i = 0; i < 12; i++ ){
    edges[i]->decref();
  }
  for ( int i = 0; i < 8; i++ ){
    verts[i]->decref();
  }
}

/*
  Evaluate the point within the volume
*/
int TMRTFIVolume::evalPoint( double u, double v, double w,
                             TMRPoint *X ){
  int fail = 0;
  TMRPoint f[6], e[12];

  // Evaluate/retrieve the points on the surfaces
  for ( int k = 0; k < 6; k++ ){
    double x, y;
    if (k < 2){
      x = v;
      y = w;
    }
    else if (k < 4){
      x = u;
      y = w;
    }
    else {
      x = u;
      y = v;
    }

    // Check the orientation of the face relative to the volume
    if (orient[k] == 0){
      fail = fail || faces[k]->evalPoint(x, y, &f[k]);
    }
    else if (orient[k] == 1){
      fail = fail || faces[k]->evalPoint(1.0-x, y, &f[k]);
    }
    else if (orient[k] == 2){
      fail = fail || faces[k]->evalPoint(x, 1.0-y, &f[k]);
    }
    else if (orient[k] == 3){
      fail = fail || faces[k]->evalPoint(1.0-x, 1.0-y, &f[k]);
    }
    else {
      fail = 1;
    }
  }

  // Evaluate/retrieve the points on the edges
  for ( int k = 0; k < 12; k++ ){
    double t;
    if (k < 4){
      t = u;
    }
    else if (k < 8){
      t = v;
    }
    else {
      t = w;
    }

    if (edge_dir[k] > 0){
      fail = fail || edges[k]->evalPoint(t, &e[k]);
    }
    else {
      fail = fail || edges[k]->evalPoint(1.0-t, &e[k]);
    }
  }

  // Evaluate the point based on the values along the corners, edges
  // and faces
  X->x = 
    (1.0-u)*f[0].x + u*f[1].x + (1.0-v)*f[2].x + 
    v*f[3].x + (1.0-w)*f[4].x + w*f[5].x 
    - ((1.0-v)*(1.0-w)*e[0].x + v*(1.0-w)*e[1].x + 
       (1.0-v)*w*e[2].x + v*w*e[3].x +
       (1.0-u)*(1.0-w)*e[4].x + u*(1.0-w)*e[5].x + 
       (1.0-u)*w*e[6].x + u*w*e[7].x +
       (1.0-u)*(1.0-v)*e[8].x + u*(1.0-v)*e[9].x + 
       (1.0-u)*v*e[10].x + u*v*e[11].x)
    + ((1.0-u)*(1.0-v)*(1.0-w)*c[0].x + u*(1.0-v)*(1.0-w)*c[1].x + 
       (1.0-u)*v*(1.0-w)*c[2].x + u*v*(1.0-w)*c[3].x + 
       (1.0-u)*(1.0-v)*w*c[4].x + u*(1.0-v)*w*c[5].x + 
       (1.0-u)*v*w*c[6].x + u*v*w*c[7].x);

  X->y = 
    (1.0-u)*f[0].y + u*f[1].y + (1.0-v)*f[2].y + 
    v*f[3].y + (1.0-w)*f[4].y + w*f[5].y 
    - ((1.0-v)*(1.0-w)*e[0].y + v*(1.0-w)*e[1].y + 
       (1.0-v)*w*e[2].y + v*w*e[3].y +
       (1.0-u)*(1.0-w)*e[4].y + u*(1.0-w)*e[5].y + 
       (1.0-u)*w*e[6].y + u*w*e[7].y +
       (1.0-u)*(1.0-v)*e[8].y + u*(1.0-v)*e[9].y + 
       (1.0-u)*v*e[10].y + u*v*e[11].y)
    + ((1.0-u)*(1.0-v)*(1.0-w)*c[0].y + u*(1.0-v)*(1.0-w)*c[1].y + 
       (1.0-u)*v*(1.0-w)*c[2].y + u*v*(1.0-w)*c[3].y + 
       (1.0-u)*(1.0-v)*w*c[4].y + u*(1.0-v)*w*c[5].y + 
       (1.0-u)*v*w*c[6].y + u*v*w*c[7].y);

  X->z = 
    (1.0-u)*f[0].z + u*f[1].z + (1.0-v)*f[2].z + 
    v*f[3].z + (1.0-w)*f[4].z + w*f[5].z 
    - ((1.0-v)*(1.0-w)*e[0].z + v*(1.0-w)*e[1].z + 
       (1.0-v)*w*e[2].z + v*w*e[3].z +
       (1.0-u)*(1.0-w)*e[4].z + u*(1.0-w)*e[5].z + 
       (1.0-u)*w*e[6].z + u*w*e[7].z +
       (1.0-u)*(1.0-v)*e[8].z + u*(1.0-v)*e[9].z + 
       (1.0-u)*v*e[10].z + u*v*e[11].z)
    + ((1.0-u)*(1.0-v)*(1.0-w)*c[0].z + u*(1.0-v)*(1.0-w)*c[1].z + 
       (1.0-u)*v*(1.0-w)*c[2].z + u*v*(1.0-w)*c[3].z + 
       (1.0-u)*(1.0-v)*w*c[4].z + u*(1.0-v)*w*c[5].z + 
       (1.0-u)*v*w*c[6].z + u*v*w*c[7].z);

  return 1;
}

/*
  Get the underlying face, edge and volume entities
*/
void TMRTFIVolume::getEntities( TMRFace ***_faces, 
                                TMREdge ***_edges, 
                                TMRVertex ***_verts ){
  if (_faces){ *_faces = faces; }
  if (_edges){ *_edges = edges; }
  if (_verts){ *_verts = verts; }
}