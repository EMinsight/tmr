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

#include "TMROctant.h"

#include "TMRHashFunction.h"

/*
  Get the child id of the octant
*/
int TMROctant::childId() {
  int id = 0;
  const int32_t h = 1 << (TMR_MAX_LEVEL - level);

  id = id | ((x & h) ? 1 : 0);
  id = id | ((y & h) ? 2 : 0);
  id = id | ((z & h) ? 4 : 0);

  return id;
}

/*
  Return the sibling of the octant
*/
void TMROctant::getSibling(int id, TMROctant *sib) {
  const int32_t h = 1 << (TMR_MAX_LEVEL - level);

  int32_t xr = ((x & h) ? x - h : x);
  int32_t yr = ((y & h) ? y - h : y);
  int32_t zr = ((z & h) ? z - h : z);

  sib->block = block;
  sib->level = level;
  sib->info = 0;
  sib->x = ((id & 1) ? xr + h : xr);
  sib->y = ((id & 2) ? yr + h : yr);
  sib->z = ((id & 4) ? zr + h : zr);
}

/*
  Get the parent of the octant
*/
void TMROctant::parent(TMROctant *p) {
  if (level > 0) {
    p->block = block;
    p->level = level - 1;
    p->info = 0;
    const int32_t h = 1 << (TMR_MAX_LEVEL - level);

    p->x = x & ~h;
    p->y = y & ~h;
    p->z = z & ~h;
  } else {
    p->block = block;
    p->level = 0;
    p->info = 0;
    p->x = x;
    p->y = y;
    p->z = z;
  }
}

/*
  Get the face neighbour
*/
void TMROctant::faceNeighbor(int face, TMROctant *neighbor) {
  const int32_t h = 1 << (TMR_MAX_LEVEL - level);
  neighbor->block = block;
  neighbor->level = level;
  neighbor->info = 0;

  neighbor->x = x + ((face == 0) ? -h : (face == 1) ? h : 0);
  neighbor->y = y + ((face == 2) ? -h : (face == 3) ? h : 0);
  neighbor->z = z + ((face == 4) ? -h : (face == 5) ? h : 0);
}

/*
  Get the neighbor along an edge
*/
void TMROctant::edgeNeighbor(int edge, TMROctant *neighbor) {
  const int32_t h = 1 << (TMR_MAX_LEVEL - level);
  neighbor->block = block;
  neighbor->level = level;
  neighbor->info = 0;

  if (edge < 4) {
    // Edges parallel to the x-direction
    neighbor->x = x;
    if (edge % 2 == 0) {
      neighbor->y = y - h;
    } else {
      neighbor->y = y + h;
    }

    if (edge < 2) {
      neighbor->z = z - h;
    } else {
      neighbor->z = z + h;
    }
  } else if (edge < 8) {
    // Edges parallel to the y-direction
    neighbor->y = y;

    if (edge % 2 == 0) {
      neighbor->x = x - h;
    } else {
      neighbor->x = x + h;
    }

    if (edge < 6) {
      neighbor->z = z - h;
    } else {
      neighbor->z = z + h;
    }
  } else {
    // Edges parallel to the z-direction
    neighbor->z = z;

    if (edge % 2 == 0) {
      neighbor->x = x - h;
    } else {
      neighbor->x = x + h;
    }

    if (edge < 10) {
      neighbor->y = y - h;
    } else {
      neighbor->y = y + h;
    }
  }
}

/*
  Get the neighbor along a corner
*/
void TMROctant::cornerNeighbor(int corner, TMROctant *neighbor) {
  const int32_t h = 1 << (TMR_MAX_LEVEL - level);
  neighbor->block = block;
  neighbor->level = level;
  neighbor->info = 0;

  neighbor->x = x + (2 * (corner & 1) - 1) * h;
  neighbor->y = y + ((corner & 2) - 1) * h;
  neighbor->z = z + ((corner & 4) / 2 - 1) * h;
}

