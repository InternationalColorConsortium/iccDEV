"""REST API layer for iccdev-mcp.

Mirrors all 25 MCP tools as HTTP endpoints using Starlette.
Each endpoint delegates to the same function backing the MCP tool,
ensuring MCP and REST always return identical results.

Usage:
    python -m iccdev_mcp.rest_api [--port 8080]
    # or via the main entry point:
    iccdev-mcp --transport sse --port 8080
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import re
import tempfile
import uuid
from pathlib import Path
from typing import Any

try:
    from starlette.applications import Starlette
    from starlette.middleware import Middleware
    from starlette.middleware.cors import CORSMiddleware
    from starlette.requests import Request
    from starlette.responses import HTMLResponse, JSONResponse, Response
    from starlette.routing import Route
    from starlette.datastructures import UploadFile
    from starlette.formparsers import MultiPartException
    HAS_STARLETTE = True
except ImportError:
    HAS_STARLETTE = False

try:
    import uvicorn
    HAS_UVICORN = True
except ImportError:
    HAS_UVICORN = False

from iccdev_mcp import profiles, cli_tools

# Maximum upload size: 20 MB
MAX_UPLOAD_BYTES = 20 * 1024 * 1024
MAX_MULTIPART_BYTES = MAX_UPLOAD_BYTES + 64 * 1024

# Maximum JSON request body size: 10 MB
MAX_JSON_BYTES = 10 * 1024 * 1024

# Upload temp directory
UPLOAD_DIR = Path(tempfile.gettempdir()) / "iccdev-mcp-uploads"

_FILE_KINDS = {
    "profile": ("*.icc", "*.icm", "*.ICC", "*.ICM"),
    "xml": ("*.xml", "*.XML"),
    "json": ("*.json", "*.JSON"),
    "tiff": ("*.tif", "*.tiff", "*.TIF", "*.TIFF"),
    "jpeg": ("*.jpg", "*.jpeg", "*.JPG", "*.JPEG"),
    "png": ("*.png", "*.PNG"),
    "cube": ("*.cube", "*.CUBE"),
}

_JSON_HEADERS = {
    "Cache-Control": "no-store",
    "X-Content-Type-Options": "nosniff",
    "X-Frame-Options": "DENY",
    "Referrer-Policy": "no-referrer",
    "Content-Security-Policy": "default-src 'none'; frame-ancestors 'none'",
}

_HTML_HEADERS = {
    **_JSON_HEADERS,
    "Content-Security-Policy": (
        "default-src 'none'; "
        "script-src 'self' 'unsafe-inline'; "
        "style-src 'self' 'unsafe-inline'; "
        "connect-src 'self'; "
        "img-src 'self' data:; "
        "base-uri 'self'; "
        "form-action 'none'; "
        "frame-ancestors 'none'"
    ),
}

_REST_UI_HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="description" content="REST dashboard for iccdev-mcp ICC profile tools">
  <title>iccdev-mcp REST Dashboard</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #0b1020;
      --panel: #121a2d;
      --panel-2: #17213a;
      --border: #2b3a5d;
      --text: #e7edf8;
      --muted: #9fb0ca;
      --accent: #67b7ff;
      --ok: #58d68d;
      --warn: #f5c26b;
      --bad: #ff6b6b;
      --mono: ui-monospace, SFMono-Regular, Consolas, Liberation Mono, monospace;
      --sans: system-ui, -apple-system, BlinkMacSystemFont, Segoe UI, sans-serif;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      background: linear-gradient(135deg, #0b1020 0%, #111a31 100%);
      color: var(--text);
      font-family: var(--sans);
      line-height: 1.5;
    }
    a { color: var(--accent); }
    .skip {
      position: absolute;
      left: -999px;
      top: 0;
      background: var(--accent);
      color: #06101f;
      padding: .5rem .75rem;
      z-index: 10;
    }
    .skip:focus { left: .75rem; top: .75rem; }
    header {
      padding: 1.25rem clamp(1rem, 4vw, 2.5rem);
      border-bottom: 1px solid var(--border);
      background: rgba(11, 16, 32, .92);
      position: sticky;
      top: 0;
      z-index: 2;
      backdrop-filter: blur(10px);
    }
    h1 { margin: 0; font-size: clamp(1.4rem, 3vw, 2rem); }
    .subtitle { margin: .25rem 0 0; color: var(--muted); }
    .shell {
      display: grid;
      grid-template-columns: minmax(260px, 340px) 1fr;
      gap: 1rem;
      padding: 1rem clamp(1rem, 4vw, 2.5rem) 2rem;
    }
    aside, main, .card {
      background: rgba(18, 26, 45, .92);
      border: 1px solid var(--border);
      border-radius: 14px;
      box-shadow: 0 16px 40px rgba(0, 0, 0, .18);
    }
    aside { padding: 1rem; align-self: start; position: sticky; top: 6rem; }
    main { padding: 1rem; min-width: 0; }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: .75rem;
      margin-bottom: 1rem;
    }
    .card { padding: .85rem; }
    .label { color: var(--muted); font-size: .78rem; text-transform: uppercase; letter-spacing: .07em; }
    .value { font: 600 1rem var(--mono); margin-top: .25rem; overflow-wrap: anywhere; }
    .ok { color: var(--ok); }
    .bad { color: var(--bad); }
    .warn { color: var(--warn); }
    .tool-group { margin-top: 1rem; }
    .tool-group h2 {
      color: var(--muted);
      font-size: .8rem;
      margin: 0 0 .45rem;
      text-transform: uppercase;
      letter-spacing: .08em;
    }
    .tool-list { display: grid; gap: .35rem; }
    button, input, select, textarea {
      font: inherit;
      border-radius: 10px;
    }
    button {
      border: 1px solid var(--border);
      background: var(--panel-2);
      color: var(--text);
      padding: .55rem .7rem;
      cursor: pointer;
    }
    button:hover, button:focus { border-color: var(--accent); outline: none; }
    button.active { background: var(--accent); color: #06101f; border-color: var(--accent); font-weight: 700; }
    button.primary { background: var(--ok); color: #06101f; border-color: var(--ok); font-weight: 700; }
    button.secondary { background: transparent; color: var(--accent); }
    button:disabled { opacity: .55; cursor: wait; }
    .tool-button { text-align: left; width: 100%; }
    .toolbar { display: flex; gap: .5rem; flex-wrap: wrap; align-items: center; margin-bottom: .75rem; }
    .tool-title { margin: 0; font-size: 1.35rem; }
    .tool-description { color: var(--muted); margin: .25rem 0 1rem; }
    .form-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: .75rem;
      margin-bottom: .75rem;
    }
    .field.full { grid-column: 1 / -1; }
    label { display: block; color: var(--muted); font-weight: 700; font-size: .82rem; margin-bottom: .25rem; }
    input, select, textarea {
      width: 100%;
      padding: .6rem .7rem;
      color: var(--text);
      background: #091022;
      border: 1px solid var(--border);
    }
    textarea { min-height: 6.5rem; resize: vertical; font-family: var(--mono); }
    input:focus, select:focus, textarea:focus { border-color: var(--accent); outline: none; }
    .hint { color: var(--muted); font-size: .82rem; margin-top: .35rem; }
    .output-head { display: flex; justify-content: space-between; gap: .75rem; align-items: center; margin: 1rem 0 .5rem; }
    .output-title { margin: 0; font-size: 1rem; }
    pre {
      margin: 0;
      min-height: 18rem;
      max-height: 34rem;
      overflow: auto;
      white-space: pre-wrap;
      overflow-wrap: anywhere;
      background: #050914;
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: .9rem;
      font-family: var(--mono);
      font-size: .86rem;
    }
    .pill {
      display: inline-flex;
      gap: .35rem;
      align-items: center;
      border: 1px solid var(--border);
      border-radius: 999px;
      padding: .25rem .55rem;
      color: var(--muted);
      font-size: .82rem;
    }
    .drop {
      border: 1px dashed var(--border);
      padding: .75rem;
      border-radius: 12px;
      background: rgba(103, 183, 255, .06);
    }
    .drop.drag { border-color: var(--accent); }
    .profile-picker {
      grid-column: 1 / -1;
      display: grid;
      grid-template-columns: minmax(180px, .65fr) minmax(240px, 1fr);
      gap: .75rem;
    }
    .profile-picker .field { margin: 0; }
    .profile-picker .hint { grid-column: 1 / -1; margin: 0; }
    footer { color: var(--muted); padding: 0 clamp(1rem, 4vw, 2.5rem) 2rem; }
    @media (max-width: 900px) {
      .shell { grid-template-columns: 1fr; }
      aside { position: static; }
      .status-grid, .form-grid, .profile-picker { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <a href="#main" class="skip">Skip to tools</a>
  <header>
    <h1>iccdev-mcp REST Dashboard</h1>
    <p class="subtitle">REST dashboard for iccDEV MCP tools: Python-native tools, CLI wrappers, upload flow, and profile browser.</p>
  </header>
  <div class="shell">
    <aside aria-label="Tool navigation">
      <div class="card">
        <div class="label">Server</div>
        <div id="serverStatus" class="value warn">Loading...</div>
      </div>
      <div id="toolNav"></div>
    </aside>
    <main id="main" tabindex="-1">
      <section class="status-grid" aria-label="Server status">
        <div class="card"><div class="label">Python API</div><div id="pythonStatus" class="value">...</div></div>
        <div class="card"><div class="label">CLI tools</div><div id="cliStatus" class="value">...</div></div>
        <div class="card"><div class="label">Profiles</div><div id="profileStatus" class="value">...</div></div>
      </section>
      <section class="card">
        <div class="toolbar">
          <span id="selectedEndpoint" class="pill">No endpoint selected</span>
          <button id="refreshBtn" class="secondary" type="button">Refresh metadata</button>
        </div>
        <h2 id="toolTitle" class="tool-title">Select a tool</h2>
        <p id="toolDescription" class="tool-description">Choose a tool from the left. Forms are generated from the iccdev-mcp REST contract.</p>
        <form id="toolForm" autocomplete="off"></form>
        <div class="toolbar">
          <button id="runBtn" class="primary" type="button">Run tool</button>
          <button id="copyBtn" class="secondary" type="button">Copy output</button>
          <button id="clearBtn" class="secondary" type="button">Clear</button>
        </div>
        <div class="output-head">
          <h3 class="output-title">Output</h3>
          <span id="runStatus" class="pill" role="status" aria-live="polite">Idle</span>
        </div>
        <pre id="output" tabindex="0">Ready.</pre>
      </section>
    </main>
  </div>
  <footer>International Color Consortium iccDEV MCP server. REST API and UI run on the same origin.</footer>
  <script>
'use strict';
const TOOL_GROUPS = [
  { title: 'Status', tools: ['health', 'tools', 'profiles', 'enum_spaces'] },
  { title: 'Python native', tools: ['inspect_header', 'profile_summary', 'sig_to_str', 'color_transform', 'roundtrip_delta'] },
  { title: 'ICC profile CLI', tools: ['dump_profile', 'pawg_report', 'to_xml', 'to_json', 'roundtrip'] },
  { title: 'Image and conversion CLI', tools: ['tiff_dump', 'jpeg_dump', 'png_dump', 'from_xml', 'from_json', 'from_cube'] },
  { title: 'Advanced CLI', tools: ['apply_profiles', 'apply_named_cmm', 'create_link', 'v5_to_v4', 'spec_sep', 'apply_search'] },
  { title: 'File management', tools: ['upload'] }
];
const TOOLS = {
  health: { label: 'Health', method: 'GET', path: '/api/health', desc: 'Server status, Python binding availability, and CLI discovery.', fields: [] },
  tools: { label: 'Tool discovery', method: 'GET', path: '/api/tools', desc: 'List the 25 MCP tools plus REST-only utility routes exposed by iccdev-mcp.', fields: [] },
  profiles: { label: 'Profile browser', method: 'GET', path: '/api/profiles', desc: 'List bundled or configured ICC profiles by Testing/ directory and filename.', fields: [{ name: 'directory', label: 'Directory', type: 'profile_directory', kind: 'profile' }, { name: 'filename', label: 'Filename', type: 'testing_file', kind: 'profile', directoryField: 'directory' }] },
  enum_spaces: { label: 'Enum color spaces', method: 'GET', path: '/api/enum-spaces', desc: 'List Python ColorSpace enum values.', fields: [] },
  inspect_header: { label: 'Inspect header', method: 'GET', path: '/api/inspect-header', desc: 'Read ICC profile header fields with computed names.', fields: [{ name: 'path', label: 'ICC profile', type: 'profile', required: true }] },
  profile_summary: { label: 'Profile summary', method: 'GET', path: '/api/profile-summary', desc: 'Compact Python-native profile classification and routing metadata.', fields: [{ name: 'path', label: 'ICC profile', type: 'profile', required: true }] },
  sig_to_str: { label: 'Signature to string', method: 'GET', path: '/api/sig-to-str', desc: 'Decode a 32-bit ICC signature integer.', fields: [{ name: 'sig', label: 'Signature integer', type: 'number', value: '1380401696', required: true }] },
  color_transform: { label: 'Color transform', method: 'POST', path: '/api/color-transform', desc: 'Apply a Python-native CMM transform between compatible RGB display profiles. Overprint, high-channel, named-color, and calculator test profiles may reject the default 3-channel pixel input with BadXform.', fields: [{ name: 'src_profile', label: 'Source profile', type: 'profile', required: true, prefer: 'rgb_transform' }, { name: 'dst_profile', label: 'Destination profile', type: 'profile', required: true, prefer: 'rgb_transform' }, { name: 'pixels', label: 'Pixels JSON', type: 'json', value: '[[0.5,0.4,0.3]]', required: true }, { name: 'rendering_intent', label: 'Rendering intent', type: 'select', value: 'perceptual', options: ['perceptual', 'relative', 'saturation', 'absolute'] }, { name: 'interpolation', label: 'Interpolation', type: 'select', value: 'tetrahedral', options: ['tetrahedral', 'linear'] }] },
  roundtrip_delta: { label: 'Roundtrip delta', method: 'POST', path: '/api/roundtrip-delta', desc: 'Measure Python-native round-trip delta for profile pixels.', fields: [{ name: 'profile', label: 'ICC profile', type: 'profile', required: true }, { name: 'pixels', label: 'Pixels JSON', type: 'json', value: '[[0.5,0.4,0.3]]', required: true }, { name: 'rendering_intent', label: 'Rendering intent', type: 'select', value: 'perceptual', options: ['perceptual', 'relative', 'saturation', 'absolute'] }] },
  dump_profile: { label: 'Dump profile', method: 'GET', path: '/api/dump', desc: 'Run iccDumpProfile with documented validation, verbosity, and tag options.', fields: [{ name: 'path', label: 'ICC profile', type: 'profile', required: true }, { name: 'validate', label: 'Validate (-v)', type: 'select', value: 'true', options: ['true', 'false'] }, { name: 'verbosity', label: 'Verbosity', type: 'number', value: '100' }, { name: 'tag', label: 'Tag', type: 'text', value: 'ALL' }] },
  pawg_report: { label: 'PAWG report', method: 'GET', path: '/api/pawg-report', desc: 'Run iccPawgReport security, conformance, and quality checklist.', fields: [{ name: 'path', label: 'ICC profile', type: 'profile', required: true }] },
  to_xml: { label: 'ICC to XML', method: 'GET', path: '/api/to-xml', desc: 'Run iccToXml.', fields: [{ name: 'path', label: 'ICC profile', type: 'profile', required: true }] },
  to_json: { label: 'ICC to JSON', method: 'GET', path: '/api/to-json', desc: 'Run iccToJson.', fields: [{ name: 'path', label: 'ICC profile', type: 'profile', required: true }] },
  roundtrip: { label: 'Round-trip test', method: 'GET', path: '/api/roundtrip', desc: 'Run iccRoundTrip with rendering intent and MPE/LUT selection.', fields: [{ name: 'path', label: 'ICC profile', type: 'profile', required: true }, { name: 'intent', label: 'Rendering intent', type: 'select', value: '1', options: ['0', '1', '2', '3'] }, { name: 'use_mpe', label: 'Use MPE', type: 'select', value: '0', options: ['0', '1'] }] },
  tiff_dump: { label: 'TIFF dump', method: 'GET', path: '/api/tiff-dump', desc: 'Run iccTiffDump on a TIFF image.', fields: [{ name: 'path', label: 'TIFF path', type: 'testing_file', kind: 'tiff', required: true }] },
  jpeg_dump: { label: 'JPEG dump', method: 'GET', path: '/api/jpeg-dump', desc: 'Run iccJpegDump on a JPEG image.', fields: [{ name: 'path', label: 'JPEG path', type: 'testing_file', kind: 'jpeg', required: true }] },
  png_dump: { label: 'PNG dump', method: 'GET', path: '/api/png-dump', desc: 'Run iccPngDump on a PNG image.', fields: [{ name: 'path', label: 'PNG path', type: 'testing_file', kind: 'png', required: true }] },
  from_xml: { label: 'XML to ICC', method: 'POST', path: '/api/from-xml', desc: 'Run iccFromXml on an XML file and return an ICC blob as base64.', fields: [{ name: 'xml_path', label: 'XML path', type: 'testing_file', kind: 'xml', required: true }] },
  from_json: { label: 'JSON to ICC', method: 'POST', path: '/api/from-json', desc: 'Run iccFromJson on a JSON file and return an ICC blob as base64.', fields: [{ name: 'json_path', label: 'JSON path', type: 'testing_file', kind: 'json', required: true }] },
  from_cube: { label: 'Cube to ICC', method: 'POST', path: '/api/from-cube', desc: 'Run iccFromCube on a .cube LUT.', fields: [{ name: 'cube_path', label: '.cube path', type: 'testing_file', kind: 'cube', required: true }] },
  apply_profiles: { label: 'Apply profiles', method: 'POST', path: '/api/apply-profiles', desc: 'Apply profile sequence to a TIFF input. Use config args for exact documented CLI forms such as -cfg config.json.', fields: [{ name: 'config_args', label: 'Raw CLI args JSON array', type: 'json', value: '[]' }, { name: 'input_tiff', label: 'Input TIFF', type: 'testing_file', kind: 'tiff' }, { name: 'profiles', label: 'Profiles JSON array', type: 'json', value: '[]' }, { name: 'intents', label: 'Profile intents JSON array', type: 'json', value: '[]' }, { name: 'encoding', label: 'Encoding', type: 'select', value: '1', options: ['0', '1', '2', '4'] }, { name: 'compress', label: 'Compress', type: 'select', value: '0', options: ['0', '1'] }, { name: 'planar', label: 'Planar', type: 'select', value: '0', options: ['0', '1'] }, { name: 'embed', label: 'Embed output ICC', type: 'select', value: '1', options: ['0', '1'] }, { name: 'interpolation', label: 'Interpolation', type: 'select', value: '0', options: ['0', '1'] }] },
  apply_named_cmm: { label: 'Apply named CMM', method: 'POST', path: '/api/apply-named-cmm', desc: 'Run iccApplyNamedCmm with config args.', fields: [{ name: 'config_args', label: 'Config args JSON array', type: 'json', value: '[]', required: true }] },
  create_link: { label: 'Create device link', method: 'POST', path: '/api/create-link', desc: 'Create a device link from profile paths.', fields: [{ name: 'profiles', label: 'Profiles JSON array', type: 'json', value: '[]', required: true }] },
  v5_to_v4: { label: 'V5 display to V4', method: 'POST', path: '/api/v5-to-v4', desc: 'Convert v5 display and observer profiles to v4.', fields: [{ name: 'display_profile', label: 'Display profile', type: 'profile', required: true }, { name: 'observer_profile', label: 'Observer profile', type: 'profile', required: true }] },
  spec_sep: { label: 'Spectral separation', method: 'POST', path: '/api/spec-sep', desc: 'Run iccSpecSepToTiff.', fields: [{ name: 'reflectance_profile', label: 'Reflectance profile', type: 'profile', required: true }, { name: 'colorant_profile', label: 'Colorant profile', type: 'profile', required: true }, { name: 'illuminant_profiles', label: 'Illuminant profiles JSON array', type: 'json', value: '[]' }] },
  apply_search: { label: 'Apply search', method: 'POST', path: '/api/apply-search', desc: 'Run iccApplySearch with config args.', fields: [{ name: 'config_args', label: 'Config args JSON array', type: 'json', value: '[]', required: true }] },
  upload: { label: 'Upload file', method: 'UPLOAD', path: '/api/upload', desc: 'Upload ICC, image, XML, JSON, or cube input for follow-up tool calls.', fields: [{ name: 'file', label: 'File', type: 'file', required: true }] }
};
let currentTool = 'health';
let profileOptions = [];
let profileDirectories = [];
let fileDirectories = {};
const profileCache = {};
const fileCache = {};
const $ = (id) => document.getElementById(id);
function setText(id, value, className) {
  const el = $(id);
  el.textContent = value;
  if (className) el.className = className;
}
function output(value) {
  $('output').textContent = typeof value === 'string' ? value : JSON.stringify(value, null, 2);
}
function parseJsonField(value, name) {
  try { return JSON.parse(value || 'null'); }
  catch (err) { throw new Error(name + ' must be valid JSON: ' + err.message); }
}
async function fetchJson(url, opts) {
  const resp = await fetch(url, opts);
  const text = await resp.text();
  let data;
  try { data = text ? JSON.parse(text) : {}; }
  catch (_) { data = { raw: text }; }
  if (!resp.ok) throw new Error((data && data.error) || ('HTTP ' + resp.status));
  return data;
}
function renderNav() {
  const nav = $('toolNav');
  nav.textContent = '';
  TOOL_GROUPS.forEach((group) => {
    const box = document.createElement('section');
    box.className = 'tool-group';
    const title = document.createElement('h2');
    title.textContent = group.title;
    box.appendChild(title);
    const list = document.createElement('div');
    list.className = 'tool-list';
    group.tools.forEach((name) => {
      const tool = TOOLS[name];
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'tool-button' + (name === currentTool ? ' active' : '');
      btn.textContent = tool.label;
      btn.addEventListener('click', () => selectTool(name));
      list.appendChild(btn);
    });
    box.appendChild(list);
    nav.appendChild(box);
  });
}
function fieldValue(field) {
  const el = $('field-' + field.name);
  if (!el) return null;
  if (field.type === 'json') return parseJsonField(el.value, field.label);
  if (field.type === 'number') return Number(el.value);
  if (field.type === 'file') return el.files[0] || null;
  if (field.type === 'profile_file' || field.type === 'testing_file') return el.value;
  return el.value;
}
function optionLabel(profile) {
  const name = profile.name || profile.path || String(profile);
  const path = profile.path || '';
  const size = profile.size ? ' (' + profile.size + ' bytes)' : '';
  if (!path) return name + size;
  const parts = path.split('/');
  const dir = parts.length > 1 ? parts[parts.length - 2] : '';
  return (dir ? dir + '/' : '') + name + size;
}
function directoryOptions(selected, directories) {
  const options = [{ name: '', label: 'All Testing directories' }].concat(directories || []);
  return options.map((dir) => {
    const option = document.createElement('option');
    option.value = dir.name || '';
    option.textContent = dir.label || dir.name || 'All Testing directories';
    if ((dir.name || '') === (selected || '')) option.selected = true;
    return option;
  });
}
function fillProfileSelect(select, profiles, placeholder, selectedValue, autoSelect) {
  const previous = selectedValue !== undefined ? selectedValue : select.value;
  select.textContent = '';
  const blank = document.createElement('option');
  blank.value = '';
  blank.textContent = placeholder || (profiles.length ? 'Select filename...' : 'No profiles found');
  select.appendChild(blank);
  let firstValue = '';
  profiles.forEach((profile) => {
    const option = document.createElement('option');
    option.value = profile.path || profile.name || profile;
    option.textContent = optionLabel(profile);
    option.dataset.filename = profile.name || '';
    if (!firstValue) firstValue = option.value;
    if (previous && previous === option.value) option.selected = true;
    select.appendChild(option);
  });
  if (!select.value && autoSelect && firstValue) {
    select.value = preferredProfileValue(profiles, select.dataset.prefer) || firstValue;
  }
}
function preferredProfileValue(profiles, prefer) {
  if (prefer !== 'rgb_transform') return '';
  const preferred = [
    /(^|[\\/])sRGB_v4_ICC_preference\.icc$/i,
    /(^|[\\/])Display[\\/]sRGB_D65_MAT\.icc$/i,
    /(^|[\\/])Display[\\/]LCDDisplay\.icc$/i,
  ];
  for (const pattern of preferred) {
    const match = profiles.find((profile) => {
      const value = profile.path || profile.name || String(profile);
      return pattern.test(value);
    });
    if (match) return match.path || match.name || String(match);
  }
  const displayRgb = profiles.find((profile) => {
    const value = profile.path || profile.name || String(profile);
    return /(^|[\\/])Display[\\/]/i.test(value) && /rgb/i.test(value);
  });
  return displayRgb ? (displayRgb.path || displayRgb.name || String(displayRgb)) : '';
}
async function profilesForDirectory(directory) {
  const key = directory || '__all__';
  if (profileCache[key]) return profileCache[key];
  const suffix = directory ? '?directory=' + encodeURIComponent(directory) : '';
  const data = await fetchJson('/api/profiles' + suffix);
  profileCache[key] = data.profiles || [];
  if (data.directories) profileDirectories = data.directories;
  return profileCache[key];
}
async function filesForDirectory(kind, directory) {
  const cacheKey = kind + ':' + (directory || '__all__');
  if (fileCache[cacheKey]) return fileCache[cacheKey];
  const params = new URLSearchParams();
  params.set('kind', kind);
  if (directory) params.set('directory', directory);
  const data = await fetchJson('/api/files?' + params.toString());
  fileCache[cacheKey] = data.files || [];
  if (data.directories) fileDirectories[kind] = data.directories;
  return fileCache[cacheKey];
}
async function populateProfileField(fieldName, directory) {
  const select = $('field-' + fieldName);
  if (!select) return;
  fillProfileSelect(select, [], 'Loading filenames...');
  try {
    const profiles = await profilesForDirectory(directory);
    fillProfileSelect(select, profiles, undefined, select.value, select.required);
  } catch (err) {
    fillProfileSelect(select, [], 'Failed to load filenames');
  }
}
async function populateTestingFile(fieldName, kind, directory, directorySelectId) {
  const select = $('field-' + fieldName);
  if (!select) return;
  fillProfileSelect(select, [], 'Loading filenames...');
  try {
    const files = await filesForDirectory(kind, directory);
    if (directorySelectId) {
      refreshDirectorySelect(directorySelectId, fileDirectories[kind] || [], directory || '');
    }
    fillProfileSelect(select, files, undefined, select.value, select.required);
  } catch (err) {
    fillProfileSelect(select, [], 'Failed to load filenames');
  }
}
function renderDirectorySelect(id, labelText, directories) {
  const wrap = document.createElement('div');
  wrap.className = 'field';
  const label = document.createElement('label');
  label.htmlFor = id;
  label.textContent = labelText;
  wrap.appendChild(label);
  const input = document.createElement('select');
  input.id = id;
  directoryOptions('', directories || profileDirectories).forEach((option) => input.appendChild(option));
  wrap.appendChild(input);
  return [wrap, input];
}
function refreshDirectorySelect(id, directories, selected) {
  const select = $(id);
  if (!select) return;
  const keep = selected !== undefined ? selected : select.value;
  select.textContent = '';
  directoryOptions(keep, directories).forEach((option) => select.appendChild(option));
  select.value = keep;
}
function renderProfilePicker(field) {
  const wrap = document.createElement('div');
  wrap.className = 'profile-picker';
  const dirId = 'field-' + field.name + '-directory';
  const fileId = 'field-' + field.name;
  const [dirWrap, dirSelect] = renderDirectorySelect(dirId, field.label + ' directory', profileDirectories);
  const fileWrap = document.createElement('div');
  fileWrap.className = 'field';
  const fileLabel = document.createElement('label');
  fileLabel.htmlFor = fileId;
  fileLabel.textContent = field.label + ' filename' + (field.required ? ' *' : '');
  fileWrap.appendChild(fileLabel);
  const fileSelect = document.createElement('select');
  fileSelect.id = fileId;
  fileSelect.required = !!field.required;
  fileSelect.dataset.prefer = field.prefer || '';
  fillProfileSelect(fileSelect, profileOptions, undefined, undefined, !!field.required);
  fileWrap.appendChild(fileSelect);
  const hint = document.createElement('p');
  hint.className = 'hint';
  hint.textContent = field.prefer === 'rgb_transform'
    ? 'Default 3-channel transform input requires compatible RGB display profiles; avoid Overprint, high-channel, named-color, and calculator test profiles unless you also provide matching pixels.'
    : 'Choose a Testing/ directory to narrow the filename list, or leave it on all directories.';
  wrap.appendChild(dirWrap);
  wrap.appendChild(fileWrap);
  wrap.appendChild(hint);
  dirSelect.addEventListener('change', () => populateProfileField(field.name, dirSelect.value));
  return wrap;
}
function renderTestingFileSelect(field) {
  const wrap = document.createElement('div');
  wrap.className = 'profile-picker';
  const dirId = field.directoryField ? 'field-' + field.directoryField : 'field-' + field.name + '-directory';
  const dirSelect = field.directoryField ? $(dirId) : null;
  let localDirSelect = dirSelect;
  if (!localDirSelect) {
    const rendered = renderDirectorySelect(dirId, field.label + ' directory', fileDirectories[field.kind] || []);
    localDirSelect = rendered[1];
    wrap.appendChild(rendered[0]);
  }
  const fileWrap = document.createElement('div');
  fileWrap.className = 'field';
  const fileLabel = document.createElement('label');
  fileLabel.htmlFor = 'field-' + field.name;
  fileLabel.textContent = field.label + (field.required ? ' *' : '');
  fileWrap.appendChild(fileLabel);
  const fileSelect = document.createElement('select');
  fileSelect.id = 'field-' + field.name;
  fileSelect.required = !!field.required;
  fillProfileSelect(fileSelect, [], 'Filenames loading...', undefined, !!field.required);
  fileWrap.appendChild(fileSelect);
  wrap.appendChild(fileWrap);
  const hint = document.createElement('p');
  hint.className = 'hint';
  hint.textContent = 'Directory and filename lists come from the iccDEV Testing/ tree.';
  wrap.appendChild(hint);
  if (localDirSelect) {
    localDirSelect.addEventListener('change', () => populateTestingFile(
      field.name,
      field.kind || 'profile',
      localDirSelect.value,
      localDirSelect.id
    ));
  }
  return wrap;
}
function renderField(field) {
  const wrap = document.createElement('div');
  wrap.className = 'field' + (field.type === 'json' || field.type === 'file' ? ' full' : '');
  const label = document.createElement('label');
  label.htmlFor = 'field-' + field.name;
  label.textContent = field.label + (field.required ? ' *' : '');
  wrap.appendChild(label);
  let input;
  if (field.type === 'profile') {
    return renderProfilePicker(field);
  }
  if (field.type === 'profile_directory') {
    const rendered = renderDirectorySelect('field-' + field.name, field.label, profileDirectories);
    return rendered[0];
  }
  if (field.type === 'testing_file') return renderTestingFileSelect(field);
  if (field.type === 'json') {
    input = document.createElement('textarea');
    input.value = field.value || '';
  } else if (field.type === 'select') {
    input = document.createElement('select');
    (field.options || []).forEach((value) => {
      const option = document.createElement('option');
      option.value = value;
      option.textContent = value;
      if (value === field.value) option.selected = true;
      input.appendChild(option);
    });
  } else if (field.type === 'file') {
    input = document.createElement('input');
    input.type = 'file';
    wrap.classList.add('drop');
  } else {
    input = document.createElement('input');
    input.type = field.type || 'text';
    input.value = field.value || '';
  }
  input.id = 'field-' + field.name;
  input.required = !!field.required;
  wrap.appendChild(input);
  return wrap;
}
function renderForm() {
  const tool = TOOLS[currentTool];
  $('toolTitle').textContent = tool.label;
  $('toolDescription').textContent = tool.desc;
  $('selectedEndpoint').textContent = tool.method + ' ' + tool.path;
  const form = $('toolForm');
  form.textContent = '';
  form.className = 'form-grid';
  if (!tool.fields.length) {
    const hint = document.createElement('p');
    hint.className = 'hint';
    hint.textContent = 'This endpoint does not require input.';
    form.appendChild(hint);
    return;
  }
  tool.fields.forEach((field) => form.appendChild(renderField(field)));
  tool.fields.forEach((field) => {
    if (field.type === 'profile') {
      const dirSelect = $('field-' + field.name + '-directory');
      populateProfileField(field.name, dirSelect ? dirSelect.value : '');
    }
    if (field.type === 'testing_file') {
      const dirSelect = field.directoryField ? $('field-' + field.directoryField) : $('field-' + field.name + '-directory');
      populateTestingFile(
        field.name,
        field.kind || 'profile',
        dirSelect ? dirSelect.value : '',
        dirSelect ? dirSelect.id : ''
      );
    }
  });
}
function selectTool(name) {
  currentTool = name;
  renderNav();
  renderForm();
  output('Ready.');
  setText('runStatus', 'Idle', 'pill');
}
function buildRequest(tool) {
  if (tool.method === 'UPLOAD') {
    const file = fieldValue(tool.fields[0]);
    if (!file) throw new Error('Choose a file to upload.');
    const form = new FormData();
    form.append('file', file);
    return [tool.path, { method: 'POST', body: form }];
  }
  if (tool.method === 'GET') {
    const params = new URLSearchParams();
    tool.fields.forEach((field) => {
      const value = fieldValue(field);
      if (value !== null && value !== '') params.set(field.name, String(value));
    });
    const suffix = params.toString();
    return [tool.path + (suffix ? '?' + suffix : ''), { method: 'GET' }];
  }
  const body = {};
  tool.fields.forEach((field) => { body[field.name] = fieldValue(field); });
  return [tool.path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }];
}
async function runCurrentTool() {
  const tool = TOOLS[currentTool];
  $('runBtn').disabled = true;
  setText('runStatus', 'Running ' + tool.label + '...', 'pill warn');
  try {
    const [url, opts] = buildRequest(tool);
    const data = await fetchJson(url, opts);
    output(data);
    setText('runStatus', 'OK', 'pill ok');
  } catch (err) {
    output('ERROR: ' + err.message);
    setText('runStatus', 'Error', 'pill bad');
  } finally {
    $('runBtn').disabled = false;
  }
}
async function refreshMetadata() {
  try {
    const health = await fetchJson('/api/health');
    const profiles = await fetchJson('/api/profiles');
    profileOptions = profiles.profiles || [];
    profileDirectories = profiles.directories || [];
    profileCache.__all__ = profileOptions;
    setText('serverStatus', health.ok ? 'OK' : 'Error', health.ok ? 'value ok' : 'value bad');
    setText('pythonStatus', health.python_api_available ? 'available' : 'missing', health.python_api_available ? 'value ok' : 'value bad');
    setText('cliStatus', String((health.cli_tools && health.cli_tools.available || []).length) + ' available', 'value');
    setText('profileStatus', String(profileOptions.length) + ' profiles', 'value');
    renderForm();
  } catch (err) {
    setText('serverStatus', err.message, 'value bad');
  }
}
document.addEventListener('DOMContentLoaded', () => {
  renderNav();
  renderForm();
  $('runBtn').addEventListener('click', runCurrentTool);
  $('refreshBtn').addEventListener('click', refreshMetadata);
  $('copyBtn').addEventListener('click', () => navigator.clipboard && navigator.clipboard.writeText($('output').textContent));
  $('clearBtn').addEventListener('click', () => output('Ready.'));
  refreshMetadata();
});
  </script>
</body>
</html>
"""


