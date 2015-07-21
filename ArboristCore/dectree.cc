// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file dectree.cc

   @brief Methods for building and walking the decision tree.

   @author Mark Seligman

   These methods are mostly mechanical.  Several methods are tasked
   with populating or depopulating tree-related data structures.  The
   tree-walking methods are clones of one another, with slight variations
   based on response or predictor type.
 */


#include "predictor.h"
#include "dectree.h"
#include "response.h"
#include "pretree.h"
#include "quant.h"

//#include <iostream>
using namespace std;

int DecTree::nTree = -1; // Immutable
int DecTree::forestSize = -1; // Derived from PreTree consumption.

unsigned int DecTree::nRow = -1; // et seq.:  observation-derived immutables.
int DecTree::nPred = -1;
int DecTree::nPredNum = -1;
int DecTree::nPredFac = -1;

int *DecTree::treeOriginForest = 0; // Output to front-end.
int *DecTree::treeSizes = 0; // Internal use only.
int **DecTree::predTree = 0;
double **DecTree::splitTree = 0;
int **DecTree::bumpTree = 0;

// Nonzero iff factors appear in decision tree.
//
int *DecTree::treeFacWidth = 0;
int **DecTree::treeFacSplits = 0;

int* DecTree::facSplitForest = 0; // Bits as integers:  alignment.
int *DecTree::facOffForest = 0;
double *DecTree::predInfo = 0;
int *DecTree::predForest = 0;
double *DecTree::numForest = 0;
int *DecTree::bumpForest = 0;
unsigned int *DecTree::inBag = 0;


/**
   @brief Sets per-session immutables derived from the observations.  Essential for initilalizing separate prediction sessions.
 */
void DecTree::ObsImmutables(int _nRow, int _nPred, int _nPredNum, int _nPredFac) {
  nRow = _nRow;
  nPred = _nPred;
  nPredNum = _nPredNum;
  nPredFac = _nPredFac;
}


/**
   @brief Unsets per-session static values.
 */
void DecTree::ObsDeImmutables() {
  nRow = nPred = nPredNum = nPredFac = -1;
}


/**
   @brief Lights off the initializations for building decision trees.

   @param _nTree is the number of trees requested.

   @return void.

 */
void DecTree::FactoryTrain(int _nTree) {
  nTree = _nTree;
  forestSize = 0;
  treeOriginForest = new int[nTree];
  treeSizes = new int[nTree];
  predInfo = new double[nPred];
  predTree = new int*[nTree];
  splitTree = new double*[nTree];
  bumpTree = new int*[nTree];
  treeFacWidth = new int[nTree]; // Factor width counts of individual trees.
  treeFacSplits = new int* [nTree]; // Tree-based factor split values.
  for (int i = 0; i < nPred; i++)
    predInfo[i] = 0.0;
  for (int i = 0; i < nTree; i++)
    treeFacWidth[i] = 0;
  
  //  Maintains forest-wide in-bag set as bits.  Achieves high compression, but
  //  may still prove too small for multi-gigarow sets.  Saving this state is
  //  necessary, however, for per-row OOB prediction scheme employed for quantile
  //  regression.
  //
  int inBagSize = ((nTree * nRow) + 8 * sizeof(unsigned int) - 1) / (8 * sizeof(unsigned int));
  inBag = new unsigned int[inBagSize];
  for (int i = 0; i < inBagSize; i++)
    inBag[i] = 0;
}