/*
  Compare two octants using the Morton enconding (z-ordering)

  This function returns -1 if self < octant, 0 if self == octant and 1
  if self > octant. Ties are broken by the level of the octant such
  that the octants will be sorted by location then level.
*/
int TMROctant::compare(const TMROctant *octant) const {
  // If these octants are on different blocks, we're done...
  if (block != octant->block) {
    return block - octant->block;
  }

  // Find the most significant bit
  uint32_t xxor = x ^ octant->x;
  uint32_t yxor = y ^ octant->y;
  uint32_t zxor = z ^ octant->z;
  uint32_t sor = xxor | yxor | zxor;

  // If there is no most-significant bit, then we are done
  if (sor == 0) {
    return level - octant->level;
  }

  // Check for the most-significant bit
  int discrim = 0;
  if (xxor > (sor ^ xxor)) {
    discrim = x - octant->x;
  } else if (yxor > (sor ^ yxor)) {
    discrim = y - octant->y;
  } else {
    discrim = z - octant->z;
  }

  if (discrim > 0) {
    return 1;
  } else if (discrim < 0) {
    return -1;
  }
  return 0;
}

/*
  Compare two octants to determine whether they have the same Morton
  position.
*/
int TMROctant::comparePosition(const TMROctant *octant) const {
  // If these octants are on different blocks, we're done...
  if (block != octant->block) {
    return block - octant->block;
  }

  // Find the most-significant bit
  uint32_t xxor = x ^ octant->x;
  uint32_t yxor = y ^ octant->y;
  uint32_t zxor = z ^ octant->z;
  uint32_t sor = xxor | yxor | zxor;

  // Note that here we do not distinguish between levels
  // Check for the most-significant bit
  int discrim = 0;
  if (xxor > (sor ^ xxor)) {
    discrim = x - octant->x;
  } else if (yxor > (sor ^ yxor)) {
    discrim = y - octant->y;
  } else {
    discrim = z - octant->z;
  }

  if (discrim > 0) {
    return 1;
  } else if (discrim < 0) {
    return -1;
  }
  return 0;
}

/*
  Compare two octants to determine whether they have the same Morton
  position and info.
*/
int TMROctant::compareNode(const TMROctant *octant) const {
  // If these octants are on different blocks, we're done...
  if (block != octant->block) {
    return block - octant->block;
  }

  // Find the most-significant bit
  uint32_t xxor = x ^ octant->x;
  uint32_t yxor = y ^ octant->y;
  uint32_t zxor = z ^ octant->z;
  uint32_t sor = xxor | yxor | zxor;

  // Note that here we do not distinguish between levels
  // Check for the most-significant bit
  int discrim = 0;
  if (xxor > (sor ^ xxor)) {
    discrim = x - octant->x;
  } else if (yxor > (sor ^ yxor)) {
    discrim = y - octant->y;
  } else {
    discrim = z - octant->z;
  }

  if (discrim > 0) {
    return 1;
  } else if (discrim < 0) {
    return -1;
  }
  return info - octant->info;
}

/*
  Determine whether the input octant is contained within the octant
  itself. This can be used to determine whether the given octant is a
  descendent of this object.
*/
int TMROctant::contains(TMROctant *oct) {
  const int32_t h = 1 << (TMR_MAX_LEVEL - level);

  // Check whether the octant lies within this octant
  if ((oct->block == block) && (oct->x >= x && oct->x < x + h) &&
      (oct->y >= y && oct->y < y + h) && (oct->z >= z && oct->z < z + h)) {
    return 1;
  }

  return 0;
}

/*
  Compare two octants within the same sub-tree
*/
static int compare_octants(const void *a, const void *b) {
  const TMROctant *ao = static_cast<const TMROctant *>(a);
  const TMROctant *bo = static_cast<const TMROctant *>(b);

  return ao->compare(bo);
}

/*
  Compare two octant positions
*/
static int compare_position(const void *a, const void *b) {
  const TMROctant *ao = static_cast<const TMROctant *>(a);
  const TMROctant *bo = static_cast<const TMROctant *>(b);

  return ao->comparePosition(bo);
}

