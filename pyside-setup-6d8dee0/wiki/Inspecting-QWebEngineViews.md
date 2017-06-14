Qt 5.5 introduced the ability to use the Chromium devtools to inspect webpages.

To enable this, you must assign a port number to the environment variable `QTWEBENGINE_REMOTE_DEBUGGING`.

There are two ways you could do this: you can set it via the terminal like this:

```sh
export QTWEBENGINE_REMOTE_DEBUGGING='1234'
```

Or you can set it in Python like this:

```python
import os
os.environ['QTWEBENGINE_REMOTE_DEBUGGING'] = '1234'
```

You have to set this before instantiating a `QWebEngineView` - it will not work if you set it afterwards.

Next, open a Chromium-based browser (like Google Chrome) and navigate to http://127.0.0.1:1234 to debug your webpages!