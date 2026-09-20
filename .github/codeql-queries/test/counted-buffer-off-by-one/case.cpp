/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 */

class CountedBuffer {
public:
  void badArray(unsigned int nIndex)
  {
    if (nIndex > m_nSize) // $ Alert
      return;
    m_values[nIndex] = 0;
  }

  void safeArray(unsigned int nIndex)
  {
    if (nIndex >= m_nSize)
      return;
    m_values[nIndex] = 0;
  }

  void adjustedCountControl(unsigned int nIndex)
  {
    if (nIndex > m_nSize - 1)
      return;
    m_values[nIndex] = 0;
  }

  unsigned char *badPointer(int nIndex)
  {
    if (nIndex < 0 || nIndex > (int)m_nSize) // $ Alert
      return 0;
    return m_raw + nIndex * 4;
  }

private:
  unsigned int m_nSize;
  int *m_values;
  unsigned char *m_raw;
};
