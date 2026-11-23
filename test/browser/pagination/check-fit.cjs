// Source-loaded, real-layout checks: automatic fitting inside an iframe.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { chromium, firefox } = require(
  process.env.PLAYWRIGHT_MODULE || "playwright",
);
const assets = path.resolve(__dirname, "../../../src/odr/internal/html/frontend");
const read = (name) => fs.readFileSync(path.join(assets, name), "utf8");
function template(key, width) {
  return `<template data-odr-page-template="${key}" data-odr-next-layout="${key}" data-odr-master-page="${key}">
    <div class="odr-page-outer odr-pagination-page odr-page" style="width:${width}px;height:400px">
      <div class="odr-pagination-inner" style="margin:20px"><div class="odr-pagination-body"></div></div>
    </div></template>`;
}
function fixture({ enabled = true, fit = "auto", width = "", pin = "" } = {}) {
  return `<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1">
    <style>${read("document.css")}
      :root{--odr-fit:${enabled ? fit : "1"};--odr-zoom:${pin}}
      x-p{height:90px;margin:0}
    </style><body><div data-odr-pagination="${enabled}" data-odr-page-fit="${fit}" data-odr-page-fit-width="${width}" data-odr-page-animation="false">
      <div class="odr-pagination-original"><div class="odr-text-flow" id="source"><x-p>First paragraph</x-p><x-p>Second paragraph</x-p></div></div>
      ${template("root", 560)}${template("wide-first", 800)}${template("wide", 800)}
    </div>${["document-view.js", "pagination.js", "viewport.js"]
      .map((name) => `<script>${read(name)}</script>`).join("")}`;
}
(async () => {
  const engine = process.env.BROWSER === "firefox" ? firefox : chromium;
  const browser = await engine.launch({
    headless: true,
    executablePath: process.env.BROWSER_EXECUTABLE || undefined,
  });
  try {
    const page = await browser.newPage({ viewport: { width: 1000, height: 800 } });
    const errors = [];
    page.on("pageerror", (error) => errors.push(error.message));
    async function load(options) {
      await page.setContent('<iframe style="width:380px;height:700px;border:0"></iframe>');
      await page.locator("iframe").evaluate((frame, html) => {
        frame.srcdoc = html;
      }, fixture(options));
      const doc = page.frames()[1];
      await doc.waitForFunction(() => window.odr && odr.getZoom);
      return doc;
    }
    async function fitted(doc, available = 380, content = 592) {
      await doc.waitForFunction(
        ({ available, content }) =>
          odr.pagination.getState().pageCount > 0 &&
          Math.abs(odr.getZoom() - available / content) < 0.002,
        { available, content },
      );
    }
    for (const enabled of [true, false]) {
      const doc = await load({ enabled });
      if (!enabled) {
        assert.equal(await doc.evaluate(() => odr.getZoom()), 1);
        await doc.evaluate(() => odr.pagination.setOptions({ enabled: true }));
      }
      await fitted(doc);
      for (let i = 0; i < 2; ++i) {
        await doc.evaluate(() => odr.pagination.setOptions({ enabled: false }));
        assert.equal(await doc.evaluate(() => odr.getZoom()), 1);
        await doc.evaluate(() => odr.pagination.setOptions({ enabled: true }));
        await fitted(doc);
      }
      await doc.evaluate(() => {
        const wide = document.createElement("x-p");
        wide.setAttribute("data-odr-master-page", "wide");
        wide.textContent = "Wider layout";
        odr.documentView.source.querySelector(".odr-text-flow").appendChild(wide);
        odr.pagination.refresh();
      });
      await fitted(doc, 380, 832);
      await doc.evaluate(() => odr.setZoom(1.25));
      await doc.evaluate(() => odr.pagination.setOptions({ enabled: false }));
      assert.equal(await doc.evaluate(() => odr.getZoom()), 1.25);
      await doc.evaluate(() => odr.pagination.setOptions({ enabled: true }));
      await doc.waitForFunction(() => odr.pagination.getState().complete);
      assert.equal(await doc.evaluate(() => odr.getZoom()), 1.25);
      await doc.evaluate(() => odr.resetZoom());
      await fitted(doc, 380, 832);
    }
    const configured = await load({ enabled: false, width: 320 });
    await configured.evaluate(() => odr.pagination.setOptions({ enabled: true }));
    await fitted(configured, 320);
    await page.locator("iframe").evaluate((frame) => { frame.style.width = "500px"; });
    await configured.waitForFunction(() => innerWidth === 500);
    await page.waitForTimeout(150);
    await fitted(configured, 320);
    const byView = await load({ fit: "view", width: 320 });
    await fitted(byView);
    const actual = await load({ fit: "none" });
    await actual.waitForFunction(() => odr.pagination.getState().complete);
    assert.equal(await actual.evaluate(() => odr.getZoom()), 1);
    await actual.evaluate(() => {
      odr.pagination.setOptions({ enabled: false });
      odr.pagination.setOptions({ enabled: true });
    });
    await actual.waitForFunction(() => odr.pagination.getState().complete);
    assert.equal(await actual.evaluate(() => odr.resetZoom()), 1);
    const pinned = await load({ pin: 1.1 });
    await pinned.waitForFunction(() => odr.pagination.getState().complete);
    assert.equal(await pinned.evaluate(() => odr.getZoom()), 1.1);
    await pinned.evaluate(() => odr.resetZoom());
    await fitted(pinned);
    assert.deepEqual(errors, []);
    console.log("PASS pagination iframe fitting, runtime toggles, mixed widths and zoom policy");
  } finally {
    await browser.close();
  }
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
