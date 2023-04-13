/*
  This file is part of the package TMR for adaptive mesh refinement.

  Copyright (C) 2015 Georgia Tech Research Corporation.
  Additional copyright (C) 2015 Graeme Kennedy.
  All rights reserved.

  TMR is licensed under the Apache License, Version 2.0 (the "License");
  you may not use this software except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "TMRTopology.h"

#include <math.h>
#include <stdio.h>

#include "TMRMesh.h"
#include "TMRNativeTopology.h"

TMR_EXTERN_C_BEGIN
#include "metis.h"
TMR_EXTERN_C_END

/*
  Create the vertex
*/
TMRVertex::TMRVertex() {
  copy = NULL;
  var = -1;
}

TMRVertex::~TMRVertex() {
  if (copy) {
    copy->decref();
  }
}

/*
  Perform an inverse evaluation by obtaining the underlying
  parametrization on the specified curve
*/
int TMRVertex::getParamOnEdge(TMREdge *edge, double *t) {
  TMRPoint p;
  int fail = evalPoint(&p);
  fail = fail || edge->invEvalPoint(p, t);
  return fail;
}

/*
  Same thing, except on the specified surface
*/
int TMRVertex::getParamsOnFace(TMRFace *face, double *u, double *v) {
  TMRPoint p;
  int fail = evalPoint(&p);
  fail = fail || face->invEvalPoint(p, u, v);
  return fail;
}

/*
  Set the vertex to copy from
*/
void TMRVertex::setCopySource(TMRVertex *vert) {
  if (vert && vert != this) {
    vert->incref();
    if (copy) {
      copy->decref();
    }
    copy = vert;
  }
}

/*
  Retrieve the source vertex
*/
void TMRVertex::getCopySource(TMRVertex **vert) { *vert = copy; }

/*
  Reset the node number to -1
*/
void TMRVertex::resetNodeNum() { var = -1; }

/*
  Set/retrieve vertex numbers
*/
int TMRVertex::setNodeNum(int *num) {
  int start = *num;
  if (var == -1) {
    if (copy) {
      copy->setNodeNum(num);
      var = copy->var;
    } else {
      var = *num;
      (*num)++;
    }
  }
  return *num - start;
  ;
}

/*
  Retrieve the vertex number
*/
int TMRVertex::getNodeNum(int *num) {
  if (var != -1) {
    *num = var;
    return 1;
  }
  return 0;
}

/*
  Set the original vertices
*/
TMREdge::TMREdge() {
  v1 = v2 = NULL;
  mesh = NULL;
  source = NULL;
  copy = NULL;
}

/*
  Free the edge
*/
TMREdge::~TMREdge() {
  if (v1) {
    v1->decref();
  }
  if (v2) {
    v2->decref();
  }
  if (source) {
    source->decref();
  }
  if (copy) {
    copy->decref();
  }
}

/*
  Perform the inverse evaluation
*/
int TMREdge::invEvalPoint(TMRPoint p, double *t) {
  *t = 0.0;
  int fail = 1;
  return fail;
}

/*
  Set the step size for the derivative
*/
double TMREdge::deriv_step_size = 1e-6;

/*
  Evaluate the derivative using a finite-difference step size
*/
int TMREdge::evalDeriv(double t, TMRPoint *X, TMRPoint *Xt) {
  int fail = 1;

  // Retrieve the parameter bounds for the curve
  double tmin, tmax;
  getRange(&tmin, &tmax);

  if (t >= tmin && t <= tmax) {
    // Evaluate the point at the original
    fail = evalPoint(t, X);
    if (fail) {
      return fail;
    }

    // Compute the approximate derivative using a forward difference
    if (t + deriv_step_size <= tmax) {
      TMRPoint p2;
      fail = evalPoint(t + deriv_step_size, &p2);
      if (fail) {
        return fail;
      }

      Xt->x = (p2.x - X->x) / deriv_step_size;
      Xt->y = (p2.y - X->y) / deriv_step_size;
      Xt->z = (p2.z - X->z) / deriv_step_size;
    } else if (t >= tmin + deriv_step_size) {
      TMRPoint p2;
      fail = evalPoint(t - deriv_step_size, &p2);
      if (fail) {
        return fail;
      }

      Xt->x = (X->x - p2.x) / deriv_step_size;
      Xt->y = (X->y - p2.y) / deriv_step_size;
      Xt->z = (X->z - p2.z) / deriv_step_size;
    }
  }

  return fail;
}

/*
  Evaluate the second derivative using a finite-difference step size
*/
int TMREdge::eval2ndDeriv(double t, TMRPoint *X, TMRPoint *Xt, TMRPoint *Xtt) {
  int fail = 1;

  // Retrieve the parameter bounds for the curve
  double tmin, tmax;
  getRange(&tmin, &tmax);

  if (t >= tmin && t <= tmax) {
    // Evaluate the point at the original
    fail = evalDeriv(t, X, Xt);
    if (fail) {
      return fail;
    }

    // Compute the approximate derivative using a forward
    // difference
    if (t + deriv_step_size <= tmax) {
      TMRPoint p, p2;
      fail = evalDeriv(t + deriv_step_size, &p, &p2);
      if (fail) {
        return fail;
      }

      Xtt->x = (p2.x - Xt->x) / deriv_step_size;
      Xtt->y = (p2.y - Xt->y) / deriv_step_size;
      Xtt->z = (p2.z - Xt->z) / deriv_step_size;
    } else if (t >= tmin + deriv_step_size) {
      TMRPoint p, p2;
      fail = evalDeriv(t - deriv_step_size, &p, &p2);
      if (fail) {
        return fail;
      }

      Xtt->x = (Xt->x - p2.x) / deriv_step_size;
      Xtt->y = (Xt->y - p2.y) / deriv_step_size;
      Xtt->z = (Xt->z - p2.z) / deriv_step_size;
    }
  }

  return fail;
}

/*
  Find the point on the surface closest to the point C(t)
*/
int TMREdge::getParamsOnFace(TMRFace *face, double t, int dir, double *u,
                             double *v) {
  TMRPoint p;
  int fail = evalPoint(t, &p);
  fail = fail || face->invEvalPoint(p, u, v);
  return fail;
}

/*
  Set the adjacent vertices
*/
void TMREdge::setVertices(TMRVertex *_v1, TMRVertex *_v2) {
  _v1->incref();
  _v2->incref();
  if (v1) {
    v1->decref();
  }
  if (v2) {
    v2->decref();
  }
  v1 = _v1;
  v2 = _v2;
}

/*
  Retrieve the adjacent vertices
*/
void TMREdge::getVertices(TMRVertex **_v1, TMRVertex **_v2) {
  if (_v1) {
    *_v1 = v1;
  }
  if (_v2) {
    *_v2 = v2;
  }
}

/*
  Set the mesh into the array
*/
void TMREdge::setMesh(TMREdgeMesh *_mesh) { mesh = _mesh; }

/*
  Retrieve the mesh pointer
*/
void TMREdge::getMesh(TMREdgeMesh **_mesh) { *_mesh = mesh; }

/*
  Set the source edge
*/
void TMREdge::setSource(TMREdge *edge) {
  if (edge && edge != this && !copy) {
    edge->incref();
    if (source) {
      source->decref();
    }
    source = edge;
  }
}

/*
  Retrieve the source edge
*/
void TMREdge::getSource(TMREdge **edge) {
  if (edge) {
    *edge = source;
  }
}

/*
  Set the source edge
*/
void TMREdge::setCopySource(TMREdge *edge) {
  if (edge && edge != this && !source) {
    edge->incref();
    if (copy) {
      copy->decref();
    }
    copy = edge;
  }
}

/*
  Retrieve the copy edge
*/
void TMREdge::getCopySource(TMREdge **edge) { *edge = copy; }