/*
  Compare two octant nodes
*/
static int compare_nodes(const void *a, const void *b) {
  const TMROctant *ao = static_cast<const TMROctant *>(a);
  const TMROctant *bo = static_cast<const TMROctant *>(b);

  return ao->compareNode(bo);
}

/*
  Store a array of octants
*/
TMROctantArray::TMROctantArray(TMROctant *_array, int _size,
                               int _use_node_index) {
  array = _array;
  size = _size;
  max_size = size;
  is_sorted = 0;
  use_node_index = _use_node_index;
}

/*
  Destroy the octant array
*/
TMROctantArray::~TMROctantArray() { delete[] array; }

/*
  Duplicate the array and return the copy
*/
TMROctantArray *TMROctantArray::duplicate() {
  TMROctant *arr = new TMROctant[size];
  memcpy(arr, array, size * sizeof(TMROctant));

  TMROctantArray *dup = new TMROctantArray(arr, size, use_node_index);
  dup->is_sorted = is_sorted;

  return dup;
}

/*
  Sort the list and remove duplicates from the array of possible
  entries.
*/
void TMROctantArray::sort() {
  if (use_node_index) {
    qsort(array, size, sizeof(TMROctant), compare_nodes);

    // Now that the Octants are sorted, remove duplicates
    int i = 0;  // Location from which to take entries
    int j = 0;  // Location to place entries

    for (; i < size; i++, j++) {
      while ((i < size - 1) && (array[i].compareNode(&array[i + 1]) == 0)) {
        i++;
      }

      if (i != j) {
        array[j] = array[i];
      }
    }

    // The new size of the array
    size = j;
  } else {
    qsort(array, size, sizeof(TMROctant), compare_octants);

    // Now that the Octants are sorted, remove duplicates
    int i = 0;  // Location from which to take entries
    int j = 0;  // Location to place entries

    for (; i < size; i++, j++) {
      while ((i < size - 1) && (array[i].comparePosition(&array[i + 1]) == 0)) {
        i++;
      }

      if (i != j) {
        array[j] = array[i];
      }
    }

    // The new size of the array
    size = j;
  }

  is_sorted = 1;
}

/*
  Determine if the array contains the specified octant
*/
TMROctant *TMROctantArray::contains(TMROctant *q, int use_position) {
  if (!is_sorted) {
    is_sorted = 1;
    sort();
  }

  if (use_node_index) {
    return (TMROctant *)bsearch(q, array, size, sizeof(TMROctant),
                                compare_nodes);
  } else {
    // Search for nodes - these will share the same
    if (use_position) {
      return (TMROctant *)bsearch(q, array, size, sizeof(TMROctant),
                                  compare_position);
    }

    // Search the array for an identical element
    return (TMROctant *)bsearch(q, array, size, sizeof(TMROctant),
                                compare_octants);
  }
}

/*
  Merge the entries of two arrays
*/
void TMROctantArray::merge(TMROctantArray *list) {
  if (!is_sorted) {
    sort();
  }
  if (!list->is_sorted) {
    list->sort();
  }

  // Keep track of the number of duplicates
  int nduplicates = 0;

  // Scan through the list and determine the number
  // of duplicates
  int j = 0, i = 0;
  for (; i < size; i++) {
    while ((j < list->size) && (list->array[j].compare(&array[i]) < 0)) {
      j++;
    }
    if (j >= list->size) {
      break;
    }
    if (array[i].compare(&list->array[j]) == 0) {
      nduplicates++;
    }
  }

  // Compute the required length of the new array
  int len = size + list->size - nduplicates;

  // Allocate a new array if required
  if (len > max_size) {
    max_size = len;

    TMROctant *temp = array;
    array = new TMROctant[max_size];
    memcpy(array, temp, size * sizeof(TMROctant));

    // Free the old array
    delete[] temp;
  }

  // Set the pointer to the end of the new array
  int end = len - 1;

  // Copy the new array back
  i = size - 1;
  j = list->size - 1;
  while (i >= 0 && j >= 0) {
    if (array[i].compare(&list->array[j]) > 0) {
      array[end] = array[i];
      end--, i--;
    } else if (list->array[j].compare(&array[i]) > 0) {
      array[end] = list->array[j];
      end--, j--;
    } else {  // b[j] == a[i]
      array[end] = array[i];
      end--, j--, i--;
    }
  }

  // Only need to copy over remaining elements from b - if any
  while (j >= 0) {
    array[j] = list->array[j];
    j--;
  }

  // Set the new size of the array
  size = len;
}

