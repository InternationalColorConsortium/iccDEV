/** @file
    File:       IccSearch.h

    Contains:   Header file for implementation of the CIccSearchVec class.

    Version:    V1

    Copyright:  (c) see Software License
*/

/*
 * Copyright (c) International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium. 
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes. 
 *
 *  
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *  
 * 
 */

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak 5-16-2025
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCSEARCH_H)
#define _ICCSEARCH_H

#include <cmath>
#include <stdexcept>
#include <vector>
#include "IccDefs.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

  typedef std::vector<icFloatNumber> icFloatVector;

  class CIccSearchVec {
  public:
    CIccSearchVec() {}
    CIccSearchVec(unsigned int n) : n(n) {
      val.resize(n, 0);
    }
    CIccSearchVec(std::initializer_list<icFloatNumber> c) {
      n = (unsigned int)c.size();
      val.resize(n);
      std::copy(c.begin(), c.end(), val.begin());
    }
    CIccSearchVec(const CIccSearchVec& lhs) {
      val = lhs.val;
      n = lhs.n;
    }
    CIccSearchVec(const icFloatVector& lhs) {
      val = lhs;
      n = (unsigned int)lhs.size();
    }
    icFloatNumber operator()(unsigned int idx) const {
      if (idx >= n) {
        throw std::range_error("Element access out of range");
      }
      return val[idx];
    }
    icFloatNumber& operator()(unsigned int idx) {
      if (idx >= n) {
        throw std::range_error("Element access out of range");
      }
      return val[idx];
    }

    CIccSearchVec operator=(const CIccSearchVec& rhs) {
      val = rhs.val;
      n = rhs.n;
      return *this;
    }

    CIccSearchVec operator=(const icFloatVector& rhs) {
      val = rhs;
      n = (unsigned int)rhs.size();
      return *this;
    }

    CIccSearchVec operator+(const CIccSearchVec& rhs) const {
      CIccSearchVec lhs(n);
      for (unsigned int i = 0; i < n; i++) {
        lhs.val[i] = val[i] + rhs.val[i];
      }
      return lhs;
    }
    CIccSearchVec operator-(const CIccSearchVec& rhs) const {
      CIccSearchVec lhs(n);
      for (unsigned int i = 0; i < n; i++) {
        lhs.val[i] = val[i] - rhs.val[i];
      }
      return lhs;
    }

    CIccSearchVec operator/(icFloatNumber rhs) const {
      CIccSearchVec lhs(n);
      for (unsigned int i = 0; i < n; i++) {
        lhs.val[i] = val[i] / rhs;
      }
      return lhs;
    }
    CIccSearchVec& operator+=(const CIccSearchVec& rhs) {
      if (n != rhs.n)
        throw std::invalid_argument(
          "The two vectors must have the same length");
      for (unsigned int i = 0; i < n; i++) {
        val[i] += rhs.val[i];
      }
      return *this;
    }

    CIccSearchVec& operator-=(const CIccSearchVec& rhs) {
      if (n != rhs.n)
        throw std::invalid_argument(
          "The two vectors must have the same length");
      for (unsigned int i = 0; i < n; i++) {
        val[i] -= rhs.val[i];
      }
      return *this;
    }

    CIccSearchVec& operator*=(icFloatNumber rhs) {
      for (unsigned int i = 0; i < n; i++) {
        val[i] *= rhs;
      }
      return *this;
    }

    CIccSearchVec& operator/=(icFloatNumber rhs) {
      for (unsigned int i = 0; i < n; i++) {
        val[i] /= rhs;
      }
      return *this;
    }
    unsigned int size() const {
      return n;
    }
    unsigned int resize(unsigned int _n) {
      val.resize(_n);
      n = _n;
      return n;
    }
    icFloatNumber length() const {
      icFloatNumber ans = 0;
      for (unsigned int i = 0; i < n; i++) {
        ans += (val[i] * val[i]);
      }
      return std::sqrt(ans);
    }
    icFloatVector& vec() {
      return val;
    }
    friend CIccSearchVec operator*(icFloatNumber a, const CIccSearchVec& b) {
      CIccSearchVec c(b.size());
      for (unsigned int i = 0; i < b.size(); i++) {
        c.val[i] = a * b.val[i];
      }
      return c;
    }

    icFloatNumber index(size_t i) const { return val[i]; }

  private:
    icFloatVector val;
    unsigned int   n;
  };

  class CIccMinSearch {
  public:
    CIccMinSearch() {}
    virtual ~CIccMinSearch() {}

    //Override costFunction to implement search
    virtual icFloatNumber costFunc(CIccSearchVec &point) = 0;

    //Set bUsebouds true, and override boundsCheck to implement bounds checking setting boundsCost with "distance" out of bounds
    virtual bool boundsCheck(const CIccSearchVec& point, icFloatNumber& boundsCost) const {
      boundsCost = 0;
      return false;
    }

    icFloatVector findMin(icFloatVector& startingPoint, const std::vector<icFloatVector>& startingSimplex = {}) {
      unsigned int nFuncCallCount = 0;
      icFloatNumber cost;
      auto  f = [&](CIccSearchVec& p) {
        nFuncCallCount++;
        if (bUseBounds && boundsCheck(p, cost))
          return cost + overBoundsCost;
        return costFunc(p);
      };

      // Getting the dimension of function input
      unsigned int nDimension = (unsigned int)startingPoint.size();
      if (nDimension <= 0)
        return icFloatVector();

      // Setting parameters
      icFloatNumber alpha, beta, gamma, delta;
      if (bAdaptive) {
        // Using the results of doi:10.1007/s10589-010-9329-3
        alpha = 1.0f;
        beta = 1.0f + 2.0f / nDimension;
        gamma = 0.75f - 1.0f / (2.0f * nDimension);
        delta = 1.0f - 1.0f / nDimension;
      }
      else {
        alpha = 1.0f;
        beta = 2.0f;
        gamma = 0.5f;
        delta = 0.5f;
      }

      // Generate initial simplex
      std::vector<CIccSearchVec> simplex(nDimension + 1);
      if (startingSimplex.empty()) {
        simplex[0] = startingPoint;
        for (unsigned int i = 1; i <= nDimension; i++) {
          CIccSearchVec p(startingPoint);
          icFloatNumber tau = (p(i - 1) < 1e-6f && p(i - 1) > -1e-6f) ? 0.00025f : 0.05f;
          p(i - 1) += tau;
          simplex[i] = p;
        }
      }
      else {
        if (startingSimplex.size() != nDimension + 1)
          return icFloatVector(nDimension);

        for (unsigned int i = 0; i < startingSimplex.size(); i++) {
          simplex[i] = startingSimplex[i];
        }
      }

      std::vector<std::pair<bool, icFloatNumber>> valueCache(nDimension + 1);
      for (auto& v : valueCache) {
        v.first = false;
      }
      unsigned int idxBiggest = 0;
      unsigned int idxSmallest = 0;
      icFloatNumber valBiggest;
      icFloatNumber valSecondBiggest;
      icFloatNumber valSmallest;

      //Perform search
      unsigned int iterations = maxIterations;
      while (iterations--) {
        // Find the points that generate the biggest, second biggest and smallest value
        icFloatNumber val;
        if (!valueCache[0].first) {
          val = f(simplex[0]);
          valueCache[0].first = true;
          valueCache[0].second = val;
        }
        else
          val = valueCache[0].second;

        valBiggest = val;
        valSmallest = val;
        valSecondBiggest = val;
        idxBiggest = 0;
        idxSmallest = 0;
        for (size_t i = 1; i < simplex.size(); i++) {
          icFloatNumber val;
          if (!valueCache[i].first) {
            val = f(simplex[i]);
            valueCache[i].first = true;
            valueCache[i].second = val;
          }
          else {
            val = valueCache[i].second;
          }
          if (val > valBiggest) {
            idxBiggest = (unsigned int)i;
            valBiggest = val;
          }
          else if (val < valSmallest) {
            idxSmallest = (unsigned int)i;
            valSmallest = val;
          }
        }

        // Calculate the difference of function values and the distance between
        // points in the simplex, so that we can know when to stop the
        // optimization
        icFloatNumber maxValDiff = 0;
        icFloatNumber maxPointDiff = 0;
        for (size_t i = 0; i < simplex.size(); i++) {
          icFloatNumber val = valueCache[i].second;
          if (i != idxBiggest && val > valSecondBiggest) {
            valSecondBiggest = val;
          }
          else if (i != idxSmallest) {
            if (std::abs(val - valSmallest) > maxValDiff)
              maxValDiff = std::abs(val - valSmallest);
            icFloatNumber diff = (simplex[i] - simplex[idxSmallest]).length();
            if (diff > maxPointDiff)
              maxPointDiff = diff;
          }
        }
        if ((maxValDiff <= funcTolerance && maxPointDiff <= valTolerance) ||
          (nFuncCallCount >= maxFuncEvals) || (iterations == 0)) {
          icFloatVector res = simplex[idxSmallest].vec();
          return res;
        }

        // Calculate the centroid
        CIccSearchVec xCenter(nDimension);
        for (unsigned int i = 0; i < nDimension; i++)
          xCenter(i) = 0;
        for (size_t i = 0; i < simplex.size(); i++) {
          if (i != idxBiggest)
            xCenter += simplex[i];
        }
        xCenter /= (icFloatNumber)nDimension;

        // Calculate the reflection point
        CIccSearchVec xReflect = xCenter + alpha * (xCenter - simplex[idxBiggest]);

        icFloatNumber valReflection = f(xReflect);
        if (valReflection < valSmallest) {
          // Expansion
          CIccSearchVec xExpand = xCenter + beta * (xReflect - xCenter);

          icFloatNumber      expansion_val = f(xExpand);
          if (expansion_val < valReflection) {
            simplex[idxBiggest] = xExpand;
            valueCache[idxBiggest].second = expansion_val;
          }
          else {
            simplex[idxBiggest] = xReflect;
            valueCache[idxBiggest].second = valReflection;
          }
        }
        else if (valReflection >= valSecondBiggest) {
          // Contraction
          CIccSearchVec xContract(nDimension);
          bool bOutside = false;

          if (valReflection < valBiggest) {
            // Outside contraction
            bOutside = true;
            xContract = xCenter + gamma * (xReflect - xCenter);
          }
          else if (valReflection >= valBiggest) {
            // Inside contraction
            xContract = xCenter - gamma * (xReflect - xCenter);
          }

          icFloatNumber valContraction = f(xContract);

          if ((bOutside && valContraction <= valReflection) ||
            (!bOutside && valContraction <= valBiggest)) {
            simplex[idxBiggest] = xContract;
            valueCache[idxBiggest].second = valContraction;
          }
          else {
            // Shrinking
            for (unsigned int i = 0; i < nDimension; i++) {
              if (i != idxSmallest) {
                simplex[i] =
                  simplex[idxSmallest] +
                  delta * (simplex[i] - simplex[idxSmallest]);
                valueCache[i].first = false;
              }
            }
          }
        }
        else {
          // Reflection is good enough
          simplex[idxBiggest] = xReflect;
          valueCache[idxBiggest].second = valReflection;
        }
      }
      icFloatVector res = simplex[idxSmallest].vec();
      return res;
    }

    protected:
      bool bAdaptive = false;
      icFloatNumber funcTolerance = 1e-4f;
      icFloatNumber valTolerance = 1e-4f;
      unsigned int maxIterations = 10000;
      unsigned int maxFuncEvals = 10000;

      bool bUseBounds = false;
      icFloatNumber overBoundsCost = 1.0e6f;  //base cost when out of bounds
  };


#ifdef USEICCDEVNAMESPACE
}; // namespace iccDEV {
#endif

#endif
