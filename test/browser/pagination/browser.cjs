const assert = require("node:assert/strict");
const { chromium, firefox } = require(
  process.env.PLAYWRIGHT_MODULE || "playwright",
);

async function check(task, { width = 1100, chromiumOnly = false } = {}) {
  const engine =
    !chromiumOnly && process.env.BROWSER === "firefox" ? firefox : chromium;
  const browser = await engine.launch({
    headless: true,
    executablePath: process.env.BROWSER_EXECUTABLE || undefined,
  });
  try {
    const page = await browser.newPage({ viewport: { width, height: 1000 } });
    const errors = [];
    page.on("pageerror", (error) => errors.push(error.message));
    await task({ page, isChromium: engine === chromium });
    assert.deepEqual(errors, []);
  } finally {
    await browser.close();
  }
}

exports.run = (task, options) =>
  check(task, options).catch((error) => {
    console.error(error);
    process.exitCode = 1;
  });