/*
  Get the underlying array
*/
void TMROctantArray::getArray(TMROctant **_array, int *_size) {
  if (_array) {
    *_array = array;
  }
  if (_size) {
    *_size = size;
  }
}

/*
  Create an queue of octants
*/
TMROctantQueue::TMROctantQueue() {
  root = tip = NULL;
  num_elems = 0;
}

/*
  Free the queue
*/
TMROctantQueue::~TMROctantQueue() {
  OctQueueNode *node = root;
  while (node) {
    OctQueueNode *tmp = node;
    node = node->next;
    delete tmp;
  }
}

/*
  Get the length of the octant queue
*/
int TMROctantQueue::length() { return num_elems; }

/*
  Push a value onto the octant queue
*/
void TMROctantQueue::push(TMROctant *oct) {
  if (!tip) {
    root = new OctQueueNode();
    root->oct = *oct;
    tip = root;
  } else {
    tip->next = new OctQueueNode();
    tip->next->oct = *oct;
    tip = tip->next;
  }
  num_elems++;
}

/*
  Pop a value from the octant queue
*/
TMROctant TMROctantQueue::pop() {
  if (!root) {
    return TMROctant();
  } else {
    num_elems--;
    TMROctant temp = root->oct;
    OctQueueNode *tmp = root;
    root = root->next;
    delete tmp;
    if (num_elems == 0) {
      tip = NULL;
    }
    return temp;
  }
}

/*
  Convert the queue to an array
*/
TMROctantArray *TMROctantQueue::toArray() {
  // Allocate the array
  TMROctant *array = new TMROctant[num_elems];

  // Scan through the queue and retrieve the octants
  OctQueueNode *node = root;
  int index = 0;
  while (node) {
    array[index] = node->oct;
    node = node->next;
    index++;
  }

  // Create the array object
  TMROctantArray *list = new TMROctantArray(array, num_elems);
  return list;
}

/*
  A hash for octants

  Note that this isn't a true hash table since it does not associate
  elements with other values. It is used to create unique lists of
  elements and nodes within the octree mesh.
*/
TMROctantHash::TMROctantHash(int _use_node_index) {
  use_node_index = _use_node_index;
  num_elems = 0;
  num_buckets = min_num_buckets;
  hash_buckets = new OctHashNode *[num_buckets];
  memset(hash_buckets, 0, num_buckets * sizeof(OctHashNode *));
}

/*
  Free the memory allocated by the octant hash
*/
TMROctantHash::~TMROctantHash() {
  // Free all the elements in the hash
  for (int i = 0; i < num_buckets; i++) {
    OctHashNode *node = hash_buckets[i];
    while (node) {
      // Delete the old guy
      OctHashNode *tmp = node;
      node = node->next;
      delete tmp;
    }
  }

  delete[] hash_buckets;
}

/*
  Covert the hash table to an array
*/
TMROctantArray *TMROctantHash::toArray() {
  // Create an array of octants
  TMROctant *array = new TMROctant[num_elems];

  // Loop over all the buckets
  for (int i = 0, index = 0; i < num_buckets; i++) {
    // Get the hash bucket and extract all the elements from this
    // bucket into the array
    OctHashNode *node = hash_buckets[i];

    while (node) {
      array[index] = node->oct;
      index++;
      node = node->next;
    }
  }

  // Create an array object and add it to the list
  TMROctantArray *list = new TMROctantArray(array, num_elems, use_node_index);
  return list;
}

