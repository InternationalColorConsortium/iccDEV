#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const repoRoot = path.resolve(__dirname, '..', '..');
const restApiPath = path.join(repoRoot, 'iccdev-mcp', 'iccdev_mcp', 'rest_api.py');
const source = fs.readFileSync(restApiPath, 'utf8');
const htmlMatch = source.match(/_REST_UI_HTML = r"""([\s\S]*?)"""(?:\r?\n){2}/);
assert(htmlMatch, 'embedded REST UI HTML not found');
const scriptMatch = htmlMatch[1].match(/<script>([\s\S]*?)<\/script>/);
assert(scriptMatch, 'embedded REST UI script not found');

class Element {
  constructor(tagName, document) {
    this.tagName = tagName.toUpperCase();
    this.ownerDocument = document;
    this.children = [];
    this.dataset = {};
    this.eventListeners = {};
    this._id = '';
    this._textContent = '';
    this._value = '';
    this.required = false;
    this.className = '';
    this.type = '';
    this.files = [];
  }

  set id(value) {
    this._id = value;
    if (value) this.ownerDocument.elements.set(value, this);
  }

  get id() {
    return this._id;
  }

  set textContent(value) {
    this._textContent = String(value);
    this.children = [];
    if (this.tagName === 'SELECT') this._value = '';
  }

  get textContent() {
    if (this.children.length) {
      return this.children.map((child) => child.textContent).join('');
    }
    return this._textContent;
  }

  set value(value) {
    this._value = String(value);
    if (this.tagName === 'SELECT') {
      for (const child of this.children) {
        child.selected = child.value === this._value;
      }
    }
  }

  get value() {
    if (this.tagName === 'SELECT') {
      const selected = this.children.find((child) => child.selected);
      return selected ? selected.value : this._value;
    }
    return this._value;
  }

  appendChild(child) {
    this.children.push(child);
    if (this.tagName === 'SELECT' && child.selected) {
      this._value = child.value;
    }
    return child;
  }

  addEventListener(name, handler) {
    this.eventListeners[name] = handler;
  }
}

class Document {
  constructor() {
    this.elements = new Map();
  }

  createElement(tagName) {
    return new Element(tagName, this);
  }

  getElementById(id) {
    return this.elements.get(id) || null;
  }

  addEventListener() {
    // Tests drive lifecycle explicitly.
  }

  ensureElement(id, tagName = 'div') {
    const element = this.createElement(tagName);
    element.id = id;
    return element;
  }
}

const document = new Document();
for (const [id, tag] of [
  ['toolNav', 'div'],
  ['toolTitle', 'h2'],
  ['toolDescription', 'p'],
  ['selectedEndpoint', 'span'],
  ['toolForm', 'form'],
  ['runBtn', 'button'],
  ['runStatus', 'span'],
  ['output', 'pre'],
  ['serverStatus', 'span'],
  ['pythonStatus', 'span'],
  ['cliStatus', 'span'],
  ['profileStatus', 'span'],
  ['refreshBtn', 'button'],
  ['copyBtn', 'button'],
  ['clearBtn', 'button'],
]) {
  document.ensureElement(id, tag);
}

const allProfiles = [
  { name: 'sRGB_D65_MAT.icc', path: '/Testing/Display/sRGB_D65_MAT.icc', size: 24708 },
  { name: 'CMYK_Hybrid_Profile.icc', path: '/Testing/hybrid/ICC/CMYK_Hybrid_Profile.icc', size: 42128 },
];
const transformProfiles = [
  { name: '17ChanPart1.icc', path: '/Testing/Overprint/17ChanPart1.icc', size: 35124 },
  { name: 'CMYK_Hybrid_Profile.icc', path: '/Testing/hybrid/ICC/CMYK_Hybrid_Profile.icc', size: 42128 },
  { name: 'sRGB_D65_MAT.icc', path: '/Testing/Display/sRGB_D65_MAT.icc', size: 24708 },
];
const hybridProfiles = [
  { name: 'CMYK_Hybrid_Profile.icc', path: '/Testing/hybrid/ICC/CMYK_Hybrid_Profile.icc', size: 42128 },
];
const directories = [
  { name: 'Display', label: 'Display', count: 1 },
  { name: 'hybrid', label: 'hybrid', count: 19 },
  { name: 'hybrid/ICC', label: 'hybrid/ICC', count: 1 },
];