/*
  Write out a representation of the curve to a VTK file
*/
void TMREdge::writeToVTK(const char *filename) {
  double t1, t2;
  getRange(&t1, &t2);

  const int npts = 100;

  // Write out the vtk file
  FILE *fp = fopen(filename, "w");
  if (fp) {
    fprintf(fp, "# vtk DataFile Version 3.0\n");
    fprintf(fp, "vtk output\nASCII\n");
    fprintf(fp, "DATASET UNSTRUCTURED_GRID\n");

    // Write out the points
    fprintf(fp, "POINTS %d float\n", npts);
    for (int k = 0; k < npts; k++) {
      double u = 1.0 * k / (npts - 1);
      double t = (1.0 - u) * t1 + u * t2;

      // Evaluate the point
      TMRPoint p;
      evalPoint(t, &p);

      // Write out the point
      fprintf(fp, "%e %e %e\n", p.x, p.y, p.z);
    }

    // Write out the cell values
    fprintf(fp, "\nCELLS %d %d\n", npts - 1, 3 * (npts - 1));
    for (int k = 0; k < npts - 1; k++) {
      fprintf(fp, "2 %d %d\n", k, k + 1);
    }

    // Write out the cell types
    fprintf(fp, "\nCELL_TYPES %d\n", npts - 1);
    for (int k = 0; k < npts - 1; k++) {
      fprintf(fp, "%d\n", 3);
    }

    fclose(fp);
  }
}

/*
  Create the edge loop object
*/
TMREdgeLoop::TMREdgeLoop(int _nedges, TMREdge *_edges[], const int _dir[]) {
  int fail = 0;
  if (_nedges == 0) {
    fail = 1;
    fprintf(stderr, "TMREdgeLoop error: Zero length segment\n");
  }

  // First, check whether the loop is closed
  TMRVertex *vinit = NULL;
  TMRVertex *vnext;
  for (int i = 0; i < _nedges; i++) {
    TMRVertex *v1, *v2;
    if (_dir[i] > 0) {
      _edges[i]->getVertices(&v1, &v2);
    } else {
      _edges[i]->getVertices(&v2, &v1);
    }
    if (i == 0) {
      vinit = v1;
    }
    vnext = v2;
    if (i == _nedges - 1) {
      if (vinit != vnext) {
        fprintf(stderr, "TMREdgeLoop error: Edge loop must be closed\n");
        fail = 1;
      }
    }
  }

  // Return if the segment is not closed
  if (fail) {
    nedges = 0;
    edges = NULL;
    dir = NULL;
    return;
  }

  nedges = _nedges;
  edges = new TMREdge *[nedges];
  dir = new int[nedges];
  for (int k = 0; k < nedges; k++) {
    edges[k] = _edges[k];
    edges[k]->incref();
    dir[k] = _dir[k];
  }
}

/*
  Free the data from the edge loop object
*/
TMREdgeLoop::~TMREdgeLoop() {
  for (int k = 0; k < nedges; k++) {
    edges[k]->decref();
  }
  delete[] edges;
  delete[] dir;
}

/*
  Retrieve the edge loop edges
*/
void TMREdgeLoop::getEdgeLoop(int *_nedges, TMREdge **_edges[],
                              const int *_dir[]) {
  if (_nedges) {
    *_nedges = nedges;
  }
  if (_edges) {
    *_edges = edges;
  }
  if (_dir) {
    *_dir = dir;
  }
}

/*
  Initialize data within the TMRSurface object
*/
TMRFace::TMRFace(int _orientation) {
  orientation = _orientation;
  max_num_loops = 0;
  num_loops = 0;
  loops = NULL;
  loop_dirs = NULL;
  mesh = NULL;
  source = NULL;
  source_volume = NULL;
  copy = NULL;
  copy_orient = 0;
}

/*
  Deallocate the curve segments (if any)
*/
TMRFace::~TMRFace() {
  if (loops) {
    for (int i = 0; i < num_loops; i++) {
      loops[i]->decref();
    }
    delete[] loop_dirs;
    delete[] loops;
  }
  if (source) {
    source->decref();
  }
}

/*
  Set the step size for the derivative
*/
double TMRFace::deriv_step_size = 1e-6;

/*
  Get the flag indicating the relative orientation of the parametric
  space and the underlying surface normal.

  normal_orient > 0 indicates that the parameter space and the surface
  normal are consistent. normal_orient < 0 indicates that the surface
  normal is flipped.
*/
int TMRFace::getOrientation() { return orientation; }

/*
  Evaluate the derivative using a finite-difference step size
*/
int TMRFace::evalDeriv(double u, double v, TMRPoint *X, TMRPoint *Xu,
                       TMRPoint *Xv) {
  int fail = 0;

  // Retrieve the parameter bounds for the curve
  double umin, vmin, umax, vmax;
  getRange(&umin, &vmin, &umax, &vmax);

  if (u >= umin && u <= umax && v >= vmin && v <= vmax) {
    // Evaluate the point at the original
    fail = evalPoint(u, v, X);

    // Compute the approximate derivative using a forward
    // difference or backward difference, depending on whether
    // the step is within the domain
    if (u + deriv_step_size <= umax) {
      TMRPoint p2;
      fail = fail || evalPoint(u + deriv_step_size, v, &p2);

      Xu->x = (p2.x - X->x) / deriv_step_size;
      Xu->y = (p2.y - X->y) / deriv_step_size;
      Xu->z = (p2.z - X->z) / deriv_step_size;
    } else if (u >= umin + deriv_step_size) {
      TMRPoint p2;
      fail = fail || evalPoint(u - deriv_step_size, v, &p2);

      Xu->x = (X->x - p2.x) / deriv_step_size;
      Xu->y = (X->y - p2.y) / deriv_step_size;
      Xu->z = (X->z - p2.z) / deriv_step_size;
    } else {
      fail = 1;
    }

    // Compute the approximate derivative using a forward
    // difference
    if (v + deriv_step_size <= vmax) {
      TMRPoint p2;
      fail = fail || evalPoint(u, v + deriv_step_size, &p2);

      Xv->x = (p2.x - X->x) / deriv_step_size;
      Xv->y = (p2.y - X->y) / deriv_step_size;
      Xv->z = (p2.z - X->z) / deriv_step_size;
    } else if (v >= vmin + deriv_step_size) {
      TMRPoint p2;
      fail = fail || evalPoint(u, v - deriv_step_size, &p2);

      Xv->x = (X->x - p2.x) / deriv_step_size;
      Xv->y = (X->y - p2.y) / deriv_step_size;
      Xv->z = (X->z - p2.z) / deriv_step_size;
    } else {
      fail = 1;
    }
  }

  return fail;
}

/*
  Evaluate the second derivative using a finite-difference step size
*/
int TMRFace::eval2ndDeriv(double u, double v, TMRPoint *X, TMRPoint *Xu,
                          TMRPoint *Xv, TMRPoint *Xuu, TMRPoint *Xuv,
                          TMRPoint *Xvv) {
  int fail = 0;

  // Retrieve the parameter bounds for the curve
  double umin, vmin, umax, vmax;
  getRange(&umin, &vmin, &umax, &vmax);

  if (u >= umin && u <= umax && v >= vmin && v <= vmax) {
    // Evaluate the point at the original
    fail = evalDeriv(u, v, X, Xu, Xv);

    // Compute the approximate derivative using a forward
    // difference or backward difference, depending on whether
    // the step is within the domain
    if (u + deriv_step_size <= umax) {
      TMRPoint p, p2u, p2v;
      fail = fail || evalDeriv(u + deriv_step_size, v, &p, &p2u, &p2v);

      Xuu->x = (p2u.x - Xu->x) / deriv_step_size;
      Xuu->y = (p2u.y - Xu->y) / deriv_step_size;
      Xuu->z = (p2u.z - Xu->z) / deriv_step_size;

      Xuv->x = (p2v.x - Xv->x) / deriv_step_size;
      Xuv->y = (p2v.y - Xv->y) / deriv_step_size;
      Xuv->z = (p2v.z - Xv->z) / deriv_step_size;
    } else if (u >= umin + deriv_step_size) {
      TMRPoint p, p2u, p2v;
      fail = fail || evalDeriv(u - deriv_step_size, v, &p, &p2u, &p2v);

      Xuu->x = (Xu->x - p2u.x) / deriv_step_size;
      Xuu->y = (Xu->y - p2u.y) / deriv_step_size;
      Xuu->z = (Xu->z - p2u.z) / deriv_step_size;

      Xuv->x = (Xv->x - p2v.x) / deriv_step_size;
      Xuv->y = (Xv->y - p2v.y) / deriv_step_size;
      Xuv->z = (Xv->z - p2v.z) / deriv_step_size;
    } else {
      fail = 1;
    }

    // Compute the approximate derivative using a forward
    // difference
    if (v + deriv_step_size <= vmax) {
      TMRPoint p, p2u, p2v;
      fail = fail || evalDeriv(u, v + deriv_step_size, &p, &p2u, &p2v);

      Xvv->x = (p2v.x - Xv->x) / deriv_step_size;
      Xvv->y = (p2v.y - Xv->y) / deriv_step_size;
      Xvv->z = (p2v.z - Xv->z) / deriv_step_size;
    } else if (v >= vmin + deriv_step_size) {
      TMRPoint p, p2u, p2v;
      fail = fail || evalDeriv(u, v - deriv_step_size, &p, &p2u, &p2v);

      Xvv->x = (Xv->x - p2v.x) / deriv_step_size;
      Xvv->y = (Xv->y - p2v.y) / deriv_step_size;
      Xvv->z = (Xv->z - p2v.z) / deriv_step_size;
    } else {
      fail = 1;
    }
  }

  return fail;
}

