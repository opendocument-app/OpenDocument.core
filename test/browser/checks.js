// The report the check pages print, and the summary line to read at a glance.
(function () {
  "use strict";

  var style = document.createElement("style");
  style.textContent =
    "#report{font:13px/1.6 monospace;margin:16px}" +
    "#report .pass::before{content:'PASS ';color:#2a7}" +
    "#report .fail::before{content:'FAIL ';color:#c33}" +
    "#summary{font:600 13px/1.6 monospace;margin:16px 16px 0}";
  document.head.appendChild(style);

  var report = document.getElementById("report");
  var failed = 0;
  var total = 0;
  var summary = null;

  // Written on every check, not once on load: a page whose last checks wait
  // for a timer or an observer still gets counted.
  function retally() {
    if (summary === null) {
      summary = document.createElement("div");
      summary.id = "summary";
      report.parentNode.insertBefore(summary, report);
    }
    summary.textContent = total + " checks, " + failed + " failed";
    summary.style.color = failed === 0 ? "#2a7" : "#c33";
  }

  window.check = function (name, condition) {
    var line = document.createElement("div");
    line.className = condition ? "pass" : "fail";
    line.textContent = name;
    report.appendChild(line);
    total += 1;
    if (!condition) {
      failed += 1;
    }
    retally();
  };

  window.click = function (element) {
    element.dispatchEvent(new MouseEvent("click", { bubbles: true }));
    document.body.offsetHeight;
  };

  // `detail` counts the clicks, as a browser counts them.
  window.doubleClick = function (element) {
    element.dispatchEvent(new MouseEvent("click", { bubbles: true, detail: 1 }));
    element.dispatchEvent(new MouseEvent("click", { bubbles: true, detail: 2 }));
    element.dispatchEvent(
      new MouseEvent("dblclick", { bubbles: true, detail: 2 })
    );
    document.body.offsetHeight;
  };

})();
