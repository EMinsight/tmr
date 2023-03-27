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

#include "TMREdgeMesh.h"

#include <math.h>
#include <stdio.h>

/*
  An integral entry for the linked list
*/
class IntegralPt {
 public:
  double t;
  double dist;
  IntegralPt *next;
};

/*
  Evaluate the distance between two points
*/
double pointDist(TMRPoint *a, TMRPoint *b) {
  return sqrt((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) +
              (a->z - b->z) * (a->z - b->z));
}

/*
  Recursive integration on an edge with an adaptive error control to
  ensure that the integral is computed with sufficient accuracy.

  input:
  t1, t2:  the limits of integration for this interval
  tol:     the absolute error measure
  ncalls:  the recursion depth
  pt:      pointer into the linked list
*/
void integrateEdge(TMREdge *edge, TMRElementFeatureSize *fs, double t1,
                   double h1, TMRPoint p1, double t2, double tol, int ncalls,
                   IntegralPt **_pt) {
  // Dereference the pointer to the integral point
  IntegralPt *pt = *_pt;

  // Find the mid point of the interval
  TMRPoint pmid;
  double tmid = 0.5 * (t1 + t2);
  edge->evalPoint(tmid, &pmid);
  double hmid = fs->getFeatureSize(pmid);

  // Evaluate the point at the end of the interval
  TMRPoint p2;
  edge->evalPoint(t2, &p2);
  double h2 = fs->getFeatureSize(p2);

  // Evaluate the approximate integral contributions
  double int1 = 2.0 * pointDist(&p1, &pmid) / (h1 + hmid);
  double int2 = 4.0 * pointDist(&pmid, &p2) / (h1 + 2.0 * hmid + h2);
  double int3 = 2.0 * pointDist(&p1, &p2) / (hmid + h2);

  // Compute the integration error
  double error = fabs(int3 - int1 - int2);

  if (((ncalls > 6) && (error < tol)) || (ncalls > 20)) {
    // Add the mid point
    pt->next = new IntegralPt;
    pt->next->dist = pt->dist + int1;
    pt->next->t = tmid;
    pt->next->next = NULL;
    pt = pt->next;

    // Add the final point p2
    pt->next = new IntegralPt;
    pt->next->dist = pt->dist + int2;
    pt->next->t = t2;
    pt->next->next = NULL;
    pt = pt->next;

    // Set the pointer to the end of the linked list
    *_pt = pt;
  } else {
    // Continue the recursive integration
    integrateEdge(edge, fs, t1, h1, p1, tmid, tol, ncalls + 1, _pt);
    integrateEdge(edge, fs, tmid, hmid, pmid, t2, tol, ncalls + 1, _pt);
  }
}

/*
  Integrate along the edge adaptively, creating a list
*/
double integrateEdge(TMREdge *edge, TMRElementFeatureSize *fs, double t1,
                     double t2, double tol, double **_tvals, double **_dist,
                     int *_nvals) {
  *_tvals = NULL;
  *_dist = NULL;
  *_nvals = 0;

  // Allocate the entry in the linked list
  IntegralPt *root = new IntegralPt;
  root->next = NULL;
  root->dist = 0.0;
  root->t = t1;

  // Evaluate the first point
  TMRPoint p1;
  edge->evalPoint(t1, &p1);
  double h1 = fs->getFeatureSize(p1);

  // Integrate over the edge
  IntegralPt *pt = root;
  integrateEdge(edge, fs, t1, h1, p1, t2, tol, 0, &pt);

  // Count up and allocate the num
  int count = 1;
  IntegralPt *curr = root;
  while (curr->next) {
    curr = curr->next;
    count++;
  }

  // Allocate arrays to store the parametric location/distance data
  double *tvals = new double[count];
  double *dist = new double[count];

  // Scan through the linked list, read out the values of the
  // parameter and its integral and delete the entries as we go...
  count = 0;
  curr = root;
  tvals[count] = curr->t;
  dist[count] = curr->dist;
  count++;

  while (curr->next) {
    IntegralPt *tmp = curr;
    curr = curr->next;
    tvals[count] = curr->t;
    dist[count] = curr->dist;
    count++;
    delete tmp;
  }

  double len = curr->dist;
  delete curr;

  // Set the pointers for the output
  *_nvals = count;
  *_tvals = tvals;
  *_dist = dist;

  return len;
}

class EdgePt {
 public:
  TMRPoint p;
  double t;

