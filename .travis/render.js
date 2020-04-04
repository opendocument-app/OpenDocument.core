var system = require('system');
var webpage = require('webpage');

var args = system.args;
var page = webpage.create();

width = 1024;
height = 768;

page.viewportSize = { width: width, height: height };
page.clipRect = { top: 0, left: 0, width: width, height: height };

page.open(args[1], function () {
    page.render(args[2]);
    phantom.exit();
});