/*
  Perform the inverse evaluation
*/
int TMRFace::invEvalPoint(TMRPoint p, double *u, double *v) {
  *u = *v = 0.0;
  int fail = 1;
  return fail;
}

/*
  Add the curves that bound the surface
*/
void TMRFace::addEdgeLoop(int loop_dir, TMREdgeLoop *loop) {
  // Increase the reference count
  loop->incref();

  if (num_loops >= max_num_loops) {
    // Extend the loops array
    max_num_loops = (2 * num_loops > 10 ? 2 * num_loops : 10);

    // Allocate the new loops array
    TMREdgeLoop **lps = new TMREdgeLoop *[max_num_loops];
    int *lp_dirs = new int[max_num_loops];

    // Copy over any existing segments
    if (num_loops > 0) {
      memcpy(lps, loops, num_loops * sizeof(TMREdgeLoop *));
      memcpy(lp_dirs, loop_dirs, num_loops * sizeof(int));
      delete[] loops;
      delete[] loop_dirs;
    }
    loops = lps;
    loop_dirs = lp_dirs;
  }

  // Set the new segment array
  loops[num_loops] = loop;
  loop_dirs[num_loops] = loop_dir;
  num_loops++;
}

/*
  Get the number of closed segments
*/
int TMRFace::getNumEdgeLoops() { return num_loops; }

/*
  Retrieve the information from the given segment number
*/
int TMRFace::getEdgeLoop(int k, TMREdgeLoop **_loop) {
  *_loop = NULL;
  int loop_dir = 0;
  if (k >= 0 && k < num_loops) {
    if (_loop) {
      *_loop = loops[k];
    }
    loop_dir = loop_dirs[k];
  }
  return loop_dir;
}

/*
  Set the mesh into the array
*/
void TMRFace::setMesh(TMRFaceMesh *_mesh) { mesh = _mesh; }

/*
  Retrieve the mesh pointer
*/
void TMRFace::getMesh(TMRFaceMesh **_mesh) { *_mesh = mesh; }

/*
  Set the source face with a relative direction
*/
void TMRFace::setSource(TMRVolume *volume, TMRFace *face) {
  if (volume && face && face != this && !copy) {
    int nloops = getNumEdgeLoops();
    if (nloops != face->getNumEdgeLoops()) {
      fprintf(stderr,
              "TMRFace Error: Topology not equivalent. "
              "Number of loops not equal. Could not set source face\n");
      return;
    }

    int *loop_counts = new int[nloops];

    // Find the number of edges in each of the loops
    for (int i = 0; i < nloops; i++) {
      TMREdgeLoop *loop;
      face->getEdgeLoop(i, &loop);
      int nedges;
      loop->getEdgeLoop(&nedges, NULL, NULL);
      loop_counts[i] = nedges;
    }

    // Check that each loop corresponds to a loop edge count
    for (int i = 0; i < nloops; i++) {
      TMREdgeLoop *loop;
      getEdgeLoop(i, &loop);
      int nedges;
      loop->getEdgeLoop(&nedges, NULL, NULL);
      for (int j = 0; j < nloops; j++) {
        if (loop_counts[j] != -1 && loop_counts[j] == nedges) {
          loop_counts[j] = -1;
          break;
        }
      }
    }

    // Check if any loop is not accounted for
    int fail = 0;
    for (int i = 0; i < nloops; i++) {
      if (loop_counts[i] != -1) {
        fail = 1;
        break;
      }
    }

    delete[] loop_counts;

    if (fail) {
      fprintf(stderr,
              "TMRFace Error: Topology not equivalent. "
              "Number of edges in the edge loops do not match "
              "Could not set source face\n");
      return;
    }

    // Check that both the face and the source are contained with the
    // proposed source volume
    int this_index = -1, source_index = -1;
    int num_faces;
    TMRFace **faces;
    volume->getFaces(&num_faces, &faces);

    // Find the indices of this face and the source - if they exist
    for (int i = 0; i < num_faces; i++) {
      if (faces[i] == face) {
        source_index = i;
      }
      if (faces[i] == this) {
        this_index = i;
      }
    }

    if (this_index >= 0 && source_index >= 0) {
      // Set the source face
      face->incref();
      volume->incref();
      if (source) {
        source->decref();
      }
      if (source_volume) {
        source_volume->decref();
      }
      source = face;
      source_volume = volume;
    }
  } else {
    fprintf(stderr, "TMRFace Warning: Unable to set source face and volume\n");
  }
}

/*
  Retrieve the source edge
*/
void TMRFace::getSource(TMRVolume **volume, TMRFace **face) {
  if (face) {
    *face = source;
  }
  if (volume) {
    *volume = source_volume;
  }
}

/*
  Set the source face
*/
void TMRFace::setCopySource(int _copy_orient, TMRFace *face) {
  if (face && face != this && !source) {
    copy_orient = 0;
    if (_copy_orient > 0) {
      copy_orient = 1;
    } else if (_copy_orient < 0) {
      copy_orient = -1;
    }

    face->incref();
    if (copy) {
      copy->decref();
    }
    copy = face;
  } else {
    fprintf(stderr, "TMRFace Warning: Unable to set copy source face\n");
  }
}

/*
  Retrieve the copy face
*/
void TMRFace::getCopySource(int *_copy_orient, TMRFace **face) {
  if (_copy_orient) {
    *_copy_orient = copy_orient;
  }
  if (face) {
    *face = copy;
  }
}

/*
  Write out a representation of the surface to a VTK file
*/
void TMRFace::writeToVTK(const char *filename) {
  double umin, vmin, umax, vmax;
  getRange(&umin, &vmin, &umax, &vmax);

  const int npts = 100;

  // Write out the vtk file
  FILE *fp = fopen(filename, "w");
  if (fp) {
    fprintf(fp, "# vtk DataFile Version 3.0\n");
    fprintf(fp, "vtk output\nASCII\n");
    fprintf(fp, "DATASET UNSTRUCTURED_GRID\n");

    // Write out the points
    fprintf(fp, "POINTS %d float\n", npts * npts);
    for (int j = 0; j < npts; j++) {
      for (int i = 0; i < npts; i++) {
        double u = 1.0 * i / (npts - 1);
        double v = 1.0 * j / (npts - 1);
        u = (1.0 - u) * umin + u * umax;
        v = (1.0 - v) * vmin + v * vmax;

        // Evaluate the point
        TMRPoint p;
        evalPoint(u, v, &p);

        // Write out the point
        fprintf(fp, "%e %e %e\n", p.x, p.y, p.z);
      }
    }

    // Write out the cell values
    fprintf(fp, "\nCELLS %d %d\n", (npts - 1) * (npts - 1),
            5 * (npts - 1) * (npts - 1));
    for (int j = 0; j < npts - 1; j++) {
      for (int i = 0; i < npts - 1; i++) {
        fprintf(fp, "4 %d %d %d %d\n", i + j * npts, i + 1 + j * npts,
                i + 1 + (j + 1) * npts, i + (j + 1) * npts);
      }
    }

    // Write out the cell types
    fprintf(fp, "\nCELL_TYPES %d\n", (npts - 1) * (npts - 1));
    for (int k = 0; k < (npts - 1) * (npts - 1); k++) {
      fprintf(fp, "%d\n", 9);
    }

    fclose(fp);
  }
}

