var system = require('system');
var webpage = require('webpage');

var args = system.args;
var page = webpage.create();

page.open(args[1], function (status) {
  if (status !== 'success') {
    console.log('load failed');
    phantom.exit(1);
    return;
  }

  page.evaluate(function() {
    var x = document.querySelectorAll('[contenteditable="true"]');
    for (var i = 0; i < x.length; ++i) {
      x[i].innerHTML = 'replaced';
    }
  });

  setTimeout(function() {
    const diff = page.evaluate(function() { return odr.generateDiff(); });
    console.log(diff);
    phantom.exit();
  }, 100);
});
