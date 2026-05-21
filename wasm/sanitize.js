/**
 * @file sanitize.js
 *
 * Client-side sanitization primitives for iccDEV WASM tool pages.
 *
 * Mirrors the server-side IccIsapiSanitize API surface on the client
 * and incorporates patterns from .github/scripts/sanitize-sed.sh v4.
 *
 * Defenses:
 *   - HTML entity encoding        (XSS via innerHTML -- CWE-79)
 *   - URI scheme/fragment guard    (DOM-XSS via location.hash -- CWE-79)
 *   - Control-char stripping       (log spoofing, header injection)
 *   - Unicode Trojan Source guard  (bidi override, zero-width -- CWE-1104)
 *   - ANSI escape stripping        (terminal hijacking)
 *   - Filename sanitization         (path traversal -- CWE-22)
 *   - Output truncation             (DoS via oversized output)
 *   - Safe DOM text helper          (.textContent enforcement)
 *
 * Copyright (c) International Color Consortium.  BSD 3-Clause.
 */

"use strict";

var iccSanitize = Object.freeze({

  /** Maximum single-line output length. */
  MAX_LINE: 1000,

  /** Maximum multi-line output length. */
  MAX_OUTPUT: 64000,

  /**
   * Escape the 5 HTML-sensitive characters.
   * Mirrors HtmlEscape() in IccIsapiSanitize.cpp.
   */
  htmlEscape: function htmlEscape(str) {
    if (typeof str !== "string") {
      return "";
    }
    return str
      .replace(/&/g,  "&amp;")
      .replace(/</g,  "&lt;")
      .replace(/>/g,  "&gt;")
      .replace(/"/g,  "&quot;")
      .replace(/'/g,  "&#39;");
  },

  /**
   * Strip C0 control characters (except TAB and LF) and DEL.
   * Mirrors the C0/DEL stripping in HtmlEscape() server-side.
   */
  stripControl: function stripControl(str) {
    if (typeof str !== "string") {
      return "";
    }
    // eslint-disable-next-line no-control-regex
    return str.replace(/[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/g, "");
  },

  /**
   * Strip Unicode control characters used in Trojan Source attacks.
   * Mirrors _strip_unicode_control() in sanitize-sed.sh v4.
   *
   * Removes: BOM, bidi overrides/embeddings/isolates, zero-width
   * chars, line/paragraph separators, interlinear annotations.
   */
  stripUnicode: function stripUnicode(str) {
    if (typeof str !== "string") {
      return "";
    }
    return str
      .replace(/[\u200B-\u200F]/g, "")
      .replace(/[\u2028-\u202F]/g, "")
      .replace(/[\u2060-\u2069]/g, "")
      .replace(/[\uFEFF]/g, "")
      .replace(/[\uFFF9-\uFFFB]/g, "");
  },

  /**
   * Strip ANSI escape sequences (CSI, OSC, bare ESC).
   * Prevents terminal hijacking in browser console output.
   */
  stripAnsi: function stripAnsi(str) {
    if (typeof str !== "string") {
      return "";
    }
    return str
      .replace(/\x1B\[[0-9;]*[A-Za-z]/g, "")
      .replace(/\x1B\][^\x07]*\x07/g, "")
      .replace(/\x1B/g, "");
  },

  /**
   * Full sanitization: strip C0, Unicode, and ANSI controls.
   * Use for any untrusted string before display.
   */
  sanitize: function sanitize(str) {
    return iccSanitize.stripAnsi(
      iccSanitize.stripUnicode(
        iccSanitize.stripControl(str)
      )
    );
  },

  /**
   * Sanitize multi-line output preserving newlines.
   * Collapses 4+ consecutive newlines to 3 (DoS prevention).
   */
  sanitizeMultiline: function sanitizeMultiline(str) {
    if (typeof str !== "string") {
      return "";
    }
    var clean = iccSanitize.sanitize(str);
    return clean.replace(/\n{4,}/g, "\n\n\n");
  },

  /**
   * Truncate string with ellipsis notice if it exceeds maxLen.
   * Mirrors _truncate() in sanitize-sed.sh and
   * TruncateForBrowser() in IccIsapiSanitize.cpp.
   */
  truncate: function truncate(str, maxLen) {
    if (typeof str !== "string") {
      return "";
    }
    var limit = maxLen || iccSanitize.MAX_OUTPUT;
    if (str.length <= limit) {
      return str;
    }
    return str.substring(0, limit) + "\n[truncated at " + limit + " chars]";
  },

  /**
   * Sanitize a filename for safe display and download attributes.
   * Mirrors SanitizeFilename() in IccIsapiSanitize.cpp and
   * sanitize_filename() in sanitize-sed.sh.
   *
   * Keeps only alphanumeric, dash, underscore, and dot.
   * Strips leading dots (hidden file prevention).
   * Removes path separators (traversal prevention).
   */
  sanitizeFilename: function sanitizeFilename(name) {
    if (typeof name !== "string" || name.length === 0) {
      return "untitled";
    }
    var basename = name.split(/[\\/]/).pop() || "untitled";
    var safe = basename
      .replace(/[^A-Za-z0-9._-]/g, "_")
      .replace(/^\.+/, "")
      .substring(0, 255);
    return safe || "untitled";
  },

  /**
   * Sanitize a URI: strip fragment, reject dangerous schemes.
   * Mirrors SanitizeUri() in IccIsapiSanitize.cpp.
   *
   * Only allows relative paths, http:, and https:.
   * Blocks javascript:, data:, vbscript:, and any other scheme.
   */
  sanitizeUri: function sanitizeUri(uri) {
    if (typeof uri !== "string" || uri.length === 0) {
      return "";
    }

    var clean = uri.replace(/[\0\n\r\t]/g, "");

    var hashIdx = clean.indexOf("#");
    if (hashIdx !== -1) {
      clean = clean.substring(0, hashIdx);
    }

    var colonIdx = clean.indexOf(":");
    if (colonIdx !== -1) {
      var isRelative = clean[0] === "." || clean[0] === "/";
      var lower = clean.toLowerCase();
      var isHttp = lower.indexOf("http://") === 0;
      var isHttps = lower.indexOf("https://") === 0;

      if (!isRelative && !isHttp && !isHttps) {
        return "";
      }
    }

    return clean;
  },

  /**
   * Safely set text content of a DOM element.
   * Never uses innerHTML. Applies full sanitization first.
   */
  safeText: function safeText(element, text) {
    if (!element) {
      return;
    }
    element.textContent = iccSanitize.sanitize(
      typeof text === "string" ? text : String(text)
    );
  },

  /**
   * Extract a query parameter from the current URL safely.
   */
  getQueryParam: function getQueryParam(name) {
    try {
      var params = new URLSearchParams(window.location.search);
      var value = params.get(name);
      return value ? iccSanitize.sanitize(value) : "";
    } catch (e) {
      return "";
    }
  },

  /**
   * Build a safe relative URL for fetch/navigation.
   */
  safeRelativeUrl: function safeRelativeUrl(path, params) {
    var clean = iccSanitize.sanitizeUri(path);
    if (!clean) {
      return "";
    }
    if (params && typeof params === "object") {
      var search = new URLSearchParams(params).toString();
      if (search) {
        clean += (clean.indexOf("?") === -1 ? "?" : "&") + search;
      }
    }
    return clean;
  }

});
