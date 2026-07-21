//
//  vizShared.hpp
//      shared types across iccProfileVisualization
//

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in
 *  the documentation and/or other materials provided with the
 *  distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *  International Color Consortium" must not be used to imply that the
 *  ICC organization endorses or promotes products derived from this
 *  software.
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

#ifndef vizShared_h
#define vizShared_h

#include <cstdint>
#include <vector>

/******************************************************************************/

const float inch2mm = 25.4f;                 // 25.4 millimeters per inch, international standard
const float mm2point = 72.0f / inch2mm;      // 2.834645669 (Shows up several places)
const float inch2point = 72.0f;              // 72 points per inch, DTP and W3C standard

/******************************************************************************/

struct point2D {
  point2D(float xx, float yy) : x(xx), y(yy) {}
  point2D() : x(0.0), y(0.0) {}

  bool operator<(const point2D& o) const {
    if (x == o.x)
      return y < o.y;
    else
      return x < o.x;
  }

  float x, y;
};

inline point2D operator+(const point2D& xx, const point2D& yy) {
  return point2D(xx.x + yy.x, xx.y + yy.y);
}

inline point2D operator-(const point2D& xx, const point2D& yy) {
  return point2D(xx.x - yy.x, xx.y - yy.y);
}

inline point2D& operator+=(point2D& xx, const point2D& yy) {
  xx.x += yy.x;
  xx.y += yy.y;
  return xx;
}

inline point2D& operator-=(point2D& xx, const point2D& yy) {
  xx.x -= yy.x;
  xx.y -= yy.y;
  return xx;
}

inline point2D operator*(const point2D& xx, const point2D& yy) {
  return point2D(xx.x * yy.x, xx.y * yy.y);
}

inline point2D operator*(const point2D& xx, const float ss) {
  return point2D(xx.x * ss, xx.y * ss);
}

inline point2D operator/(const point2D& xx, const float ss) {
  return point2D(xx.x / ss, xx.y / ss);
}

inline point2D operator*(const float ss, const point2D& yy) {
  return point2D(ss * yy.x, ss * yy.y);
}

inline point2D operator/(const point2D& xx, const point2D& yy) {
  return point2D(xx.x / yy.x, xx.y / yy.y);
}

/******************************************************************************/

typedef std::vector<point2D> pointList;

/******************************************************************************/

struct Rect2D {
  Rect2D(float leftin, float rightin, float bottomin, float topin) :
    left(leftin), right(rightin), bottom(bottomin), top(topin)
    {}

  Rect2D() : left(0.0f), right(0.0f), bottom(0.0f), top(0.0f)
    {}

  Rect2D(const point2D &ll, const point2D &tr) :
    left(ll.x), right(tr.x), bottom(ll.y), top(tr.y)
    {}

  point2D Size() const  { return point2D( right-left, bottom-top ); }

  float left, right, bottom, top;
};

/******************************************************************************/
/******************************************************************************/

#endif /* vizShared_h */