/*
  Container for faces bounding a volume

  input:
  nfaces:   the number of faces
  faces:    the TMRFace objects
  orient:   the orientation of the face
*/
TMRVolume::TMRVolume(int nfaces, TMRFace **_faces) {
  num_faces = nfaces;
  faces = new TMRFace *[num_faces];
  for (int i = 0; i < num_faces; i++) {
    faces[i] = _faces[i];
    faces[i]->incref();
  }

  mesh = NULL;
}

/*
  Free the volume object
*/
TMRVolume::~TMRVolume() {
  for (int i = 0; i < num_faces; i++) {
    faces[i]->decref();
  }
  delete[] faces;
}

/*
  Get the parameter range for this volume
*/
void TMRVolume::getRange(double *umin, double *vmin, double *wmin, double *umax,
                         double *vmax, double *wmax) {
  *umin = *vmin = *wmin = 0.0;
  *umax = *vmax = *wmax = 0.0;
}

/*
  Given the parametric point u,v,w compute the physical location x,y,z
*/
int TMRVolume::evalPoint(double u, double v, double w, TMRPoint *X) {
  X->zero();
  return 1;
}

/*
  Get the faces that enclose this volume
*/
void TMRVolume::getFaces(int *_num_faces, TMRFace ***_faces) {
  if (_num_faces) {
    *_num_faces = num_faces;
  }
  if (_faces) {
    *_faces = faces;
  }
}

/*
  Set the mesh into the array
*/
void TMRVolume::setMesh(TMRVolumeMesh *_mesh) { mesh = _mesh; }

/*
  Retrieve the mesh pointer
*/
void TMRVolume::getMesh(TMRVolumeMesh **_mesh) { *_mesh = mesh; }

/*
  Write out a representation of the volume to a VTK file
*/
void TMRVolume::writeToVTK(const char *filename) {
  double umin, vmin, wmin, umax, vmax, wmax;
  getRange(&umin, &vmin, &wmin, &umax, &vmax, &wmax);

  const int npts = 5;

  // Write out the vtk file
  FILE *fp = fopen(filename, "w");
  if (fp) {
    fprintf(fp, "# vtk DataFile Version 3.0\n");
    fprintf(fp, "vtk output\nASCII\n");
    fprintf(fp, "DATASET UNSTRUCTURED_GRID\n");

    // Write out the points
    fprintf(fp, "POINTS %d float\n", npts * npts * npts);
    for (int k = 0; k < npts; k++) {
      for (int j = 0; j < npts; j++) {
        for (int i = 0; i < npts; i++) {
          double u = 1.0 * i / (npts - 1);
          double v = 1.0 * j / (npts - 1);
          double w = 1.0 * k / (npts - 1);
          u = (1.0 - u) * umin + u * umax;
          v = (1.0 - v) * vmin + v * vmax;
          w = (1.0 - w) * wmin + w * wmax;

          // Evaluate the point
          TMRPoint p;
          evalPoint(u, v, w, &p);

          // Write out the point
          fprintf(fp, "%e %e %e\n", p.x, p.y, p.z);
        }
      }
    }

    // Write out the cell values
    fprintf(fp, "\nCELLS %d %d\n", (npts - 1) * (npts - 1) * (npts - 1),
            9 * (npts - 1) * (npts - 1) * (npts - 1));
    for (int k = 0; k < npts - 1; k++) {
      for (int j = 0; j < npts - 1; j++) {
        for (int i = 0; i < npts - 1; i++) {
          fprintf(fp, "8 %d %d %d %d  %d %d %d %d\n",
                  i + j * npts + k * npts * npts,
                  i + 1 + j * npts + k * npts * npts,
                  i + 1 + (j + 1) * npts + k * npts * npts,
                  i + (j + 1) * npts + k * npts * npts,
                  i + j * npts + (k + 1) * npts * npts,
                  i + 1 + j * npts + (k + 1) * npts * npts,
                  i + 1 + (j + 1) * npts + (k + 1) * npts * npts,
                  i + (j + 1) * npts + (k + 1) * npts * npts);
        }
      }
    }

    // Write out the cell types
    fprintf(fp, "\nCELL_TYPES %d\n", (npts - 1) * (npts - 1) * (npts - 1));
    for (int k = 0; k < (npts - 1) * (npts - 1) * (npts - 1); k++) {
      fprintf(fp, "%d\n", 12);
    }

    fclose(fp);
  }
}

/*
  The TMRModel class containing all of the required geometry
  objects.
*/
TMRModel::TMRModel(int _num_vertices, TMRVertex **_vertices, int _num_edges,
                   TMREdge **_edges, int _num_faces, TMRFace **_faces,
                   int _num_volumes, TMRVolume **_volumes) {
  vertices = NULL;
  edges = NULL;
  faces = NULL;
  volumes = NULL;
  ordered_verts = NULL;
  ordered_edges = NULL;
  ordered_faces = NULL;
  ordered_volumes = NULL;
  initialize(_num_vertices, _vertices, _num_edges, _edges, _num_faces, _faces,
             _num_volumes, _volumes);

  int fix_me = verify();

  if (fix_me) {
    TMRVertex **temp_verts = new TMRVertex *[num_vertices];
    memcpy(temp_verts, vertices, num_vertices * sizeof(TMRVertex *));

    TMREdge **temp_edges = new TMREdge *[num_edges];
    memcpy(temp_edges, edges, num_edges * sizeof(TMREdge *));

    TMRFace **temp_faces = new TMRFace *[num_faces];
    memcpy(temp_faces, faces, num_faces * sizeof(TMRFace *));

    TMRVolume **temp_vols = new TMRVolume *[num_volumes];
    memcpy(temp_vols, volumes, num_volumes * sizeof(TMRVolume *));

    initialize(num_vertices, temp_verts, num_edges, temp_edges, num_faces,
               temp_faces, num_volumes, temp_vols);

    delete[] temp_verts;
    delete[] temp_edges;
    delete[] temp_faces;
    delete[] temp_vols;
  }
}

void TMRModel::initialize(int _num_vertices, TMRVertex **_vertices,
                          int _num_edges, TMREdge **_edges, int _num_faces,
                          TMRFace **_faces, int _num_volumes,
                          TMRVolume **_volumes) {
  // Handle the vertices
  int count = 0;
  for (int i = 0; i < _num_vertices; i++) {
    if (_vertices[i]) {
      _vertices[i]->incref();
      count++;
    }
  }

  if (vertices) {
    for (int i = 0; i < num_vertices; i++) {
      if (vertices[i]) {
        vertices[i]->decref();
      }
    }
    delete[] vertices;
  }

  num_vertices = count;
  vertices = new TMRVertex *[num_vertices];

  for (int index = 0, i = 0; i < _num_vertices; i++) {
    if (_vertices[i]) {
      vertices[index] = _vertices[i];
      index++;
    }
  }

  // Handle the edges
  count = 0;
  for (int i = 0; i < _num_edges; i++) {
    if (_edges[i]) {
      _edges[i]->incref();
      count++;
    }
  }

  if (edges) {
    for (int i = 0; i < num_edges; i++) {
      if (edges[i]) {
        edges[i]->decref();
      }
    }
    delete[] edges;
  }

  num_edges = count;
  edges = new TMREdge *[num_edges];

  for (int index = 0, i = 0; i < _num_edges; i++) {
    if (_edges[i]) {
      edges[index] = _edges[i];
      index++;
    }
  }

  // Handle the faces
  count = 0;
  for (int i = 0; i < _num_faces; i++) {
    if (_faces[i]) {
      _faces[i]->incref();
      count++;
    }
  }

  if (faces) {
    for (int i = 0; i < num_faces; i++) {
      if (faces[i]) {
        faces[i]->decref();
      }
    }
    delete[] faces;
  }

  num_faces = count;
  faces = new TMRFace *[num_faces];

  for (int index = 0, i = 0; i < _num_faces; i++) {
    if (_faces[i]) {
      faces[index] = _faces[i];
      index++;
    }
  }

  // Handle the volumes
  count = 0;
  for (int i = 0; i < _num_volumes; i++) {
    if (_volumes[i]) {
      _volumes[i]->incref();
      count++;
    }
  }

  if (volumes) {
    for (int i = 0; i < num_volumes; i++) {
      if (volumes[i]) {
        volumes[i]->decref();
      }
    }
    delete[] volumes;
  }

  num_volumes = count;
  volumes = new TMRVolume *[num_volumes];

  for (int index = 0, i = 0; i < _num_volumes; i++) {
    if (_volumes[i]) {
      volumes[index] = _volumes[i];
      index++;
    }
  }

  if (ordered_verts) {
    delete[] ordered_verts;
  }
  if (ordered_edges) {
    delete[] ordered_edges;
  }
  if (ordered_faces) {
    delete[] ordered_faces;
  }
  if (ordered_volumes) {
    delete[] ordered_volumes;
  }

  // Order the entities
  ordered_verts = new OrderedPair<TMRVertex>[num_vertices];
  ordered_edges = new OrderedPair<TMREdge>[num_edges];
  ordered_faces = new OrderedPair<TMRFace>[num_faces];
  ordered_volumes = new OrderedPair<TMRVolume>[num_volumes];

  for (int i = 0; i < num_vertices; i++) {
    ordered_verts[i].num = i;
    ordered_verts[i].obj = vertices[i];
  }

  for (int i = 0; i < num_edges; i++) {
    ordered_edges[i].num = i;
    ordered_edges[i].obj = edges[i];
  }

  for (int i = 0; i < num_faces; i++) {
    ordered_faces[i].num = i;
    ordered_faces[i].obj = faces[i];
  }

  for (int i = 0; i < num_volumes; i++) {
    ordered_volumes[i].num = i;
    ordered_volumes[i].obj = volumes[i];
  }

  // Sort the vertices, curves and surfaces
  qsort(ordered_verts, num_vertices, sizeof(OrderedPair<TMRVertex>),
        compare_ordered_pairs<TMRVertex>);
  qsort(ordered_edges, num_edges, sizeof(OrderedPair<TMREdge>),
        compare_ordered_pairs<TMREdge>);
  qsort(ordered_faces, num_faces, sizeof(OrderedPair<TMRFace>),
        compare_ordered_pairs<TMRFace>);
  qsort(ordered_volumes, num_volumes, sizeof(OrderedPair<TMRVolume>),
        compare_ordered_pairs<TMRVolume>);
}

