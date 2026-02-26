from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import yaml
from matplotlib.widgets import Button, Slider, TextBox

from kinematics import (
    clamp_to_limits,
    dh_transform,
    dof,
    ee_position,
    fk_chain_points,
    initial_joint_angles_deg,
    ik_dls_position_only,
    load_joint_specs,
)


class ArmGui:
    def __init__(self, config_path: Path) -> None:
        self.config_path = config_path
        self.joints = load_joint_specs(config_path)
        self.dof = dof(self.joints)
        self.q_current = initial_joint_angles_deg(self.joints)

        with config_path.open("r", encoding="utf-8") as f:
            cfg = yaml.safe_load(f)
        ik_cfg = cfg.get("simulation", {}).get("ik", {})
        control_cfg = cfg.get("simulation", {}).get("control", {})
        self.ik_max_iters = int(ik_cfg.get("max_iters", 120))
        self.ik_damping = float(ik_cfg.get("damping", 0.04))
        self.ik_tol = float(ik_cfg.get("tolerance_m", 1e-3))
        self.dt_s = float(control_cfg.get("dt_s", 0.01))

        self.reach = sum(abs(j.a_m) + abs(j.d_m) for j in self.joints) + 0.1
        self.target_xyz = ee_position(self.joints, self.q_current)
        self.view_rot_deg = np.array([0.0, 0.0, 0.0], dtype=float)

        self.fig = plt.figure(figsize=(11, 7))
        self.ax = self.fig.add_axes([0.07, 0.24, 0.60, 0.72], projection="3d")
        self._setup_axes()
        self.base_elev = 25.0
        self.base_azim = -60.0
        self.base_roll = 0.0
        self._apply_camera_view()
        # Lock camera interaction; view is controlled only by Rx/Ry/Rz sliders.
        if hasattr(self.ax, "disable_mouse_rotation"):
            self.ax.disable_mouse_rotation()
        if hasattr(self.ax, "mouse_init"):
            self.ax.mouse_init(rotate_btn=[], zoom_btn=[])

        self.line = None
        self.joint_scatter = None
        self.ee_scatter = None
        self.target_scatter = None
        self.joint_range_lines = []
        self.joint_angle_markers = []

        self.x_box = TextBox(self.fig.add_axes([0.72, 0.75, 0.23, 0.06]), "X (m)", initial="0.0")
        self.y_box = TextBox(self.fig.add_axes([0.72, 0.66, 0.23, 0.06]), "Y (m)", initial="0.0")
        self.z_box = TextBox(self.fig.add_axes([0.72, 0.57, 0.23, 0.06]), "Z (m)", initial="0.0")
        self.move_btn = Button(self.fig.add_axes([0.72, 0.47, 0.23, 0.07]), "Move To XYZ")
        self.move_btn.on_clicked(self.on_move_clicked)

        self.view_rx_slider = Slider(
            self.fig.add_axes([0.72, 0.41, 0.23, 0.025]), "Rx", -180.0, 180.0, valinit=0.0, valstep=1.0
        )
        self.view_ry_slider = Slider(
            self.fig.add_axes([0.72, 0.37, 0.23, 0.025]), "Ry", -180.0, 180.0, valinit=0.0, valstep=1.0
        )
        self.view_rz_slider = Slider(
            self.fig.add_axes([0.72, 0.33, 0.23, 0.025]), "Rz", -180.0, 180.0, valinit=0.0, valstep=1.0
        )
        self.view_rx_slider.on_changed(self.on_view_slider_changed)
        self.view_ry_slider.on_changed(self.on_view_slider_changed)
        self.view_rz_slider.on_changed(self.on_view_slider_changed)
        self.reset_view_btn = Button(self.fig.add_axes([0.72, 0.28, 0.23, 0.04]), "Reset View")
        self.reset_view_btn.on_clicked(self.on_reset_view_clicked)

        self.joint_angle_boxes = []
        angle_panel_top = 0.20
        angle_step = 0.03
        for i in range(self.dof):
            ax_box = self.fig.add_axes([0.07, angle_panel_top - i * angle_step, 0.18, 0.025])
            box = TextBox(ax_box, f"J{i + 1} (deg)", initial=f"{self.q_current[i]:.2f}")
            self.joint_angle_boxes.append(box)
        self.set_joint_btn = Button(self.fig.add_axes([0.27, 0.02, 0.18, 0.06]), "Apply Joint Angles")
        self.set_joint_btn.on_clicked(self.on_set_joint_clicked)

        self.status_text = self.fig.text(
            0.72,
            0.22,
            f"DOF: {self.dof}\nUse XYZ move, joint fields, and Rx/Ry/Rz sliders.",
            va="top",
        )
        self.ee_text = self.fig.text(0.72, 0.14, "", va="top")

        self._set_boxes_to_current_ee()
        self._draw_robot(self.q_current)

    def _setup_axes(self) -> None:
        self.ax.set_title(f"{self.dof}-DOF Arm Target Tracking")
        self.ax.set_xlabel("X (m)")
        self.ax.set_ylabel("Y (m)")
        self.ax.set_zlabel("Z (m)")
        self.ax.set_xlim(-self.reach, self.reach)
        self.ax.set_ylim(-self.reach, self.reach)
        self.ax.set_zlim(-0.05, self.reach)
        self.ax.set_box_aspect([1.0, 1.0, 1.0])
        self.ax.grid(True)

    def _set_boxes_to_current_ee(self) -> None:
        p = ee_position(self.joints, self.q_current)
        self.x_box.set_val(f"{p[0]:.3f}")
        self.y_box.set_val(f"{p[1]:.3f}")
        self.z_box.set_val(f"{p[2]:.3f}")

    def _apply_camera_view(self) -> None:
        elev = self.base_elev + self.view_rot_deg[0]
        azim = self.base_azim + self.view_rot_deg[1]
        roll = self.base_roll + self.view_rot_deg[2]
        try:
            self.ax.view_init(elev=elev, azim=azim, roll=roll)
        except TypeError:
            self.ax.view_init(elev=elev, azim=azim)

    def _draw_robot(self, q_deg: np.ndarray) -> None:
        points = fk_chain_points(self.joints, q_deg)
        self._apply_camera_view()
        if self.line is None:
            (self.line,) = self.ax.plot(points[:, 0], points[:, 1], points[:, 2], "-o", linewidth=3)
        else:
            self.line.set_data(points[:, 0], points[:, 1])
            self.line.set_3d_properties(points[:, 2])

        if self.joint_scatter is not None:
            self.joint_scatter.remove()
        self.joint_scatter = self.ax.scatter(
            points[:-1, 0], points[:-1, 1], points[:-1, 2], s=35, c="black"
        )

        if self.ee_scatter is not None:
            self.ee_scatter.remove()
        # Real end-effector location is always shown in red.
        self.ee_scatter = self.ax.scatter([points[-1, 0]], [points[-1, 1]], [points[-1, 2]], s=90, c="red")

        if self.target_scatter is not None:
            self.target_scatter.remove()
        self.target_scatter = self.ax.scatter(
            [self.target_xyz[0]],
            [self.target_xyz[1]],
            [self.target_xyz[2]],
            marker="x",
            s=120,
            c="blue",
        )
        self._draw_joint_range_overlays(q_deg)

        ee = ee_position(self.joints, q_deg)
        self.ee_text.set_text(f"Real EE XYZ (m)\nX: {ee[0]: .4f}\nY: {ee[1]: .4f}\nZ: {ee[2]: .4f}")
        for i, box in enumerate(self.joint_angle_boxes):
            box.set_val(f"{q_deg[i]:.2f}")

        self.fig.canvas.draw_idle()

    def _draw_joint_range_overlays(self, q_deg: np.ndarray) -> None:
        for line in self.joint_range_lines:
            line.remove()
        for marker in self.joint_angle_markers:
            marker.remove()
        self.joint_range_lines = []
        self.joint_angle_markers = []

        t = np.eye(4, dtype=float)
        radius = 0.07 * self.reach
        samples = 40

        for i, joint in enumerate(self.joints):
            center = t[:3, 3].copy()
            axis = t[:3, :3] @ np.array([0.0, 0.0, 1.0], dtype=float)
            axis = axis / (np.linalg.norm(axis) + 1e-12)

            ref = t[:3, :3] @ np.array([1.0, 0.0, 0.0], dtype=float)
            ref = ref - axis * np.dot(ref, axis)
            if np.linalg.norm(ref) < 1e-9:
                ref = np.array([1.0, 0.0, 0.0], dtype=float) - axis * axis[0]
            ref = ref / (np.linalg.norm(ref) + 1e-12)
            ortho = np.cross(axis, ref)
            ortho = ortho / (np.linalg.norm(ortho) + 1e-12)

            arc_angles = np.radians(np.linspace(joint.min_deg, joint.max_deg, samples))
            arc = np.array(
                [
                    center + radius * (np.cos(a) * ref + np.sin(a) * ortho)
                    for a in arc_angles
                ],
                dtype=float,
            )
            (arc_line,) = self.ax.plot(arc[:, 0], arc[:, 1], arc[:, 2], color="orange", linewidth=1.6)
            self.joint_range_lines.append(arc_line)

            a_cur = np.radians(float(q_deg[i]))
            cur = center + radius * (np.cos(a_cur) * ref + np.sin(a_cur) * ortho)
            marker = self.ax.scatter([cur[0]], [cur[1]], [cur[2]], s=26, c="orange")
            self.joint_angle_markers.append(marker)

            theta_i = np.radians(float(q_deg[i])) + joint.theta_offset_rad
            t = t @ dh_transform(joint.a_m, joint.alpha_rad, joint.d_m, theta_i)

    def _animate_to(self, q_goal: np.ndarray) -> None:
        q_goal = clamp_to_limits(self.joints, q_goal)
        steps = max(25, int(np.max(np.abs(q_goal - self.q_current)) // 2) + 1)
        for t in np.linspace(0.0, 1.0, steps):
            q_step = self.q_current + t * (q_goal - self.q_current)
            self._draw_robot(q_step)
            plt.pause(self.dt_s)
        self.q_current = q_goal.copy()

    def on_move_clicked(self, _event) -> None:
        try:
            target = np.array(
                [float(self.x_box.text), float(self.y_box.text), float(self.z_box.text)], dtype=float
            )
        except ValueError:
            self.status_text.set_text("Invalid input. Use numeric X/Y/Z values in meters.")
            self.fig.canvas.draw_idle()
            return

        q_goal, converged = ik_dls_position_only(
            self.joints,
            target_xyz_m=target,
            q_init_deg=self.q_current,
            max_iters=self.ik_max_iters,
            damping=self.ik_damping,
            tolerance_m=self.ik_tol,
        )
        self.target_xyz = target
        self._animate_to(q_goal)
        final_err = np.linalg.norm(target - ee_position(self.joints, self.q_current))
        self.status_text.set_text(
            f"Move complete.\nConverged: {converged}\nFinal position error: {final_err:.4f} m"
        )
        self.fig.canvas.draw_idle()

    def on_set_joint_clicked(self, _event) -> None:
        try:
            q_goal = np.array([float(box.text) for box in self.joint_angle_boxes], dtype=float)
        except ValueError:
            self.status_text.set_text("Invalid joint input. Use numeric angle values in the joint fields.")
            self.fig.canvas.draw_idle()
            return

        q_goal = clamp_to_limits(self.joints, q_goal)
        self._animate_to(q_goal)

        ee = ee_position(self.joints, self.q_current)
        self.target_xyz = ee.copy()
        self.status_text.set_text("Applied joint angle fields (with limit clamping).")
        self.fig.canvas.draw_idle()

    def on_view_slider_changed(self, _val) -> None:
        self.view_rot_deg = np.array(
            [self.view_rx_slider.val, self.view_ry_slider.val, self.view_rz_slider.val], dtype=float
        )
        self._draw_robot(self.q_current)
        self.status_text.set_text(f"View Rx={self.view_rot_deg[0]:.0f}, Ry={self.view_rot_deg[1]:.0f}, Rz={self.view_rot_deg[2]:.0f}")
        self.fig.canvas.draw_idle()

    def on_reset_view_clicked(self, _event) -> None:
        self.view_rot_deg = np.array([0.0, 0.0, 0.0], dtype=float)
        self.view_rx_slider.set_val(0.0)
        self.view_ry_slider.set_val(0.0)
        self.view_rz_slider.set_val(0.0)
        self._draw_robot(self.q_current)
        self.status_text.set_text("View rotation reset to Rx=Ry=Rz=0.")
        self.fig.canvas.draw_idle()

    def run(self) -> None:
        plt.show()


def main() -> None:
    config_path = Path(__file__).resolve().parent.parent / "configs" / "robot_arm.yaml"
    gui = ArmGui(config_path=config_path)
    gui.run()


if __name__ == "__main__":
    main()
