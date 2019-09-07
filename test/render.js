var system = require('system');
var webpage = require('webpage');

var args = system.args;
var page = webpage.create();

page.settings.userAgent = 'Mozilla/5.0 (Linux; U; Android 4.0.3; ko-kr; LG-L160L Build/IML74K) AppleWebkit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30';
page.open(args[1], function() {
    setTimeout(function() {
        page.viewportSize = { width: 414, height: 736 };
        page.evaluate(function() { document.body.bgColor = 'white'; });

        console.log(args[2]);
        page.render(args[2]);
        phantom.exit();
    }, 200);
});
