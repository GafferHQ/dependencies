## Project Status

2015-12-29

Currently, we are running the tests by hand, but we want to do automated tests in the near future.

Here is the last result of the pyside2 test suite on OS X El Capitan, after issuing the following commands:
```
$ time python3 setup.py build --debug --no-examples --ignore-git --qmake=/usr/local/Cellar/qt5/5.5.1/bin/qmake \
  --build-tests --jobs=9
$ pushd pyside_build/py3.4-qt5.5.1-64bit-debug/pyside2
$ make test
```

```
86% tests passed, 55 tests failed out of 391

Total Test time (real) = 246.80 sec

The following tests FAILED:
	 22 - signals_disconnect_test (Failed)
	 28 - signals_multiple_connections_test (SEGFAULT)
	 38 - signals_ref06_test (Failed)
	 41 - signals_short_circuit_test (Failed)
	 51 - signals_signal_signature_test (Failed)
	 54 - signals_static_metaobject_test (Failed)
	 87 - QtCore_deepcopy_test (Failed)
	115 - QtCore_qfile_test (Failed)
	117 - QtCore_qinstallmsghandler_test (Failed)
	123 - QtCore_qobject_connect_notify_test (Failed)
	133 - QtCore_qobject_tr_as_instance_test (Failed)
	155 - QtCore_qurl_test (Failed)
	156 - QtCore_repr_test (Failed)
	163 - QtCore_translation_test (Failed)
	169 - QtGui_bug_480 (Failed)
	183 - QtGui_pyside_reload_test (Failed)
	191 - QtGui_qmatrix_test (Failed)
	245 - QtWidgets_bug_722 (Failed)
	258 - QtWidgets_bug_862 (Failed)
	295 - QtWidgets_qgraphicsscene_test (Failed)
	297 - QtWidgets_qinputcontext_test (Failed)
	314 - QtWidgets_qstandarditemmodel_test (Failed)
	328 - QtWidgets_returnquadruplesofnumbers_test (Failed)
	336 - QtNetwork_basic_auth_test (Failed)
	348 - QtTest_touchevent_test (SEGFAULT)
	352 - QtUiTools_bug_360 (Failed)
	353 - QtUiTools_bug_376 (Failed)
	354 - QtUiTools_bug_392 (Failed)
	355 - QtUiTools_bug_426 (Failed)
	356 - QtUiTools_bug_552 (Failed)
	357 - QtUiTools_bug_797 (Failed)
	358 - QtUiTools_bug_909 (Failed)
	359 - QtUiTools_bug_913 (Failed)
	360 - QtUiTools_bug_958 (Failed)
	361 - QtUiTools_bug_965 (Failed)
	362 - QtUiTools_bug_1060 (Failed)
	363 - QtUiTools_uiloader_test (Failed)
	364 - QtUiTools_ui_test (Failed)
	371 - QtScript_qscriptvalue_test (Failed)
	372 - QtScriptTools_debugger_test (Failed)
	373 - QtQml_bug_451 (Failed)
	374 - QtQml_bug_456 (Failed)
	375 - QtQml_bug_557 (Failed)
	376 - QtQml_bug_726 (Failed)
	377 - QtQml_bug_814 (Failed)
	378 - QtQml_bug_825 (Timeout)
	379 - QtQml_bug_847 (Failed)
	380 - QtQml_bug_915 (Failed)
	381 - QtQml_bug_926 (Failed)
	382 - QtQml_bug_951 (Failed)
	383 - QtQml_bug_995 (Failed)
	384 - QtQml_bug_997 (Failed)
	386 - QtQml_qqmlnetwork_test (Failed)
	387 - QtQml_qquickview_test (Failed)
	389 - QtQml_registertype (Failed)
```