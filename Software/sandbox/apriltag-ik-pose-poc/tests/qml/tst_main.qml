import QtQuick 2.15
import QtTest 1.3

TestCase {
    id: testCase
    name: "ApriltagIkPoseMainUi"
    when: windowShown

    property var createdWindow: null

    property var visitedObjects: ({})

    function findByObjectName(item, name) {
        if (!item) {
            return null
        }
        var key = String(item)
        if (visitedObjects[key]) {
            return null
        }
        visitedObjects[key] = true
        if (item.objectName === name) {
            return item
        }
        var lists = []
        if (item.contentItem) {
            lists.push([item.contentItem])
        }
        if (item.children) {
            lists.push(item.children)
        }
        if (item.data) {
            lists.push(item.data)
        }
        for (var listIndex = 0; listIndex < lists.length; ++listIndex) {
            var childList = lists[listIndex]
            for (var i = 0; i < childList.length; ++i) {
                var found = findByObjectName(childList[i], name)
                if (found) {
                    return found
                }
            }
        }
        return null
    }

    function findNamed(item, name) {
        visitedObjects = ({})
        return findByObjectName(item, name)
    }

    function createMainWindow() {
        var component = Qt.createComponent("../../qml/main.qml")
        compare(component.status, Component.Ready, component.errorString())
        var window = component.createObject(null)
        verify(window !== null)
        wait(50)
        return window
    }

    function cleanup() {
        if (createdWindow) {
            createdWindow.close()
            createdWindow.destroy()
            createdWindow = null
        }
        poseController.resetMock()
    }

    QtObject {
        id: poseController

        signal poseChanged()
        signal configChanged()

        property var chainPoints: [
            {x: 0, y: 0, z: 0, label: "J0"},
            {x: 1, y: 0, z: 0, label: "J1"},
            {x: 1, y: 0, z: 3, label: "J2"},
            {x: 1, y: 0, z: 4, label: "J3"},
            {x: 1, y: 0, z: 7, label: "J4"},
            {x: 1, y: 0, z: 8, label: "J5"},
            {x: 3, y: 0, z: 8, label: "EE"}
        ]
        property var tagPoints: [
            {x: 2, y: 0, z: 1.5},
            {x: 0, y: 0, z: 1.5}
        ]
        property var targetPoint: ({x: 3, y: 0, z: 8})
        property var actualEndEffector: ({x: 3, y: 0, z: 8})
        property var tagEndEffector: ({x: 3, y: 0, z: 8})
        property var jointAngles: [
            {name: "joint0_base_yaw", angleDeg: 0, tagAngleDeg: 0},
            {name: "joint1_shoulder", angleDeg: 0, tagAngleDeg: 0},
            {name: "joint2_elbow", angleDeg: 0, tagAngleDeg: 0},
            {name: "joint3_wrist_pitch", angleDeg: 0, tagAngleDeg: 0},
            {name: "joint4_wrist_yaw", angleDeg: 0, tagAngleDeg: 0},
            {name: "joint5_tool_roll", angleDeg: 0, tagAngleDeg: 0}
        ]
        property var dimensions: [
            {name: "L0", value: 1},
            {name: "L1", value: 3},
            {name: "L2", value: 1},
            {name: "L3", value: 3},
            {name: "L4", value: 1},
            {name: "L5", value: 2},
            {name: "W0", value: 1},
            {name: "W1", value: 1},
            {name: "W2", value: 1},
            {name: "G0", value: 2}
        ]
        property var dhRows: [
            {name: "joint0_base_yaw", a_m: "L0", alpha_rad: "-1.57079632679", d_m: "0", theta_offset_rad: "0", initial_deg: "0", min_deg: "-100", max_deg: "100"},
            {name: "joint1_shoulder", a_m: "L1", alpha_rad: "0", d_m: "0", theta_offset_rad: "-1.57079632679", initial_deg: "0", min_deg: "-90", max_deg: "90"},
            {name: "joint2_elbow", a_m: "L2", alpha_rad: "-1.57079632679", d_m: "0", theta_offset_rad: "0", initial_deg: "0", min_deg: "-120", max_deg: "120"},
            {name: "joint3_wrist_pitch", a_m: "L3", alpha_rad: "1.57079632679", d_m: "0", theta_offset_rad: "0", initial_deg: "0", min_deg: "-120", max_deg: "120"},
            {name: "joint4_wrist_yaw", a_m: "L4", alpha_rad: "-1.57079632679", d_m: "0", theta_offset_rad: "0", initial_deg: "0", min_deg: "-120", max_deg: "120"},
            {name: "joint5_tool_roll", a_m: "0", alpha_rad: "0", d_m: "L5", theta_offset_rad: "0", initial_deg: "0", min_deg: "-180", max_deg: "180"}
        ]
        property real axisLimit: 11
        property bool ikConverged: true
        property bool tagConverged: true
        property real ikErrorM: 0
        property real tagRmsErrorM: 0
        property real endEffectorCompareErrorM: 0
        property real motionSpeedDegPerS: 5
        property bool moving: false
        property bool cubeTestRunning: false
        property int cubeTestStep: 0
        property int cubeTestStepCount: 0
        property string message: "Mock ready."
        property int cubeRunCount: 0
        property int moveCount: 0
        property int resetCount: 0
        property int saveCount: 0
        property int reloadCount: 0

        function moveToTarget(_x, _y, _z) {
            moveCount += 1
        }

        function runCubeTest() {
            cubeRunCount += 1
            cubeTestRunning = true
            cubeTestStep = 1
            cubeTestStepCount = 9
            poseChanged()
        }

        function resetZero() {
            resetCount += 1
        }

        function reloadConfig() {
            reloadCount += 1
            configChanged()
            return true
        }

        function saveConfig(_dimensions, _dhRows) {
            saveCount += 1
            return true
        }

        function resetMock() {
            cubeRunCount = 0
            moveCount = 0
            resetCount = 0
            saveCount = 0
            reloadCount = 0
            cubeTestRunning = false
            cubeTestStep = 0
            cubeTestStepCount = 0
        }
    }

    function test_mainWindowLoadsPoseAndConfigPages() {
        createdWindow = createMainWindow()
        compare(createdWindow.currentPage, "pose")

        var cubeButton = findNamed(createdWindow, "cubeButton")
        verify(cubeButton !== null)
        compare(cubeButton.text, "Cube")

        var configButton = findNamed(createdWindow, "configPageButton")
        verify(configButton !== null)
        mouseClick(configButton)
        compare(createdWindow.currentPage, "config")

        var saveButton = findNamed(createdWindow, "saveConfigButton")
        var reloadButton = findNamed(createdWindow, "reloadConfigButton")
        verify(saveButton !== null)
        verify(reloadButton !== null)
    }

    function test_cubeButtonStartsCubeTestThroughController() {
        createdWindow = createMainWindow()

        var cubeButton = findNamed(createdWindow, "cubeButton")
        verify(cubeButton !== null)
        compare(poseController.cubeRunCount, 0)

        mouseClick(cubeButton)
        compare(poseController.cubeRunCount, 1)
        verify(poseController.cubeTestRunning)
        compare(poseController.cubeTestStepCount, 9)
    }
}
