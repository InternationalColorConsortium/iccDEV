/**
 * iccDEV WASM Runtime -- shared module loader and UI helpers.
 *
 * Each tool page includes this script, then calls:
 *   const app = new IccWasmApp('iccDumpProfile', { ... });
 *   app.init();
 *
 * WASM modules are loaded from a configurable base URL.
 * Default: same directory as the HTML page (for GitHub Pages deployment).
 */
'use strict';

class IccWasmApp {
  /**
   * @param {string} toolName - Base name of the tool, e.g. 'iccDumpProfile'
   * @param {object} opts
   * @param {string} [opts.wasmBase] - Base URL for .js/.wasm files
   * @param {string} [opts.dropZoneId='drop-zone'] - ID of the drop zone element
   * @param {string} [opts.outputId='output'] - ID of the output <pre> element
   * @param {string} [opts.statusId='status'] - ID of the status bar text
   * @param {string} [opts.indicatorId='indicator'] - ID of the status indicator dot
   * @param {string} [opts.runBtnId='run-btn'] - ID of the run button
   * @param {Function} [opts.getArgs] - Returns array of CLI args for callMain
   * @param {Function} [opts.beforeRun] - Optional async hook after MEMFS setup and before callMain
   */
  constructor(toolName, opts = {}) {
    this.toolName = toolName;
    this.wasmBase = opts.wasmBase || this._detectBase();
    this.opts = opts;
    this.module = null;
    this.fileData = null;
    this.fileName = null;
    this.stdout = [];
    this.stderr = [];
  }

  _detectBase() {
    // Fixed relative path only -- never read from URL query parameters.
    // Reading wasmBase from location.search would allow an attacker to
    // inject an arbitrary script source (CWE-94 script injection):
    //   dump.html?wasmBase=https://evil.com/  -->  loads evil.com/iccDumpProfile.js
    return './';
  }

  async init() {
    this._bindUI();
    await this._loadModule();
  }