def _json(data: Any, status: int = 200) -> JSONResponse:
    """Return a JSON response with consistent headers."""
    return JSONResponse(data, status_code=status, headers=_JSON_HEADERS)


def _error(msg: str, status: int = 400) -> JSONResponse:
    """Return an error JSON response."""
    return _json({"error": msg}, status=status)


async def _json_body(request: Request) -> dict[str, Any] | JSONResponse:
    """Read and validate a bounded JSON object request body."""
    content_length = request.headers.get("content-length")
    if content_length:
        try:
            if int(content_length) > MAX_JSON_BYTES:
                return _error(
                    f"JSON body too large (max {MAX_JSON_BYTES // 1024 // 1024} MB)",
                    413,
                )
        except ValueError:
            return _error("Invalid Content-Length header", 400)

    body = await request.body()
    if len(body) > MAX_JSON_BYTES:
        return _error(
            f"JSON body too large (max {MAX_JSON_BYTES // 1024 // 1024} MB)",
            413,
        )
    try:
        parsed = json.loads(body.decode("utf-8"))
    except UnicodeDecodeError:
        return _error("Invalid JSON body: expected UTF-8", 400)
    except json.JSONDecodeError as e:
        return _error(f"Invalid JSON body: {e.msg}", 400)
    if not isinstance(parsed, dict):
        return _error("Invalid JSON body: expected an object", 400)
    return parsed


