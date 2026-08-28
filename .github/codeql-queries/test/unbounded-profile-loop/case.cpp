void consume(unsigned int value);

class BeginGuarded {
public:
  bool Begin()
  {
    if (m_nNodes > 65536)
      return false;
    return true;
  }

  void Apply()
  {
    for (unsigned int i = 0; i < m_nNodes; i++)
      consume(i);
  }

private:
  unsigned int m_nNodes;
};

class Unguarded {
public:
  void Apply()
  {
    for (unsigned int i = 0; i < m_nCount; i++)
      consume(i);
  }

private:
  unsigned int m_nCount;
};

class CIccApplyThreadedCmmPool {
public:
  void Apply()
  {
    for (unsigned int i = 0; i < m_jobCount; i++)
      consume(i);
  }

private:
  unsigned int m_jobCount;
};

class CIccSampledCalculatorCurve {
public:
  void Begin()
  {
    for (unsigned int i = 0; i < m_nCount; i++)
      consume(i);
  }

private:
  unsigned int m_nCount;
};