  static int compare(const void *a, const void *b) {
    double ta = (static_cast<const EdgePt *>(a))->t;
    double tb = (static_cast<const EdgePt *>(b))->t;
    if (ta < tb) {
      return -1;
    }
    if (ta > tb) {
      return 1;
    }
    return 0;
  }
};

/*
  Get the relative orientations of the edge and the copy source
  edge. This code returns +/- 1.
*/
int TMREdgeMesh::getEdgeCopyOrient(TMREdge *edge_source) {
  TMREdge *copy_edge;
  edge_source->getCopySource(&copy_edge);

  if (copy_edge) {
    // Get the vertex sources from the source edge
    TMRVertex *v1, *v2, *t;
    edge_source->getVertices(&v1, &v2);
    v1->getCopySource(&t);
    if (t) {
      v1 = t;
    }
    v2->getCopySource(&t);
    if (t) {
      v2 = t;
    }

    // Get the vertex sources for the copy source edge
    TMRVertex *v1_copy, *v2_copy;
    copy_edge->getVertices(&v1_copy, &v2_copy);
    v1_copy->getCopySource(&t);
    if (t) {
      v1_copy = t;
    }
    v2_copy->getCopySource(&t);
    if (t) {
      v2_copy = t;
    }

    int orient = 0;
    if (edge_source->isDegenerate() && copy_edge->isDegenerate()) {
      if (v1_copy == v1) {
        orient = 1;
      } else {
        fprintf(stderr,
                "TMREdgeMesh Error: Degenerate copy edge not set correctly\n");
      }
    } else if (!edge_source->isDegenerate() && !copy_edge->isDegenerate()) {
      if (v1_copy == v1 && v2_copy == v2) {
        orient = 1;
      } else if (v1_copy == v2 && v2_copy == v1) {
        orient = -1;
      } else {
        fprintf(stderr,
                "TMREdgeMesh Error: Edge vertex sources not set correctly\n");
      }
    } else {
      fprintf(stderr, "TMREdgeMesh Error: Edge and copy source do not match\n");
    }

    return orient;
  }

  // The edges have the same orientation because edge_source is the
  // source edge
  return 1;
}

/*
  Create a mesh along curve
*/
TMREdgeMesh::TMREdgeMesh(MPI_Comm _comm, TMREdge *_edge, TMRPoint *_X,
                         int _npts) {
  comm = _comm;
  edge = _edge;
  edge->incref();

  npts = 0;
  pts = NULL;
  X = NULL;
  vars = NULL;

  prescribed_mesh = 0;

  if (_X && _npts > 0) {
    prescribed_mesh = 1;

    // Allocate space for the number of points
    npts = _npts;

    // Allocate an array of the edge points
    EdgePt *epts = new EdgePt[npts];
    for (int i = 0; i < npts; i++) {
      double t;
      edge->invEvalPoint(_X[i], &t);
      epts[i].p = _X[i];
      epts[i].t = t;
    }

    // Sort the edges
    qsort(epts, npts, sizeof(EdgePt), EdgePt::compare);

    // Retrieve the sorted points
    pts = new double[npts];
    X = new TMRPoint[npts];

    // Copy over the point locations
    for (int i = 0; i < npts; i++) {
      X[i] = epts[i].p;
      pts[i] = epts[i].t;
    }

    // Free the edge points
    delete[] epts;
  }
}

/*
  Destroy the mesh for this curve, and free the underlying data
*/
TMREdgeMesh::~TMREdgeMesh() {
  edge->decref();
  if (pts) {
    delete[] pts;
  }
  if (X) {
    delete[] X;
  }
  if (vars) {
    delete[] vars;
  }
}

