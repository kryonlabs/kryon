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
const document = {
    body: new Element('body'),
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
    addEventListener() {}
};
const originalAppend = document.body.appendChild.bind(document.body);
document.body.appendChild = (child) => {
    if (child.id) byId[child.id] = child;
    return originalAppend(child);
};

globalThis.document = document;
globalThis.window = globalThis;
globalThis.devicePixelRatio = 1;
globalThis.addEventListener = () => {};
globalThis.navigator = {};

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
    if (!nodes.some((n) => n.dataset.kryKind === 'texture' && n.tagName === 'IMG'))
        fail.push('texture img node');
    if (K && K.root && K.root.style.background !== 'rgba(18,20,24,1)')
        fail.push('root background');
    console.log('dom nodes:', nodes.length, JSON.stringify(kinds));
    if (fail.length) {
        console.error('DOM SMOKE FAIL:', fail.join(', '));
        process.exit(1);
    }
    console.log('dom backend smoke ok');
    process.exit(0);
}, 1500);
