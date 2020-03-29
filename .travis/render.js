var system = require('system');
var webpage = require('webpage');

var args = system.args;
var page = webpage.create();

page.open(args[1], function () {
    setTimeout(function () {
        page.viewportSize = {width: 1024, height: 768};
        page.evaluate(function () {
            document.body.bgColor = 'white';
        });
        page.render(args[2]);
        phantom.exit();
    }, 200);
});
