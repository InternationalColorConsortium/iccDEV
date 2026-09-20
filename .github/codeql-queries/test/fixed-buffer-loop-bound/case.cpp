/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 */

struct Position {
  unsigned int offset;
  unsigned int size;
};

class ToneMapWriter {
public:
  void bad()
  {
    Position positions[16];
    for (int i = 0; i < m_nOutputChannels; i++) { // $ Alert
      positions[i].offset = 0;
      positions[i].size = 0;
    }
  }

  void fixedControl()
  {
    Position positions[16];
    for (int i = 0; i < 16; i++) {
      positions[i].offset = 0;
      positions[i].size = 0;
    }
  }

private:
  unsigned short m_nOutputChannels;
};