/**
   @brief Loads trained forest from front end.

   @param _nTree is the number of trees in the forest.

   @param _forestSize is the length of the multi-vector holding all tree parameters.

   @param _preds[] are the predictors associated with tree nonterminals.

   @param _splits[] are the splitting values associated with nonterminals, or scores.

   @param _origins[] are the offsets into the multivector denoting each individual tree vector.

   @param _facOff[] are the offsets into the multi-bitvector denoting each tree's factor splitting values.

   @param _facSplits[] are the factor splitting values. 

   @return void.
*/
void DecTree::ForestReload(int _nTree, int _forestSize, int _preds[], double _splits[], int _bump[], int _origins[], int _facOff[], int _facSplits[]) {
  nTree = _nTree;
  forestSize = _forestSize;
  predForest = _preds;
  numForest = _splits;
  treeOriginForest = _origins;

  // Only used if categorical predictors present.
  //
  facOffForest = _facOff;
  facSplitForest = _facSplits;

  // Populates a packed table from two distinct vectors.
  bumpForest = new int[forestSize];
  for (int i = 0; i < forestSize; i++) {
    bumpForest[i] = _bump[i];
  }
}


/**
  @brief Resets addresses of vectors used during prediction.  Most are allocated
  by the front end so are not deallocated here.

  @return void
*/
void DecTree::DeFactoryPredict() {
  delete [] bumpForest; // Built on reload.
  bumpForest = 0;
  predForest = 0;
  numForest = 0;
  facSplitForest = 0;
  facOffForest = 0;
  forestSize = nTree = -1;
  ObsDeImmutables();
  
  Quant::DeFactoryPredict();
  Predictor::DeFactory();
}


/**
   @brief General deallocation after train/validate session.

   @return void
 */
void DecTree::DeFactoryTrain() {
  delete [] treeSizes;
  delete [] treeOriginForest;
  delete [] predTree; // Contents deleted at consumption.
  delete [] splitTree; // "
  delete [] bumpTree; // "
  delete [] treeFacWidth;
  delete [] treeFacSplits; // Inidividual components deleted when tree written.
  delete [] inBag;
  delete [] predInfo;
  delete [] predForest;
  delete [] numForest;
  delete [] bumpForest;
  delete [] facOffForest; // Always built, but may be all zeroes.
  if (facSplitForest != 0) // Not built if no splitting factors.
    delete [] facSplitForest;

  facOffForest = 0;
  facSplitForest = 0;

  treeSizes = 0;
  treeOriginForest = 0;
  predTree = 0;
  splitTree = 0;
  bumpTree = 0;
  treeFacWidth = 0;
  treeFacSplits = 0;
  inBag = 0;

  bumpForest = 0;
  predForest = 0;
  numForest = 0;
  predInfo =  0;

  nTree = forestSize = -1;
  ObsDeImmutables();
}


/**
   @brief Consumes remaining tree-based information into forest-wide data structures.

   @param cumFacWidth outputs the sum of all widths of factor bitvectors.

   @return Length of forest-wide vectors, plus output reference parameter.

 */
int DecTree::ConsumeTrees(int &cumFacWidth) {
  facOffForest = new int[nTree];

  cumFacWidth = 0;
  for (int tn = 0; tn < nTree; tn++) {
    facOffForest[tn] = cumFacWidth;
    cumFacWidth += treeFacWidth[tn];
  }

  if (cumFacWidth > 0) {
    facSplitForest = new int[cumFacWidth];

    int *facSplit = facSplitForest;
    for (int tn = 0; tn < nTree; tn++) {
      int fw = treeFacWidth[tn];
      if (fw > 0) {
	int *fs = treeFacSplits[tn];
	for (int i = 0; i < fw; i++) {
	  facSplit[i] = fs[i];
	}
	delete [] treeFacSplits[tn];
	treeFacSplits[tn] = 0;
	facSplit += fw;
      }
    }
  }

  predForest = new int[forestSize];
  numForest = new double[forestSize];
  bumpForest = new int[forestSize];

  for (int i = 0; i < nTree; i++) {
    int start = treeOriginForest[i];
    for (int j = 0; j < treeSizes[i]; j++) {
      predForest[start + j] = predTree[i][j];
      numForest[start + j] = splitTree[i][j];
      bumpForest[start + j] = bumpTree[i][j];
    }
    delete [] predTree[i];
    delete [] splitTree[i];
    delete [] bumpTree[i];
  }
  Quant::ConsumeTrees();

  return forestSize;
}


