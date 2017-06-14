
import unittest

from PySide2.QtWidgets import QApplication
from PySide2.QtGui import QPixmap

import image_rc

class TestRccImage(unittest.TestCase):
    def testImage(self):
        app = QApplication([])
        image = QPixmap(":image")
        self.assertFalse(image.isNull())

if __name__ == '__main__':
    unittest.main()
