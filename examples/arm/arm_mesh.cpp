#include <math.h>
#include <stdio.h>

#include "TMRBspline.h"
#include "TMRFaceMesh.h"
#include "TMRMesh.h"
#include "TMRNativeTopology.h"
#include "TMRQuadForest.h"
#include "TMRTopology.h"

TMREdge *createSemiCircle(TMRPoint center, double r, double theta) {
  // Set the points and weights for the B-spline circle
  const int nctl = 5;
  const int ku = 3;
  TMRPoint p[nctl];
  double wts[nctl];
  memset(p, 0, nctl * sizeof(TMRPoint));

  for (int k = 0; k < nctl; k++) {
    p[k] = center;
  }

  double Tu[] = {0.0, 0.0, 0.0, 0.5, 0.5, 1.0, 1.0, 1.0};

  // Set the weights
  double sqrt2 = 1.0 / sqrt(2.0);

  // Compute the sine/cosine
  double c = cos(theta);
  double s = sin(theta);

  // Use the transformation
  // [ c  -s ]
  // [ s   c ]

  // c + (r,0)
  p[0].x += r * c;
  p[0].y += r * s;
  wts[0] = 1.0;

  // c + (r,r)
  p[1].x += (c - s) * r;
  p[1].y += (c + s) * r;
  wts[1] = sqrt2;

  // c + (0,r)
  p[2].x -= s * r;
  p[2].y += c * r;
  wts[2] = 1.0;

  // c + (-r,r)
  p[3].x -= (c + s) * r;
  p[3].y += (c - s) * r;
  wts[3] = sqrt2;

  // c + (-r,0)
  p[4].x -= c * r;
  p[4].y -= s * r;
  wts[4] = 1.0;

  // Create the circle
  TMRBsplineCurve *curve = new TMRBsplineCurve(nctl, ku, Tu, wts, p);

  // Return the curve
  return new TMREdgeFromCurve(curve);
}

/*
  Create a line between two points
*/
TMREdge *createLine(TMRPoint p1, TMRPoint p2) {
  TMRPoint p[2];
  p[0] = p1;
  p[1] = p2;
  TMRCurve *bspline = new TMRBsplineCurve(2, 2, p);
  return new TMREdgeFromCurve(bspline);
}

/*
  Create a line between two specified vertices
*/
TMREdge *createLine(TMRVertex *v1, TMRVertex *v2) {
  TMRPoint p[2];
  v1->evalPoint(&p[0]);
  v2->evalPoint(&p[1]);
  TMRBsplineCurve *bspline = new TMRBsplineCurve(2, 2, p);
  TMREdge *edge = new TMREdgeFromCurve(bspline);
  edge->setVertices(v1, v2);
  return edge;
}

