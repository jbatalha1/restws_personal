<?php

/**
 * @file
 * An exception defining the HTTP error code and message.
 */

/**
 * An exception defining the HTTP error code and message.
 */
class RestWSException extends Exception {

  public function getHTTPError() {
    $code = $this->getCode();
    switch ($code) {
      case 403:
        return '403 Forbidden';
      case 404:
        return '404 Not Found';
      case 406:
        return '406 Not Acceptable';
      case 412:
        return '412 Precondition Failed';
      case 422:
        return '422 Unprocessable Entity';
      default:
        return '500 Internal Server Error';
    }
  }
}
