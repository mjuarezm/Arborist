// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file pretree.cc

   @brief Methods implementing production and consumption of the pre-tree.

   @author Mark Seligman

 */

#include "pretree.h"
#include "predictor.h"
#include "response.h"
#include "sample.h"
#include "samplepred.h"

//#include <iostream>
using namespace std;

// Leaf accumulators are not reused, so there is no need to record
// sample indices or ranks until the final row has been visited.
//
// Records terminal-node information for elements of the next level in the pre-tree.
// A terminal node may later be converted to non-terminal if found to be splitable.
// Initializing as terminal by default offers several advantages, such as avoiding
// the need to revise dangling non-terminals from an earlier level.
//

unsigned int PreTree::nRow = 0;
unsigned int PreTree::heightEst = 0;

/**
   @brief Caches the row count and computes an initial estimate of node count.

   @param _nRow is the number of observations.

   @param _nSamp is the number of samples.

   @param _minH is the minimal splitable index node size.

   @return void.
 */
void PreTree::Immutables(unsigned int _nRow, unsigned int _nSamp, unsigned int _minH) {
  nRow = _nRow;

  // Static initial estimate of pre-tree heights employs a minimal enclosing
  // balanced tree.  This is probably naive, given that decision trees
  // are not generally balanced.
  //
  // In any case, 'heightEst' is re-estimated following construction of the
  // first PreTree.  Hence the value is not really immutable.  Nodes can also
  // be reallocated during the interlevel pass as needed.
  //
  unsigned twoL = 1; // 2^level, beginning from level zero (root).
  while (twoL * _minH < _nSamp) {
    twoL <<= 1;
  }

  // Terminals plus accumulated nonterminals.
  heightEst = (twoL << 2); // - 1, for exact count.
}


void PreTree::DeImmutables() {
  nRow = heightEst = 0;
}


/**
   @brief Per-tree initializations.

   @param _treeBlock is the number of PreTree initialize.

   @return void.
 */
PreTree::PreTree() {
  nodeCount = heightEst;   // Initial height estimate.
  nodeVec = new PTNode[nodeCount];
  nodeVec[0].id = 0;
  nodeVec[0].lhId = -1;
  const unsigned int slotBits = 8 * sizeof(unsigned int);
  inBag = new unsigned int [(nRow + slotBits - 1) / slotBits];
  treeHeight = leafCount = 1;
  treeBitOffset = 0;
  treeSplitBits = BitFactory();
}


/**
   @brief Refines the height estimate using the actual height of a
   constructed PreTree.

   @param height is an actual height value.

   @return void.
 */
void PreTree::RefineHeight(unsigned int height) {
  while (heightEst <= height) // Assigns next power-of-two above 'height'.
    heightEst <<= 1;
}


/**
   @brief Invokes Sample methods to initialize bag-related data sets and to stage.

   @param samplePred outputs the staged sample/predictor object.

   @param sum outputs the sum of bagged response values.

   @return in-bag count for tree.
 */
SplitPred *PreTree::BagRows(const PredOrd *predOrd, SamplePred *samplePred, int &_bagCount, double &_sum) {
  SplitPred *splitPred;
  sample = Response::StageSamples(predOrd, inBag, samplePred, splitPred, _sum, _bagCount);

  bagCount = _bagCount;
  sample2PT = new int[bagCount];
  for (int i = 0; i < bagCount; i++) {
    sample2PT[i] = 0; // Unique root nodes zero.
  }

  return splitPred;
}

/**
   @brief Allocates the bit string for the current (pre)tree and initializes to false.

    // Should be wide enough to hold all factor bits for an entire tree:
    //    #nodes * width of widest factor.
    //

   @return pointer to allocated vector.
*/
bool *PreTree::BitFactory(int _bitLength) {
  bool *tsb = 0;
  if (Predictor::NPredFac() > 0) {
    bitLength = _bitLength == 0 ? nodeCount * Predictor::MaxFacCard() : _bitLength;
    tsb = new bool[bitLength];
    for (int i = 0; i < bitLength; i++)
      tsb[i] = false;
  }

  return tsb;
}


/**
   @brief Per-tree finalizer.
 */
PreTree::~PreTree() {
  if (Predictor::NPredFac() > 0) {
    delete [] treeSplitBits;
  }
  delete sample;
  delete [] nodeVec;
  delete [] sample2PT;
  delete [] inBag;
}

