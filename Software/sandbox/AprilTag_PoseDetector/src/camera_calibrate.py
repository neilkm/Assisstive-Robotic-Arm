import cv2
import numpy as np
import yaml
from pathlib import Path

def main():
    # === CONFIG ===
    cam_index = 0
    chessboard_size = (9, 6)   # inner corners (columns, rows)
    square_size_m = 0.0245     # <-- SET THIS: physical chessboard square size in meters

    out_yaml = Path(__file__).parent / "camera.yaml"

    # Prepare object points (0,0,0), (1,0,0) ... scaled by square_size_m
    objp = np.zeros((chessboard_size[0] * chessboard_size[1], 3), np.float32)
    objp[:, :2] = np.mgrid[0:chessboard_size[0], 0:chessboard_size[1]].T.reshape(-1, 2)
    objp *= square_size_m

    objpoints = []
    imgpoints = []

    cap = cv2.VideoCapture(cam_index)
    if not cap.isOpened():
        raise RuntimeError("Could not open webcam.")

    print("Calibration capture:")
    print(" - Press SPACE to capture a frame when chessboard is detected")
    print(" - Press ENTER to run calibration")
    print(" - Press q to quit")

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        found, corners = cv2.findChessboardCorners(gray, chessboard_size, None)

        view = frame.copy()
        if found:
            cv2.drawChessboardCorners(view, chessboard_size, corners, found)
            cv2.putText(view, "Chessboard FOUND - press SPACE to capture",
                        (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,255,0), 2)
        else:
            cv2.putText(view, "Chessboard NOT found",
                        (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,0,255), 2)

        cv2.putText(view, f"Captures: {len(objpoints)}",
                    (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255,255,255), 2)

        cv2.imshow("camera_calibrate", view)
        key = cv2.waitKey(1) & 0xFF

        if key == ord('q'):
            break
        if key == 32 and found:  # SPACE
            # Refine corners
            criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)
            corners2 = cv2.cornerSubPix(gray, corners, (11,11), (-1,-1), criteria)

            objpoints.append(objp.copy())
            imgpoints.append(corners2)
            print(f"Captured {len(objpoints)}")
        if key == 13:  # ENTER
            if len(objpoints) < 10:
                print("Need at least 10 captures for decent calibration.")
                continue

            h, w = gray.shape[:2]
            ret, K, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, (w, h), None, None)

            data = {
                "camera_matrix": K.tolist(),
                "dist_coeffs": dist.flatten().tolist(),
                "reprojection_error": float(ret),
                "image_size": [int(w), int(h)]
            }
            with open(out_yaml, "w") as f:
                yaml.safe_dump(data, f)

            print(f"Saved calibration to {out_yaml}")
            print(f"Reprojection error: {ret}")
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()

