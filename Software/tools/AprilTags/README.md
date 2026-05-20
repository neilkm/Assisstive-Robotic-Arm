# AprilTags Qt Layout Tool

Standalone headless Qt/QML/C++ tool for generating printable AprilTag PDFs.

The QML file is the source of truth for the print layout. `TagSheetLayout.qml`
hand places the page, corner `Tag` objects, center cut-guide rectangles, info
text, and optional checkerboard page using anchors and rectangles. There is no
GUI window; C++ loads the QML object tree and writes a PDF.

## Build And Run

```bash
./scripts/run_apriltags.sh
```

The script configures and builds into:

```text
Assisstive-Robotic-Arm/builds/tools/AprilTags
```

Then it runs the built generator. Build artifacts stay in `builds/tools`, but
PDF output defaults to:

```text
~/Downloads/apriltags.pdf
```

## Options

```bash
./scripts/run_apriltags.sh \
  --family 36h11 \
  --tag-size-inches 1.0 \
  --pages 2 \
  --checkerboard yes \
  --start-id 0 \
  --output april_tags.pdf
```

Relative `--output` values are written under `~/Downloads`. Absolute output
paths are used as provided.

Supported families are `16h5`, `25h9`, `36h10`, and `36h11`.

## QML Data

Each QML `Tag` receives an `aprilTag` object from C++:

- `aprilTag.idNumber`
- `aprilTag.familyId`
- `aprilTag.tagSizeInches`
- `aprilTag.imageSource`

Example:

```qml
Tag {
    slotName: "top-left"
    indexOnPage: 0
    aprilTag: tagProvider.tagAt(root.pageIndex * 4 + indexOnPage)
    anchors.left: parent.left
    anchors.top: parent.top
}
```

Edit `TagSheetLayout.qml` to change the page layout directly.