async def ui_index(request: Request) -> HTMLResponse:
    """GET / or /ui -- Interactive REST dashboard."""
    return HTMLResponse(_REST_UI_HTML, headers=_HTML_HEADERS)


async def favicon(request: Request) -> Response:
    """Return an empty favicon response to avoid browser 404 noise."""
    return Response(status_code=204, headers=_JSON_HEADERS)


def _resolve_input(path: str) -> Path:
    """Resolve and validate an input path.

    Raises FileNotFoundError or ValueError on invalid paths.
    """
    if not path or not path.strip():
        raise ValueError("Empty path")
    return profiles.resolve_profile_path(path)


def _resolve_inputs(paths: list) -> list[str]:
    """Resolve and validate a list of input paths."""
    resolved = []
    for p in paths:
        if not isinstance(p, str):
            raise ValueError(f"Path must be a string, got {type(p).__name__}")
        resolved.append(str(_resolve_input(p)))
    return resolved


# -- Health / Discovery endpoints ------------------------------------------

API_TOOLS = [
    {"name": "inspect_header", "method": "GET", "path": "/api/inspect-header",
     "description": "Profile header fields as JSON", "type": "native"},
    {"name": "profile_summary", "method": "GET", "path": "/api/profile-summary",
     "description": "Compact profile classification metadata", "type": "native"},
    {"name": "color_transform", "method": "POST", "path": "/api/color-transform",
     "description": "Apply color transform between profiles", "type": "native"},
    {"name": "roundtrip_delta", "method": "POST", "path": "/api/roundtrip-delta",
     "description": "Measure round-trip transform error", "type": "native"},
    {"name": "sig_to_str", "method": "GET", "path": "/api/sig-to-str",
     "description": "Decode ICC signature integer", "type": "native"},
    {"name": "enum_spaces", "method": "GET", "path": "/api/enum-spaces",
     "description": "List all ColorSpace enum values", "type": "native"},
    {"name": "list_profiles", "method": "GET", "path": "/api/profiles",
     "description": "List available ICC profiles", "type": "utility"},
    {"name": "health_check", "method": "GET", "path": "/api/health",
     "description": "Server status and tool availability", "type": "utility"},
    {"name": "dump_profile", "method": "GET", "path": "/api/dump",
     "description": "Full profile dump (iccDumpProfile)", "type": "cli"},
    {"name": "pawg_report", "method": "GET", "path": "/api/pawg-report",
     "description": "PAWG profile assessment report", "type": "cli"},
    {"name": "to_xml", "method": "GET", "path": "/api/to-xml",
     "description": "Convert ICC to XML", "type": "cli"},
    {"name": "from_xml", "method": "POST", "path": "/api/from-xml",
     "description": "Convert XML to ICC", "type": "cli"},
    {"name": "to_json", "method": "GET", "path": "/api/to-json",
     "description": "Convert ICC to JSON", "type": "cli"},
    {"name": "from_json", "method": "POST", "path": "/api/from-json",
     "description": "Convert JSON to ICC", "type": "cli"},
    {"name": "round_trip_test", "method": "GET", "path": "/api/roundtrip",
     "description": "Round-trip transform fidelity", "type": "cli"},
    {"name": "tiff_dump", "method": "GET", "path": "/api/tiff-dump",
     "description": "TIFF metadata and embedded ICC", "type": "cli"},
    {"name": "jpeg_dump", "method": "GET", "path": "/api/jpeg-dump",
     "description": "JPEG metadata and embedded ICC", "type": "cli"},
    {"name": "png_dump", "method": "GET", "path": "/api/png-dump",
     "description": "PNG metadata and embedded ICC", "type": "cli"},
    {"name": "from_cube", "method": "POST", "path": "/api/from-cube",
     "description": ".cube LUT to ICC profile", "type": "cli"},
    {"name": "apply_profiles", "method": "POST", "path": "/api/apply-profiles",
     "description": "Multi-profile TIFF transform", "type": "cli"},
    {"name": "apply_named_cmm", "method": "POST", "path": "/api/apply-named-cmm",
     "description": "Named CMM color transform", "type": "cli"},
    {"name": "create_link", "method": "POST", "path": "/api/create-link",
     "description": "Create device link profile", "type": "cli"},
    {"name": "v5_to_v4", "method": "POST", "path": "/api/v5-to-v4",
     "description": "v5 DspObs to v4 conversion", "type": "cli"},
    {"name": "spec_sep", "method": "POST", "path": "/api/spec-sep",
     "description": "Spectral separation", "type": "cli"},
    {"name": "apply_search", "method": "POST", "path": "/api/apply-search",
     "description": "Search-based color transform", "type": "cli"},
]

