// `HtmlConfig::host_message_handler`: every `odr.on*` callback as one JSON
// `{type, detail}` string. Last in the body, so it replaces the defaults.
(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});
  if (typeof odr.hostMessageHandler !== "string") {
    return;
  }

  // called on its parent: a WebKit `postMessage` throws without its handler
  var path = odr.hostMessageHandler.split(".");
  var method = path.pop();
  var target = window;
  for (var i = 0; i < path.length && target; ++i) {
    target = target[path[i]];
  }
  // a page opened outside the host keeps the callbacks it has
  if (!target || typeof target[method] !== "function") {
    return;
  }

  // a string, because an Android `addJavascriptInterface` method takes no object
  function send(message) {
    target[method](JSON.stringify(message));
  }

  function forward(type) {
    return function (detail) {
      send({ type: type, detail: detail === undefined ? null : detail });
    };
  }

  odr.onEditModeChange = forward("editModeChange");
  odr.onEditChange = forward("editChange");
  odr.onEditRefused = forward("editRefused");
  odr.onCellsStale = forward("cellsStale");
  odr.onSelectionChange = forward("selectionChange");
  odr.onAnnotationChange = forward("annotationChange");
  odr.onError = function (code, message) {
    send({ type: "error", detail: { code: code, message: message } });
  };
  odr.onZoomChange = function (zoom, followsFit) {
    send({ type: "zoomChange", detail: { zoom: zoom, followsFit: followsFit } });
  };
})();
