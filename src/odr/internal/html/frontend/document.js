// The text editor, attached to `odr.editing`. Still the skeleton
// `editing.md` phase 3 replaces: no selection model, no mark, no undo.
(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});

  var runs = document.querySelectorAll("[data-odr-path]");
  if (runs.length === 0) {
    return;
  }

  var modified = {};

  function operations() {
    var ops = [];
    for (var path in modified) {
      if (Object.prototype.hasOwnProperty.call(modified, path)) {
        ops.push({ op: "setText", path: path, text: modified[path].innerText });
      }
    }
    return ops;
  }

  /// The markup carries the address alone, so one page serves both modes.
  function editable(on) {
    for (var i = 0; i < runs.length; ++i) {
      if (on) {
        runs[i].setAttribute("contenteditable", "true");
      } else {
        runs[i].removeAttribute("contenteditable");
      }
    }
  }

  new MutationObserver(function (mutations) {
    if (!odr.editing.isEnabled()) {
      return;
    }
    var moved = false;
    for (var i = 0; i < mutations.length; ++i) {
      if (mutations[i].type !== "characterData") {
        continue;
      }
      // The nearest owner, not the direct parent: a search `<mark>` may sit
      // between the edited text and the element carrying the path.
      var parent = mutations[i].target.parentElement;
      var owner = parent && parent.closest("[data-odr-path]");
      if (owner) {
        modified[owner.getAttribute("data-odr-path")] = owner;
        moved = true;
      }
    }
    if (moved) {
      odr.editing.changed();
    }
  }).observe(document.body, {
    childList: true,
    subtree: true,
    characterData: true,
  });

  // A run is one line, so a new line inside it has nowhere to go in the file.
  document.addEventListener("keydown", function (event) {
    if (!odr.editing.isEnabled() || event.key !== "Enter") {
      return;
    }
    var target = event.target;
    var owner = target && target.closest && target.closest("[data-odr-path]");
    if (!owner) {
      return;
    }
    event.preventDefault();
    odr.editing.refuse("newLine", { path: owner.getAttribute("data-odr-path") });
  });

  odr.editing.attach({
    enable: function () {
      editable(true);
    },
    disable: function () {
      editable(false);
    },
    operations: operations,
    committed: function () {
      modified = {};
    },
  });
})();
