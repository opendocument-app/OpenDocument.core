# host bridge checks

`host-bridge.js` sends the `odr.on*` callbacks to the function that
`HtmlConfig::host_message_handler` names. The checks run by hand, like the
rest of `test/browser/`.

```bash
test/browser/bridge/serve        # serves on :8736
open http://localhost:8736/webkit.html
open http://localhost:8736/android.html
open http://localhost:8736/absent.html
```

Each page puts a stub of the host's handler on `window` and writes the
`odr.hostMessageHandler` string that `write_host_bridge_script` writes inline.
Then it loads `host-bridge.js` last, as the library does.

- `webkit.html`: every callback reaches
  `webkit.messageHandlers.reader.postMessage` as one JSON `{type, detail}`
  string. The stub throws unless it is called on its handler, as WebKit does.
- `android.html`: the same string reaches `window.reader.postMessage`.
- `absent.html`: with no handler on the page, the page keeps its own callbacks.
