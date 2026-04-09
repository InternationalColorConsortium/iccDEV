/**
 * @file sanitize.js
 *
 * Client-side sanitization primitives for iccDEV IIS ISAPI pages.
 *
 * Mirrors the server-side IccIsapiSanitize API surface on the client.
 * Loaded before site.js; provides the global `iccSanitize` object.
 *
 * Defenses:
 *   - HTML entity encoding        (XSS via innerHTML — CWE-79)
 *   - URI scheme/fragment guard    (DOM-XSS via location.hash — CWE-79)
 *   - Control-char stripping       (log spoofing, header injection)
 *   - Safe DOM text helper         (.textContent enforcement)
 *
 * Reference DOM-XSS vectors this prevents:
 *   document.location.replace(document.location.hash.split("#")[1])
 *   element.innerHTML = new URLSearchParams(location.search).get("q")
 *   ?q=<a"'\x0A`= +%20>;...hostname?url=https://attacker:8889
 *
 * Copyright (c) International Color Consortium.  BSD 3-Clause.
 */

"use strict";

const iccSanitize = Object.freeze({

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
   * Strip C0 control characters (except TAB \x09 and LF \x0A) and DEL \x7F.
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
   * Sanitize a URI: strip fragment, reject dangerous schemes.
   * Mirrors SanitizeUri() in IccIsapiSanitize.cpp.
   *
   * Only allows relative paths (./  ../  /), http:, and https:.
   * Blocks javascript:, data:, vbscript:, and any other scheme.
   *
   * Also strips null bytes, newlines, and tabs that confuse URL parsers.
   * These characters let attackers bypass naive scheme checks:
   *   "java\tscript:alert(1)" → some parsers see "javascript:alert(1)"
   *   "java\x00script:alert(1)" → C-string truncation to "java"
   */
  sanitizeUri: function sanitizeUri(uri) {
    if (typeof uri !== "string" || uri.length === 0) {
      return "";
    }

    // Strip parser-confusing characters: null, newline, carriage return, tab
    var clean = uri.replace(/[\0\n\r\t]/g, "");

    // Strip fragment (#...) — prevents DOM-XSS via location.hash reflection
    var hashIdx = clean.indexOf("#");
    if (hashIdx !== -1) {
      clean = clean.substring(0, hashIdx);
    }

    // Check for scheme prefix (colon before first slash)
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
   * Never uses innerHTML. Strips control characters first.
   * Use this instead of direct .textContent assignment when the
   * source is untrusted (query params, server responses, user input).
   */
  safeText: function safeText(element, text) {
    if (!element) {
      return;
    }
    element.textContent = iccSanitize.stripControl(
      typeof text === "string" ? text : String(text)
    );
  },

  /**
   * Extract a query parameter from the current URL safely.
   * Returns the decoded value or empty string if not found.
   * Strips control characters from the result.
   */
  getQueryParam: function getQueryParam(name) {
    try {
      var params = new URLSearchParams(window.location.search);
      var value = params.get(name);
      return value ? iccSanitize.stripControl(value) : "";
    } catch (e) {
      return "";
    }
  },

  /**
   * Build a safe relative URL for fetch/navigation.
   * Rejects absolute URLs with non-http(s) schemes and strips fragments.
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
