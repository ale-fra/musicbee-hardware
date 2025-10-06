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

  /**
   * Start the backend request on a background FreeRTOS task. Returns
   * true if the task was created successfully. The caller can poll
   * {@link pollResult} to obtain the outcome once the request
   * completes.
   */
  bool beginPostPlayAsync(const String &cardUid);

  /**
   * Returns true while a background request is still running.
   */
  bool isBusy() const;

  /**
   * Poll for the result of the most recent asynchronous request. When
   * the request completes, this method stores the success flag in
   * `outSuccess` and returns true. Otherwise it returns false to
   * indicate that the operation is still in progress.
   */
  bool pollResult(bool &outSuccess);

  /**
   * Resolve a hostname, performing an mDNS lookup when the provided
   * host ends with `.local`. The resolved hostname (IP string or the
   * original host if no lookup was necessary) is written to
   * `resolvedOut`.
   */
  static bool resolveHostname(const String &host, String &resolvedOut);

private:
  bool performPostPlay(const String &cardUid);
  static void requestTask(void *param);

  volatile bool requestInProgress = false;
  volatile bool requestCompleted = false;
  volatile bool lastRequestSuccess = false;
  String pendingUid;
};