/*
  Add an octant to the hash table.

  A new octant is added only if it is unique within the list of
  objects. The function returns true if the octant is added, and false
  if it already exists within the hash table.

  input:
  oct:   the octant that may be added to the hash table

  returns:
  true if the octant is added, false if it is not
*/
int TMROctantHash::addOctant(TMROctant *oct) {
  if (num_elems > 10 * num_buckets) {
    // Redistribute the octants to new buckets
    int num_old_buckets = num_buckets;
    num_buckets = 2 * num_buckets;
    OctHashNode **new_buckets = new OctHashNode *[num_buckets];
    memset(new_buckets, 0, num_buckets * sizeof(OctHashNode *));

    // Keep track of the end bucket
    OctHashNode **end_buckets = new OctHashNode *[num_buckets];

    // Redistribute the octant nodes based on the new
    // number of buckets within the hash data structure
    for (int i = 0; i < num_old_buckets; i++) {
      OctHashNode *node = hash_buckets[i];
      while (node) {
        int bucket = getBucket(&(node->oct));

        // If this is the first new bucket, create
        // the new node
        if (!new_buckets[bucket]) {
          new_buckets[bucket] = new OctHashNode;
          new_buckets[bucket]->oct = node->oct;
          end_buckets[bucket] = new_buckets[bucket];
        } else {
          end_buckets[bucket]->next = new OctHashNode;
          end_buckets[bucket]->next->oct = node->oct;
          end_buckets[bucket] = end_buckets[bucket]->next;
        }

        // Delete the old guy
        OctHashNode *tmp = node;

        // Increment the node to the next hash
        node = node->next;

        // Free the node
        delete tmp;
      }
    }

    delete[] end_buckets;
    delete[] hash_buckets;
    hash_buckets = new_buckets;
  }

  int bucket = getBucket(oct);

  // If no octant has been added to the bucket,
  // create a new bucket
  if (!hash_buckets[bucket]) {
    hash_buckets[bucket] = new OctHashNode;
    hash_buckets[bucket]->oct = *oct;
    num_elems++;
    return 1;
  } else {
    // Get the head node for the corresponding bucket
    OctHashNode *node = hash_buckets[bucket];
    if (use_node_index) {
      while (node) {
        // The octant is in the list, quit now and return false
        if (node->oct.compareNode(oct) == 0) {
          return 0;
        }

        // If the next node does not exist, quit
        // while node is the last node in the linked
        // list
        if (!node->next) {
          break;
        }
        node = node->next;
      }
    } else {
      while (node) {
        // The octant is in the list, quit now and return false
        if (node->oct.compare(oct) == 0) {
          return 0;
        }

        // If the next node does not exist, quit
        // while node is the last node in the linked
        // list
        if (!node->next) {
          break;
        }
        node = node->next;
      }
    }

    // Add the octant as the last node
    node->next = new OctHashNode;
    node->next->oct = *oct;
    num_elems++;
  }

  return 1;
}

/*
  Get a bucket to place the octant in.

  This code creates a value based on the octant location within the
  mesh and then takes the remainder of the number of buckets.
*/
int TMROctantHash::getBucket(TMROctant *oct) {
  // The has value
  uint32_t val = 0;

  if (use_node_index) {
    uint32_t u = 0, v = 0, w = 0, x = 0;
    u = oct->block;
    v = (1 << TMR_MAX_LEVEL) + oct->x;
    w = (1 << TMR_MAX_LEVEL) + oct->y;
    x = (1 << TMR_MAX_LEVEL) + oct->z;
    // y = oct->info;

    // Compute the hash value
    // val = TMRIntegerFiveTupleHash(u, v, w, x, y);
    val = TMRIntegerFourTupleHash(u, v, w, x);
  } else {
    uint32_t u = 0, v = 0, w = 0, x = 0;
    u = oct->block;
    v = (1 << TMR_MAX_LEVEL) + oct->x;
    w = (1 << TMR_MAX_LEVEL) + oct->y;
    x = (1 << TMR_MAX_LEVEL) + oct->z;

    // Compute the hash value
    val = TMRIntegerFourTupleHash(u, v, w, x);
  }

  // Compute the bucket value
  int bucket = val % num_buckets;

  return bucket;
}