/**
  @brief Consumes block of PreTrees into decision trees.

  @param treeBlock is the number of PreTrees in the block.

  @param treeStart is the zero-based index of the first tree in the block.

  @return sum of bag counts over trees in block.
*/
int DecTree::BlockConsume(PreTree *ptBlock[], int treeBlock, int treeStart) {
  int totBagCount = 0; // Sums bag counts in current block.

  for (int treeIdx = 0; treeIdx < treeBlock; treeIdx++) {
    PreTree *pt = ptBlock[treeIdx];
    int treeSize = pt->TreeHeight();
    int bagCount = pt->BagCount();
    totBagCount += bagCount;
    int treeNum = treeStart + treeIdx;
    SetBagRow(pt->InBag(), treeNum);
    treeSizes[treeNum] = treeSize;
    predTree[treeNum] = new int[treeSize];
    splitTree[treeNum] = new double[treeSize];
    bumpTree[treeNum] = new int[treeSize];

    // Consumes pretree nodes, ranks and split bits via separate calls.
    //
    pt->ConsumeNodes(predTree[treeNum], splitTree[treeNum], bumpTree[treeNum]);
    Quant::TreeRanks(pt, bumpTree[treeNum], predTree[treeNum], treeNum);

    ConsumeSplitBits(pt, treeFacWidth[treeNum], treeFacSplits[treeNum]);
    delete pt;
    
    treeOriginForest[treeNum] = forestSize;
    forestSize += treeSize;
  }
  delete [] ptBlock;

  return totBagCount;
}


/**
 @brief Consumes splitting bitvector for the current pretree.

 @param treeNum is the index of the tree under constuction.

 @param facWidth is the count of splitting bits to be copied.

 @return void.
*/
void DecTree::ConsumeSplitBits(PreTree *pt, int &treeFW, int *&treeFS) {
  int facWidth = pt->SplitFacWidth();
  treeFW = facWidth;
  if (facWidth > 0) {
    treeFS = new int[facWidth];
    pt->ConsumeSplitBits(treeFS);
  }
  else
    treeFS = 0;
}


/**
  @brief Sets bit for <row, tree> with tree as faster-moving index.

  @param ptInBag[] records a PreTree's in-bag rows as compressed bits.

  @param trainRows is the number of rows in the training set.

  @param treeNum is the decision tree for which in-bag state is being set.
  
  @return void.
*/
void DecTree::SetBagRow(const unsigned int ptInBag[], int treeNum) {
  const unsigned int slotBits = 8 * sizeof(unsigned int);
  int slotRow = 0;
  int slot = 0;
  for (unsigned int baseRow = 0; baseRow < nRow; baseRow += slotBits, slot++) {
    unsigned int ptSlot = ptInBag[slot];
    unsigned int mask = 1;
    unsigned int supRow = nRow < baseRow + slotBits ? nRow : baseRow + slotBits;
    for (unsigned int row = baseRow; row < supRow; row++, mask <<= 1) {
      if (ptSlot & mask) { // row is in-bag.
	unsigned int off, bit;
	unsigned int val = BagCoord(treeNum, row, off, bit);
        val |= (1 << bit);
        inBag[off] = val;
      }
    }
    slotRow += slotBits;
  }
}


/**
   @brief Determines whether a given row index is in-bag in a given tree.

   @param treeNum is the index of a given tree.

   @param row is the row index to be tested.

   @return True iff the row is in-bag.
 */
bool DecTree::InBag(int treeNum, unsigned int row) {
  unsigned int bit, off;
  unsigned int val = BagCoord(treeNum, row, off, bit);

  return (val & (1 << bit)) > 0;
}