REST_UTILITY_ROUTES = [
    {"name": "tools_inventory", "method": "GET", "path": "/api/tools",
     "description": "REST API inventory and route metadata", "type": "rest"},
    {"name": "list_files", "method": "GET", "path": "/api/files",
     "description": "List Testing/ files by kind for dashboard selectors", "type": "rest"},
    {"name": "upload_file", "method": "POST", "path": "/api/upload",
     "description": "Upload an input file for follow-up REST tool calls", "type": "rest"},
]


async def api_health(request: Request) -> JSONResponse:
    """GET /api/health -- Server status and tool availability."""
    tools_info = cli_tools.discover_tools()

    try:
        import iccdev  # noqa: F401
        python_api = True
    except ImportError:
        python_api = False

    return _json({
        "ok": True,
        "server": "iccdev-mcp",
        "version": "0.1.0",
        "python_api_available": python_api,
        "cli_tools": {
            "available": tools_info["available"],
            "missing": tools_info["missing"],
        },
        "tools_count": len(API_TOOLS),
        "rest_utility_routes_count": len(REST_UTILITY_ROUTES),
    })


async def api_tools(request: Request) -> JSONResponse:
    """GET /api/tools -- List all tools with descriptions."""
    return _json({
        "tools": API_TOOLS,
        "count": len(API_TOOLS),
        "rest_utility_routes": REST_UTILITY_ROUTES,
        "rest_utility_routes_count": len(REST_UTILITY_ROUTES),
        "total_rest_routes": len(API_TOOLS) + len(REST_UTILITY_ROUTES),
    })


