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

#ifndef TMR_OCTANT_H
#define TMR_OCTANT_H

#include <stdlib.h>

#include "TMRBase.h"

/*
  The TMR Octant class

  This class defines an octant that is used to order both the elements
  and nodes within the mesh. The methods can be used to compare
  octants, find the parent, child identification number, and find
  neighbours.
*/
class TMROctant {
 public:
  int childId();
  void getSibling(int id, TMROctant *sib);
  void parent(TMROctant *parent);
  void faceNeighbor(int face, TMROctant *neighbor);
  void edgeNeighbor(int edge, TMROctant *neighbor);
  void cornerNeighbor(int corner, TMROctant *neighbor);
  int compare(const TMROctant *oct) const;
  int comparePosition(const TMROctant *oct) const;
  int compareNode(const TMROctant *oct) const;
  int contains(TMROctant *oct);

  int32_t block;    // The block that owns this octant
  int32_t x, y, z;  // The x,y,z coordinates
  int32_t tag;      // A tag to store additional data
  int16_t level;    // The refinement level
  int16_t info;     // Info about the octant
};

/*
  A array of octants that may or may not be sorted

  When the array is sorted, the octants are made unique by discarding
  octants with a smaller level (that have larger side lengths).  After
  the array is sorted, it is searchable either based on elements (when
  use_nodes=0) or by node (use_nodes=1). The difference is that the
  node search ignores the mesh level.
*/
class TMROctantArray {
 public:
  TMROctantArray(TMROctant *array, int size, int _use_node_index = 0);
  ~TMROctantArray();

  TMROctantArray *duplicate();
  void getArray(TMROctant **_array, int *_size);
  void sort();
  TMROctant *contains(TMROctant *q, int use_nodes = 0);
  void merge(TMROctantArray *list);

 private:
  int use_node_index;
  int is_sorted;
  int size, max_size;
  TMROctant *array;
};

/*
  Create a queue of octants

  This class defines a queue of octants that are used for the balance
  and coarsen operations.
*/
class TMROctantQueue {
 public:
  TMROctantQueue();
  ~TMROctantQueue();

  int length();
  void push(TMROctant *oct);
  TMROctant pop();
  TMROctantArray *toArray();

 private:
  // Class that defines an element within the queue
  class OctQueueNode {
   public:
    OctQueueNode() { next = NULL; }
    TMROctant oct;
    OctQueueNode *next;
  };

  // Keep track of the number of elements in the queue
  int num_elems;
  OctQueueNode *root, *tip;
};

/*
  Build a hash table based on the Morton ordering

  This object enables the creation of a unique set of octants such
  that no two have the same position/level combination. This hash
  table can then be made into an array of unique elements or nodes.
*/
class TMROctantHash {
 public:
  TMROctantHash(int _use_node_index = 0);
  ~TMROctantHash();

  TMROctantArray *toArray();
  int addOctant(TMROctant *oct);

 private:
  // The minimum bucket size
  static const int min_num_buckets = (1 << 12) - 1;

  class OctHashNode {
   public:
    OctHashNode() { next = NULL; }
    TMROctant oct;
    OctHashNode *next;
  };

  // Keep track of whether to use a node-based search
  int use_node_index;

  // Keep track of the bucket size
  int num_buckets;
  OctHashNode **hash_buckets;

  // Keep track of the number of elements
  int num_elems;

  // Get the buckets to place the octant in
  int getBucket(TMROctant *oct);
};

#endif  // TMR_OCTANT_H