void DecTree::WriteForest(int *rPreds, double *rSplits, int *rBump, int *rOrigins, int *rFacOff, int * rFacSplits) {
  for (int tn = 0; tn < nTree; tn++) {
    int tOrig = treeOriginForest[tn];
    int facOrigin = facOffForest[tn];
    rOrigins[tn] = tOrig;
    rFacOff[tn] = facOrigin;
    WriteTree(tn, tOrig, facOrigin, rPreds + tOrig, rSplits + tOrig, rBump + tOrig, rFacSplits + facOrigin);
  }
  DeFactoryTrain();
}


// Writes the tree-specific splitting information for export.
// Predictor indices are not written as 1-based indices.
//
void DecTree::WriteTree(int treeNum, int tOrig, int tFacOrig, int *outPreds, double* outSplitVals, int *outBump, int *outFacSplits) {
  int *preds = predForest + tOrig;
  double *splitVal = numForest + tOrig;
  int *bump = bumpForest + tOrig;

  for (int i = 0; i < treeSizes[treeNum]; i++) {
    outPreds[i] = preds[i]; // + 1;
    // Bumps the within-tree offset by cumulative tree offset so that tree is written
    // using absolute offsets within the global factor split vector.
    //
    // N.B.:  Both OOB and replay prediction use tree-relative offset numbers.
    //
    outSplitVals[i] = splitVal[i];
    outBump[i] = bump[i];
  }

  // Even with factor predictors these could all be zero, as in the case of mixed predictor
  // types in which only the numerical predictors split.
  //
  int facWidth = treeFacWidth[treeNum];
  if (facWidth > 0) {
    int *facSplit = facSplitForest + tFacOrig;
    for (int i = 0; i < facWidth; i++)
      outFacSplits[i] = facSplit[i];
  }
}


/**
   @brief Scales the predictor Info values by the tree count.

   @param outPredInfo outputs the Info values.

   @return Formally void, with output parameter vector.
 */
void DecTree::ScaleInfo(double outPredInfo[]) {
  for (int i = 0; i < nPred; i++)
    outPredInfo[i] = predInfo[i] / nTree;
}


void DecTree::PredictCtg(int *censusIn, unsigned int ctgWidth, int yCtg[], int *confusion, double error[], bool useBag) {
  int *census;
  if (censusIn == 0) {
    census = new int[ctgWidth * nRow];
  }
  else {
    census = censusIn;
  }
  
  for (unsigned int i = 0; i < ctgWidth * nRow; i++)
    census[i] = 0;
  
  PredictAcrossCtg(census, ctgWidth, useBag);
  Vote(census, yCtg, confusion, error, ctgWidth, useBag);

  if (censusIn == 0)
    delete [] census;
}

/**
   @brief Main driver for prediting categorical response.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */
void DecTree::PredictAcrossCtg(int *census, unsigned int ctgWidth, bool useBag) {
  // TODO:  Look into effects of false sharing of census rows.
  if (nPredFac == 0) {
    PredictAcrossNumCtg(census, ctgWidth, useBag);
  }
  else if (nPredNum == 0) {
    PredictAcrossFacCtg(census, ctgWidth, useBag);
  }
  else {
    PredictAcrossMixedCtg(census, ctgWidth, useBag);
  }
}


/**
   @param yCtg contains the training response, in the case of bagged prediction, otherwise the predicted response.

   @param ctgWidth is the cardinality of the response.

   @param error is an output vector of classification errors.

   @bool useBag indicates whether prediction is restricted to out-of-bag rows.

   @return void.
*/
void DecTree::Vote(const int *census, int yCtg[], int *confusion, double error[], unsigned int ctgWidth, bool useBag) {
  for (unsigned int row = 0; row < nRow; row++) {
    int argMax = -1;
    double popMax = 0.0;
    for (unsigned int ctg = 0; ctg < ctgWidth; ctg++) {
      int ctgPop = census[row * ctgWidth + ctg];
      if (ctgPop > popMax) {
	popMax = ctgPop;
	argMax = ctg;
      }
    }
    if (argMax >= 0) {
      if (useBag) {
	int rsp = yCtg[row];
	confusion[rsp + ctgWidth * argMax]++;
      }
      else
	yCtg[row] = argMax;
    }
  }

  
  if (useBag) { // Otherwise, no test-vector against which to compare.
    // Fills in classification error vector.
    //
    for (unsigned int rsp = 0; rsp < ctgWidth; rsp++) {
      int numWrong = 0;
      for (unsigned int predicted = 0; predicted < ctgWidth; predicted++) {
	if (predicted != rsp) {  // Mispredictions are off-diagonal.
	  numWrong += confusion[rsp + ctgWidth * predicted];
	}
      }
      error[rsp] = double(numWrong) / double(numWrong + confusion[rsp + ctgWidth * rsp]);
    }
  }
  else // Prediction only:  not training.
    DeFactoryPredict();
}


