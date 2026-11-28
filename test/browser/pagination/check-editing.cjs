// Native keyboard/composition checks against make-demo.py's core-rendered HTML.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const { run } = require("./browser.cjs");
run(async ({ page, isChromium }) => {
  await page.goto("http://127.0.0.1:8735/demo.html");
  await page.waitForFunction(() => !document.getElementById("split").disabled);
  await page.click("#edit");
  const doc = page
    .frames()
    .find((frame) => frame.url().includes("demo-document"));
  await doc.waitForFunction(() => {
    const run = Array.from(
      odr.documentView.source.querySelectorAll("x-s[data-odr-id]"),
    ).find((run) => run.textContent.length > 1000);
    return (
      run &&
      document.querySelectorAll(
        '.odr-pagination-body [data-odr-source-id="' +
          run.getAttribute("data-odr-id") +
          '"]',
      ).length > 1
    );
  });
  const initial = await doc.evaluate(() => {
    const view = odr.documentView;
    const run = Array.from(
      view.source.querySelectorAll("x-s[data-odr-id]"),
    ).find((run) => run.textContent.length > 1000);
    window.testRun = run;
    const fragment = document.querySelectorAll(
      '.odr-pagination-body [data-odr-source-id="' +
        run.getAttribute("data-odr-id") +
        '"]',
    )[1];
    const offset = view.mapping(fragment).start + 5;
    fragment.scrollIntoView({ block: "center" });
    document.body.focus();
    view.restore({
      anchor: { element: run, offset },
      focus: { element: run, offset },
    });
    return { text: run.textContent, offset };
  });
  await doc.evaluate(() => {
    const view = odr.documentView;
    const fragment = document.querySelectorAll(
      '.odr-pagination-body [data-odr-source-id="' +
        testRun.getAttribute("data-odr-id") +
        '"]',
    )[0];
    const offset = view.mapping(fragment).end;
    window.testBoundary = offset;
    view.restore({
      anchor: { element: testRun, offset, affinity: "upstream" },
      focus: { element: testRun, offset, affinity: "upstream" },
    });
  });
  await page.keyboard.press("ArrowRight");
  assert.equal(
    await doc.evaluate(() => odr.documentView.capture().focus.offset),
    await doc.evaluate(() => testBoundary),
  );
  await page.keyboard.press("Backspace");
  assert.equal(
    await doc.evaluate(() => testRun.textContent),
    initial.text.slice(0, (await doc.evaluate(() => testBoundary)) - 1) +
      initial.text.slice(await doc.evaluate(() => testBoundary)),
  );
  await page.click("#undo");
  assert.equal(await doc.evaluate(() => testRun.textContent), initial.text);
  console.log(
    "PASS native cursor navigation and Backspace across a page boundary",
  );
  await doc.evaluate(
    (offset) =>
      odr.documentView.restore({
        anchor: { element: testRun, offset },
        focus: { element: testRun, offset },
      }),
    initial.offset,
  );
  await page.keyboard.insertText(" LIVE😀 ");
  assert.equal(
    await doc.evaluate(() => testRun.textContent),
    initial.text.slice(0, initial.offset) +
      " LIVE😀 " +
      initial.text.slice(initial.offset),
  );
  assert.equal(
    await doc.evaluate(() => odr.pagination.getState().active),
    true,
  );
  await page.click("#undo");
  assert.equal(await doc.evaluate(() => testRun.textContent), initial.text);
  await page.click("#redo");
  assert.equal(
    await doc.evaluate(() => odr.documentView.capture().focus.offset),
    initial.offset + 8,
  );
  console.log("PASS native typing on a continuation and toolbar undo/redo");

  // Exercise Chromium's real composition path, including successive input events.
  if (isChromium) {
    await doc.evaluate(() => {
      window.compositionBefore = testRun.textContent;
      window.compositionOffset = odr.documentView.capture().focus.offset;
      window.compositionNode = getSelection().anchorNode.parentElement;
    });
    await doc.locator("body").focus();
    const client = await page.context().newCDPSession(page);
    await client.send("Input.imeSetComposition", {
      text: "漢",
      selectionStart: 1,
      selectionEnd: 1,
    });
    assert.equal(
      await doc.evaluate(
        () => odr.documentView.isComposing() && compositionNode.isConnected,
      ),
      true,
    );
    await client.send("Input.imeSetComposition", {
      text: "漢字",
      selectionStart: 2,
      selectionEnd: 2,
    });
    assert.equal(
      await doc.evaluate(() => testRun.textContent),
      await doc.evaluate(
        () =>
          compositionBefore.slice(0, compositionOffset) +
          "漢字" +
          compositionBefore.slice(compositionOffset),
      ),
    );
    await client.send("Input.insertText", { text: "漢字" });
    assert.equal(
      await doc.evaluate(() => odr.documentView.isComposing()),
      false,
    );
    console.log(
      "PASS native composition mirrors each update without replacing its fragment",
    );
    const replacement = await doc.evaluate(() => {
      const view = odr.documentView;
      const fragments = document.querySelectorAll(
        '.odr-pagination-body [data-odr-source-id="' +
          testRun.getAttribute("data-odr-id") +
          '"]',
      );
      const offset = view.mapping(fragments[1]).start;
      const text = testRun.textContent;
      view.restore({
        anchor: { element: testRun, offset: offset - 4 },
        focus: { element: testRun, offset: offset + 4 },
      });
      return { offset, text };
    });
    await client.send("Input.imeSetComposition", {
      text: "文",
      selectionStart: 1,
      selectionEnd: 1,
    });
    await client.send("Input.insertText", { text: "文" });
    assert.equal(
      await doc.evaluate(() => testRun.textContent),
      replacement.text.slice(0, replacement.offset - 4) +
        "文" +
        replacement.text.slice(replacement.offset + 4),
    );
    await page.click("#undo");
    assert.equal(
      await doc.evaluate(() => testRun.textContent),
      replacement.text,
    );
    console.log(
      "PASS composition replaces a cross-page selection and undoes as one gesture",
    );
  }

  await doc.evaluate(() => {
    document.body.focus();
    odr.documentView.restore({
      anchor: { element: testRun, offset: 30 },
      focus: { element: testRun, offset: 30 },
    });
  });
  await page.keyboard.press("Enter");
  await page.keyboard.insertText("New opening. ");
  await doc.evaluate(() => {
    const run = odr.documentView.source.querySelector(
      "x-p:nth-child(3) x-s[data-odr-id]",
    );
    odr.documentView.restore({
      anchor: { element: run, offset: 0 },
      focus: { element: run, offset: 3 },
    });
    if (!odr.editing.format({ bold: true })) throw new Error("format refused");
  });
  const snapshot = await doc.evaluate(() => ({
    operations: JSON.parse(odr.generateDiff()),
    paragraphs: Array.from(odr.documentView.source.querySelectorAll("x-p")).map(
      (p) => p.textContent,
    ),
    runs: Array.from(
      odr.documentView.source.querySelectorAll("x-s[data-odr-id]"),
    ).map((run) => ({
      text: run.textContent,
      bold: getComputedStyle(run).fontWeight === "700",
    })),
  }));
  assert.equal(snapshot.paragraphs.length, 4);
  assert(snapshot.operations.ops.some((op) => op.op === "splitParagraph"));
  assert(snapshot.operations.ops.some((op) => op.op === "setTextStyle"));
  if (isChromium) {
    await doc.evaluate(() => {
      const runs = odr.documentView.source.querySelectorAll("x-s[data-odr-id]");
      const run = runs[runs.length - 1],
        offset = run.textContent.length;
      document.body.focus();
      odr.documentView.restore({
        anchor: { element: run, offset },
        focus: { element: run, offset },
      });
    });
    await page.keyboard.press("Enter");
    const client = await page.context().newCDPSession(page);
    await client.send("Input.imeSetComposition", {
      text: "空",
      selectionStart: 1,
      selectionEnd: 1,
    });
    await client.send("Input.insertText", { text: "空" });
    assert.equal(
      await doc.evaluate(
        () =>
          odr.documentView.source.querySelector("x-p:last-child").textContent,
      ),
      "空",
    );
    assert.equal(
      await doc.evaluate(() => odr.documentView.capture().focus.offset),
      1,
    );
    await page.click("#undo");
    assert.equal(
      await doc.evaluate(
        () =>
          odr.documentView.source.querySelector("x-p:last-child").textContent,
      ),
      "",
    );
    await page.click("#undo");
    assert.deepEqual(
      await doc.evaluate(() =>
        Array.from(odr.documentView.source.querySelectorAll("x-p")).map(
          (p) => p.textContent,
        ),
      ),
      snapshot.paragraphs,
    );
    console.log("PASS native composition into a new empty paragraph and undo");
  }
  if (process.argv[2])
    fs.writeFileSync(process.argv[2], JSON.stringify(snapshot, null, 2));
  console.log(
    "PASS native Enter, typing and formatting produce replayable source operations",
  );
});
