# -*- coding: utf-8 -*-

"""
***************************************************************************
    ToolsTest
    ---------------------
    Date                 : July 2016
    Copyright            : (C) 2016 by Nyall Dawson
    Email                : nyall dot dawson at gmail dot com
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""

__author__ = 'Nyall Dawson'
__date__ = 'July 2016'
__copyright__ = '(C) 2016, Nyall Dawson'

# This will get replaced with a git SHA1 when you do a git archive

__revision__ = '$Format:%H$'

from qgis.testing import start_app, unittest
from processing.tests.TestData import points2
from processing.tools import vector
from qgis.core import (QgsVectorLayer, QgsFeatureRequest)
from processing.core.ProcessingConfig import ProcessingConfig

import os.path
import errno
import shutil

dataFolder = os.path.join(os.path.dirname(__file__), '../../../../tests/testdata/')
tmpBaseFolder = os.path.join(os.sep, 'tmp', 'qgis_test', str(os.getpid()))


def mkDirP(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise

start_app()


class VectorTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        mkDirP(tmpBaseFolder)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(tmpBaseFolder)
        pass

    # See http://hub.qgis.org/issues/15698
    def test_ogrLayerName(self):
        tmpdir = os.path.join(tmpBaseFolder, 'ogrLayerName')
        os.mkdir(tmpdir)

        def linkTestfile(f, t):
            os.link(os.path.join(dataFolder, f), os.path.join(tmpdir, t))

        # URI from OGR provider
        linkTestfile('geom_data.csv', 'a.csv')
        name = vector.ogrLayerName(tmpdir)
        self.assertEqual(name, 'a')

        # URI from OGR provider
        linkTestfile('wkt_data.csv', 'b.csv')
        name = vector.ogrLayerName(tmpdir + '|layerid=0')
        self.assertEqual(name, 'a')
        name = vector.ogrLayerName(tmpdir + '|layerid=1')
        self.assertEqual(name, 'b')

        # URI from OGR provider
        name = vector.ogrLayerName(tmpdir + '|layerid=2')
        self.assertEqual(name, 'invalid-layerid')

        # URI from OGR provider
        name = vector.ogrLayerName(tmpdir + '|layername=f')
        self.assertEqual(name, 'f') # layername takes precedence

        # URI from OGR provider
        name = vector.ogrLayerName(tmpdir + '|layerid=0|layername=f2')
        self.assertEqual(name, 'f2') # layername takes precedence

        # URI from OGR provider
        name = vector.ogrLayerName(tmpdir + '|layername=f2|layerid=0')
        self.assertEqual(name, 'f2') # layername takes precedence

        # URI from Sqlite provider
        name = vector.ogrLayerName('dbname=\'/tmp/x.sqlite\' table="t" (geometry) sql=')
        self.assertEqual(name, 't')

        # URI from PostgreSQL provider
        name = vector.ogrLayerName('port=5493 sslmode=disable key=\'edge_id\' srid=0 type=LineString table="city_data"."edge" (geom) sql=')
        self.assertEqual(name, 'city_data.edge')

    def testFeatures(self):
        ProcessingConfig.initialize()

        test_data = points2()
        test_layer = QgsVectorLayer(test_data, 'test', 'ogr')

        # test with all features
        features = vector.features(test_layer)
        self.assertEqual(len(features), 8)
        self.assertEqual(set([f.id() for f in features]), set([0, 1, 2, 3, 4, 5, 6, 7]))

        # test with selected features
        previous_value = ProcessingConfig.getSetting(ProcessingConfig.USE_SELECTED)
        ProcessingConfig.setSettingValue(ProcessingConfig.USE_SELECTED, True)
        test_layer.selectByIds([2, 4, 6])
        features = vector.features(test_layer)
        self.assertEqual(len(features), 3)
        self.assertEqual(set([f.id() for f in features]), set([2, 4, 6]))

        # selection, but not using selected features
        ProcessingConfig.setSettingValue(ProcessingConfig.USE_SELECTED, False)
        test_layer.selectByIds([2, 4, 6])
        features = vector.features(test_layer)
        self.assertEqual(len(features), 8)
        self.assertEqual(set([f.id() for f in features]), set([0, 1, 2, 3, 4, 5, 6, 7]))

        # using selected features, but no selection
        ProcessingConfig.setSettingValue(ProcessingConfig.USE_SELECTED, True)
        test_layer.removeSelection()
        features = vector.features(test_layer)
        self.assertEqual(len(features), 8)
        self.assertEqual(set([f.id() for f in features]), set([0, 1, 2, 3, 4, 5, 6, 7]))

        # test that feature request is honored
        ProcessingConfig.setSettingValue(ProcessingConfig.USE_SELECTED, False)
        features = vector.features(test_layer, QgsFeatureRequest().setFilterFids([1, 3, 5]))
        self.assertEqual(set([f.id() for f in features]), set([1, 3, 5]))

        # test that feature request is honored when using selections
        ProcessingConfig.setSettingValue(ProcessingConfig.USE_SELECTED, True)
        test_layer.selectByIds([2, 4, 6])
        features = vector.features(test_layer, QgsFeatureRequest().setFlags(QgsFeatureRequest.NoGeometry))
        self.assertTrue(all([not f.hasGeometry() for f in features]))
        features = vector.features(test_layer, QgsFeatureRequest().setFlags(QgsFeatureRequest.NoGeometry))
        self.assertEqual(set([f.id() for f in features]), set([2, 4, 6]))

        ProcessingConfig.setSettingValue(ProcessingConfig.USE_SELECTED, previous_value)

    def testValues(self):
        ProcessingConfig.initialize()

        test_data = points2()
        test_layer = QgsVectorLayer(test_data, 'test', 'ogr')

        # field by index
        res = vector.values(test_layer, 0)
        self.assertEqual(res[0], [1, 2, 3, 4, 5, 6, 7, 8])

        # field by name
        res = vector.values(test_layer, 'id')
        self.assertEqual(res['id'], [1, 2, 3, 4, 5, 6, 7, 8])

        # two fields
        res = vector.values(test_layer, 0, 3)
        self.assertEqual(res[0], [1, 2, 3, 4, 5, 6, 7, 8])
        self.assertEqual(res[3], [2, 1, 0, 2, 1, 0, 0, 0])

        # two fields by name
        res = vector.values(test_layer, 'id', 'id_2')
        self.assertEqual(res['id'], [1, 2, 3, 4, 5, 6, 7, 8])
        self.assertEqual(res['id_2'], [2, 1, 0, 2, 1, 0, 0, 0])

        # two fields by name and index
        res = vector.values(test_layer, 'id', 3)
        self.assertEqual(res['id'], [1, 2, 3, 4, 5, 6, 7, 8])
        self.assertEqual(res[3], [2, 1, 0, 2, 1, 0, 0, 0])

        # test with selected features
        previous_value = ProcessingConfig.getSetting(ProcessingConfig.USE_SELECTED)
        ProcessingConfig.setSettingValue(ProcessingConfig.USE_SELECTED, True)
        test_layer.selectByIds([2, 4, 6])
        res = vector.values(test_layer, 0)
        self.assertEqual(set(res[0]), set([5, 7, 3]))

        ProcessingConfig.setSettingValue(ProcessingConfig.USE_SELECTED, previous_value)

    def testUniqueValues(self):
        ProcessingConfig.initialize()

        test_data = points2()
        test_layer = QgsVectorLayer(test_data, 'test', 'ogr')

        # field by index
        v = vector.uniqueValues(test_layer, 3)
        self.assertEqual(len(v), len(set(v)))
        self.assertEqual(set(v), set([2, 1, 0]))

        # field by name
        v = vector.uniqueValues(test_layer, 'id_2')
        self.assertEqual(len(v), len(set(v)))
        self.assertEqual(set(v), set([2, 1, 0]))

        # test with selected features
        previous_value = ProcessingConfig.getSetting(ProcessingConfig.USE_SELECTED)
        ProcessingConfig.setSettingValue(ProcessingConfig.USE_SELECTED, True)
        test_layer.selectByIds([2, 4, 6])
        v = vector.uniqueValues(test_layer, 'id')
        self.assertEqual(len(v), len(set(v)))
        self.assertEqual(set(v), set([5, 7, 3]))

        ProcessingConfig.setSettingValue(ProcessingConfig.USE_SELECTED, previous_value)


if __name__ == '__main__':
    unittest.main()
