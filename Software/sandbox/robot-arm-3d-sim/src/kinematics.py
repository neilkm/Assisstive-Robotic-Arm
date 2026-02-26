from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import List, Sequence, Tuple

import numpy as np
import yaml


@dataclass
class JointSpec:
    name: str
    joint_type: str
    rotation_axis_local: str
    a_m: float
    alpha_rad: float
    d_m: float
    theta_offset_rad: float
    initial_deg: float
    min_deg: float
    max_deg: float


def load_joint_specs(config_path: str | Path) -> List[JointSpec]:
    path = Path(config_path)
    with path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f)

    joints = data["robot"]["joints"]
    specs: List[JointSpec] = []
    for j in joints:
        specs.append(
            JointSpec(
                name=j["name"],
                joint_type=j["type"],
                rotation_axis_local=str(j.get("rotation_axis_local", "z")),
                a_m=float(j["a_m"]),
                alpha_rad=float(j["alpha_rad"]),
                d_m=float(j["d_m"]),
                theta_offset_rad=float(j["theta_offset_rad"]),
                initial_deg=float(j.get("initial_deg", 0.0)),
                min_deg=float(j["min_deg"]),
                max_deg=float(j["max_deg"]),
            )
        )
    return specs


def dof(joints: Sequence[JointSpec]) -> int:
    return len(joints)


def initial_joint_angles_deg(joints: Sequence[JointSpec]) -> np.ndarray:
    return np.array([j.initial_deg for j in joints], dtype=float)


def dh_transform(a: float, alpha: float, d: float, theta: float) -> np.ndarray:
    cth, sth = math.cos(theta), math.sin(theta)
    cal, sal = math.cos(alpha), math.sin(alpha)
    return np.array(
        [
            [cth, -sth * cal, sth * sal, a * cth],
            [sth, cth * cal, -cth * sal, a * sth],
            [0.0, sal, cal, d],
            [0.0, 0.0, 0.0, 1.0],
        ],
        dtype=float,
    )


def fk(joints: Sequence[JointSpec], q_deg: Sequence[float]) -> np.ndarray:
    if len(joints) != len(q_deg):
        raise ValueError("q_deg size must match robot DOF")

    t = np.eye(4, dtype=float)
    for i, joint in enumerate(joints):
        if joint.joint_type != "revolute":
            raise NotImplementedError("Only revolute joints are implemented")
        theta = math.radians(q_deg[i]) + joint.theta_offset_rad
        t = t @ dh_transform(joint.a_m, joint.alpha_rad, joint.d_m, theta)
    return t


def ee_position(joints: Sequence[JointSpec], q_deg: Sequence[float]) -> np.ndarray:
    return fk(joints, q_deg)[:3, 3]


def fk_chain_points(joints: Sequence[JointSpec], q_deg: Sequence[float]) -> np.ndarray:
    if len(joints) != len(q_deg):
        raise ValueError("q_deg size must match robot DOF")

    t = np.eye(4, dtype=float)
    points = [np.array([0.0, 0.0, 0.0], dtype=float)]
    for i, joint in enumerate(joints):
        theta = math.radians(q_deg[i]) + joint.theta_offset_rad
        t = t @ dh_transform(joint.a_m, joint.alpha_rad, joint.d_m, theta)
        points.append(t[:3, 3].copy())
    return np.array(points, dtype=float)


def numerical_jacobian(
    joints: Sequence[JointSpec], q_deg: Sequence[float], eps_deg: float = 0.1
) -> np.ndarray:
    q = np.array(q_deg, dtype=float)
    j = np.zeros((3, len(q)), dtype=float)
    p0 = ee_position(joints, q)
    for i in range(len(q)):
        q_eps = q.copy()
        q_eps[i] += eps_deg
        p_eps = ee_position(joints, q_eps)
        j[:, i] = (p_eps - p0) / math.radians(eps_deg)
    return j


def clamp_to_limits(joints: Sequence[JointSpec], q_deg: np.ndarray) -> np.ndarray:
    out = q_deg.copy()
    for i, joint in enumerate(joints):
        out[i] = min(max(out[i], joint.min_deg), joint.max_deg)
    return out


def ik_dls_position_only(
    joints: Sequence[JointSpec],
    target_xyz_m: Sequence[float],
    q_init_deg: Sequence[float] | None = None,
    max_iters: int = 120,
    damping: float = 0.04,
    tolerance_m: float = 1e-3,
) -> Tuple[np.ndarray, bool]:
    q = (
        np.array(q_init_deg, dtype=float)
        if q_init_deg is not None
        else initial_joint_angles_deg(joints)
    )
    target = np.array(target_xyz_m, dtype=float)

    for _ in range(max_iters):
        p = ee_position(joints, q)
        err = target - p
        if np.linalg.norm(err) < tolerance_m:
            return clamp_to_limits(joints, q), True

        jac = numerical_jacobian(joints, q)
        jt = jac.T
        ident = np.eye(jac.shape[0], dtype=float)
        dq_rad = jt @ np.linalg.inv(jac @ jt + (damping**2) * ident) @ err
        q += np.degrees(dq_rad)
        q = clamp_to_limits(joints, q)

    return q, False


def build_range_trajectory(joints: Sequence[JointSpec], steps: int = 100) -> np.ndarray:
    q0 = np.array([j.min_deg for j in joints], dtype=float)
    q1 = np.array([j.max_deg for j in joints], dtype=float)
    traj = np.array([q0 + (q1 - q0) * (k / max(steps - 1, 1)) for k in range(steps)], dtype=float)
    return np.array([clamp_to_limits(joints, q) for q in traj], dtype=float)


# Backward-compatible alias.
def build_sweep_trajectory(joints: Sequence[JointSpec], steps: int = 100) -> np.ndarray:
    return build_range_trajectory(joints, steps)


if __name__ == "__main__":
    cfg = Path(__file__).resolve().parent.parent / "configs" / "robot_arm.yaml"
    specs = load_joint_specs(cfg)
    print(f"Loaded DOF: {dof(specs)}")
    sample = initial_joint_angles_deg(specs)
    print("Sample end-effector transform:")
    print(fk(specs, sample))
