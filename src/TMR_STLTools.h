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

#ifndef TMR_STL_TOOLS_H
#define TMR_STL_TOOLS_H

#include "TACSBVec.h"
#include "TMROctForest.h"

/*
  The following file contains the tools required to generate an STL
  file for visualization from the data contained in the TMROctForest
  object.

  The generation of the STL file requires two steps:

  1) The data is written in parallel across all processors to an
  intermediate binary file format that contains the point loops

  2) The point loop data file is post-processed to create the actual
  STL file in the standard ASCII format.

  This two-step process is required because the design variables are
  distributed across processors.
*/

int TMR_GenerateSTLTriangles(int root, TMROctForest *filter, TACSBVec *x,
                             int x_offset, double cutoff, int *_ntris,
                             TMR_STLTriangle **_tris);

/*
  Given the design variables, write out a binary file containing the
  intersection of the level set with the grid using a combination of
  marching cubes/boundary face intersections.

  Notes: The filename is a global name that must be the same on all
  processors. The file is written using MPI/IO. Only one file is
  generated.

  input:
  filename:   the filename
  filter:     the octant forest
  x:          the vertex-values of the design variables
  x_offset:   the offset variable values
  cutoff      the level set design variable value

  binary output data format:
  1 integer representing the number of triangles = ntri
  3*ntri doubles representing the cell-vertices in CCW ordering
*/
extern int TMR_GenerateBinFile(const char *filename, TMROctForest *filter,
                               TACSBVec *x, int x_offset, double cutoff);

/*
  Take the binary file generated from above and convert to the .STL
  data format (in ASCII).

  Note that this is a serial code and should only be called by a
  single processor.
*/
extern int TMR_ConvertBinToSTL(const char *binfile, const char *stlfile);

#endif  // TMR_STL_TOOLS_H