  _bindUI() {
    // ARIA live regions for dynamic content
    this._setupAria();

    // Drop zone
    const dz = document.getElementById(this.opts.dropZoneId || 'drop-zone');
    if (dz) {
      // Keyboard activation (WCAG 2.1.1)
      if (!dz.hasAttribute('tabindex')) dz.setAttribute('tabindex', '0');
      if (!dz.hasAttribute('role')) dz.setAttribute('role', 'button');

      const fileInput = dz.querySelector('input[type="file"]');
      dz.addEventListener('click', () => fileInput && fileInput.click());
      dz.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          if (fileInput) fileInput.click();
        }
      });
      dz.addEventListener('dragover', (e) => {
        e.preventDefault();
        dz.classList.add('dragover');
      });
      dz.addEventListener('dragleave', () => dz.classList.remove('dragover'));
      dz.addEventListener('drop', (e) => {
        e.preventDefault();
        dz.classList.remove('dragover');
        if (e.dataTransfer.files.length > 0) this._handleFile(e.dataTransfer.files[0]);
      });
      if (fileInput) {
        fileInput.addEventListener('change', (e) => {
          if (e.target.files.length > 0) this._handleFile(e.target.files[0]);
        });
      }
    }

    // Run button
    const btn = document.getElementById(this.opts.runBtnId || 'run-btn');
    if (btn) {
      btn.addEventListener('click', () => this.run());
    }

    // Download button
    const dlBtn = document.getElementById('download-btn');
    if (dlBtn) {
      dlBtn.addEventListener('click', () => this.downloadOutput());
    }

    // Clear button
    const clrBtn = document.getElementById('clear-btn');
    if (clrBtn) {
      clrBtn.addEventListener('click', () => {
        var out = document.getElementById(this.opts.outputId || 'output');
        if (out) out.textContent = '';
      });
    }
  }

  bindFileDropZone(dropZone, onFiles) {
    if (!dropZone) return null;
    const fileInput = dropZone.querySelector('input[type="file"]');
    if (!dropZone.hasAttribute('tabindex')) dropZone.setAttribute('tabindex', '0');
    if (!dropZone.hasAttribute('role')) dropZone.setAttribute('role', 'button');
    dropZone.addEventListener('click', function() { if (fileInput) fileInput.click(); });
    dropZone.addEventListener('keydown', function(e) {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        if (fileInput) fileInput.click();
      }
    });
    dropZone.addEventListener('dragover', function(e) {
      e.preventDefault();
      dropZone.classList.add('dragover');
    });
    dropZone.addEventListener('dragleave', function() { dropZone.classList.remove('dragover'); });
    dropZone.addEventListener('drop', function(e) {
      e.preventDefault();
      dropZone.classList.remove('dragover');
      onFiles(e.dataTransfer.files);
    });
    if (fileInput) {
      fileInput.addEventListener('change', function(e) { onFiles(e.target.files); });
    }
    return fileInput;
  }

  _setupAria() {
    // Status bar: announce state changes to screen readers
    const statusEl = document.getElementById(this.opts.statusId || 'status');
    if (statusEl) {
      var bar = statusEl.closest('.status-bar');
      if (bar) {
        bar.setAttribute('role', 'status');
        bar.setAttribute('aria-live', 'polite');
      }
    }
    // Output panel: announce new output
    var outContainer = document.getElementById('output-container');
    if (outContainer) {
      outContainer.setAttribute('role', 'log');
      outContainer.setAttribute('aria-live', 'polite');
    }
    // Icons: hide decorative emoji from screen readers
    document.querySelectorAll('.drop-zone .icon').forEach(function(el) {
      el.setAttribute('aria-hidden', 'true');
    });
    // All drop zones: ensure keyboard accessible
    document.querySelectorAll('.drop-zone').forEach(function(dz) {
      if (!dz.hasAttribute('tabindex')) dz.setAttribute('tabindex', '0');
      if (!dz.hasAttribute('role')) dz.setAttribute('role', 'button');
    });
  }

  async _handleFile(file) {
    const buffer = await file.arrayBuffer();
    this.fileData = new Uint8Array(buffer);
    this.fileName = file.name;

    // Update drop zone display -- use safeText to prevent control char injection
    const dz = document.getElementById(this.opts.dropZoneId || 'drop-zone');
    const fnEl = dz && dz.querySelector('.filename');
    if (fnEl) {
      const label = file.name + ' (' + this._formatSize(file.size) + ')';
      if (typeof iccSanitize !== 'undefined') {
        iccSanitize.safeText(fnEl, label);
      } else {
        fnEl.textContent = label;
      }
    }

    // Enable run button
    const btn = document.getElementById(this.opts.runBtnId || 'run-btn');
    if (btn) btn.disabled = false;
  }

  async _loadModule() {
    this._setStatus('loading', 'Loading WASM module...');

    try {
      // Dynamically load the tool's JS module
      const scriptUrl = this.wasmBase + this.toolName + '.js';
      await this._loadScript(scriptUrl);

      // The script defines a global `createModule` function
      if (typeof createModule !== 'function') {
        throw new Error('createModule not found after loading ' + scriptUrl);
      }

      this._setStatus('ready', 'Module loaded -- ready');
    } catch (err) {
      this._setStatus('error', 'Failed to load module: ' + err.message);
      console.error(err);
    }
  }

  _loadScript(url) {
    return new Promise((resolve, reject) => {
      const s = document.createElement('script');
      s.src = url;
      s.onload = resolve;
      s.onerror = () => reject(new Error('Script load failed: ' + url));
      document.head.appendChild(s);
    });
  }

  async run() {
    if (!this.fileData) {
      this._setStatus('error', 'No file loaded');
      return;
    }

    this.stdout = [];
    this.stderr = [];
    this._clearOutput();
    this._setStatus('loading', 'Running ' + this.toolName + '...');

    const btn = document.getElementById(this.opts.runBtnId || 'run-btn');
    if (btn) btn.disabled = true;

    // Release previous module to prevent memory leaks
    if (this.module) {
      this.module = null;
    }

    try {
      const self = this;
      this.module = await createModule({
        print: function(text) {
          self.stdout.push(text);
          self._appendOutput(text, 'stdout');
        },
        printErr: function(text) {
          self.stderr.push(text);
          self._appendOutput(text, 'stderr');
        },
        noInitialRun: true
      });

      // Write the uploaded file to the virtual filesystem
      var safeName = this.fileName;
      if (typeof iccSanitize !== 'undefined') {
        safeName = iccSanitize.sanitizeFilename(safeName);
      }
      const inputPath = '/' + safeName;
      this.module.FS.writeFile(inputPath, this.fileData);

      if (this.opts.beforeRun) {
        await this.opts.beforeRun.call(this, inputPath);
      }

      // Get CLI arguments from the page
      const args = this.opts.getArgs
        ? this.opts.getArgs(inputPath)
        : [inputPath];

      // Run the tool
      const startTime = performance.now();
      let exitCode;
      try {
        exitCode = this.module.callMain(args);
      } catch (e) {
        // Emscripten exit() throws; treat as normal exit
        if (e.name === 'ExitStatus' || e.message?.includes('exit')) {
          exitCode = typeof e.status === 'number' ? e.status : 1;
        } else {
          throw e;
        }
      }
      const elapsed = ((performance.now() - startTime) / 1000).toFixed(2);

      // Clean up virtual filesystem
      try { this.module.FS.unlink(inputPath); } catch (_) { /* ignore */ }

      if (exitCode === 0) {
        this._setStatus('ready', 'Completed in ' + elapsed + 's (exit 0)');
      } else {
        this._setStatus('error', 'Exit code ' + exitCode + ' (' + elapsed + 's)');
      }
    } catch (err) {
      this._setStatus('error', 'Runtime error: ' + err.message);
      this._appendOutput('ERROR: ' + err.message, 'stderr');
      console.error(err);
    } finally {
      if (btn) btn.disabled = !this.fileData;
    }
  }

  /**
   * Run with additional output files to extract from the virtual FS.
   * @param {string} outputPath - Path in the virtual FS to read after execution
   * @returns {Uint8Array|null} - Output file contents, or null if not found
   */
  async runWithOutput(outputPath) {
    await this.run();
    if (this.module) {
      try {
        return this.module.FS.readFile(outputPath);
      } catch (_) {
        return null;
      }
    }
    return null;
  }

  async writeFileToFS(file) {
    if (!this.module) {
      throw new Error('WASM module is not initialized');
    }
    var safeName = file.name;
    if (typeof iccSanitize !== 'undefined') {
      safeName = iccSanitize.sanitizeFilename(safeName);
    }
    var path = '/' + safeName;
    var buf = await file.arrayBuffer();
    this.module.FS.writeFile(path, new Uint8Array(buf));
    return path;
  }

  // --- UI helpers ---

  _setStatus(state, text) {
    const indicator = document.getElementById(this.opts.indicatorId || 'indicator');
    const statusText = document.getElementById(this.opts.statusId || 'status');
    if (indicator) {
      indicator.className = 'status-indicator ' + state;
    }
    if (statusText) {
      statusText.textContent = text;
      statusText.className = 'status-text' + (state === 'error' ? ' error' : '');
    }
  }

  _clearOutput() {
    const el = document.getElementById(this.opts.outputId || 'output');
    if (el) {
      el.textContent = '';
      el.closest('.output-content')?.classList.remove('empty');
    }
  }

  _appendOutput(text, stream) {
    const el = document.getElementById(this.opts.outputId || 'output');
    if (!el) return;
    el.closest('.output-content')?.classList.remove('empty');

    // Full sanitization: strip C0, Unicode bidi, ANSI escapes.
    var clean = typeof iccSanitize !== 'undefined'
      ? iccSanitize.sanitize(text)
      : text;

    // Truncate oversized output to prevent browser DoS
    if (typeof iccSanitize !== 'undefined' && el.textContent.length > iccSanitize.MAX_OUTPUT) {
      el.appendChild(document.createTextNode('\n[output truncated]\n'));
      return;
    }

    if (stream === 'stderr') {
      const span = document.createElement('span');
      span.className = 'stderr';
      span.textContent = clean + '\n';
      el.appendChild(span);
    } else {
      el.appendChild(document.createTextNode(clean + '\n'));
    }

    // Auto-scroll to bottom
    const container = el.closest('.output-content');
    if (container) container.scrollTop = container.scrollHeight;
  }

  getOutput() {
    return this.stdout.join('\n');
  }

  getStderr() {
    return this.stderr.join('\n');
  }

  downloadOutput(filename) {
    const text = this.getOutput();
    if (!text) return;
    const blob = new Blob([text], { type: 'text/plain' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = filename || (this.toolName + '-output.txt');
    a.click();
    URL.revokeObjectURL(a.href);
  }

  downloadBinary(data, filename) {
    if (!data) return;
    const blob = new Blob([data], { type: 'application/octet-stream' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = filename;
    a.click();
    URL.revokeObjectURL(a.href);
  }

  _formatSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(1) + ' MB';
  }
}

// Export for use in tool pages
window.IccWasmApp = IccWasmApp;