/**
   @brief Speculatively sets the two offspring slots as terminal lh, rh.

   @param _parId is the pretree index of the parent.

   @param ptLH outputs the left-hand node index.

   @param ptRH outputs the right-hand node index.

   @return void, with output reference parameters.
*/
void PreTree::TerminalOffspring(int _parId, int &ptLH, int &ptRH) {
  ptLH = treeHeight++;
  nodeVec[_parId].lhId = ptLH;
  nodeVec[ptLH].id = ptLH;
  nodeVec[ptLH].lhId = -1;

  ptRH = treeHeight++;
  nodeVec[ptRH].id = ptRH;
  nodeVec[ptRH].lhId = -1;

  leafCount += 2;
}

/**
   @brief Fills in some fields for (generic) node found splitable.

   @param _id is the node index.

   @param _info is the information content.

   @param _splitVal is the splitting value.

   @param _predIdx is the splitting predictor index.

   @return void.
*/
void PreTree::NonTerminal(int _id, double _info, double _splitVal, int _predIdx) {
  PTNode *ptS = &nodeVec[_id];
  ptS->predIdx = _predIdx;
  ptS->info = _info;
  ptS->splitVal = _splitVal;
  leafCount--;
}

double PreTree::Replay(SamplePred *samplePred, int predIdx, int level, int start, int end, int ptId) {
  return samplePred->Replay(sample2PT, predIdx, level, start, end, ptId);
}

/**
   @brief Updates the high watermark for the preTree vector.  Forces a reallocation to
   twice the existing size, if necessary.

   N.B.:  reallocations incur considerable resynchronization costs if precipitated
   from the coprocessor.

   @param levelWidth is the count of nodes in the next level.

   @return void.
*/
void PreTree::CheckStorage(int splitNext, int leafNext) {
  if (treeHeight + splitNext + leafNext > nodeCount)
    ReNodes();
  if (Predictor::NPredFac() > 0) {
    if (treeBitOffset + splitNext * Predictor::MaxFacCard() > bitLength)
      ReBits();
  }
}

/**
 @brief Guestimates safe height by doubling high watermark.

 @return void.
*/
void PreTree::ReNodes() {
  nodeCount <<= 1;
  PTNode *PTtemp = new PTNode[nodeCount];
  for (int i = 0; i < treeHeight; i++)
    PTtemp[i] = nodeVec[i];

  delete [] nodeVec;
  nodeVec = PTtemp;
}

/**
 @brief Tree split bits accumulate, so data must be copied on realloc.

 @return void.
*/
void PreTree::ReBits() {
  bool *TStemp = BitFactory(bitLength << 1);
  for (int i = 0; i < treeBitOffset; i++)
    TStemp[i] = treeSplitBits[i];
  delete [] treeSplitBits;
  treeSplitBits = TStemp;
}


/**
  @brief Writes factor bits from all levels into contiguous vector and resets bit state.
  @param outBits outputs the split-value bit vector.
  
  @return void, with output vector parameter.
*/
//
// N.B.:  Should not be called unless FacWidth() > 0.
//
void PreTree::ConsumeSplitBits(int outBits[]) {
  for (int i = 0; i < treeBitOffset; i++) {
    outBits[i] = treeSplitBits[i]; // Upconverts to integer type for output to front end.
  }
  delete [] treeSplitBits;
  treeSplitBits = 0;
  treeBitOffset = 0;
}


/**
   @brief Consumes pretree nodes into the vectors needed by the decision tree.

   @param nodeVal outputs splitting predictor / leaf extent : nonterminal / terminal.

   @param numVec outputs splitting value / leaf score : nonterminal / terminal.

   @param bumpVec outputs the left-hand node increment.

   @return tree size equal to the maximum offset filled in, also output parameter vectors.
*/
void PreTree::ConsumeNodes(int nodeVal[], double numVec[], int bumpVec[]) {
  sample->Scores(sample2PT, treeHeight, nodeVal, numVec); // Virtual call.
  for (int idx = 0; idx < treeHeight; idx++) {
    nodeVec[idx].Consume(nodeVal[idx], numVec[idx], bumpVec[idx]);
  }
}


/**
   @brief Consumes the node fields.

   @param pred is the predictor associated with a split.

   @param num is the splitting value, for a split, otherwise the score, for a leaf,
   filled in elsewhere.

   @param bump is the distance to the left-hand subnode, if a split, otherwize zero.

   @return void.
 */
void PTNode::Consume(int &pred, double &num, int &bump) {
  if (lhId > 0) { // Consumes splits.
    pred = predIdx;
    num = splitVal;
    bump = lhId - id;
  }
  else { // Consumes leaves.  Scores already written into num.
    bump = 0;
  }
}


/**
   @brief Reports field values useful for quantile computation.

   @return tree index referenced by sample index passed.
 */
int PreTree::QuantileFields(int sIdx, int &sCount, unsigned int &rank) const {
  sCount = sample->QuantileFields(sIdx, rank);

  return sample2PT[sIdx];
}