/*
  Free the geometry objects
*/
TMRModel::~TMRModel() {
  for (int i = 0; i < num_vertices; i++) {
    if (vertices[i]) {
      vertices[i]->decref();
    }
  }
  for (int i = 0; i < num_edges; i++) {
    if (edges[i]) {
      edges[i]->decref();
    }
  }
  for (int i = 0; i < num_faces; i++) {
    if (faces[i]) {
      faces[i]->decref();
    }
  }
  for (int i = 0; i < num_volumes; i++) {
    if (volumes[i]) {
      volumes[i]->decref();
    }
  }
  delete[] vertices;
  delete[] edges;
  delete[] faces;
  delete[] volumes;
  delete[] ordered_verts;
  delete[] ordered_edges;
  delete[] ordered_faces;
  delete[] ordered_volumes;
}

/*
  Verify that all objects that are referenced in the geometry have a
  verifiable list name and that objects are referred to one or more
  times (but not zero times!)
*/
int TMRModel::verify() {
  int fail = 0;
  int *verts = new int[num_vertices];
  int *crvs = new int[num_edges];
  memset(verts, 0, num_vertices * sizeof(int));
  memset(crvs, 0, num_edges * sizeof(int));

  for (int face = 0; face < num_faces; face++) {
    int nloops = faces[face]->getNumEdgeLoops();

    for (int k = 0; k < nloops; k++) {
      TMREdgeLoop *loop;
      faces[face]->getEdgeLoop(k, &loop);

      int ndgs;
      TMREdge **loop_edges;
      loop->getEdgeLoop(&ndgs, &loop_edges, NULL);

      // Loop over all of the curves and check whether the data exists
      // or not
      for (int j = 0; j < ndgs; j++) {
        int cindex = getEdgeIndex(loop_edges[j]);
        if (cindex < 0) {
          fail = 1;
          fprintf(stderr,
                  "TMRModel error: Missing edge %d in "
                  "edge loop %d for face %d\n",
                  j, k, face);
        } else {
          crvs[cindex]++;
        }

        TMRVertex *v1, *v2;
        loop_edges[j]->getVertices(&v1, &v2);
        if (!v1 || !v2) {
          fail = 1;
          fprintf(stderr, "TMRModel error: Vertices not set for edge %d\n",
                  cindex);
        }

        int v1index = getVertexIndex(v1);
        int v2index = getVertexIndex(v2);
        if (v1index < 0 || v2index < 0) {
          fail = 1;
          fprintf(stderr,
                  "TMRModel error: Vertices do not exist "
                  "within the vertex list\n");
        } else {
          verts[v1index]++;
          verts[v2index]++;
        }
      }
    }
  }

  // Check if any of the counts are zero
  int print_all_errors = 0;

  if (print_all_errors) {
    for (int i = 0; i < num_vertices; i++) {
      if (verts[i] == 0) {
        const char *name = NULL;
        if (vertices[i] && (name = vertices[i]->getName())) {
          fprintf(stderr,
                  "TMRModel warning: Vertex %d with name %s unreferenced\n", i,
                  name);
        } else {
          fprintf(stderr, "TMRModel warning: Vertex %d unreferenced\n", i);
        }
        fail = 1;
      }
    }
    for (int i = 0; i < num_edges; i++) {
      if (crvs[i] == 0) {
        const char *name = NULL;
        if (edges[i] && (name = edges[i]->getName())) {
          fprintf(stderr,
                  "TMRModel warning: Edge %d with name %s unreferenced\n", i,
                  name);
        } else {
          fprintf(stderr, "TMRModel warning: Edge %d unreferenced\n", i);
        }
        fail = 1;
      }
    }
  }

  int verts_count = 0, crvs_count = 0;
  for (int i = 0; i < num_vertices; i++) {
    if (verts[i] == 0) {
      vertices[i]->decref();
      vertices[i] = NULL;
      verts_count++;
      fail = 1;
    }
  }
  for (int i = 0; i < num_edges; i++) {
    if (crvs[i] == 0) {
      edges[i]->decref();
      edges[i] = NULL;
      crvs_count++;
      fail = 1;
    }
  }
  if (verts_count > 0) {
    fprintf(stderr, "TMRModel warning: %d vertices unreferenced\n",
            verts_count);
  }
  if (crvs_count > 0) {
    fprintf(stderr, "TMRModel warning: %d edges unreferenced\n", crvs_count);
  }

  delete[] verts;
  delete[] crvs;

  return fail;
}

/*
  Retrieve the vertices
*/
void TMRModel::getVertices(int *_num_vertices, TMRVertex ***_vertices) {
  if (_num_vertices) {
    *_num_vertices = num_vertices;
  }
  if (_vertices) {
    *_vertices = vertices;
  }
}

/*
  Retrieve the curves
*/
void TMRModel::getEdges(int *_num_edges, TMREdge ***_edges) {
  if (_num_edges) {
    *_num_edges = num_edges;
  }
  if (_edges) {
    *_edges = edges;
  }
}

/*
  Retrieve the surfaces
*/
void TMRModel::getFaces(int *_num_faces, TMRFace ***_faces) {
  if (_num_faces) {
    *_num_faces = num_faces;
  }
  if (_faces) {
    *_faces = faces;
  }
}

/*
  Retrieve the volumes
*/
void TMRModel::getVolumes(int *_num_volumes, TMRVolume ***_volumes) {
  if (_num_volumes) {
    *_num_volumes = num_volumes;
  }
  if (_volumes) {
    *_volumes = volumes;
  }
}

/*
  Static member function for sorting the ordered pairs
*/
template <class ctype>
int TMRModel::compare_ordered_pairs(const void *avoid, const void *bvoid) {
  const OrderedPair<ctype> *a = static_cast<const OrderedPair<ctype> *>(avoid);
  const OrderedPair<ctype> *b = static_cast<const OrderedPair<ctype> *>(bvoid);

  if (a->obj && b->obj) {
    return a->obj->getEntityId() - b->obj->getEntityId();
  } else if (a->obj) {
    return 1;
  } else if (b->obj) {
    return -1;
  }
  return 0;
}