# -- Python-native tool endpoints -----------------------------------------

async def api_inspect_header(request: Request) -> JSONResponse:
    """GET /api/inspect-header?path= -- Profile header as JSON."""
    path = request.query_params.get("path", "")
    if not path:
        return _error("Missing required parameter: path")

    try:
        resolved = profiles.resolve_profile_path(path)
        from iccdev_mcp.server import inspect_header
        result = inspect_header(str(resolved))
        return _json(result)
    except ImportError:
        return _error("iccdev Python package not installed", 503)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_profile_summary(request: Request) -> JSONResponse:
    """GET /api/profile-summary?path= -- Compact profile metadata."""
    path = request.query_params.get("path", "")
    if not path:
        return _error("Missing required parameter: path")

    try:
        resolved = profiles.resolve_profile_path(path)
        from iccdev_mcp.server import profile_summary
        result = profile_summary(str(resolved))
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ImportError as e:
        return _error(f"iccdev Python API not available: {e}", 503)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_color_transform(request: Request) -> JSONResponse:
    """POST /api/color-transform -- Apply color transform."""
    body = await _json_body(request)
    if isinstance(body, JSONResponse):
        return body

    src = body.get("src_profile", "")
    dst = body.get("dst_profile", "")
    pixels = body.get("pixels", [])
    intent = body.get("rendering_intent", "perceptual")
    interp = body.get("interpolation", "tetrahedral")

    if not src or not dst:
        return _error("Missing required fields: src_profile, dst_profile")
    if not isinstance(pixels, list):
        return _error("pixels must be a list of lists")
    if not pixels:
        return _error("Missing required field: pixels")

    try:
        from iccdev_mcp.server import color_transform
        result = color_transform(src, dst, pixels, intent, interp)
        return _json(result)
    except ImportError:
        return _error("iccdev Python package not installed", 503)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_roundtrip_delta(request: Request) -> JSONResponse:
    """POST /api/roundtrip-delta -- Measure round-trip transform error."""
    body = await _json_body(request)
    if isinstance(body, JSONResponse):
        return body

    profile_path = body.get("profile", "")
    pixels = body.get("pixels", [])
    intent = body.get("rendering_intent", "perceptual")

    if not profile_path:
        return _error("Missing required field: profile")
    if not isinstance(pixels, list):
        return _error("pixels must be a list of lists")
    if not pixels:
        return _error("Missing required field: pixels")

    try:
        from iccdev_mcp.server import roundtrip_delta
        result = roundtrip_delta(profile_path, pixels, intent)
        return _json(result)
    except ImportError:
        return _error("iccdev Python package not installed", 503)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_sig_to_str(request: Request) -> JSONResponse:
    """GET /api/sig-to-str?sig= -- Decode ICC signature integer."""
    sig_str = request.query_params.get("sig", "")
    if not sig_str:
        return _error("Missing required parameter: sig")

    try:
        sig = int(sig_str)
    except ValueError:
        return _error("Parameter 'sig' must be an integer")

    try:
        from iccdev_mcp.server import icc_sig_to_str
        result = icc_sig_to_str(sig)
        return _json(result)
    except ImportError:
        return _error("iccdev Python package not installed", 503)
    except Exception as e:
        return _error(str(e), 500)


