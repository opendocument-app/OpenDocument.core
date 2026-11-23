const assert = require("node:assert/strict");
const { chromium, firefox } = require(
  process.env.PLAYWRIGHT_MODULE || "playwright",
);
(async () => {
  const engine = process.env.BROWSER === "firefox" ? firefox : chromium;
  const browser = await engine.launch({
    headless: true,
    executablePath: process.env.BROWSER_EXECUTABLE || undefined,
  });
  try {
    const page = await browser.newPage();
    const errors = [];
    page.on("pageerror", (error) => errors.push(error.message));
    await page.goto("http://127.0.0.1:8756/fixture.html");
    await page.waitForFunction(() => window.odr && odr.editing.isEnabled());
    const initial = await page.evaluate(
      () => document.querySelector('[data-odr-id="2"]').textContent,
    );
    async function select(start, end = start) {
      await page.evaluate(
        ([start, end]) => {
          const run = document.querySelector('[data-odr-id="1"]');
          document.body.focus();
          odr.documentView.restore({
            anchor: { element: run, offset: start },
            focus: { element: run, offset: end },
          });
        },
        [start, end],
      );
    }
    async function text() {
      const state = await page.evaluate(() => ({
        source: odr.documentView.source.querySelector("x-p").textContent,
        shown: Array.from(
          document.querySelectorAll('#projection [data-odr-source-id="1"]'),
        )
          .map((n) => n.textContent)
          .join(""),
      }));
      assert.equal(state.shown, state.source);
      return state.source;
    }
    assert.equal(await text(), initial);
    await select(8);
    await page.keyboard.type("X");
    assert.equal(await text(), initial.slice(0, 8) + "X" + initial.slice(8));
    const ops = await page.evaluate(() => JSON.parse(odr.generateDiff()).ops);
    assert.equal(ops.length, 1);
    assert.equal(ops[0].op, "setText");
    await page.evaluate(() => odr.editing.undo());
    assert.equal(await text(), initial);
    await page.evaluate(() => odr.editing.redo());
    assert.equal(await text(), initial.slice(0, 8) + "X" + initial.slice(8));
    await page.evaluate(() => odr.editing.undo());
    console.log(
      "PASS continuation typing exports source text; undo/redo restore it",
    );
    await select(14, 3);
    await page.keyboard.type("Y");
    assert.equal(await text(), initial.slice(0, 3) + "Y" + initial.slice(14));
    await page.evaluate(() => odr.editing.undo());
    assert.equal(await text(), initial);
    assert.deepEqual(
      await page.evaluate(() => {
        const s = odr.documentView.capture();
        return [s.anchor.offset, s.focus.offset];
      }),
      [14, 3],
    );
    console.log(
      "PASS backward selection across fragments and logical undo selection",
    );
    await select(10);
    await page.keyboard.press("Backspace");
    assert.equal(await text(), initial.slice(0, 8) + initial.slice(10));
    await page.evaluate(() => odr.editing.undo());
    await select(12);
    await page.keyboard.press("Enter");
    assert.equal(
      await page.evaluate(
        () => odr.documentView.source.querySelectorAll("x-p").length,
      ),
      3,
    );
    await page.keyboard.press("Backspace");
    assert.equal(await text(), initial);
    await select(3, 14);
    assert.equal(
      await page.evaluate(() => odr.editing.format({ italic: true })),
      true,
    );
    assert.equal(await text(), initial);
    await page.evaluate(() => odr.editing.undo());
    console.log(
      "PASS Unicode deletion, paragraph split/merge and cross-fragment formatting",
    );
    if (engine === chromium) {
      const client = await page.context().newCDPSession(page);
      await select(5, 12);
      await client.send("Input.imeSetComposition", {
        text: "漢",
        selectionStart: 1,
        selectionEnd: 1,
      });
      assert.equal(
        await text(),
        initial.slice(0, 5) + "漢" + initial.slice(12),
      );
      await client.send("Input.insertText", { text: "漢" });
      await page.evaluate(() => odr.editing.undo());
      assert.equal(await text(), initial);
      assert.equal(
        await page.evaluate(() => odr.documentView.isComposing()),
        false,
      );
      console.log(
        "PASS native composition replaces a split selection as one undo gesture",
      );
    }
    assert.deepEqual(errors, []);
  } finally {
    await browser.close();
  }
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
