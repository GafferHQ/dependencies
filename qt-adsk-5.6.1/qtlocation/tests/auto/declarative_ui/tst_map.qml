/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.0
import QtLocation 5.6
import QtPositioning 5.5

Item {
    width:100
    height:100
    // General-purpose elements for the test:
    Plugin { id: testPlugin; name: "qmlgeo.test.plugin"; allowExperimental: true }
    Plugin { id: testPlugin2; name: "gmlgeo.test.plugin"; allowExperimental: true }
    Plugin { id: herePlugin; name: "here";
        parameters: [
            PluginParameter {
                name: "here.app_id"
                value: "stub"
            },
            PluginParameter {
                name: "here.token"
                value: "stub"
            }
        ]
    }

    property variant coordinate1: QtPositioning.coordinate(10, 11)
    property variant coordinate2: QtPositioning.coordinate(12, 13)
    property variant coordinate3: QtPositioning.coordinate(50, 50, 0)
    property variant coordinate4: QtPositioning.coordinate(80, 80, 0)
    property variant coordinate5: QtPositioning.coordinate(20, 180)
    property variant invalidCoordinate: QtPositioning.coordinate()
    property variant altitudelessCoordinate: QtPositioning.coordinate(50, 50)

    Map { id: mapZoomOnCompleted; width: 200; height: 200;
        zoomLevel: 3; center: coordinate1; plugin: testPlugin;
        Component.onCompleted: { zoomLevel = 7 } }
    Map { id: mapZoomDefault; width: 200; height: 200;
        center: coordinate1; plugin: testPlugin; }
    Map { id: mapZoomUserInit; width: 200; height: 200;
        zoomLevel: 4; center: coordinate1; plugin: testPlugin; }
    Map {id: map; plugin: testPlugin; center: coordinate1; width: 100; height: 100}
    Map {id: coordinateMap; plugin: herePlugin; center: coordinate3; width: 1000; height: 1000; zoomLevel: 15}

    SignalSpy {id: mapCenterSpy; target: map; signalName: 'centerChanged'}
    SignalSpy {id: mapZoomSpy; target: mapZoomOnCompleted; signalName: 'zoomLevelChanged'}

    TestCase {
        when: windowShown
        name: "MapProperties"

        function fuzzy_compare(val, ref) {
            var tolerance = 0.01;
            if ((val > ref - tolerance) && (val < ref + tolerance))
                return true;
            console.log('map fuzzy cmp returns false for value, ref: ' + val + ', ' + ref)
            return false;
        }

        function init() {
            mapCenterSpy.clear();
        }

        function test_map_center() {
            // coordinate is set at map element declaration
            compare(map.center.latitude, 10)
            compare(map.center.longitude, 11)

            // change center and its values
            mapCenterSpy.clear();
            compare(mapCenterSpy.count, 0)
            map.center = coordinate2
            compare(mapCenterSpy.count, 1)
            map.center = coordinate2
            compare(mapCenterSpy.count, 1)

            // change center to dateline
            mapCenterSpy.clear()
            compare(mapCenterSpy.count, 0)
            map.center = coordinate5
            compare(mapCenterSpy.count, 1)
            compare(map.center, coordinate5)

            map.center = coordinate2

            verify(isNaN(map.center.altitude));
            compare(map.center.longitude, 13)
            compare(map.center.latitude, 12)
        }

        function test_zoom_limits()
        {
            map.center.latitude = 30
            map.center.longitude = 60
            map.zoomLevel = 4

            //initial plugin values
            compare(map.minimumZoomLevel, 0)
            compare(map.maximumZoomLevel, 20)

            //Higher min level than curr zoom, should change curr zoom
            map.minimumZoomLevel = 5
            map.maximumZoomLevel = 18
            compare(map.zoomLevel, 5)
            compare(map.minimumZoomLevel, 5)
            compare(map.maximumZoomLevel, 18)

            //Trying to set higher than max, max should be set.
            map.maximumZoomLevel = 21
            compare(map.minimumZoomLevel, 5)
            compare(map.maximumZoomLevel, 20)

            //Negative values should be ignored
            map.minimumZoomLevel = -1
            map.maximumZoomLevel = -2
            compare(map.minimumZoomLevel, 5)
            compare(map.maximumZoomLevel, 20)

            //Max limit lower than curr zoom, should change curr zoom
            map.zoomLevel = 18
            map.maximumZoomLevel = 16
            compare(map.zoomLevel, 16)

            //reseting default
            map.minimumZoomLevel = 0
            map.maximumZoomLevel = 20
            compare(map.minimumZoomLevel, 0)
            compare(map.maximumZoomLevel, 20)
        }

        function test_zoom()
        {
            wait(100)
            compare(mapZoomOnCompleted.zoomLevel, 7)
            compare(mapZoomDefault.zoomLevel, 8)
            compare(mapZoomUserInit.zoomLevel, 4)

            mapZoomSpy.clear()
            mapZoomOnCompleted.zoomLevel = 6
            tryCompare(mapZoomSpy, "count", 1)

        }

        function test_pan()
        {
            map.center.latitude = 30
            map.center.longitude = 60
            map.zoomLevel = 4
            mapCenterSpy.clear();

            // up left
            tryCompare(mapCenterSpy, "count", 0)
            map.pan(-20,-20)
            tryCompare(mapCenterSpy, "count", 1)
            verify(map.center.latitude > 30)
            verify(map.center.longitude < 60)
            map.center.latitude = 30
            map.center.longitude = 60
            mapCenterSpy.clear()
            // up
            map.pan(0,-20)
            tryCompare(mapCenterSpy, "count", 1)
            verify(map.center.latitude > 30)
            compare(map.center.longitude, 60)
            map.center.latitude = 30
            map.center.longitude = 60
            mapCenterSpy.clear()
            // up right
            tryCompare(mapCenterSpy, "count", 0)
            map.pan(20,-20)
            tryCompare(mapCenterSpy, "count", 1)
            verify(map.center.latitude > 30)
            verify(map.center.longitude > 60)
            map.center.latitude = 30
            map.center.longitude = 60
            mapCenterSpy.clear()
            // left
            map.pan(-20,0)
            tryCompare(mapCenterSpy, "count", 1)
            verify (fuzzy_compare(map.center.latitude, 30))
            verify(map.center.longitude < 60)
            map.center.latitude = 30
            map.center.longitude = 60
            mapCenterSpy.clear()
            // center
            map.pan(0,0)
            tryCompare(mapCenterSpy, "count", 0)
            compare(map.center.latitude, 30)
            compare(map.center.longitude, 60)
            map.center.latitude = 30
            map.center.longitude = 60
            mapCenterSpy.clear()
            // right
            map.pan(20,0)
            tryCompare(mapCenterSpy, "count", 1)
            verify (fuzzy_compare(map.center.latitude, 30))
            verify(map.center.longitude > 60)
            map.center.latitude = 30
            map.center.longitude = 60
            mapCenterSpy.clear()
            // down left
            map.pan(-20,20)
            tryCompare(mapCenterSpy, "count", 1)
            verify (map.center.latitude < 30 )
            verify (map.center.longitude < 60 )
            map.center.latitude = 30
            map.center.longitude = 60
            mapCenterSpy.clear()
            // down
            map.pan(0,20)
            tryCompare(mapCenterSpy, "count", 1)
            verify (map.center.latitude < 30 )
            verify (fuzzy_compare(map.center.longitude, 60))
            map.center.latitude = 30
            map.center.longitude = 60
            mapCenterSpy.clear()
            // down right
            map.pan(20,20)
            tryCompare(mapCenterSpy, "count", 1)
            verify (map.center.latitude < 30 )
            verify (map.center.longitude > 60 )
            map.center.latitude = 30
            map.center.longitude = 60
            mapCenterSpy.clear()
        }

        function test_coordinate_conversion()
        {
            wait(1000)
            mapCenterSpy.clear();
            compare(coordinateMap.center.latitude, 50)
            compare(coordinateMap.center.longitude, 50)
            // valid to screen position
            var point = coordinateMap.fromCoordinate(coordinateMap.center)
            verify (point.x > 495 && point.x < 505)
            verify (point.y > 495 && point.y < 505)
            // valid coordinate without altitude
            point = coordinateMap.fromCoordinate(altitudelessCoordinate)
            verify (point.x > 495 && point.x < 505)
            verify (point.y > 495 && point.y < 505)
            // out of map area
            point = coordinateMap.fromCoordinate(coordinate4)
            verify(isNaN(point.x))
            verify(isNaN(point.y))
            // invalid coordinates
            point = coordinateMap.fromCoordinate(invalidCoordinate)
            verify(isNaN(point.x))
            verify(isNaN(point.y))
            point = coordinateMap.fromCoordinate(null)
            verify(isNaN(point.x))
            verify(isNaN(point.y))
            // valid point to coordinate
            var coord = coordinateMap.toCoordinate(Qt.point(500,500))
            verify(coord.latitude > 49 && coord.latitude < 51)
            verify(coord.longitude > 49 && coord.longitude < 51)
            // beyond
            coord = coordinateMap.toCoordinate(Qt.point(2000, 2000))
            verify(isNaN(coord.latitude))
            verify(isNaN(coord.longitde))
            // invalid
            coord = coordinateMap.toCoordinate(Qt.point(-5, -6))
            verify(isNaN(coord.latitude))
            verify(isNaN(coord.longitde))
        }
    }
}