/**
   @brief Categorical prediction across rows with numerical predictor type.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */
void DecTree::PredictAcrossNumCtg(int *census, unsigned int ctgWidth, bool useBag) {
  double *transpose = new double[nPred * nRow];
  unsigned int row;
  
#pragma omp parallel default(shared) private(row)
  {
#pragma omp for schedule(dynamic, 1)    
  for (row = 0; row < nRow; row++) {
    double *rowSlice = transpose + row * nPred;
    int *rowPred = census + row * ctgWidth;
    PredictRowNumCtg(row, rowSlice, rowPred, useBag);
  }
  }

  delete [] transpose;
}

/**
   @brief Categorical prediction across rows with factor predictor type.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */
void DecTree::PredictAcrossFacCtg(int *census, unsigned int ctgWidth, bool useBag) {
  int *transpose = new int[nPred * nRow];
  unsigned int row;

#pragma omp parallel default(shared) private(row)
  {
#pragma omp for schedule(dynamic, 1)
    
  for (row = 0; row < nRow; row++) {
    int *rowSlice = transpose + row * nPred;
    int *rowPred = census + row * ctgWidth;
    PredictRowFacCtg(row, rowSlice, rowPred, useBag);
  }
  }
  
  delete [] transpose;
}

/**
   @brief Categorical prediction across rows with mixed predictor types.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */
void DecTree::PredictAcrossMixedCtg(int *census, unsigned int ctgWidth, bool useBag) {
  double *transposeN = new double[nPredNum * nRow];
  int *transposeI = new int[nPredFac * nRow];
  unsigned int row;

#pragma omp parallel default(shared) private(row)
  {
#pragma omp for schedule(dynamic, 1) 
  for (row = 0; row < nRow; row++) {
    double *rowSliceN = transposeN + row * nPredNum;
    int *rowSliceI = transposeI + row * nPredFac;
    int *rowPred = census + row * ctgWidth;
    PredictRowMixedCtg(row, rowSliceN, rowSliceI, rowPred, useBag);
  }
  }

  delete [] transposeI;
  delete [] transposeN;
}

/**
   @brief Main driver for prediting regression response.

   @param outVec contains the predictions.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */
void DecTree::PredictAcrossReg(double outVec[], bool useBag) {
  double *prediction;

  if (useBag)
    prediction = new double[nRow];
  else
    prediction = outVec;
  int *predictLeaves = new int[nTree * nRow];

  // TODO:  Also catch mixed case in which no factors split, and avoid mixed case
  // in which no numericals split.
  if (nPredFac == 0)
    PredictAcrossNumReg(prediction, predictLeaves, useBag);
  else if (nPredNum == 0) // Purely factor predictors.
    PredictAcrossFacReg(prediction, predictLeaves, useBag);
  else  // Mixed numerical and factor
    PredictAcrossMixedReg(prediction, predictLeaves, useBag);

  Quant::PredictRows(treeOriginForest, bumpForest, predForest, forestSize, predictLeaves);
  delete [] predictLeaves;

  if (useBag) {
    double SSE = 0.0;
    for (unsigned int row = 0; row < nRow; row++) {
      SSE += (prediction[row] - Response::response->y[row]) * (prediction[row] - Response::response->y[row]);
    }
    // TODO:  repair assumption that every row is sampled:
    // Assumes nonzero nRow:
    outVec[0] = SSE / nRow;

    delete [] prediction;
  }
  else
    DeFactoryPredict();
}


