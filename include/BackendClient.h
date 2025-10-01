/*
 * BackendClient.h
 *
 * Handles communication with the jukebox backend service. It exposes
 * a single method to notify the backend when a tag has been read.
 * The backend is expected to accept a POST request at the endpoint
 * `/api/v1/cards/{uid}/play` and return a JSON response. Only the
 * status code is used to determine success or failure.
 */

#pragma once

#include <Arduino.h>

class BackendClient {
public:
  /**
   * Perform a POST request to the backend indicating that a card with
   * the given UID has been presented. Returns true if the HTTP
   * response code is in the 200‑299 range. On failure, the method
   * returns false and logs the error via Serial.
   */
  bool postPlay(const String &cardUid);
};