/*
  Retrieve the index given the vertex point
*/
int TMRModel::getVertexIndex(TMRVertex *vertex) {
  OrderedPair<TMRVertex> pair;
  pair.num = -1;
  pair.obj = vertex;

  // Search for the ordered pair
  OrderedPair<TMRVertex> *item = (OrderedPair<TMRVertex> *)bsearch(
      &pair, ordered_verts, num_vertices, sizeof(OrderedPair<TMRVertex>),
      compare_ordered_pairs<TMRVertex>);
  if (item) {
    return item->num;
  }

  fprintf(stderr, "TMRModel error: Vertex index search failed\n");
  return -1;
}

/*
  Retrieve the index given the pointer to the curve object
*/
int TMRModel::getEdgeIndex(TMREdge *edge) {
  OrderedPair<TMREdge> pair;
  pair.num = -1;
  pair.obj = edge;

  // Search for the ordered pair
  OrderedPair<TMREdge> *item = (OrderedPair<TMREdge> *)bsearch(
      &pair, ordered_edges, num_edges, sizeof(OrderedPair<TMREdge>),
      compare_ordered_pairs<TMREdge>);
  if (item) {
    return item->num;
  }

  fprintf(stderr, "TMRModel error: Edge index search failed\n");
  return -1;
}

/*
  Retrieve the index given the pointer to the surface object
*/
int TMRModel::getFaceIndex(TMRFace *face) {
  OrderedPair<TMRFace> pair;
  pair.num = -1;
  pair.obj = face;

  // Search for the ordered pair
  OrderedPair<TMRFace> *item = (OrderedPair<TMRFace> *)bsearch(
      &pair, ordered_faces, num_faces, sizeof(OrderedPair<TMRFace>),
      compare_ordered_pairs<TMRFace>);
  if (item) {
    return item->num;
  }

  fprintf(stderr, "TMRModel error: Face index search failed\n");
  return -1;
}

/*
  Retrieve the index given the pointer to the volume
*/
int TMRModel::getVolumeIndex(TMRVolume *volume) {
  OrderedPair<TMRVolume> pair;
  pair.num = -1;
  pair.obj = volume;

  // Search for the ordered pair
  OrderedPair<TMRVolume> *item = (OrderedPair<TMRVolume> *)bsearch(
      &pair, ordered_volumes, num_volumes, sizeof(OrderedPair<TMRVolume>),
      compare_ordered_pairs<TMRVolume>);
  if (item) {
    return item->num;
  }

  fprintf(stderr, "TMRModel error: Volume index search failed\n");
  return -1;
}

/*
  The main topology class that contains the objects used to build the
  underlying mesh.
*/
TMRTopology::TMRTopology(MPI_Comm _comm, TMRModel *_geo) {
  // Set the communicator
  comm = _comm;

  // Increase the ref. count to the geometry object
  geo = _geo;
  geo->incref();

  // NULL all the data associated with either a mapped 2D or 3D
  // topology
  edge_to_vertices = NULL;
  face_to_edges = NULL;
  face_to_vertices = NULL;
  volume_to_edges = NULL;
  volume_to_faces = NULL;
  volume_to_vertices = NULL;

  // Reordering of the faces
  face_to_new_num = NULL;
  new_num_to_face = NULL;

  // Reordering of the blocks/volumes
  volume_to_new_num = NULL;
  new_num_to_volume = NULL;

  // Get the geometry objects
  int num_vertices, num_edges, num_faces, num_volumes;
  TMRVertex **vertices;
  TMREdge **edges;
  TMRFace **faces;
  TMRVolume **volumes;
  geo->getVertices(&num_vertices, &vertices);
  geo->getEdges(&num_edges, &edges);
  geo->getFaces(&num_faces, &faces);
  geo->getVolumes(&num_volumes, &volumes);

  if (num_volumes > 0) {
    // Check whether all of the volumes are actually
    for (int i = 0; i < num_volumes; i++) {
      TMRTFIVolume *vol = dynamic_cast<TMRTFIVolume *>(volumes[i]);
      if (!vol) {
        fprintf(
            stderr,
            "TMRTopology error: All volumes must be of type TMRTFIVolume\n");
      }
    }

    // Create the face -> edge information
    int *volume_faces = new int[6 * num_volumes];
    for (int i = 0; i < num_volumes; i++) {
      // Get the faces
      int nfaces;
      TMRFace **f;
      volumes[i]->getFaces(&nfaces, &f);

      if (nfaces != 6) {
        fprintf(stderr,
                "TMRTopology error: TMRVolume %d does not contain 6 faces\n",
                i);
      }
      // Search for the face indices
      for (int j = 0; j < 6; j++) {
        if (j < nfaces) {
          volume_faces[6 * i + j] = geo->getFaceIndex(f[j]);
        } else {
          // Face does not exist
          volume_faces[6 * i + j] = -1;
        }
      }
    }

    // Face index to the new face number
    volume_to_new_num = new int[num_volumes];
    new_num_to_volume = new int[num_volumes];

    // Do not use the RCM reordering for the volumes
    int use_rcm = 0;
    reorderEntities(6, num_faces, num_volumes, volume_faces, volume_to_new_num,
                    new_num_to_volume, use_rcm);

    // Free the temporary volume to faces pointer
    delete[] volume_faces;

    // Compute the volume connectivity
    computeVolumeConn();
  } else {
    // From the edge loop number, get the edge in coordinate
    // ordering
    const int coordinate_to_edge[] = {3, 1, 0, 2};

    // Create the face -> edge information
    int *face_edges = new int[4 * num_faces];
    for (int i = 0; i < num_faces; i++) {
      int nloops = faces[i]->getNumEdgeLoops();
      if (nloops != 1) {
        fprintf(stderr, "TMRTopology error: TMRFace %d contains %d loops\n", i,
                nloops);
      }
      if (nloops >= 1) {
        // Get the first loop
        TMREdgeLoop *loop;
        faces[i]->getEdgeLoop(0, &loop);

        // Get the edges associated with this loop
        int nedges;
        TMREdge **e;
        loop->getEdgeLoop(&nedges, &e, NULL);

        if (nedges != 4) {
          fprintf(stderr,
                  "TMRTopology error: TMRFace %d does not contain 4 edges\n",
                  i);
        }

        // Search for the face indices
        for (int jp = 0; jp < 4; jp++) {
          int j = coordinate_to_edge[jp];
          if (jp < nedges) {
            face_edges[4 * i + jp] = geo->getEdgeIndex(e[j]);
          } else {
            // Edge does not exist in the model
            face_edges[4 * i + jp] = -1;
          }
        }
      } else {
        for (int jp = 0; jp < 4; jp++) {
          face_edges[4 * i + jp] = -1;
        }
      }
    }

    // Face index to the new face number
    face_to_new_num = new int[num_faces];
    new_num_to_face = new int[num_faces];

    int use_rcm = 0;
    reorderEntities(4, num_edges, num_faces, face_edges, face_to_new_num,
                    new_num_to_face, use_rcm);

    // Delete face edges
    delete[] face_edges;

    // Allocate the connectivity
    computeFaceConn();
  }
}

/*
  Free the topology data
*/
TMRTopology::~TMRTopology() {
  // Free the geometry object
  geo->decref();

  // Free any face connectivity information
  if (edge_to_vertices) {
    delete[] edge_to_vertices;
  }
  if (face_to_vertices) {
    delete[] face_to_vertices;
  }
  if (face_to_edges) {
    delete[] face_to_edges;
  }
  if (volume_to_vertices) {
    delete[] volume_to_vertices;
  }
  if (volume_to_edges) {
    delete[] volume_to_edges;
  }
  if (volume_to_faces) {
    delete[] volume_to_faces;
  }

  // Free the reordering data - if it exists
  if (face_to_new_num) {
    delete[] face_to_new_num;
  }
  if (new_num_to_face) {
    delete[] new_num_to_face;
  }
  if (volume_to_new_num) {
    delete[] volume_to_new_num;
  }
  if (new_num_to_volume) {
    delete[] new_num_to_volume;
  }
}