TMRTopology *setUpTopology(MPI_Comm comm, double r1, double r2, double L,
                           double t, double htarget) {
  TMRPoint p1;
  p1.x = p1.y = p1.z = 0.0;

  TMRPoint p2;
  p2.x = L;
  p2.y = p2.z = 0.0;

  TMRPoint p3;
  p3.x = 0.0;
  p3.y = r1 + t;
  p3.z = 0.0;

  TMRPoint p4;
  p4.x = L;
  p4.y = r2 + t;
  p4.z = 0.0;

  TMRPoint p5;
  p5.x = 0.0;
  p5.y = -(r1 + t);
  p5.z = 0.0;

  TMRPoint p6;
  p6.x = L;
  p6.y = -(r2 + t);
  p6.z = 0.0;

  // Create the planar surface
  const int nu = 4, ku = 4;
  const int nv = 4, kv = 4;
  TMRPoint pts[nu * nv];

  for (int j = 0; j < nv; j++) {
    for (int i = 0; i < nu; i++) {
      double u = 1.0 * i / (nu - 1);
      double v = 1.0 * j / (nv - 1);
      pts[nu * j + i].x = -20.0 + 40.0 * u;
      pts[nu * j + i].y = -10.0 + 20.0 * v;
      pts[nu * j + i].z = 0.0;
    }
  }

  TMRBsplineSurface *surf = new TMRBsplineSurface(nu, nv, ku, kv, pts);
  TMRFace *face = new TMRFaceFromSurface(surf);
  face->incref();

  // Set the curves that form the outline of the bracket
  TMREdge *outer1 = createSemiCircle(p1, r1 + t, 0.5 * M_PI);
  TMREdge *outer2 = createSemiCircle(p2, r2 + t, 1.5 * M_PI);
  TMREdge *line1 = createLine(p3, p4);
  TMREdge *line2 = createLine(p5, p6);

  TMREdge *inner11 = createSemiCircle(p1, r1, 0.0);
  TMREdge *inner12 = createSemiCircle(p1, r1, M_PI);
  TMREdge *inner21 = createSemiCircle(p2, r2, 0.0);
  TMREdge *inner22 = createSemiCircle(p2, r2, M_PI);

  // Create the vertices
  int num_vertices = 8;
  TMRVertex *v[8];
  v[0] = new TMRVertexFromEdge(outer1, 0.0);
  v[1] = new TMRVertexFromEdge(outer1, 1.0);
  v[2] = new TMRVertexFromEdge(outer2, 0.0);
  v[3] = new TMRVertexFromEdge(outer2, 1.0);

  // Create the start/end vertices for each curve
  v[4] = new TMRVertexFromEdge(inner11, 0.0);
  v[5] = new TMRVertexFromEdge(inner12, 0.0);
  v[6] = new TMRVertexFromEdge(inner21, 0.0);
  v[7] = new TMRVertexFromEdge(inner22, 0.0);

  // Set the vertices into the lines
  outer1->setVertices(v[0], v[1]);
  outer2->setVertices(v[2], v[3]);
  line1->setVertices(v[0], v[3]);
  line2->setVertices(v[1], v[2]);

  // Set the vertices for the inner circles
  inner11->setVertices(v[4], v[5]);
  inner12->setVertices(v[5], v[4]);

  inner21->setVertices(v[5], v[6]);
  inner22->setVertices(v[6], v[7]);

  // Create the curves
  int dir[4];
  int num_edges = 8;
  TMREdge *edges[8];
  edges[0] = outer1;
  dir[0] = 1;
  edges[1] = line2;
  dir[1] = 1;
  edges[2] = outer2;
  dir[2] = 1;
  edges[3] = line1;
  dir[3] = -1;

  // Add the outer curve loop
  face->addEdgeLoop(1, new TMREdgeLoop(4, edges, dir));

  // Add the inner curve segments
  edges[4] = inner12;
  dir[0] = -1;
  edges[5] = inner11;
  dir[1] = -1;
  face->addEdgeLoop(1, new TMREdgeLoop(2, &edges[4], dir));

  // Add the inner curve segments
  edges[6] = inner21;
  dir[0] = -1;
  edges[7] = inner22;
  dir[1] = -1;
  face->addEdgeLoop(1, new TMREdgeLoop(2, &edges[6], dir));

  TMREntity::setTolerances(1e-14, 1e-14);

  // Create the geometry object
  TMRModel *geo = new TMRModel(num_vertices, v, num_edges, edges, 1, &face);
  geo->incref();

  // Create the mesh
  TMRMesh *mesh = new TMRMesh(comm, geo);
  mesh->incref();

  // Mesh the geometry
  TMRMeshOptions options;
  mesh->mesh(options, htarget);

  // Write the surface mesh to the VTK file
  TMRFaceMesh *surf_mesh;
  face->getMesh(&surf_mesh);
  surf_mesh->writeToVTK("quads.vtk");

  // Free the objects
  mesh->decref();
  geo->decref();

  return NULL;
}

int main(int argc, char *argv[]) {
  MPI_Init(&argc, &argv);
  TMRInitialize();

  // Set the communicator
  MPI_Comm comm = MPI_COMM_WORLD;

  int mpi_rank;
  MPI_Comm_rank(comm, &mpi_rank);

  // Create the dimensions of the part
  double L = 10.0;
  double t = 2.0;
  double r1 = 2.0;
  double r2 = 1.0;

  // Create the topology
  double htarget = 0.1;

  for (int i = 0; i < argc; i++) {
    if (sscanf(argv[i], "h=%lf", &htarget) == 1) {
      break;
    }
  }

  printf("Target element length = %f\n", htarget);

  setUpTopology(comm, r1, r2, L, t, htarget);

  TMRFinalize();
  MPI_Finalize();

  return (0);
}
