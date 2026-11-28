// Native typing must keep the viewport at the page boundary during reflow.
const assert = require("node:assert/strict");
const { run } = require("./browser.cjs");
run(
  async ({ page }) => {
    for (const scenario of [
      "end of page",
      "start of continuation",
      "margin at viewport top",
      "budgeted reflow",
    ]) {
      await page.goto("http://127.0.0.1:8735/demo.html");
      await page.waitForFunction(
        () => !document.getElementById("split").disabled,
      );
      const doc = page
        .frames()
        .find((frame) => frame.url().includes("demo-document"));
      await doc.evaluate(() => document.fonts.ready);
      // Allow the viewport's initial font/zoom settling to finish.
      await page.waitForTimeout(600);
      await page.click("#edit");
      const before = await doc.evaluate((scenario) => {
        const view = odr.documentView;
        const run = Array.from(
          view.source.querySelectorAll("x-s[data-odr-id]"),
        ).find((run) => run.textContent.length > 1000);
        const fragment = document
          .querySelectorAll(
            '.odr-pagination-body [data-odr-source-id="' +
              run.getAttribute("data-odr-id") +
              '"]',
          )
          .item(1);
        const offset =
          view.mapping(fragment).start + (scenario === "end of page" ? -1 : 1);
        document.body.focus();
        view.restore({
          anchor: { element: run, offset },
          focus: { element: run, offset },
        });
        odr.setPage(0);
        const bottom =
          document.querySelector(".odr-pagination-page").getBoundingClientRect()
            .bottom + scrollY;
        scrollTo(
          0,
          bottom -
            (scenario === "margin at viewport top" ? 10 : innerHeight / 2),
        );
        if (scenario === "budgeted reflow") {
          const now = performance.now.bind(performance);
          let elapsed = 0;
          performance.now = () => now() + (elapsed += 10);
        }
        return scrollY;
      }, scenario);
      assert(before > 200, "the regression must start scrolled down");
      async function checkPosition(action) {
        const immediate = await doc.evaluate(() => scrollY);
        assert(
          Math.abs(immediate - before) < 2,
          `${scenario}, ${action}: immediate scroll moved from ${before} to ${immediate}`,
        );
        await doc.waitForFunction(() => {
          const node = getSelection().anchorNode;
          return (
            node &&
            !document.querySelector(".odr-pagination-pending").contains(node)
          );
        });
        const state = await doc.evaluate(() => {
          const rect = getSelection().getRangeAt(0).getBoundingClientRect();
          return {
            scroll: scrollY,
            caretTop: rect.top,
            caretBottom: rect.bottom,
            height: rect.height,
            viewport: innerHeight,
          };
        });
        assert(
          Math.abs(state.scroll - before) < 2,
          `${scenario}, ${action}: scroll moved from ${before} to ${state.scroll}`,
        );
        assert(
          state.height > 0 &&
            state.caretTop >= 0 &&
            state.caretBottom <= state.viewport,
          `${scenario}, ${action}: caret left the viewport`,
        );
      }
      for (const letter of "abcdef") {
        await page.keyboard.type(letter);
        await checkPosition("typing " + letter);
      }
      await page.waitForTimeout(150);
      await checkPosition("deferred reflow");
      await page.keyboard.press("Backspace");
      await checkPosition("Backspace");
      await page.click("#undo");
      await checkPosition("undo");
      await page.click("#redo");
      await checkPosition("redo");
      console.log("PASS stable scroll and visible caret: " + scenario);
    }
  },
  { width: 1200 },
);