function response(data) {
  return {
    ok: true,
    status: 200,
    text: async () => JSON.stringify(data),
  };
}

async function fetchStub(url) {
  if (url === '/api/health') {
    return response({
      ok: true,
      python_api_available: true,
      cli_tools: { available: new Array(17).fill('tool'), missing: [] },
    });
  }
  if (url.startsWith('/api/profiles')) {
    const parsed = new URL(url, 'http://127.0.0.1');
    const directory = parsed.searchParams.get('directory') || '';
    const profiles = directory === 'hybrid' ? hybridProfiles : allProfiles;
    return response({
      profiles,
      directories,
      count: profiles.length,
      directory,
    });
  }
  throw new Error(`unexpected fetch URL: ${url}`);
}

const context = vm.createContext({
  console,
  document,
  fetch: fetchStub,
  FormData: class FormData {},
  navigator: { clipboard: { writeText: () => undefined } },
  URLSearchParams,
});

vm.runInContext(scriptMatch[1], context, { filename: 'rest_api_ui.js' });

async function runInUiContext(code) {
  return await vm.runInContext(`(async () => { ${code} })()`, context);
}

(async () => {
  await runInUiContext(`
    currentTool = 'profile_summary';
    profileDirectories = ${JSON.stringify(directories)};
    profileOptions = ${JSON.stringify(allProfiles)};
    profileCache.__all__ = profileOptions;
    renderForm();
    await populateProfileField('path', '');
  `);

  const profileSelect = document.getElementById('field-path');
  assert(profileSelect, 'Profile summary filename select missing');
  assert.equal(
    profileSelect.value,
    '/Testing/Display/sRGB_D65_MAT.icc',
    'required Profile summary selector should auto-select the first profile',
  );

  const request = await runInUiContext(`
    const [url, opts] = buildRequest(TOOLS.profile_summary);
    return { url, method: opts.method };
  `);
  assert.equal(request.method, 'GET');
  assert.match(
    request.url,
    /\/api\/profile-summary\?path=%2FTesting%2FDisplay%2FsRGB_D65_MAT\.icc/,
    'Profile summary request should include a path parameter',
  );

  const directorySelect = document.getElementById('field-path-directory');
  assert(directorySelect, 'Profile summary directory select missing');
  const directoryValues = directorySelect.children.map((option) => option.value);
  assert(
    directoryValues.includes('hybrid/ICC'),
    'nested Testing/hybrid/ICC directory should appear in the profile directory dropdown',
  );

  directorySelect.value = 'hybrid';
  await runInUiContext("await populateProfileField('path', 'hybrid');");
  assert.equal(
    profileSelect.value,
    '/Testing/hybrid/ICC/CMYK_Hybrid_Profile.icc',
    'hybrid directory should auto-select an available nested hybrid ICC profile',
  );

  await runInUiContext(`
    currentTool = 'color_transform';
    profileOptions = ${JSON.stringify(transformProfiles)};
    profileCache.__all__ = profileOptions;
    renderForm();
    await populateProfileField('src_profile', '');
    await populateProfileField('dst_profile', '');
  `);

  const srcSelect = document.getElementById('field-src_profile');
  const dstSelect = document.getElementById('field-dst_profile');
  assert(srcSelect, 'Color transform source selector missing');
  assert(dstSelect, 'Color transform destination selector missing');
  assert.equal(
    srcSelect.value,
    '/Testing/Display/sRGB_D65_MAT.icc',
    'Color transform should prefer RGB display profiles over high-channel profiles',
  );
  assert.equal(
    dstSelect.value,
    '/Testing/Display/sRGB_D65_MAT.icc',
    'Color transform destination should prefer RGB display profiles',
  );
  assert.match(
    document.getElementById('toolForm').textContent,
    /avoid Overprint, high-channel, named-color, and calculator test profiles/,
    'Color transform form should explain BadXform-prone profile classes',
  );

  console.log('REST dashboard UI regression tests passed');
})().catch((error) => {
  console.error(error && error.stack ? error.stack : error);
  process.exit(1);
});
