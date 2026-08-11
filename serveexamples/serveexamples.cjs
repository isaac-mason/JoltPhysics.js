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
	let fn = decodeURIComponent(req.originalUrl.split('?')[0]);
	if (fn === '/' || fn === '/Examples' || fn === '/Examples/') {
		return res.redirect('/Examples/index.html');
	}
	if (fn === '/favicon.ico') {
		return res.status(404).end();
	}
	if (!path.extname(fn) && fs.existsSync(path.join(ROOT, fn + '.html'))) {
		fn += '.html';
	}
	let filePath = path.join(ROOT, fn);
	// Examples/index.html uses relative links (e.g. falling_shapes.html); if the
	// browser URL was left at / those resolve at repo root — fall back to Examples/.
	if (!fs.existsSync(filePath) && fn.endsWith('.html')) {
		const inExamples = path.join(ROOT, 'Examples', path.basename(fn));
		if (fs.existsSync(inExamples)) filePath = inExamples;
	}
	if (!fs.existsSync(filePath)) {
		return res.status(404).end();
	}
	res.sendFile(filePath);
});

app.listen(3000);
console.log('To browse the examples go to: http://localhost:3000/Examples/');