/*
  Find the points along the edge for the mesh
*/
void TMREdgeMesh::mesh(TMRMeshOptions options, TMRElementFeatureSize *fs) {
  // Check if the mesh has already been allocated
  if (prescribed_mesh) {
    return;
  }

  int mpi_rank, mpi_size;
  MPI_Comm_size(comm, &mpi_size);
  MPI_Comm_rank(comm, &mpi_rank);

  // Get the source edge
  TMREdge *source, *copy;
  edge->getSource(&source);
  edge->getCopySource(&copy);

  // Figure out if there is a source edge and whether or not it has
  // been meshed.
  npts = -1;
  if (source && source != edge) {
    TMREdgeMesh *mesh;
    source->getMesh(&mesh);
    if (!mesh) {
      mesh = new TMREdgeMesh(comm, source);
      mesh->mesh(options, fs);
      source->setMesh(mesh);
    }

    // Retrieve the number of points along the source edge
    mesh->getMeshPoints(&npts, NULL, NULL);
  } else if (copy && copy != edge) {
    // Set the edge mesh
    TMREdgeMesh *mesh;
    copy->getMesh(&mesh);
    if (!mesh) {
      mesh = new TMREdgeMesh(comm, copy);
      mesh->mesh(options, fs);
      copy->setMesh(mesh);
    }

    // Retrieve the vertices
    TMRVertex *v1, *v2, *t;
    edge->getVertices(&v1, &v2);
    v1->getCopySource(&t);
    if (t) {
      v1 = t;
    }
    v2->getCopySource(&t);
    if (t) {
      v2 = t;
    }

    // Get the vertex copies. These must be set to copy from the
    // verties that the edge are set to copy from.
    TMRVertex *v1_copy, *v2_copy;
    copy->getVertices(&v1_copy, &v2_copy);
    v1_copy->getCopySource(&t);
    if (t) {
      v1_copy = t;
    }
    v2_copy->getCopySource(&t);
    if (t) {
      v2_copy = t;
    }

    // Check the orientation. Note that if no orientation is set,
    // then we don't copy the edge.
    npts = 0;
    if ((v1_copy == v1 && v2_copy == v2) || (v1_copy == v2 && v2_copy == v1)) {
      mesh->getMeshPoints(&npts, NULL, NULL);
    } else {
      fprintf(stderr,
              "TMREdgeMesh Error: Failed to copy mesh from copy edge source\n");
    }
  }

  if (mpi_rank == 0) {
    if (copy && copy != edge) {
      // Set the edge mesh
      TMREdgeMesh *mesh;
      copy->getMesh(&mesh);

      // Allocate space for the points/x locations
      pts = new double[npts];
      X = new TMRPoint[npts];

      // Get the orientation of the copy
      int orient = getEdgeCopyOrient(edge);
      if (orient == 0) {
        fprintf(stderr, "TMREdgeMesh Error: Copy edge is not set correctly\n");
      }

      int edge_index = mesh->npts - 2;
      if (orient > 0) {
        edge_index = 1;
      }
      for (int i = 1; i < npts - 1; i++, edge_index += orient) {
        int icode = edge->invEvalPoint(mesh->X[edge_index], &pts[i]);
        if (icode) {
          fprintf(stderr,
                  "TMREdgeMesh Error: Inverse evaluation failed with code %d\n",
                  icode);
        }
        edge->evalPoint(pts[i], &X[i]);
      }

      // Set the end points of the edge
      double tmin, tmax;
      edge->getRange(&tmin, &tmax);
      pts[0] = tmin;
      pts[npts - 1] = tmax;
    } else {
      // Get the limits of integration that will be used
      double tmin, tmax;
      edge->getRange(&tmin, &tmax);

      // Get the associated vertices
      TMRVertex *v1, *v2;
      edge->getVertices(&v1, &v2);

      if (!edge->isDegenerate()) {
        // Set the integration error tolerance
        double integration_eps = 1e-8;

        // Integrate along the curve to obtain the distance function such
        // that dist(tvals[i]) = int_{tmin}^{tvals[i]} ||d{C(t)}dt||_{2} dt
        int nvals;
        double *dist, *tvals;
        integrateEdge(edge, fs, tmin, tmax, integration_eps, &tvals, &dist,
                      &nvals);

        // Only compute the number of points if there is no source edge
        if (npts < 0) {
          // Compute the number of points along this curve
          npts = (int)(ceil(dist[nvals - 1]));
          if (npts < 2) {
            npts = 2;
          }

          // If we have an even number of points, increment by one to ensure
          // that we have an even number of segments along the boundary
          if (npts % 2 != 1) {
            npts++;
          }

          // If the start/end vertex are the same, then the minimum number
          // of points is 5
          if ((v1 == v2) && npts < 5) {
            npts = 5;
          }
        }

        // The average non-dimensional distance between points
        double d = dist[nvals - 1] / (npts - 1);

        // Allocate the parametric points that will be used
        pts = new double[npts];

        // Set the starting/end location of the points
        pts[0] = tmin;
        pts[npts - 1] = tmax;

        // Perform the integration so that the points are evenly spaced
        // along the curve
        for (int j = 1, k = 1; (j < nvals && k < npts - 1); j++) {
          while ((k < npts - 1) && (dist[j - 1] <= d * k && d * k < dist[j])) {
            double u = 0.0;
            if (dist[j] > dist[j - 1]) {
              u = (d * k - dist[j - 1]) / (dist[j] - dist[j - 1]);
            }
            pts[k] = tvals[j - 1] + (tvals[j] - tvals[j - 1]) * u;
            k++;
          }
        }

        // Free the integration result
        delete[] tvals;
        delete[] dist;
      } else {
        // This is a degenerate edge
        npts = 2;
        pts = new double[npts];
        pts[0] = tmin;
        pts[1] = tmax;
      }

      // Allocate the points
      X = new TMRPoint[npts];
      for (int i = 0; i < npts; i++) {
        edge->evalPoint(pts[i], &X[i]);
      }
    }
  }

  if (mpi_size > 1) {
    // Broadcast the number of points to all the processors
    MPI_Bcast(&npts, 1, MPI_INT, 0, comm);

    if (mpi_rank != 0) {
      pts = new double[npts];
      X = new TMRPoint[npts];
    }

    // Broadcast the parametric locations and points
    MPI_Bcast(pts, npts, MPI_DOUBLE, 0, comm);
    MPI_Bcast(X, npts, TMRPoint_MPI_type, 0, comm);
  }
}

