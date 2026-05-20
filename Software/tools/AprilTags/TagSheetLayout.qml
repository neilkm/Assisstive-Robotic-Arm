import QtQuick 2.15

Item {
    id: document
    objectName: "document"

    property real inch: 72
    property real paperWidthInches: 8.5
    property real paperHeightInches: 11.0

    width: paperWidthInches * inch
    height: paperHeightInches * inch

    component Paper: Item {
        objectName: "page"
        property int pageIndex: 0
        property real widthInches: document.paperWidthInches
        property real heightInches: document.paperHeightInches

        width: widthInches * document.inch
        height: heightInches * document.inch

        Rectangle {
            anchors.fill: parent
            color: "white"
        }
    }

    component Tag: Rectangle {
        objectName: "tag"
        property var aprilTag
        property int indexOnPage: 0
        property string slotName: ""
        property real leftMarginInches: 0
        property real rightMarginInches: 0
        property real topMarginInches: 0
        property real bottomMarginInches: 0

        width: aprilTag.tagSizeInches * document.inch
        height: aprilTag.tagSizeInches * document.inch
        color: "transparent"

        Image {
            anchors.fill: parent
            source: aprilTag.imageSource
            fillMode: Image.PreserveAspectFit
            smooth: false
        }
    }

    component DottedCutGuide: Item {
        objectName: "cutGuide"
        property real dotLength: 0.12 * document.inch
        property real dotGap: 0.08 * document.inch
        property real lineWidth: 1
        property int horizontalDots: Math.max(1, Math.floor(width / (dotLength + dotGap)))
        property int verticalDots: Math.max(1, Math.floor(height / (dotLength + dotGap)))

        Repeater {
            model: parent.horizontalDots

            Rectangle {
                objectName: "printRect"
                width: parent.dotLength
                height: parent.lineWidth
                x: index * (parent.dotLength + parent.dotGap)
                y: 0
                color: "black"
            }
        }

        Repeater {
            model: parent.horizontalDots

            Rectangle {
                objectName: "printRect"
                width: parent.dotLength
                height: parent.lineWidth
                x: index * (parent.dotLength + parent.dotGap)
                y: parent.height - parent.lineWidth
                color: "black"
            }
        }

        Repeater {
            model: parent.verticalDots

            Rectangle {
                objectName: "printRect"
                width: parent.lineWidth
                height: parent.dotLength
                x: 0
                y: index * (parent.dotLength + parent.dotGap)
                color: "black"
            }
        }

        Repeater {
            model: parent.verticalDots

            Rectangle {
                objectName: "printRect"
                width: parent.lineWidth
                height: parent.dotLength
                x: parent.width - parent.lineWidth
                y: index * (parent.dotLength + parent.dotGap)
                color: "black"
            }
        }
    }

    Repeater {
        model: tagProvider.pageCount

        Paper {
            id: paper
            pageIndex: index

            DottedCutGuide {
                id: outerCutGuide
                anchors.fill: parent
                anchors.margins: 0.5 * document.inch
            }

            Tag {
                id: tag0
                slotName: "top-left"
                indexOnPage: 0
                aprilTag: tagProvider.tagAt(paper.pageIndex * 4 + indexOnPage)
                leftMarginInches: 0
                topMarginInches: 0
                anchors.left: outerCutGuide.left
                anchors.top: outerCutGuide.top
                anchors.leftMargin: leftMarginInches * document.inch
                anchors.topMargin: topMarginInches * document.inch
            }

            Tag {
                id: tag1
                slotName: "top-right"
                indexOnPage: 1
                aprilTag: tagProvider.tagAt(paper.pageIndex * 4 + indexOnPage)
                rightMarginInches: 0
                topMarginInches: 0
                anchors.right: outerCutGuide.right
                anchors.top: outerCutGuide.top
                anchors.rightMargin: rightMarginInches * document.inch
                anchors.topMargin: topMarginInches * document.inch
            }

            Tag {
                id: tag2
                slotName: "bottom-left"
                indexOnPage: 2
                aprilTag: tagProvider.tagAt(paper.pageIndex * 4 + indexOnPage)
                leftMarginInches: 0
                bottomMarginInches: 0
                anchors.left: outerCutGuide.left
                anchors.bottom: outerCutGuide.bottom
                anchors.leftMargin: leftMarginInches * document.inch
                anchors.bottomMargin: bottomMarginInches * document.inch
            }

            Tag {
                id: tag3
                slotName: "bottom-right"
                indexOnPage: 3
                aprilTag: tagProvider.tagAt(paper.pageIndex * 4 + indexOnPage)
                rightMarginInches: 0
                bottomMarginInches: 0
                anchors.right: outerCutGuide.right
                anchors.bottom: outerCutGuide.bottom
                anchors.rightMargin: rightMarginInches * document.inch
                anchors.bottomMargin: bottomMarginInches * document.inch
            }

            DottedCutGuide {
                id: cutGuide
                anchors.top: tag0.bottom
                anchors.bottom: tag3.top
                anchors.left: tag2.right
                anchors.right: tag1.left

                Text {
                    objectName: "printText"
                    anchors.centerIn: parent
                    width: parent.width - 0.25 * document.inch
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                    text: tagProvider.pageInfoText(paper.pageIndex)
                    color: "black"
                    font.pixelSize: 10
                }
            }
        }
    }

    Paper {
        id: checkerboardPage
        objectName: "checkerboardPage"
        visible: tagProvider.checkerboardEnabled
        pageIndex: tagProvider.pageCount

        Text {
            objectName: "printText"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 0.25 * document.inch
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignTop
            text: "Checkerboard square size: 0.75 in"
            color: "black"
            font.pixelSize: 12
        }

        Grid {
            id: checkerboard
            rows: 7
            columns: 9
            anchors.centerIn: parent

            Repeater {
                model: 63

                Rectangle {
                    objectName: "printRect"
                    width: 0.75 * document.inch
                    height: 0.75 * document.inch
                    color: ((Math.floor(index / 9) + (index % 9)) % 2) === 0 ? "black" : "white"
                }
            }
        }

        DottedCutGuide {
            anchors.fill: checkerboard
        }
    }
}