/**
   @brief Multi-row prediction for regression tree, with predictors of only numeric.

   @param prediction contains the mean score across trees.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */

void DecTree::PredictAcrossNumReg(double prediction[], int *predictLeaves, bool useBag) {
  double *transpose = new double[nPred * nRow];
  unsigned int row;

  // N.B.:  Parallelization by row assumes that nRow >> nTree.
  // TODO:  Consider blocking, to cut down on memory.  Mut. mut. for the
  // other two methods.
#pragma omp parallel default(shared) private(row)
  {
#pragma omp for schedule(dynamic, 1)
  for (row = 0; row < nRow; row++) {
    double score = 0.0;
    int treesSeen = 0;
    int *leaves = predictLeaves + row * nTree;
    double *rowSlice = transpose + row * nPred;
    PredictRowNumReg(row, rowSlice, leaves, useBag);
    for (int tc = 0; tc < nTree; tc++) {
      int leafIdx = leaves[tc];
      if (leafIdx >= 0) {
	treesSeen++;
	score +=  *(numForest + treeOriginForest[tc] + leafIdx);
      }
    }
    prediction[row] = score / treesSeen; // Assumes >= 1 tree seen.
  }
  }

  delete [] transpose;
}

/**
   @brief Multi-row prediction for regression tree, with predictors of both numeric and factor type.

   @param prediction contains the mean score across trees.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */
void DecTree::PredictAcrossFacReg(double prediction[], int *predictLeaves, bool useBag) {
  int *transpose = new int[nPred * nRow];
  unsigned int row;

#pragma omp parallel default(shared) private(row)
  {
#pragma omp for schedule(dynamic, 1)
  for (row = 0; row < nRow; row++) {
    double score = 0.0;
    int treesSeen = 0;
    int *leaves = predictLeaves + row * nTree;
    int *rowSlice = transpose + row * nPred;
    PredictRowFacReg(row, rowSlice, leaves, useBag);
    for (int tc = 0; tc < nTree; tc++) {
      int leafIdx = leaves[tc];
      if (leafIdx >= 0) {
	treesSeen++;
	score +=  *(numForest + treeOriginForest[tc] + leafIdx);
      }
    }
    prediction[row] = score / treesSeen; // Assumes >= 1 tree seen.
  }
  }

  delete [] transpose;
}

/**
   @brief Multi-row prediction for regression tree, with predictors of both numeric and factor type.

   @param prediction contains the mean score across trees.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */
void DecTree::PredictAcrossMixedReg(double prediction[], int *predictLeaves, bool useBag) {
  double *transposeN = new double[nPredNum * nRow];
  int *transposeI = new int[nPredFac * nRow];
  unsigned int row;

#pragma omp parallel default(shared) private(row)
  {
#pragma omp for schedule(dynamic, 1)
  for (row = 0; row < nRow; row++) {
    double score = 0.0;
    int treesSeen = 0;
    double *rowSliceN = transposeN + row * nPredNum;
    int *rowSliceI = transposeI + row * nPredFac;
    int *leaves = predictLeaves + row * nTree;
    PredictRowMixedReg(row, rowSliceN, rowSliceI, leaves, useBag);
    for (int tc = 0; tc < nTree; tc++) {
      int leafIdx = leaves[tc];
      if (leafIdx >= 0) {
        treesSeen++;
        score +=  *(numForest + treeOriginForest[tc] + leafIdx);
      }
    }
    prediction[row] = score / treesSeen; // Assumes >= 1 tree seen.
  }
  }

  delete [] transposeN;
  delete [] transposeI;
}

/**
   @brief Prediction for regression tree, with predictors of only numeric type.

   @param row is the row of data over which a prediction is made.

   @param rowT is a numeric data array section corresponding to the row.

   @param leaves[] are the tree terminals predicted for each row.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */

void DecTree::PredictRowNumReg(unsigned int row, double rowT[], int leaves[], bool useBag) {
  for (int i = 0; i < nPred; i++)
    rowT[i] = Predictor::numBase[row + i* nRow];

  // TODO:  Use row at rank.
  int tc;
  for (tc = 0; tc < nTree; tc++) {
    if (useBag && InBag(tc, row)) {
      leaves[tc] = -1;
      continue;
    }
    int idx = 0;
    int tOrig = treeOriginForest[tc];
    int *preds = predForest + tOrig;
    double *splitVal = numForest + tOrig;
    int *bumps = bumpForest + tOrig;

    int bump = bumps[idx];
    while (bump != 0) {
      int pred = preds[idx];
      idx += (rowT[pred] <= splitVal[idx] ? bump : bump + 1);
      bump = bumps[idx];
    }
    leaves[tc] = idx;
  }
}


/**
   @brief Prediction for classification tree, with predictors of only numeric type.

   @param row is the row of data over which a prediction is made.

   @param rowT is a numeric data array section corresponding to the row.

   @param prd[] are the tree terminals predicted for each row.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */

void DecTree::PredictRowNumCtg(unsigned int row, double rowT[], int prd[], bool useBag) {
  for (int i = 0; i < nPred; i++)
    rowT[i] = Predictor::numBase[row + i* nRow];

  // TODO:  Use row at rank.
  //int ct = 0;
  int tc;
  for (tc = 0; tc < nTree; tc++) {
    if (useBag && InBag(tc, row))
      continue;
    int idx = 0;
    int tOrig = treeOriginForest[tc];
    //ct++;
    int *preds = predForest + tOrig;
    double *splitVal = numForest + tOrig;
    int *bumps = bumpForest + tOrig;

    int bump = bumps[idx];
    while (bump != 0) {
      int pred = preds[idx];
      idx += (rowT[pred] <= splitVal[idx] ? bump : bump + 1);
      bump = bumps[idx];
    }
    int ctgPredict = splitVal[idx];
    prd[ctgPredict]++;
  }
}

// Temporary clone of regression tree version.
//
void DecTree::PredictRowFacCtg(unsigned int row, int rowT[], int prd[], bool useBag) {
  for (int i = 0; i < nPred; i++)
    rowT[i] = Predictor::facBase[row + i* nRow];


  for (int tc = 0; tc < nTree; tc++) {
    if (useBag && InBag(tc, row))
      continue;
    int tOrig = treeOriginForest[tc];
    int *preds = predForest + tOrig;
    double *splitVal = numForest + tOrig;
    int *bumps = bumpForest + tOrig;
    int *fs = facSplitForest + facOffForest[tc];

    int idx = 0;
    int bump = bumps[idx];
    while (bump != 0) {
      int pred = preds[idx];
      int facOff = int(splitVal[idx]);
      int facId = Predictor::FacIdx(pred);
      idx += (fs[facOff + rowT[facId]] ? bump : bump + 1);
      bump = bumps[idx];
    }
    int ctgPredict = splitVal[idx];
    prd[ctgPredict]++;
  }
}


/**
   @brief Prediction for classification tree, with predictors of both numeric and factor type.

   @param row is the row of data over which a prediction is made.

   @param rowNT is a numeric data array section corresponding to the row.

   @param rowFT is a factor data array section corresponding to the row.

   @param prd[] are the tree terminals predicted for each row.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */
void DecTree::PredictRowMixedCtg(unsigned int row, double rowNT[], int rowFT[], int prd[], bool useBag) {
  for (int i = 0; i < nPredNum; i++)
    rowNT[i] = Predictor::numBase[row + i * nRow];
  for (int i = 0; i < nPredFac; i++)
    rowFT[i] = Predictor::facBase[row + i * nRow];

  for (int tc = 0; tc < nTree; tc++) {
    if (useBag && InBag(tc, row))
      continue;
    int tOrig = treeOriginForest[tc];
    int *preds = predForest + tOrig;
    double *splitVal = numForest + tOrig;
    int *bumps = bumpForest + tOrig;
    int *fs = facSplitForest + facOffForest[tc];

    int idx = 0;
    int bump = bumps[idx];
    while (bump != 0) {
      int pred = preds[idx]; 
      int facId = Predictor::FacIdx(pred);
      idx += (facId < 0 ? (rowNT[pred] <= splitVal[idx] ?  bump : bump + 1) : (fs[int(splitVal[idx]) + rowFT[facId]] ? bump : bump + 1));
      bump = bumps[idx];
    }
    int ctgPredict = splitVal[idx];
    prd[ctgPredict]++;
  }
}


/**
   @brief Prediction for regression tree, with factor-valued predictors only.

   @param row is the row of data over which a prediction is made.

   @param rowT is a factor data array section corresponding to the row.

   @param leaves[] are the tree terminals predicted for each row.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */
void DecTree::PredictRowFacReg(unsigned int row, int rowT[], int leaves[],  bool useBag) {
  for (int i = 0; i < nPredFac; i++)
    rowT[i] = Predictor::facBase[row + i * nRow];

  int tc;
  for (tc = 0; tc < nTree; tc++) {
    if (useBag && InBag(tc, row)) {
      leaves[tc] = -1;
      continue;
    }
    int idx = 0;
    int tOrig = treeOriginForest[tc];
    int *preds = predForest + tOrig;
    double *splitVal = numForest + tOrig;
    int *bumps = bumpForest + tOrig;
    int *fs = facSplitForest + facOffForest[tc];

    int bump = bumps[idx];
    while (bump != 0) {
      int facOff = int(splitVal[idx]);
      int pred = preds[idx];
      int facId = Predictor::FacIdx(pred);
      idx += (fs[facOff + rowT[facId]] ? bump : bump + 1);
      bump = bumps[idx];
    }
    leaves[tc] = idx;
    // TODO:  Instead of runtime check, can guarantee this by checking last level for non-negative
    // predictor fields.
  }
}


/**
   @brief Prediction for regression tree, with predictors of both numeric and factor type.

   @param row is the row of data over which a prediction is made.

   @param rowNT is a numeric data array section corresponding to the row.

   @param rowFT is a factor data array section corresponding to the row.

   @param leaves[] are the tree terminals predicted for each row.

   @param useBag indicates whether prediction is restricted to out-of-bag data.

   @return Void with output vector parameter.
 */
void DecTree::PredictRowMixedReg(unsigned int row, double rowNT[], int rowFT[], int leaves[], bool useBag) {
  for (int i = 0; i < nPredNum; i++)
    rowNT[i] = Predictor::numBase[row + i * nRow];
  for (int i = 0; i < nPredFac; i++)
    rowFT[i] = Predictor::facBase[row + i * nRow];

  int tc;
  for (tc = 0; tc < nTree; tc++) {
    if (useBag && InBag(tc, row)) {
      leaves[tc] = -1;
      continue;
    }
    int tOrig = treeOriginForest[tc];
    int *preds = predForest + tOrig;
    double *splitVal = numForest + tOrig;
    int *bumps = bumpForest + tOrig;
    int *fs = facSplitForest + facOffForest[tc];

    int idx = 0;
    int bump = bumps[idx];
    while (bump != 0) {
      int pred = preds[idx];
      int facId = Predictor::FacIdx(pred);
      idx += (facId < 0 ? (rowNT[pred] <= splitVal[idx] ?  bump : bump + 1)  : (fs[int(splitVal[idx]) + rowFT[facId]] ? bump : bump + 1));
      bump = bumps[idx];
    }
    leaves[tc] = idx;
  }
  // TODO:  Instead of runtime check, can guarantee this by checking last level for non-negative
  // predictor fields.
}