/*
  Order the internal mesh points and return the number of owned points
  that were ordered.
*/
int TMREdgeMesh::setNodeNums(int *num) {
  if (!vars && pts) {
    int start = *num;

    // Retrieve the vertices
    TMRVertex *v1, *v2;
    edge->getVertices(&v1, &v2);

    // Allocate/set the node numbers
    vars = new int[npts];

    int success = 0;
    TMREdge *copy;
    edge->getCopySource(&copy);

    if (copy && copy != edge) {
      int orient = getEdgeCopyOrient(edge);

      if (orient) {
        TMREdgeMesh *copy_mesh;
        copy->getMesh(&copy_mesh);

        if (copy_mesh) {
          // Set the node numbers of the mesh we're copying from
          copy_mesh->setNodeNums(num);

          if (copy_mesh->npts == npts) {
            // We've successfully copied the edge
            success = 1;

            int edge_index = npts - 2;
            if (orient > 0) {
              edge_index = 1;
            }
            for (int i = 1; i < npts - 1; i++, edge_index += orient) {
              vars[i] = copy_mesh->vars[edge_index];
            }
          }
        } else {
          fprintf(stderr, "TMREdgeMesh Error: Copy mesh not created\n");
        }
      } else {
        fprintf(stderr,
                "TMREdgeMesh Error: Copy edge source is not set correctly\n");
      }
    } else {
      success = 1;

      // Set the internal node numbers
      for (int i = 1; i < npts - 1; i++) {
        vars[i] = *num;
        (*num)++;
      }
    }

    if (!success) {
      fprintf(stderr, "TMREdgeMesh Error: Node numbers not set on edge\n");
    }

    // Set the variable numbers at the end points
    v1->getNodeNum(&vars[0]);
    v2->getNodeNum(&vars[npts - 1]);

    return *num - start;
  }

  return 0;
}

void TMREdgeMesh::writeToVTK(const char *filename) {
  // Write out the vtk file
  FILE *fp = fopen(filename, "w");
  if (fp) {
    fprintf(fp, "# vtk DataFile Version 3.0\n");
    fprintf(fp, "vtk output\nASCII\n");
    fprintf(fp, "DATASET UNSTRUCTURED_GRID\n");

    // Write out the points
    fprintf(fp, "POINTS %d float\n", npts);
    for (int k = 0; k < npts; k++) {
      // Write out the point
      fprintf(fp, "%e %e %e\n", X[k].x, X[k].y, X[k].z);
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
  Retrieve the internal node numbers, associated with the same
  set of points/same order as the getMeshPoints code
*/
int TMREdgeMesh::getNodeNums(const int **_vars) {
  if (_vars) {
    *_vars = vars;
  }
  return npts;
}

/*
  Get the mesh points
*/
void TMREdgeMesh::getMeshPoints(int *_npts, const double **_pts,
                                TMRPoint **_X) {
  if (_npts) {
    *_npts = npts;
  }
  if (_pts) {
    *_pts = pts;
  }
  if (_X) {
    *_X = X;
  }
}
