/*
 * ActionCards.h
 *
 * Defines data structures and helpers for special NFC command cards
 * that trigger local firmware actions instead of contacting the backend.
 * Action cards are configured via Config.h so deployments can map
 * specific UIDs to built-in actions.
 */

#pragma once

#include <Arduino.h>
#include <cstddef>

/** Types of command cards supported by the firmware. */
enum class ActionCardType {
  Reset,
};

/** Mapping between a card UID and the action it should trigger. */
struct ActionCardEntry {
  const char *uid;
  ActionCardType type;
};

/** Lookup table utility for resolving action cards by UID. */
class ActionCardRegistry {
public:
  ActionCardRegistry(const ActionCardEntry *entries, size_t count)
      : _entries(entries), _count(count) {}

  /**
   * Return the first matching action card entry for the provided UID.
   * Matching is case-insensitive to accommodate different UID formats.
   * Returns nullptr if the UID is not associated with an action card.
   */
  const ActionCardEntry *findByUid(const String &uid) const;

private:
  const ActionCardEntry *_entries = nullptr;
  size_t _count = 0;
};