async def api_enum_spaces(request: Request) -> JSONResponse:
    """GET /api/enum-spaces -- List all ColorSpace enum values."""
    try:
        from iccdev_mcp.server import enum_spaces
        result = enum_spaces()
        return _json(result)
    except ImportError:
        return _error("iccdev Python package not installed", 503)
    except Exception as e:
        return _error(str(e), 500)


# -- File management endpoints --------------------------------------------

async def api_list_profiles(request: Request) -> JSONResponse:
    """GET /api/profiles?directory=&filename= -- List available profiles."""
    directory = request.query_params.get("directory")
    filename = request.query_params.get("filename")
    try:
        result = profiles.list_profiles(directory)
        if filename:
            result = [
                item for item in result
                if item["name"] == filename or item["path"] == filename
            ]
        return _json({
            "profiles": result,
            "directories": profiles.list_profile_directories(),
            "directory": directory or "",
            "filename": filename or "",
            "count": len(result),
        })
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except Exception as e:
        return _error(str(e), 500)


async def api_list_files(request: Request) -> JSONResponse:
    """GET /api/files?kind=&directory= -- List Testing/ files by kind."""
    kind = request.query_params.get("kind", "profile")
    directory = request.query_params.get("directory")
    extensions = _FILE_KINDS.get(kind)
    if not extensions:
        return _error(f"Unsupported file kind: {kind}")
    try:
        result = profiles.list_files(directory, extensions)
        return _json({
            "files": result,
            "directories": profiles.list_file_directories(extensions),
            "kind": kind,
            "directory": directory or "",
            "count": len(result),
        })
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except Exception as e:
        return _error(str(e), 500)


