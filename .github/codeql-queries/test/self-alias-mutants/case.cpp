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

  // G: equality-and-return guard whose own branch deletes the alias and
  //    returns, leaving m_g dangling.  A real use-after-free.  Must alert.
  void SetG(Value *v)
  {
    if (m_g == v) {
      delete m_g;
      return;
    }
    delete m_g;
    m_g = v;
  }

private:
  Value *m_a;
  Value *m_b;
  Value *m_c;
  Value *m_d;
  Value *m_e;
  Value *m_f;
  Value *m_g;
};
