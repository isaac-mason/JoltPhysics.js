const express = require('express');
const app = express();
const path = require('path');
const fs = require('fs');

// Serve from the repo ROOT so the examples can import both ../dist/* (the built library)
// and ./js/* (the shared harness). COOP/COEP are required for the multi-threaded examples
// (SharedArrayBuffer / crossOriginIsolated) and are harmless for the rest.
const ROOT = path.join(__dirname, '..');

app.get('/*', function (req, res) {
	res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
	res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
	res.setHeader('Cross-Origin-Resource-Policy', 'cross-origin');
	let fn = req.originalUrl.split('?')[0];
	if (fn === '/') fn = '/Examples/index.html';
	else if (!path.extname(fn) && fs.existsSync(path.join(ROOT, fn + '.html'))) fn += '.html';
	res.sendFile(path.join(ROOT, fn));
});

app.listen(3000);
console.log('To browse the examples go to: http://localhost:3000/Examples/');
