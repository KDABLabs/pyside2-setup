#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the test suite of Qt for Python.
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

import os
import sys
import unittest

from pathlib import Path
sys.path.append(os.fspath(Path(__file__).resolve().parents[1]))
from shiboken_paths import init_paths
init_paths()

import sample
from shiboken_test_helper import objectFullname

from shiboken6 import Shiboken
_init_pyside_extension()   # trigger bootstrap

from shibokensupport.signature import get_signature


class TestEnumFromRemovedNamespace(unittest.TestCase):
    def testEnumPromotedToGlobal(self):
        sample.RemovedNamespace1_Enum
        self.assertEqual(sample.RemovedNamespace1_Enum_Value0, 0)
        self.assertEqual(sample.RemovedNamespace1_Enum_Value1, 1)
        sample.RemovedNamespace1_AnonymousEnum_Value0
        sample.RemovedNamespace2_Enum
        sample.RemovedNamespace2_Enum_Value0

    def testNames(self):
        # Test if invisible namespace does not appear on type name
        self.assertEqual(objectFullname(sample.RemovedNamespace1_Enum),
                         "sample.RemovedNamespace1_Enum")
        self.assertEqual(objectFullname(sample.ObjectOnInvisibleNamespace),
                         "sample.ObjectOnInvisibleNamespace")

        # Function arguments
        signature = get_signature(sample.ObjectOnInvisibleNamespace.toInt)
        self.assertEqual(objectFullname(signature.parameters['e'].annotation),
                         "sample.RemovedNamespace1_Enum")
        signature = get_signature(sample.ObjectOnInvisibleNamespace.consume)
        self.assertEqual(objectFullname(signature.parameters['other'].annotation),
                         "sample.ObjectOnInvisibleNamespace")

    def testGlobalFunctionFromRemovedNamespace(self):
        self.assertEqual(sample.mathSum(1, 2), 3)

    def testEnumPromotedToUpperNamespace(self):
        sample.UnremovedNamespace
        sample.UnremovedNamespace.RemovedNamespace3_Enum
        sample.UnremovedNamespace.RemovedNamespace3_Enum_Value0
        sample.UnremovedNamespace.RemovedNamespace3_AnonymousEnum_Value0

    def testNestedFunctionFromRemovedNamespace(self):
            self.assertEqual(sample.UnremovedNamespace.nestedMathSum(1, 2), 3)


if __name__ == '__main__':
    unittest.main()

