/*
 * ActionCards.cpp
 *
 * Implements lookup helpers for NFC command cards that trigger
 * firmware-level actions without contacting the backend.
 */

#include "ActionCards.h"

const ActionCardEntry *ActionCardRegistry::findByUid(const String &uid) const {
  if (_entries == nullptr || _count == 0) {
    return nullptr;
  }

  for (size_t i = 0; i < _count; ++i) {
    const ActionCardEntry &entry = _entries[i];
    if (entry.uid == nullptr || entry.uid[0] == '\0') {
      continue;
    }
    if (uid.equalsIgnoreCase(entry.uid)) {
      return &entry;
    }
  }

  return nullptr;
}

