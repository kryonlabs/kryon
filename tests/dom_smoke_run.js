class Element {
    constructor(tag) {
        this.tagName = tag.toUpperCase();
        this.children = [];
        this.parentNode = null;
        this.style = {};
        this.dataset = {};
        this.attributes = {};
        this.textContent = '';
        this.id = '';
        this.tabIndex = 0;
    }
    appendChild(child) {
        child.parentNode = this;
        this.children.push(child);
        return child;
    }
    removeChild(child) {
        this.children = this.children.filter((c) => c !== child);
        child.parentNode = null;
        return child;
    }
    removeAttribute(name) {
        delete this.attributes[name];
    }
    setAttribute(name, value) {
        this.attributes[name] = String(value);
        if (name === 'id') this.id = String(value);
        if (name === 'href') this.href = String(value);
        if (name === 'content') this.content = String(value);
        if (name === 'name') this.name = String(value);
        if (name === 'rel') this.rel = String(value);
        if (name === 'alt') this.alt = String(value);
    }
    focus() {}
    addEventListener() {}
    getBoundingClientRect() {
        return {
            left: 0,
            top: 0,
            width: parseFloat(this.style.width || '0') || 0,
            height: parseFloat(this.style.height || '0') || 0
        };
    }
}

const byId = {};
function walk(node, fn) {
    if (fn(node)) return node;
    for (const child of node.children) {
        const found = walk(child, fn);
        if (found) return found;
    }
    return null;
}
const eventListeners = {};
function addEventListener(type, callback) {
    if (!eventListeners[type]) eventListeners[type] = [];
    eventListeners[type].push(callback);
}
function dispatchEvent(type) {
    for (const callback of eventListeners[type] || [])
        callback({type});
}

const document = {
    body: new Element('body'),
    head: new Element('head'),
    documentElement: new Element('html'),
    title: '',
    fonts: {add() {}},
    createElement(tag) {
        const el = new Element(tag);
        return el;
    },
    getElementById(id) {
        return byId[id] || null;
    },
    querySelector(selector) {
        if (selector === 'meta[name="description"]')
            return walk(this.head, (el) => el.tagName === 'META' && el.attributes.name === 'description');
        if (selector === 'meta[name="theme-color"]')
            return walk(this.head, (el) => el.tagName === 'META' && el.attributes.name === 'theme-color');
        if (selector === 'link[rel="canonical"]')
            return walk(this.head, (el) => el.tagName === 'LINK' && el.attributes.rel === 'canonical');
        return null;
    },
    addEventListener
};
const originalAppend = document.body.appendChild.bind(document.body);
document.body.appendChild = (child) => {
    if (child.id) byId[child.id] = child;
    return originalAppend(child);
};

globalThis.document = document;
globalThis.window = globalThis;
globalThis.devicePixelRatio = 1;
globalThis.addEventListener = addEventListener;
globalThis.dispatchEvent = dispatchEvent;
globalThis.navigator = {};
globalThis.location = { pathname: '/', hash: '' };
globalThis.history = {
    pushState(_state, _title, value) {
        applyLocation(value);
    },
    replaceState(_state, _title, value) {
        applyLocation(value);
    }
};

function applyLocation(value) {
    const text = String(value || '');
    const hashIndex = text.indexOf('#');
    if (hashIndex >= 0) {
        globalThis.location.pathname = text.slice(0, hashIndex) || '/';
        globalThis.location.hash = text.slice(hashIndex);
    } else {
        globalThis.location.pathname = text || '/';
        globalThis.location.hash = '';
    }
}

const Module = require('./dom_smoke.js');

setTimeout(() => {
    const K = globalThis.__kryDom;
    const fail = [];
    if (Module && Module.EXITSTATUS)
        fail.push('program exit status ' + Module.EXITSTATUS);
    if (!K) fail.push('missing __kryDom');
    if (K && !K.root) fail.push('missing root');
    if (K && K.root && K.root.style.cursor !== 'pointer')
        fail.push('root cursor pointer');
    const nodes = K ? K.nodes.filter((n) => n && n.style.display !== 'none') : [];
    const kinds = {};
    for (const n of nodes)
        kinds[n.dataset.kryKind] = (kinds[n.dataset.kryKind] || 0) + 1;
    if (!kinds.rect || kinds.rect < 2) fail.push('rect nodes');
    if (!kinds['rect-outline']) fail.push('outline node');
    if (!kinds.line) fail.push('line node');
    if (!kinds.circle) fail.push('circle node');
    if (!kinds.text) fail.push('text node');
    if (!kinds.texture) fail.push('texture node');
    if (!nodes.some((n) => n.textContent === 'hello dom'))
        fail.push('hello dom text');
    if (!nodes.some((n) => n.textContent === 'clicked'))
        fail.push('injected button click');
    if (!nodes.some((n) => n.textContent === 'route version'))
        fail.push('route version text');
    if (!nodes.some((n) => n.dataset.kryKind === 'texture' && n.tagName === 'IMG'))
        fail.push('texture img node');
    if (K && K.root && K.root.style.background !== 'rgba(18,20,24,1)')
        fail.push('root background');
    if (document.title !== 'DOM smoke title')
        fail.push('document title');
    const description = document.querySelector('meta[name="description"]');
    if (!description || description.attributes.content !== 'DOM smoke description')
        fail.push('description meta');
    const canonical = document.querySelector('link[rel="canonical"]');
    if (!canonical || canonical.attributes.href !== '/dom-smoke')
        fail.push('canonical link');
    const theme = document.querySelector('meta[name="theme-color"]');
    if (!theme || theme.attributes.content !== '#121418')
        fail.push('theme color meta');
    if (globalThis.location.pathname !== '/dom-smoke' ||
        globalThis.location.hash !== '#ready')
        fail.push('route replace');
    if (K && K.routeVersion !== 1)
        fail.push('route version after replace');
    applyLocation('/dom-back#old');
    dispatchEvent('popstate');
    if (K && K.routeVersion !== 2)
        fail.push('route version after popstate');
    if (!nodes.some((n) => n.tagName === 'MAIN' && n.dataset.krySemantic === 'page'))
        fail.push('semantic page main');
    if (!nodes.some((n) => n.tagName === 'H1' && n.dataset.krySemantic === 'heading'))
        fail.push('semantic h1');
    if (!nodes.some((n) => n.tagName === 'A' && n.attributes.href === '/docs'))
        fail.push('semantic link');
    if (!nodes.some((n) => n.dataset.kryKind === 'rect-gradient' &&
        String(n.style.background).startsWith('linear-gradient')))
        fail.push('gradient node');
    console.log('dom nodes:', nodes.length, JSON.stringify(kinds));
    if (fail.length) {
        console.error('DOM SMOKE FAIL:', fail.join(', '));
        process.exit(1);
    }
    console.log('dom backend smoke ok');
    process.exit(0);
}, 1500);
