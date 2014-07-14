# Copyright (C)  2012-2014   Mark Seligman
##
## This file is part of ArboristBridgeR.
##
## ArboristBridgeR is free software: you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 2 of the License, or
## (at your option) any later version.
##
## ArboristBridgeR is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with ArboristBridgeR.  If not, see <http://www.gnu.org/licenses/>.

#include <Rcpp.h>

using namespace Rcpp;
using namespace std;

#include "util.h"

// Wrapper for uniform PRNG call-back.
//
double *Util::Unif(int count) {
  RNGScope scope;
  NumericVector rVec = runif(count);

  return rVec.begin();
}
