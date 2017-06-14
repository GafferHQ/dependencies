## `load_ui` examples

#### Base instance as argument

The `uic.loadUi` function of PyQt4 and PyQt5 as well as the `QtUiTools.QUiLoader().load` function of PySide/PySide2 are mapped to a convenience function in Qt.py called `load_ui`. This function only accepts the filename argument of the .ui file.

A popular approach is to provide a base instance argument to PyQt's `uic.loadUi`, into which all widgets are loaded:

```python
# PyQt4, PyQt5
class MainWindow(QtWidgets.QWidget):
    def __init__(self, parent=None):
        QtWidgets.QWidget.__init__(self, parent)
        uic.loadUi('uifile.ui', self)  # Loads all widgets of uifile.ui into self
```

PySide does not support this out of the box, but it can be implemented in various ways. In the example in `baseinstance1.py`, a support function `setup_ui` is defined which wraps `load_ui` and provides this second base instance argument. In `baseinstance2.py`, another approach is used where `pysideuic` is required for PySide/PySide2 and `uic` is required for PyQt4/PyQt5.
