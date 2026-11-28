// Native IME must respect edit gates and finish before host/view transitions.
const assert = require("node:assert/strict");
const { run } = require("./browser.cjs");
run(
  async ({ page }) => {
    const client = await page.context().newCDPSession(page);
    for (const action of [
      "scope",
      "field",
      "undo",
      "format",
      "search",
      "print",
      "disable",
      "missing compositionend",
    ]) {
      await page.goto("http://127.0.0.1:8735/demo.html");
      const doc = page
        .frames()
        .find((frame) => frame.url().includes("demo-document"));
      await doc.waitForFunction(
        () => window.odr && odr.pagination.getState().complete,
      );
      await doc.evaluate(() => document.fonts.ready);
      await page.waitForTimeout(250);
      await page.click("#edit");
      const before = await doc.evaluate((action) => {
        const view = odr.documentView;
        let runs = view.source.querySelectorAll("x-s[data-odr-id]");
        const first = runs[0];
        let last = first;
        if (action === "scope") {
          document.body.setAttribute("data-odr-editing-scope", "paragraph");
          last = runs[1];
        } else if (action === "field") {
          const field = document.createElement("x-s");
          field.contentEditable = "false";
          field.textContent = "FIELD";
          last = document.createElement("x-s");
          last.setAttribute("data-odr-id", "-999");
          last.textContent = "tail";
          first.after(field, last);
          odr.pagination.refresh();
        }
        window.refusals = [];
        odr.onEditRefused = (event) => refusals.push(event.reason);
        document.body.focus();
        view.restore({
          anchor: { element: first, offset: 2 },
          focus: { element: last, offset: 2 },
        });
        return view.source.textContent;
      }, action);
      await client.send("Input.imeSetComposition", {
        text: "漢",
        selectionStart: 1,
        selectionEnd: 1,
      });
      if (action === "scope" || action === "field") {
        const state = await doc.evaluate(() => ({
          text: odr.documentView.source.textContent,
          composing: odr.documentView.isComposing(),
          ops: JSON.parse(odr.generateDiff()).ops,
          refusals,
        }));
        assert.equal(state.text, before);
        assert.equal(state.composing, false);
        assert.deepEqual(state.ops, []);
        assert.deepEqual(state.refusals, [
          action === "scope" ? "outOfScope" : "range",
        ]);
      } else {
        assert.equal(
          await doc.evaluate(() => odr.documentView.isComposing()),
          true,
        );
        await doc.evaluate((action) => {
          if (action === "undo") odr.editing.undo();
          else if (action === "format") odr.editing.format({ italic: true });
          else if (action === "search") {
            odr.search("walk");
            odr.resetSearch();
          } else if (action === "print") {
            dispatchEvent(new Event("beforeprint"));
            dispatchEvent(new Event("afterprint"));
          } else if (action === "disable") odr.editing.disable();
          else {
            document.body.addEventListener(
              "compositionend",
              (event) => event.stopImmediatePropagation(),
              { capture: true, once: true },
            );
            odr.editing.undo();
          }
        }, action);
        await doc.waitForFunction(
          () =>
            odr.pagination.getState().active &&
            odr.pagination.getState().complete,
        );
        assert.equal(
          await doc.evaluate(() => odr.documentView.isComposing()),
          false,
        );
        const text = await doc.evaluate(
          () => odr.documentView.source.textContent,
        );
        assert.equal(
          text,
          action === "undo" || action === "missing compositionend"
            ? before
            : before.slice(0, 2) + "漢" + before.slice(2),
        );
      }
      await doc.waitForFunction(() => odr.pagination.getState().complete);
      assert.equal(
        await doc.evaluate(() =>
          Array.from(document.querySelectorAll(".odr-pagination-body"))
            .map((p) => p.textContent)
            .join(""),
        ),
        await doc.evaluate(() => odr.documentView.source.textContent),
      );
      console.log("PASS native composition: " + action);
    }
    await page.goto("http://127.0.0.1:8735/demo.html");
    const doc = page
      .frames()
      .find((frame) => frame.url().includes("demo-document"));
    await doc.waitForFunction(
      () => window.odr && odr.pagination.getState().complete,
    );
    await page.click("#edit");
    const replacement = await doc.evaluate(() => {
      const view = odr.documentView;
      const source = Array.from(
        view.source.querySelectorAll("x-s[data-odr-id]"),
      ).find((run) => run.textContent.length > 1000);
      const before = source.textContent;
      const fragment = document.querySelector(
        '.odr-pagination-body [data-odr-source-id="' +
          source.getAttribute("data-odr-id") +
          '"]',
      );
      const start = view.mapping(fragment).start + 2;
      document.body.focus();
      view.restore({
        anchor: { element: source, offset: start },
        focus: { element: source, offset: start + 4 },
      });
      document.body.dispatchEvent(
        new InputEvent("beforeinput", {
          bubbles: true,
          cancelable: false,
          inputType: "insertReplacementText",
          data: "X",
        }),
      );
      const selection = getSelection();
      const node = selection.anchorNode;
      const offset = selection.anchorOffset;
      node.insertData(offset, "X");
      selection.setBaseAndExtent(node, offset + 1, node, offset + 1);
      document.body.dispatchEvent(
        new InputEvent("input", {
          bubbles: true,
          inputType: "insertReplacementText",
          data: "X",
        }),
      );
      const actual = source.textContent;
      odr.editing.disable();
      return {
        actual,
        expected: before.slice(0, start) + "X" + before.slice(start + 4),
      };
    });
    assert.equal(replacement.actual, replacement.expected);
    console.log(
      "PASS noncancelable replacement preserves the split run suffix",
    );
  },
  { chromiumOnly: true },
);
