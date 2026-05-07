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
        property real targetActualErrorM: 0
        property real targetCalculatedErrorM: 0
        property real endEffectorRotationDeg: 0
        property real endEffectorRotationMinDeg: -180
        property real endEffectorRotationMaxDeg: 180
        property real approachAngleDeg: 0
        property string solverWarning: ""
        property var tagVisibility: [
            {index: 0, id: "apriltag0", visible: true},
            {index: 1, id: "apriltag1", visible: true},
            {index: 2, id: "apriltag2", visible: true},
            {index: 3, id: "apriltag3", visible: true},
            {index: 4, id: "apriltag4", visible: true},
            {index: 5, id: "apriltag5", visible: true},
            {index: 6, id: "apriltag6", visible: true},
            {index: 7, id: "apriltag7", visible: true}
        ]
        property real motionSpeedDegPerS: 5
        property bool moving: false
        property bool cubeTestRunning: false
        property int cubeTestStep: 0
        property int cubeTestStepCount: 0
        property string message: "Mock ready."
        property int cubeRunCount: 0
        property int moveCount: 0
        property int resetCount: 0
        property int rotationSetCount: 0
        property int rotationNudgeCount: 0
        property int approachSetCount: 0
        property int tagVisibilitySetCount: 0
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

        function setEndEffectorRotation(value) {
            rotationSetCount += 1
            endEffectorRotationDeg = value
            poseChanged()
        }

        function nudgeEndEffectorRotation(delta) {
            rotationNudgeCount += 1
            endEffectorRotationDeg += delta
            poseChanged()
        }

        function setApproachAngle(value) {
            approachSetCount += 1
            approachAngleDeg = value
            poseChanged()
        }

        function setAprilTagVisible(index, isVisible) {
            tagVisibilitySetCount += 1
            var updated = tagVisibility.slice()
            updated[index] = {index: index, id: updated[index].id, visible: isVisible}
            tagVisibility = updated
            poseChanged()
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
            rotationSetCount = 0
            rotationNudgeCount = 0
            approachSetCount = 0
            tagVisibilitySetCount = 0
            saveCount = 0
            reloadCount = 0
            endEffectorRotationDeg = 0
            approachAngleDeg = 0
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
        var rotationInput = findNamed(createdWindow, "rotationInput")
        var rotationSetButton = findNamed(createdWindow, "rotationSetButton")
        var approachSetButton = findNamed(createdWindow, "approachSetButton")
        verify(saveButton !== null)
        verify(reloadButton !== null)
        verify(rotationInput !== null)
        verify(rotationSetButton !== null)
        verify(approachSetButton !== null)
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

    function test_rotationControlsDriveJoint5ControllerApi() {
        createdWindow = createMainWindow()

        var plusButton = findNamed(createdWindow, "rotationPlusButton")
        var setButton = findNamed(createdWindow, "rotationSetButton")
        var rotationInput = findNamed(createdWindow, "rotationInput")
        verify(plusButton !== null)
        verify(setButton !== null)
        verify(rotationInput !== null)

        mouseClick(plusButton)
        compare(poseController.rotationNudgeCount, 1)
        compare(poseController.endEffectorRotationDeg, 15)

        rotationInput.text = "42"
        mouseClick(setButton)
        compare(poseController.rotationSetCount, 1)
        compare(poseController.endEffectorRotationDeg, 42)
    }

    function test_approachAndTagVisibilityControlsDriveControllerApi() {
        createdWindow = createMainWindow()

        var approachInput = findNamed(createdWindow, "approachInput")
        var approachSetButton = findNamed(createdWindow, "approachSetButton")
        var tag0Button = findNamed(createdWindow, "tagVisibilityButton_0")
        verify(approachInput !== null)
        verify(approachSetButton !== null)
        verify(tag0Button !== null)

        approachInput.text = "-33"
        mouseClick(approachSetButton)
        compare(poseController.approachSetCount, 1)
        compare(poseController.approachAngleDeg, -33)

        mouseClick(tag0Button)
        compare(poseController.tagVisibilitySetCount, 1)
        compare(poseController.tagVisibility[0].visible, false)
    }
}