/*
  Compute the volume connectivity
*/
void TMRTopology::computeVolumeConn() {
  // First compute the connectivity between faces/edges and vertices
  computeFaceConn();

  // Get the geometry objects
  int num_vertices, num_edges, num_faces, num_volumes;
  TMRVertex **vertices;
  TMREdge **edges;
  TMRFace **faces;
  TMRVolume **volumes;
  geo->getVertices(&num_vertices, &vertices);
  geo->getEdges(&num_edges, &edges);
  geo->getFaces(&num_faces, &faces);
  geo->getVolumes(&num_volumes, &volumes);

  // Allocate the data needed for the connectivity
  volume_to_faces = new int[6 * num_volumes];
  volume_to_edges = new int[12 * num_volumes];
  volume_to_vertices = new int[8 * num_vertices];

  for (int i = 0; i < num_volumes; i++) {
    // Get the volume - and reorder it
    int n = i;
    if (volume_to_new_num) {
      n = volume_to_new_num[i];
    }

    // Dynamic cast to a TMRTFIVolume
    TMRTFIVolume *vol = dynamic_cast<TMRTFIVolume *>(volumes[i]);

    if (vol) {
      // Get the faces associated with the volume
      TMRFace **f;
      TMREdge **e;
      TMRVertex **v;
      vol->getEntities(&f, &e, &v);

      // Set the volume -> face information
      for (int j = 0; j < 6; j++) {
        volume_to_faces[6 * n + j] = geo->getFaceIndex(f[j]);
      }

      // Set the volume -> edge information
      for (int j = 0; j < 12; j++) {
        volume_to_edges[12 * n + j] = geo->getEdgeIndex(e[j]);
      }

      // Set the volume -> vertex information
      for (int j = 0; j < 8; j++) {
        volume_to_vertices[8 * n + j] = geo->getVertexIndex(v[j]);
      }
    }
  }
}

/*
  Compute the face connectivity
*/
void TMRTopology::computeFaceConn() {
  // Get the geometry objects
  int num_vertices, num_edges, num_faces;
  TMRVertex **vertices;
  TMREdge **edges;
  TMRFace **faces;
  geo->getVertices(&num_vertices, &vertices);
  geo->getEdges(&num_edges, &edges);
  geo->getFaces(&num_faces, &faces);

  // From the edge loop number, get the edge in coordinate ordering
  const int coordinate_to_edge[] = {3, 1, 0, 2};

  // Allocate the face to edges
  face_to_edges = new int[4 * num_faces];

  // Post-process to get the new face ordering
  for (int i = 0; i < num_faces; i++) {
    // Get the new face number
    int f = i;
    if (face_to_new_num) {
      f = face_to_new_num[i];
    }

    // Get the edge loop for the face
    TMREdgeLoop *loop;
    faces[i]->getEdgeLoop(0, &loop);
    TMREdge **e;
    loop->getEdgeLoop(NULL, &e, NULL);

    // Search for the edge indices
    for (int jp = 0; jp < 4; jp++) {
      int j = coordinate_to_edge[jp];
      face_to_edges[4 * f + jp] = geo->getEdgeIndex(e[j]);
    }
  }

  // Create the edge -> vertex information
  edge_to_vertices = new int[2 * num_edges];
  for (int i = 0; i < num_edges; i++) {
    if (edges[i]) {
      TMRVertex *v1, *v2;
      edges[i]->getVertices(&v1, &v2);
      edge_to_vertices[2 * i] = geo->getVertexIndex(v1);
      edge_to_vertices[2 * i + 1] = geo->getVertexIndex(v2);
    } else {
      edge_to_vertices[2 * i] = 0;
      edge_to_vertices[2 * i + 1] = 0;
    }
  }

  // Create the face -> vertex information. Within the TMR geometry
  // routines, the ordering is as shown on the left.  The
  // quadrant/octree code uses the coordinate ordering shown on the
  // right.
  //
  //  From TMRSurface         Coordinate-ordering
  //  v3---e2---v2            v2---e3---v3
  //  |         |             |         |
  //  e3        e1            e0        e1
  //  |         |             |         |
  //  v0---e0---v1            v0---e2---v1
  //  C.C.W. direction        Coordinate direction

  face_to_vertices = new int[4 * num_faces];
  for (int i = 0; i < num_faces; i++) {
    // Get the new face number
    int f = i;
    if (face_to_new_num) {
      f = face_to_new_num[i];
    }

    if (faces[i]) {
      // Get the edge loop and direction
      TMREdgeLoop *loop;
      faces[i]->getEdgeLoop(0, &loop);

      const int *edge_orient;
      loop->getEdgeLoop(NULL, NULL, &edge_orient);

      // Coordinate-ordered edge e0
      int edge = face_to_edges[4 * f];
      if (edge_orient[3] > 0) {
        face_to_vertices[4 * f] = edge_to_vertices[2 * edge + 1];
        face_to_vertices[4 * f + 2] = edge_to_vertices[2 * edge];
      } else {
        face_to_vertices[4 * f] = edge_to_vertices[2 * edge];
        face_to_vertices[4 * f + 2] = edge_to_vertices[2 * edge + 1];
      }

      // Coordinate-ordered edge e1
      edge = face_to_edges[4 * f + 1];
      if (edge_orient[1] > 0) {
        face_to_vertices[4 * f + 1] = edge_to_vertices[2 * edge];
        face_to_vertices[4 * f + 3] = edge_to_vertices[2 * edge + 1];
      } else {
        face_to_vertices[4 * f + 1] = edge_to_vertices[2 * edge + 1];
        face_to_vertices[4 * f + 3] = edge_to_vertices[2 * edge];
      }
    } else {
      for (int k = 0; k < 4; k++) {
        face_to_vertices[4 * f + k] = 0;
      }
    }
  }
}

/*
  Compute the connectivity between faces or volumes given the
  connectivity between faces to edges or volumes to faces.
*/
void TMRTopology::computeConnectivty(int num_entities, int num_edges,
                                     int num_faces, const int ftoedges[],
                                     int **_face_to_face_ptr,
                                     int **_face_to_face) {
  // The edge to face pointer information
  int *edge_to_face_ptr = new int[num_edges + 1];
  int *edge_to_face = new int[num_entities * num_faces];
  memset(edge_to_face_ptr, 0, (num_edges + 1) * sizeof(int));
  for (int i = 0; i < num_faces; i++) {
    for (int j = 0; j < num_entities; j++) {
      if (ftoedges[num_entities * i + j] >= 0) {
        edge_to_face_ptr[1 + ftoedges[num_entities * i + j]]++;
      }
    }
  }

  // Readjust the pointer into the edge array
  for (int i = 0; i < num_edges; i++) {
    edge_to_face_ptr[i + 1] += edge_to_face_ptr[i];
  }
  edge_to_face_ptr[0] = 0;

  // Set the edge to face pointer
  for (int i = 0; i < num_faces; i++) {
    for (int j = 0; j < num_entities; j++) {
      int e = ftoedges[num_entities * i + j];
      if (e >= 0) {
        edge_to_face[edge_to_face_ptr[e]] = i;
        edge_to_face_ptr[e]++;
      }
    }
  }

  // Reset the edge to face pointer
  for (int i = num_edges; i >= 1; i--) {
    edge_to_face_ptr[i] = edge_to_face_ptr[i - 1];
  }
  edge_to_face_ptr[0] = 0;

  // Set the pointer from the face to face
  int max_face_to_face_size = 0;
  for (int i = 0; i < num_faces; i++) {
    for (int j = 0; j < num_entities; j++) {
      int e = ftoedges[num_entities * i + j];
      if (e >= 0) {
        for (int kp = edge_to_face_ptr[e]; kp < edge_to_face_ptr[e + 1]; kp++) {
          int f = edge_to_face[kp];
          if (i != f) {
            max_face_to_face_size++;
          }
        }
      }
    }
  }

  // Set the face pointer
  int *face_to_face_ptr = new int[num_faces + 1];
  int *face_to_face = new int[max_face_to_face_size];

  face_to_face_ptr[0] = 0;
  for (int i = 0; i < num_faces; i++) {
    face_to_face_ptr[i + 1] = face_to_face_ptr[i];
    for (int j = 0; j < num_entities; j++) {
      int e = ftoedges[num_entities * i + j];
      if (e >= 0) {
        for (int kp = edge_to_face_ptr[e]; kp < edge_to_face_ptr[e + 1]; kp++) {
          int f = edge_to_face[kp];
          if (i != f) {
            int add_me = 1;
            for (int k = face_to_face_ptr[i]; k < face_to_face_ptr[i + 1];
                 k++) {
              if (face_to_face[k] == f) {
                add_me = 0;
                break;
              }
            }
            if (add_me) {
              face_to_face[face_to_face_ptr[i + 1]] = f;
              face_to_face_ptr[i + 1]++;
            }
          }
        }
      }
    }
  }

  delete[] edge_to_face_ptr;
  delete[] edge_to_face;

  // Set the output pointers
  *_face_to_face_ptr = face_to_face_ptr;
  *_face_to_face = face_to_face;
}