async def api_upload(request: Request) -> JSONResponse:
    """POST /api/upload -- Upload an ICC/TIFF/JPEG/PNG/XML/JSON file."""
    content_length = request.headers.get("content-length")
    if content_length:
        try:
            if int(content_length) > MAX_MULTIPART_BYTES:
                return _error(
                    f"File too large (max {MAX_UPLOAD_BYTES // 1024 // 1024} MB)",
                    413,
                )
        except ValueError:
            return _error("Invalid Content-Length header", 400)

    content_type = request.headers.get("content-type", "")
    if "multipart/form-data" not in content_type:
        return _error("Content-Type must be multipart/form-data")

    try:
        form = await request.form(
            max_files=1,
            max_fields=1,
            max_part_size=MAX_UPLOAD_BYTES + 1,
        )
    except MultiPartException as e:
        if "maximum size" in str(e):
            return _error(
                f"File too large (max {MAX_UPLOAD_BYTES // 1024 // 1024} MB)",
                413,
            )
        return _error("Invalid multipart upload", 400)
    upload = form.get("file")
    if upload is None or not isinstance(upload, UploadFile):
        return _error("Missing file in upload")

    # Read one byte past the limit so oversized uploads fail before storing.
    content = await upload.read(MAX_UPLOAD_BYTES + 1)
    if len(content) > MAX_UPLOAD_BYTES:
        return _error(
            f"File too large (max {MAX_UPLOAD_BYTES // 1024 // 1024} MB)",
            413,
        )

    # Sanitize filename: use UUID prefix to prevent collisions and path games
    filename = getattr(upload, "filename", "upload.icc") or "upload.icc"
    safe_name = Path(filename.replace("\\", "/")).name
    safe_name = re.sub(r"[^A-Za-z0-9._-]", "_", safe_name)
    if not safe_name or safe_name in {".", ".."} or safe_name.startswith("."):
        safe_name = "upload.icc"
    safe_name = f"{uuid.uuid4().hex[:8]}_{safe_name}"

    # Save to temp directory
    UPLOAD_DIR.mkdir(parents=True, exist_ok=True)
    dest = UPLOAD_DIR / safe_name
    dest.write_bytes(content)

    return _json({
        "uploaded": True,
        "path": str(dest),
        "size": len(content),
        "filename": safe_name,
    })


# -- Subprocess tool endpoints --------------------------------------------

async def api_dump_profile(request: Request) -> JSONResponse:
    """GET /api/dump?path=&validate=&verbosity=&tag= -- Profile dump."""
    path = request.query_params.get("path", "")
    if not path:
        return _error("Missing required parameter: path")
    try:
        resolved = profiles.resolve_profile_path(path)
        validate = request.query_params.get("validate", "false").lower() == "true"
        verbosity = int(request.query_params.get("verbosity", "100"))
        tag = request.query_params.get("tag", "ALL")
        result = cli_tools.run_dump_profile(
            str(resolved),
            validate=validate,
            verbosity=verbosity,
            tag=tag,
        )
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_pawg_report(request: Request) -> JSONResponse:
    """GET /api/pawg-report?path= -- PAWG profile assessment report."""
    path = request.query_params.get("path", "")
    if not path:
        return _error("Missing required parameter: path")
    try:
        resolved = profiles.resolve_profile_path(path)
        result = cli_tools.run_pawg_report(str(resolved))
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_to_xml(request: Request) -> JSONResponse:
    """GET /api/to-xml?path= -- Convert ICC to XML."""
    path = request.query_params.get("path", "")
    if not path:
        return _error("Missing required parameter: path")
    try:
        resolved = profiles.resolve_profile_path(path)
        result = cli_tools.run_to_xml(str(resolved))
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except Exception as e:
        return _error(str(e), 500)


async def api_from_xml(request: Request) -> JSONResponse:
    """POST /api/from-xml -- Convert XML to ICC (upload or path)."""
    body = await _json_body(request)
    if isinstance(body, JSONResponse):
        return body

    xml_path = body.get("xml_path", "")
    if not xml_path:
        return _error("Missing required field: xml_path")

    try:
        resolved = _resolve_input(xml_path)
        result = cli_tools.run_from_xml(str(resolved))
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_to_json(request: Request) -> JSONResponse:
    """GET /api/to-json?path= -- Convert ICC to JSON."""
    path = request.query_params.get("path", "")
    if not path:
        return _error("Missing required parameter: path")
    try:
        resolved = profiles.resolve_profile_path(path)
        result = cli_tools.run_to_json(str(resolved))
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except Exception as e:
        return _error(str(e), 500)


async def api_from_json(request: Request) -> JSONResponse:
    """POST /api/from-json -- Convert JSON to ICC."""
    body = await _json_body(request)
    if isinstance(body, JSONResponse):
        return body

    json_path = body.get("json_path", "")
    if not json_path:
        return _error("Missing required field: json_path")

    try:
        resolved = _resolve_input(json_path)
        result = cli_tools.run_from_json(str(resolved))
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_tiff_dump(request: Request) -> JSONResponse:
    """GET /api/tiff-dump?path= -- TIFF metadata and embedded ICC."""
    path = request.query_params.get("path", "")
    if not path:
        return _error("Missing required parameter: path")
    try:
        resolved = profiles.resolve_profile_path(path)
        result = cli_tools.run_tiff_dump(str(resolved))
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_jpeg_dump(request: Request) -> JSONResponse:
    """GET /api/jpeg-dump?path= -- JPEG metadata and embedded ICC."""
    path = request.query_params.get("path", "")
    if not path:
        return _error("Missing required parameter: path")
    try:
        resolved = profiles.resolve_profile_path(path)
        result = cli_tools.run_jpeg_dump(str(resolved))
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_png_dump(request: Request) -> JSONResponse:
    """GET /api/png-dump?path= -- PNG metadata and embedded ICC."""
    path = request.query_params.get("path", "")
    if not path:
        return _error("Missing required parameter: path")
    try:
        resolved = profiles.resolve_profile_path(path)
        result = cli_tools.run_png_dump(str(resolved))
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_from_cube(request: Request) -> JSONResponse:
    """POST /api/from-cube -- Convert .cube LUT to ICC profile."""
    body = await _json_body(request)
    if isinstance(body, JSONResponse):
        return body

    cube_path = body.get("cube_path", "")
    if not cube_path:
        return _error("Missing required field: cube_path")

    try:
        resolved = _resolve_input(cube_path)
        result = cli_tools.run_from_cube(str(resolved))
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_apply_profiles(request: Request) -> JSONResponse:
    """POST /api/apply-profiles -- Multi-profile TIFF transform."""
    body = await _json_body(request)
    if isinstance(body, JSONResponse):
        return body

    config_args = body.get("config_args", [])
    input_tiff = body.get("input_tiff", "")
    profile_paths = body.get("profiles", [])
    intents = body.get("intents", [])
    encoding = body.get("encoding", 1)
    compress = body.get("compress", 0)
    planar = body.get("planar", 0)
    embed = body.get("embed", 1)
    interpolation = body.get("interpolation", 0)

    if config_args:
        if not isinstance(config_args, list):
            return _error("config_args must be a list of strings")
        if not all(isinstance(arg, str) for arg in config_args):
            return _error("config_args must contain only strings")
        try:
            return _json(cli_tools.run_apply_profiles_args(config_args))
        except Exception as e:
            return _error(str(e), 500)

    if not input_tiff or not profile_paths:
        return _error("Missing required fields: input_tiff, profiles")

    if not isinstance(profile_paths, list):
        return _error("profiles must be a list of path strings")
    if intents and not isinstance(intents, list):
        return _error("intents must be a list of integers")

    try:
        resolved_input = _resolve_input(input_tiff)
        resolved_profiles = _resolve_inputs(profile_paths)
        if intents:
            resolved_intents = [int(intent) for intent in intents]
            if len(resolved_intents) != len(resolved_profiles):
                return _error("intents length must match profiles length")
        else:
            resolved_intents = [1] * len(resolved_profiles)

        fd, tmp_output = tempfile.mkstemp(suffix=".tif")
        os.close(fd)
        try:
            result = cli_tools.run_apply_profiles(
                str(resolved_input), tmp_output,
                int(encoding), int(compress), int(planar), int(embed),
                int(interpolation),
                resolved_profiles,
                resolved_intents,
            )
            if result["returncode"] == 0 and Path(tmp_output).exists():
                result["output_base64"] = base64.b64encode(
                    Path(tmp_output).read_bytes()
                ).decode("ascii")
            return _json(result)
        finally:
            Path(tmp_output).unlink(missing_ok=True)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_apply_named_cmm(request: Request) -> JSONResponse:
    """POST /api/apply-named-cmm -- Named CMM color transform."""
    body = await _json_body(request)
    if isinstance(body, JSONResponse):
        return body

    config_args = body.get("config_args", [])
    if not config_args:
        return _error("Missing required field: config_args (list of strings)")

    if not isinstance(config_args, list):
        return _error("config_args must be a list of strings")

    # Validate all args are strings
    for i, arg in enumerate(config_args):
        if not isinstance(arg, str):
            return _error(f"config_args[{i}] must be a string")

    try:
        result = cli_tools.run_apply_named_cmm(config_args)
        return _json(result)
    except Exception as e:
        return _error(str(e), 500)


