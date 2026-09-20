/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the
 * ICC Software License are met.
 */

class Value {
};

class Owner {
public:
  void SetValue(Value *value)
  {
    delete m_value;
    m_value = value;
  }

  void SetGuardedValue(Value *value)
  {
    if (m_value == value)
      return;
    delete m_value;
    m_value = value;
  }

private:
  Value *m_value;
};
