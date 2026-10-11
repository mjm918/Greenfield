// Runs against the local build by default, or WEB_URL for deployed acceptance.
// Install playwright@1.56.1 under build/browser-tools and set NODE_PATH there.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const { chromium } = require('playwright');

const evidence = path.resolve('build/evidence');
fs.mkdirSync(evidence, { recursive: true });
const prefix = process.env.WEB_URL ? 'pages' : 'web';

async function readSave(page) {
    return page.evaluate(() => new Promise((resolve, reject) => {
        const request = indexedDB.open('/greenfield-save');
        request.onerror = () => reject(request.error);
        request.onsuccess = () => {
            const db = request.result;
            const tx = db.transaction('FILE_DATA', 'readonly');
            const read = tx.objectStore('FILE_DATA').get('/greenfield-save/country.gfv2');
            read.onsuccess = () => resolve(read.result ? Array.from(read.result.contents) : null);
            read.onerror = () => reject(read.error);
            tx.oncomplete = () => db.close();
        };
    }));
}

async function ready(page) {
    await page.waitForFunction(() => window.Module && Module.greenfieldReady, null, { timeout: 90000 });
    await page.waitForFunction(() => {
        const canvas = document.getElementById('canvas');
        const frame = document.getElementById('game-frame');
        return canvas.width === frame.clientWidth && canvas.height === frame.clientHeight;
    });
    await page.evaluate(() => {
        window.saveResults = [];
        const ccall = Module.ccall;
        Module.ccall = function (name, type, types, args) {
            const result = ccall(name, type, types, args);
            if (name === 'ViewBrowserSaveComplete') window.saveResults.push(args[1]);
            return result;
        };
    });
}

async function clickGame(page, x, y, options = {}) {
    const box = await page.locator('#canvas').boundingBox();
    const scale = Math.min(box.width / 1440, box.height / 900);
    await page.mouse.move(1, 1);
    await page.mouse.move(box.x + (box.width - 1440 * scale) / 2 + x * scale,
                          box.y + (box.height - 900 * scale) / 2 + y * scale);
    await page.mouse.down(options);
    await frames(page);
    await page.mouse.up(options);
    await frames(page);
}

async function frames(page) {
    // Software WebGL in CI can take several hundred ms per frame.
    await page.waitForTimeout(500);
}

async function key(page, name) {
    await page.keyboard.down(name);
    await frames(page);
    await page.keyboard.up(name);
    await frames(page);
}

async function save(page, expected = 1) {
    const count = await page.evaluate(() => window.saveResults.length);
    await key(page, 'F5');
    await page.waitForFunction(count => window.saveResults.length > count, count);
    assert.equal(await page.evaluate(() => window.saveResults.at(-1)), expected);
    return Buffer.from(await readSave(page));
}