async def api_create_link(request: Request) -> JSONResponse:
    """POST /api/create-link -- Create device link profile."""
    body = await _json_body(request)
    if isinstance(body, JSONResponse):
        return body

    profile_paths = body.get("profiles", [])

    if not profile_paths:
        return _error("Missing required field: profiles")

    if not isinstance(profile_paths, list):
        return _error("profiles must be a list of path strings")

    try:
        resolved_profiles = _resolve_inputs(profile_paths)

        fd, tmp_output = tempfile.mkstemp(suffix=".icc")
        os.close(fd)
        try:
            result = cli_tools.run_apply_to_link(
                resolved_profiles, tmp_output
            )
            if result["returncode"] == 0 and Path(tmp_output).exists():
                result["icc_base64"] = base64.b64encode(
                    Path(tmp_output).read_bytes()
                ).decode("ascii")
            return _json(result)
        finally:
            Path(tmp_output).unlink(missing_ok=True)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_v5_to_v4(request: Request) -> JSONResponse:
    """POST /api/v5-to-v4 -- Convert v5 DspObs to v4."""
    body = await _json_body(request)
    if isinstance(body, JSONResponse):
        return body

    display = body.get("display_profile", "")
    observer = body.get("observer_profile", "")

    if not display or not observer:
        return _error(
            "Missing required fields: display_profile, observer_profile"
        )

    try:
        resolved_display = _resolve_input(display)
        resolved_observer = _resolve_input(observer)
        result = cli_tools.run_v5_to_v4(
            str(resolved_display), str(resolved_observer)
        )
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_spec_sep(request: Request) -> JSONResponse:
    """POST /api/spec-sep -- Spectral separation to TIFF."""
    body = await _json_body(request)
    if isinstance(body, JSONResponse):
        return body

    reflectance = body.get("reflectance_profile", "")
    colorant = body.get("colorant_profile", "")
    illuminants = body.get("illuminant_profiles", [])

    if not reflectance or not colorant:
        return _error(
            "Missing required fields: reflectance_profile, colorant_profile"
        )

    try:
        resolved_ref = _resolve_input(reflectance)
        resolved_col = _resolve_input(colorant)
        resolved_illum = None
        if illuminants:
            if not isinstance(illuminants, list):
                return _error("illuminant_profiles must be a list")
            resolved_illum = _resolve_inputs(illuminants)

        fd, tmp_output = tempfile.mkstemp(suffix=".tif")
        os.close(fd)
        try:
            result = cli_tools.run_spec_sep_to_tiff(
                str(resolved_ref), str(resolved_col),
                tmp_output, resolved_illum
            )
            if result["returncode"] == 0 and Path(tmp_output).exists():
                result["output_base64"] = base64.b64encode(
                    Path(tmp_output).read_bytes()
                ).decode("ascii")
            return _json(result)
        finally:
            Path(tmp_output).unlink(missing_ok=True)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


async def api_apply_search(request: Request) -> JSONResponse:
    """POST /api/apply-search -- Search-based color transform."""
    body = await _json_body(request)
    if isinstance(body, JSONResponse):
        return body

    config_args = body.get("config_args", [])
    if not config_args:
        return _error("Missing required field: config_args (list of strings)")

    if not isinstance(config_args, list):
        return _error("config_args must be a list of strings")

    for i, arg in enumerate(config_args):
        if not isinstance(arg, str):
            return _error(f"config_args[{i}] must be a string")

    try:
        result = cli_tools.run_apply_search(config_args)
        return _json(result)
    except Exception as e:
        return _error(str(e), 500)


async def api_roundtrip_test(request: Request) -> JSONResponse:
    """GET /api/roundtrip?path=&intent=&use_mpe= -- Round-trip test."""
    path = request.query_params.get("path", "")
    if not path:
        return _error("Missing required parameter: path")
    try:
        resolved = profiles.resolve_profile_path(path)
        intent = int(request.query_params.get("intent", "1"))
        use_mpe = int(request.query_params.get("use_mpe", "0"))
        result = cli_tools.run_round_trip(str(resolved), intent, use_mpe)
        return _json(result)
    except FileNotFoundError as e:
        return _error(str(e), 404)
    except ValueError as e:
        return _error(str(e), 400)
    except Exception as e:
        return _error(str(e), 500)


# -- Route table -----------------------------------------------------------

def _build_routes() -> list:
    """Build the Starlette route table."""
    return [
        # Interactive UI
        Route("/", ui_index, methods=["GET"]),
        Route("/ui", ui_index, methods=["GET"]),
        Route("/favicon.ico", favicon, methods=["GET"]),

        # Health and discovery
        Route("/api/health", api_health, methods=["GET"]),
        Route("/api/tools", api_tools, methods=["GET"]),

        # Python-native tools
        Route("/api/inspect-header", api_inspect_header, methods=["GET"]),
        Route("/api/profile-summary", api_profile_summary, methods=["GET"]),
        Route("/api/color-transform", api_color_transform, methods=["POST"]),
        Route("/api/roundtrip-delta", api_roundtrip_delta, methods=["POST"]),
        Route("/api/sig-to-str", api_sig_to_str, methods=["GET"]),
        Route("/api/enum-spaces", api_enum_spaces, methods=["GET"]),

        # File management
        Route("/api/profiles", api_list_profiles, methods=["GET"]),
        Route("/api/files", api_list_files, methods=["GET"]),
        Route("/api/upload", api_upload, methods=["POST"]),

        # Subprocess tools
        Route("/api/dump", api_dump_profile, methods=["GET"]),
        Route("/api/pawg-report", api_pawg_report, methods=["GET"]),
        Route("/api/to-xml", api_to_xml, methods=["GET"]),
        Route("/api/from-xml", api_from_xml, methods=["POST"]),
        Route("/api/to-json", api_to_json, methods=["GET"]),
        Route("/api/from-json", api_from_json, methods=["POST"]),
        Route("/api/roundtrip", api_roundtrip_test, methods=["GET"]),
        Route("/api/tiff-dump", api_tiff_dump, methods=["GET"]),
        Route("/api/jpeg-dump", api_jpeg_dump, methods=["GET"]),
        Route("/api/png-dump", api_png_dump, methods=["GET"]),
        Route("/api/from-cube", api_from_cube, methods=["POST"]),
        Route("/api/apply-profiles", api_apply_profiles, methods=["POST"]),
        Route("/api/apply-named-cmm", api_apply_named_cmm, methods=["POST"]),
        Route("/api/create-link", api_create_link, methods=["POST"]),
        Route("/api/v5-to-v4", api_v5_to_v4, methods=["POST"]),
        Route("/api/spec-sep", api_spec_sep, methods=["POST"]),
        Route("/api/apply-search", api_apply_search, methods=["POST"]),
    ]


def create_app() -> "Starlette":
    """Create the Starlette ASGI application."""
    if not HAS_STARLETTE:
        raise ImportError(
            "starlette is required for REST API mode. "
            "Install with: pip install 'iccdev-mcp[rest]'"
        )

    middleware = [
        Middleware(
            CORSMiddleware,
            allow_origin_regex=r"^https?://(localhost|127\.0\.0\.1)(:\d+)?$",
            allow_methods=["GET", "POST"],
            allow_headers=["content-type"],
        ),
    ]

    return Starlette(
        debug=False,
        routes=_build_routes(),
        middleware=middleware,
    )


def main():
    """CLI entry point for REST API server."""
    if not HAS_STARLETTE or not HAS_UVICORN:
        print(
            "REST API requires starlette and uvicorn. "
            "Install with: pip install 'iccdev-mcp[rest]'"
        )
        raise SystemExit(1)

    parser = argparse.ArgumentParser(
        prog="iccdev-mcp-rest",
        description="REST API server for iccDEV ICC profile tools",
    )
    parser.add_argument(
        "--host", default="127.0.0.1", help="Bind address (default: 127.0.0.1)"
    )
    parser.add_argument(
        "--port", type=int, default=8080, help="Port (default: 8080)"
    )
    args = parser.parse_args()

    app = create_app()
    uvicorn.run(app, host=args.host, port=args.port, log_level="info")


if __name__ == "__main__":
    main()
