/*
 * The ICC Software License, Version 0.2
 *
 * Copyright (c) 2003-2012 The International Color Consortium. All rights
 * reserved.
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
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
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
 * individuals on behalf of the International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

class Value {
};

class Owner {
public:
  // A: the fixture's vulnerable case -- must alert.
  void SetA(Value *v)
  {
    delete m_a;
    m_a = v;
  }

  // B: the fixture's guarded case -- must NOT alert.
  void SetB(Value *v)
  {
    if (m_b == v)
      return;
    delete m_b;
    m_b = v;
  }

  // C: compound equality-and-return guard, as IccCmm.cpp:8718 writes it.
  //    Correct and null-safe.  Must NOT alert.
  void SetC(Value *v)
  {
    if (v && v == m_c)
      return;
    delete m_c;
    m_c = v;
  }

  // D: guarded delete, as IccMpeSpectral.cpp:972 and IccTagEmbedIcc.cpp:178
  //    write it.  Correct.  Must NOT alert.
  void SetD(Value *v)
  {
    if (m_d != v)
      delete m_d;
    m_d = v;
  }

  // E: guarded delete with braces and a NULL reset, the IccTagEmbedIcc form.
  //    Correct.  Must NOT alert.
  void SetE(Value *v)
  {
    if (v != m_e) {
      delete m_e;
      m_e = 0;
    }
    m_e = v;
  }

  // F: INVERTED guard -- deletes only when the pointers alias.  This is the
  //    defect the rule exists to find, wearing a guard.  Must alert.
  void SetF(Value *v)
  {
    if (m_f == v)
      delete m_f;
    m_f = v;
  }

  // G: equality-and-return leaves m_g dangling, but does not reach the later
  //    store. It is outside this rule's delete-then-store contract. Must NOT
  //    alert.
  void SetG(Value *v)
  {
    if (m_g == v) {
      delete m_g;
      return;
    }
    delete m_g;
    m_g = v;
  }

  // H: a compound predicate does not establish that equal pointers return.
  //    Must alert.
  void SetH(Value *v, bool ready)
  {
    if (m_h == v && ready)
      return;
    delete m_h;
    m_h = v;
  }

  // I: a nested return can fall through after the pointers alias. Must alert.
  void SetI(Value *v, bool ready)
  {
    if (m_i == v) {
      if (ready)
        return;
    }
    delete m_i;
    m_i = v;
  }

  // J: a nested return after the null check can fall through. Must alert.
  void SetJ(Value *v, bool ready)
  {
    if (v && v == m_j) {
      if (ready)
        return;
    }
    delete m_j;
    m_j = v;
  }

  // K: a post-delete guard cannot make the preceding delete safe. Must alert.
  void SetK(Value *v)
  {
    delete m_k;
    if (v && v == m_k)
      return;
    m_k = v;
  }

  // L: owning arrays have the same self-aliasing lifetime defect. Must alert.
  void SetL(Value *v)
  {
    delete [] m_l;
    m_l = v;
  }

  // M: delete and assignment in separate branches are not a setter UAF.
  //    Must NOT alert.
  void SetM(Value *v, bool replace)
  {
    if (replace)
      delete m_m;
    else
      m_m = v;
  }

private:
  Value *m_a;
  Value *m_b;
  Value *m_c;
  Value *m_d;
  Value *m_e;
  Value *m_f;
  Value *m_g;
  Value *m_h;
  Value *m_i;
  Value *m_j;
  Value *m_k;
  Value *m_l;
  Value *m_m;
};