(async () => {
    let server, browser, page;
    const errors = [];
    try {
        let url = process.env.WEB_URL;
        if (!url) {
            const root = path.resolve('build-web/bin');
            server = http.createServer((req, res) => {
                const route = new URL(req.url, 'http://localhost').pathname;
                const file = path.resolve(root, '.' + route.replace(/^\/Greenfield/, '').replace(/\/$/, '/index.html'));
                if (!file.startsWith(root + path.sep) || !fs.existsSync(file)) {
                    res.writeHead(404).end();
                    return;
                }
                const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.data': 'application/octet-stream' };
                res.setHeader('Content-Type', types[path.extname(file)] || 'application/octet-stream');
                fs.createReadStream(file).pipe(res);
            });
            await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
            url = `http://127.0.0.1:${server.address().port}/Greenfield/`;
        }
        browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
        const context = await browser.newContext({ viewport: { width: 1440, height: 1000 } });
        page = await context.newPage();
        page.on('pageerror', error => errors.push(error.message));
        page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
        await page.goto(url);
        await ready(page);
        console.log('Browser ready');
        assert.equal(await page.evaluate(() => Module.greenfieldStorageReady), true);
        assert.match(await page.locator('#canvas').evaluate(canvas => canvas.getContext('webgl2').getParameter(7938)), /WebGL 2/);
        await page.screenshot({ path: path.join(evidence, `${prefix}-menu.png`) });
        await clickGame(page, 720, 385); // Begin your story through the real menu.
        await key(page, 'F3');
        await key(page, 'Space'); // Pause at a nonzero tick.
        const initial = await save(page);
        console.log('Initial save completed');
        assert.equal(initial.toString('ascii', 0, 7), 'GFTRIBE');
        assert.equal(initial.readUInt32LE(8), 4);
        assert(initial.readBigUInt64LE(24) > 0n, 'simulation must advance');
        await key(page, 'Space');
        await key(page, 'F2');
        await key(page, 'Space');
        const advanced = await save(page);
        console.log('Simulation advanced and saved');
        assert(advanced.readBigUInt64LE(24) > initial.readBigUInt64LE(24));
        await page.screenshot({ path: path.join(evidence, `${prefix}-gameplay.png`) });

        await page.reload();
        await ready(page);
        console.log('Reload completed');
        assert.deepEqual(Buffer.from(await readSave(page)), advanced, 'reload must retain stored village');
        await clickGame(page, 720, 385); // Focus the game through its menu, then load paused.
        await key(page, 'F9');
        await page.screenshot({ path: path.join(evidence, `${prefix}-loaded.png`) });
        const restored = await save(page);
        assert(restored.equals(advanced), `load/save changed state: ${advanced.readBigUInt64LE(24)} -> ${restored.readBigUInt64LE(24)}`);

        // A browser-storage failure must preserve the last durable save and permit an explicit retry.
        await key(page, 'Space');
        await key(page, 'F1');
        await key(page, 'Space');
        await page.evaluate(() => {
            window.originalTransaction = IDBDatabase.prototype.transaction;
            IDBDatabase.prototype.transaction = function (stores, mode, ...rest) {
                const transaction = window.originalTransaction.call(this, stores, mode, ...rest);
                if (mode === 'readwrite') {
                    // Real quota failures arrive asynchronously on the transaction.
                    queueMicrotask(() => transaction.dispatchEvent(new Event('error', { cancelable: true })));
                    Object.defineProperty(transaction, 'error', { value: new DOMException('Test quota failure', 'QuotaExceededError') });
                    queueMicrotask(() => transaction.abort());
                }
                return transaction;
            };
        });
        assert.deepEqual(await save(page, 0), advanced);
        await page.screenshot({ path: path.join(evidence, `${prefix}-save-failure.png`) });
        await page.evaluate(() => { IDBDatabase.prototype.transaction = window.originalTransaction; });
        const recovered = await save(page);
        assert(recovered.readBigUInt64LE(24) > advanced.readBigUInt64LE(24));
        await page.setViewportSize({ width: 1024, height: 768 });
        await page.waitForFunction(() => document.getElementById('canvas').width === 1024);
        await key(page, 'h');
        await page.screenshot({ path: path.join(evidence, `${prefix}-guide-small.png`) });
        await key(page, 'h');
        await key(page, 'f');
        await page.waitForFunction(() => document.fullscreenElement !== null);
        await page.screenshot({ path: path.join(evidence, `${prefix}-fullscreen.png`) });
        await page.evaluate(() => document.exitFullscreen());

        const blocked = await browser.newContext();
        await blocked.addInitScript(() => {
            IDBFactory.prototype.open = function () { throw new DOMException('Test blocked storage', 'SecurityError'); };
        });
        const blockedPage = await blocked.newPage();
        await blockedPage.goto(url);
        await ready(blockedPage);
        assert.equal(await blockedPage.evaluate(() => Module.greenfieldStorageReady), false);
        assert.match(await blockedPage.locator('#storage-status').textContent(), /Saving unavailable/);
        await key(blockedPage, 'Escape');
        await key(blockedPage, 'F5');
        assert.deepEqual(await blockedPage.evaluate(() => window.saveResults), []);
        await blocked.close();
        assert.deepEqual(errors, [], 'browser must have no runtime errors');
        const report = { url, renderer: 'WebGL 2', initialTick: initial.readBigUInt64LE(24).toString(),
            restoredTick: advanced.readBigUInt64LE(24).toString(), recoveredTick: recovered.readBigUInt64LE(24).toString(),
            saveBytes: advanced.length, checks: ['startup', 'gameplay', 'pause', 'save', 'reload', 'canonical round trip',
                'storage failure and recovery', 'resize', 'guide', 'fullscreen', 'blocked storage'], errors };
        fs.writeFileSync(path.join(evidence, `${prefix}-smoke.json`), JSON.stringify(report, null, 2) + '\n');
        console.log(JSON.stringify(report, null, 2));
    } catch (error) {
        if (page) {
            await page.screenshot({ path: path.join(evidence, `${prefix}-failure.png`) });
            console.error(await page.locator('#status').textContent(), errors);
        }
        throw error;
    } finally {
        if (browser) await browser.close();
        if (server) await new Promise(resolve => server.close(resolve));
    }
})().catch(error => { console.error(error); process.exitCode = 1; });
