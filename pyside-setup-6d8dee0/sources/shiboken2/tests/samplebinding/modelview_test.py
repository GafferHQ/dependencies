#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the test suite of PySide2.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

'''Test case for objects that keep references to other object without owning them (e.g. model/view relationships).'''

import unittest
from sample import ObjectModel, ObjectType, ObjectView


object_name = 'test object'

class MyObject(ObjectType):
    pass

class ListModelKeepsReference(ObjectModel):
    def __init__(self, parent=None):
        ObjectModel.__init__(self, parent)
        self.obj = MyObject()
        self.obj.setObjectName(object_name)

    def data(self):
        return self.obj

class ListModelDoesntKeepsReference(ObjectModel):
    def data(self):
        obj = MyObject()
        obj.setObjectName(object_name)
        return obj


class ModelViewTest(unittest.TestCase):

    def testListModelDoesntKeepsReference(self):
        model = ListModelDoesntKeepsReference()
        view = ObjectView(model)
        obj = view.getRawModelData()
        self.assertEqual(type(obj), MyObject)
        self.assertEqual(obj.objectName(), object_name)

    def testListModelKeepsReference(self):
        model = ListModelKeepsReference()
        view = ObjectView(model)
        obj = view.getRawModelData()
        self.assertEqual(type(obj), MyObject)
        self.assertEqual(obj.objectName(), object_name)


if __name__ == '__main__':
    unittest.main()