/*
  Reorder volumes or faces to group things according to MPI rank
*/
void TMRTopology::reorderEntities(int num_entities, int num_edges,
                                  int num_faces, const int *ftoedges,
                                  int *entity_to_new_num,
                                  int *new_num_to_entity, int use_rcm) {
  // Get the mpi size
  int mpi_rank, mpi_size;
  MPI_Comm_rank(comm, &mpi_rank);
  MPI_Comm_size(comm, &mpi_size);

  if (mpi_rank == 0) {
    // Compute the new ordering
    int *face_to_face_ptr, *face_to_face;
    computeConnectivty(num_entities, num_edges, num_faces, ftoedges,
                       &face_to_face_ptr, &face_to_face);

    if (mpi_size == 1 || use_rcm) {
      // Set a pointer to the new numbers
      int *vars = entity_to_new_num;
      int *levset = new_num_to_entity;

      // Tag all the values to indicate that we have not yet visited
      // these entities
      for (int i = 0; i < num_faces; i++) {
        vars[i] = -1;
      }

      // Set the start and end location for each level set
      int start = 0, end = 0;

      // The number of ordered entities
      int n = 0;

      // Keep going until everything has been ordered
      while (n < num_faces) {
        // Find the next root
        int root = -1, max_degree = num_faces + 1;
        for (int i = 0; i < num_faces; i++) {
          if (vars[i] < 0 &&
              face_to_face_ptr[i + 1] - face_to_face_ptr[i] < max_degree) {
            root = i;
            max_degree = face_to_face_ptr[i + 1] - face_to_face_ptr[i];
          }
        }

        // Nothing is left to order
        if (root < 0) {
          break;
        }

        // Set the next root within the level set and continue
        levset[end] = root;
        vars[root] = n;
        n++;
        end++;

        while (start < end) {
          int next = end;
          // Iterate over the nodes added to the previous level set
          for (int current = start; current < end; current++) {
            int node = levset[current];

            // Add all the nodes in the next level set
            for (int j = face_to_face_ptr[node]; j < face_to_face_ptr[node + 1];
                 j++) {
              int next_node = face_to_face[j];

              if (vars[next_node] < 0) {
                vars[next_node] = n;
                levset[next] = next_node;
                n++;
                next++;
              }
            }
          }

          start = end;
          end = next;
        }
      }
    } else {
      // Set the pointer to the new entities array
      int *partition = new_num_to_entity;

      // Set the default options
      int options[METIS_NOPTIONS];
      METIS_SetDefaultOptions(options);

      // Use 0-based numbering
      options[METIS_OPTION_NUMBERING] = 0;

      // The objective value in METIS
      int objval = 0;

      // Partition based on the size of the mesh
      int ncon = 1;
      METIS_PartGraphRecursive(&num_faces, &ncon, face_to_face_ptr,
                               face_to_face, NULL, NULL, NULL, &mpi_size, NULL,
                               NULL, options, &objval, partition);

      int *offset = new int[mpi_size + 1];
      memset(offset, 0, (mpi_size + 1) * sizeof(int));
      for (int i = 0; i < num_faces; i++) {
        offset[partition[i] + 1]++;
      }
      for (int i = 0; i < mpi_size; i++) {
        offset[i + 1] += offset[i];
      }

      // Order the faces according to their partition
      for (int i = 0; i < num_faces; i++) {
        entity_to_new_num[i] = offset[partition[i]];
        offset[partition[i]]++;
      }

      // Free the local offset data
      delete[] offset;
    }

    delete[] face_to_face_ptr;
    delete[] face_to_face;
  }

  // Broadcast the new ordering
  MPI_Bcast(entity_to_new_num, num_faces, MPI_INT, 0, comm);

  // Now overwrite the new_num_to_face == partition array
  for (int i = 0; i < num_faces; i++) {
    new_num_to_entity[entity_to_new_num[i]] = i;
  }
}

/*
  Get the volume associated with the given volume number
*/
void TMRTopology::getVolume(int vol_num, TMRVolume **volume) {
  int num_volumes;
  TMRVolume **volumes;
  geo->getVolumes(&num_volumes, &volumes);
  *volume = NULL;

  int v = vol_num;
  if (new_num_to_volume) {
    v = new_num_to_volume[vol_num];
  }
  if (volumes && (v >= 0 && v < num_volumes)) {
    *volume = volumes[v];
  }
}

/*
  Get the number of volumes
*/
int TMRTopology::getNumVolumes() {
  int nvol;
  geo->getVolumes(&nvol, NULL);
  return nvol;
}

/*
  Get the number of faces
*/
int TMRTopology::getNumFaces() {
  int nfaces;
  geo->getFaces(&nfaces, NULL);
  return nfaces;
}

/*
  Get the number of edges
*/
int TMRTopology::getNumEdges() {
  int nedges;
  geo->getEdges(&nedges, NULL);
  return nedges;
}

/*
  Get the number of vertices
*/
int TMRTopology::getNumVertices() {
  int nverts;
  geo->getVertices(&nverts, NULL);
  return nverts;
}

/*
  Retrieve the face object
*/
void TMRTopology::getFace(int face_num, TMRFace **face) {
  int num_faces;
  TMRFace **faces;
  geo->getFaces(&num_faces, &faces);
  *face = NULL;

  int f = face_num;
  if (new_num_to_face) {
    f = new_num_to_face[face_num];
  }
  if (faces && (f >= 0 && f < num_faces)) {
    *face = faces[f];
  }
}

/*
  Retrieve the curve object associated with the given face/edge index
*/
void TMRTopology::getEdge(int edge_num, TMREdge **edge) {
  int num_edges;
  TMREdge **edges;
  geo->getEdges(&num_edges, &edges);
  *edge = NULL;

  if (edges && (edge_num >= 0 && edge_num < num_edges)) {
    *edge = edges[edge_num];
  }
}

/*
  Retrieve the vertex object associated with the face/vertex index
*/
void TMRTopology::getVertex(int vert_num, TMRVertex **vert) {
  int num_vertices;
  TMRVertex **verts;
  geo->getVertices(&num_vertices, &verts);
  *vert = NULL;

  if (verts && (vert_num >= 0 && vert_num < num_vertices)) {
    *vert = verts[vert_num];
  }
}

/*
  Retrieve the connectivity information
*/
void TMRTopology::getConnectivity(int *nnodes, int *nedges, int *nfaces,
                                  const int **face_nodes,
                                  const int **face_edges) {
  int num_vertices, num_edges, num_faces;
  geo->getVertices(&num_vertices, NULL);
  geo->getEdges(&num_edges, NULL);
  geo->getFaces(&num_faces, NULL);
  *nnodes = num_vertices;
  *nedges = num_edges;
  *nfaces = num_faces;
  *face_nodes = face_to_vertices;
  *face_edges = face_to_edges;
}

/*
  Retrieve the connectivity for the underlying mesh
*/
void TMRTopology::getConnectivity(int *nnodes, int *nedges, int *nfaces,
                                  int *nvolumes, const int **volume_nodes,
                                  const int **volume_edges,
                                  const int **volume_faces) {
  int num_vertices, num_edges, num_faces, num_volumes;
  geo->getVertices(&num_vertices, NULL);
  geo->getEdges(&num_edges, NULL);
  geo->getFaces(&num_faces, NULL);
  geo->getVolumes(&num_volumes, NULL);
  *nnodes = num_vertices;
  *nedges = num_edges;
  *nfaces = num_faces;
  *nvolumes = num_volumes;
  *volume_nodes = volume_to_vertices;
  *volume_edges = volume_to_edges;
  *volume_faces = volume_to_faces;
}
