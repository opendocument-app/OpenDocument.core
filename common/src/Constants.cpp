#include <common/Constants.h>

#define MULTILINE(...) #__VA_ARGS__

namespace odr {

const char *Constants::getVersion() {
    return "";
}

const char *Constants::getCommit() {
    return "";
}

const char *Constants::getHtmlBeginToStyle() {
    // clang-format off
    return MULTILINE(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8"/>
    <base target="_blank"/>
    <meta name="viewport" content="width=device-width,initial-scale=1.0,user-scalable=yes"/>
    <title>odr</title>
    <style>
    );
    // clang-format on
}

const char *Constants::getHtmlStyleToBody() {
    // clang-format off
    return MULTILINE(
    </style>
    <body>
    );
    // clang-format on
}

const char *Constants::getHtmlBodyToScript() {
    // clang-format off
    return MULTILINE(
    </body>
    <script>
    );
    // clang-format on
}

const char *Constants::getHtmlScriptToEnd() {
    // clang-format off
    return MULTILINE(
    </script>
    </html>
    );
    // clang-format on
}

const char *Constants::getOpenDocumentDefaultCss() {
    // clang-format off
    return MULTILINE(
* {
  margin: 0px;
  position: relative;
}
body {
  padding: 5px;
}
span {
  white-space: pre-wrap;
}
table {
  width: 0px;
}
p {
  padding: 0 !important;
}
span {
  margin: 0 !important;
}
    );
    // clang-format on
}

const char *Constants::getOpenDocumentSpreadsheetDefaultCss() {
    // clang-format off
    return MULTILINE(
table {
  border-collapse: collapse;
  display: block;
}
td {
  vertical-align: top;
}
p {
  font-family: "Arial";
  font-size: 10pt;
}
    );
    // clang-format on
}

const char *Constants::getDefaultScript() {
    // clang-format off
    return MULTILINE(
function download(filename, text) {
  var element = document.createElement('a');
  element.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(text));
  element.setAttribute('download', filename);
  element.style.display = 'none';
  document.body.appendChild(element);
  element.click();
  document.body.removeChild(element);
}

window.addEventListener('keydown', function(event) {
  if (event.ctrlKey || event.metaKey) {
    switch (String.fromCharCode(event.which).toLowerCase()) {
    case 's':
      event.preventDefault();
      download('test.json', generateDiff());
      break;
    }
  }
});

function generateDiff() {
  var result = {
    modifiedText: {}
  };
  for(let [k, v] of Object.entries(editJournal['modifiedText'])) {
    result['modifiedText'][k] = v.innerText;
  }
  return JSON.stringify(result);
}

function mutation(mutations, observer) {
  for(let m of mutations) {
    if (m.type == 'characterData') {
      const node = m.target.parentNode;
      const id = node.getAttribute('data-odr-cid');
      if (id) {
        editJournal['modifiedText'][parseInt(id)] = node;
      }
    } else {
      console.log(m);
    }
  }
};

var editJournal = {
  modifiedText: {},
};

const observer = new MutationObserver(mutation);
const config = { attributes: false, childList: true, subtree: true, characterData: true };
observer.observe(document.body, config);
    );
    // clang-format on
}

}
