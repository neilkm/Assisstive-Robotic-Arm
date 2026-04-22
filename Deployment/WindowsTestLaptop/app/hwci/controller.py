import argparse
import json
import os
import shlex
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

try:
    import yaml
except ImportError:
    yaml = None

from .commit_tags import parse_commit_message
from .device_health import collect_device_status


class HardwareCiError(RuntimeError):
    pass


def load_config(path):
    if yaml is None:
        raise HardwareCiError(
            "Missing Python dependency: PyYAML. Run "
            "Deployment/WindowsTestLaptop/scripts/setup_wsl.sh first."
        )

    config_path = Path(path).expanduser().resolve()
    if not config_path.exists():
        raise HardwareCiError(f"Config file not found: {config_path}")

    with config_path.open("r", encoding="utf-8") as handle:
        config = yaml.safe_load(handle) or {}

    return config_path, config


def resolve_repo_path(config, config_path):
    configured = Path(config.get("repo", {}).get("path", ".")).expanduser()
    if configured.is_absolute():
        return configured
    return (config_path.parents[3] / configured).resolve()


def run_process(args, cwd, log_file=None, check=True):
    with subprocess.Popen(
        args,
        cwd=str(cwd),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    ) as process:
        output = []
        for line in process.stdout:
            output.append(line)
            print(line, end="")
            if log_file is not None:
                with log_file.open("a", encoding="utf-8") as handle:
                    handle.write(line)

    if check and process.returncode != 0:
        raise HardwareCiError(
            f"Command failed with exit code {process.returncode}: {shlex.join(args)}"
        )

    return "".join(output).strip()


def run_shell(command, cwd, log_file, env, dry_run):
    with log_file.open("a", encoding="utf-8") as handle:
        handle.write(f"$ {command}\n")

    print(f"$ {command}")
    if dry_run:
        with log_file.open("a", encoding="utf-8") as handle:
            handle.write("[dry-run] command not executed\n")
        return

    with subprocess.Popen(
        command,
        cwd=str(cwd),
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        env=env,
    ) as process:
        for line in process.stdout:
            print(line, end="")
            with log_file.open("a", encoding="utf-8") as handle:
                handle.write(line)

    if process.returncode != 0:
        raise HardwareCiError(f"Command failed with exit code {process.returncode}: {command}")


def git(repo_path, *args):
    return run_process(["git", *args], repo_path)


class HardwareCiController:
    def __init__(self, config_path, config, dry_run_override=None):
        self.config_path = config_path
        self.config = config
        self.repo_path = resolve_repo_path(config, config_path)

        controller_config = config.get("controller", {})
        self.dry_run = bool(controller_config.get("dry_run", False))
        if dry_run_override is not None:
            self.dry_run = dry_run_override

        self.remote = config.get("repo", {}).get("remote", "origin")
        self.branch = config.get("repo", {}).get("branch", "main")
        self.require_tag = controller_config.get("require_tag", "hw-ci")
        self.poll_interval_seconds = int(controller_config.get("poll_interval_seconds", 60))
        self.work_dir = self._repo_relative(controller_config.get("work_dir", "Deployment/WindowsTestLaptop/runs"))
        self.state_file = self._repo_relative(
            controller_config.get("state_file", "Deployment/WindowsTestLaptop/.hwci/state.json")
        )

    def _repo_relative(self, value):
        path = Path(value).expanduser()
        if path.is_absolute():
            return path
        return (self.repo_path / path).resolve()

    def poll_forever(self):
        while True:
            try:
                self.check_remote_once()
            except Exception as exc:
                print(f"[hwci] poll failed: {exc}", file=sys.stderr)
            time.sleep(self.poll_interval_seconds)

    def check_remote_once(self):
        git(self.repo_path, "fetch", self.remote, self.branch)
        remote_ref = f"{self.remote}/{self.branch}"
        sha = git(self.repo_path, "rev-parse", remote_ref)
        message = git(self.repo_path, "log", "-1", "--pretty=%B", remote_ref)

        state = self._read_state()
        if state.get("last_seen_sha") == sha:
            print(f"[hwci] no new commit on {remote_ref}: {sha[:12]}")
            return

        plan = parse_commit_message(message)
        plan["requires_hw_ci"] = self._has_required_tag(plan)
        if not plan["requires_hw_ci"]:
            print(f"[hwci] newest commit does not include [{self.require_tag}]: {sha[:12]}")
            self._write_state({"last_seen_sha": sha, "last_status": "skipped-no-tag"})
            return

        git(self.repo_path, "checkout", self.branch)
        git(self.repo_path, "pull", "--ff-only", self.remote, self.branch)
        status = self.execute_plan(plan, sha, message)
        self._write_state({"last_seen_sha": sha, "last_status": status})

    def run_current_head_once(self, force=False):
        sha = git(self.repo_path, "rev-parse", "HEAD")
        message = git(self.repo_path, "log", "-1", "--pretty=%B", "HEAD")
        plan = parse_commit_message(message)
        plan["requires_hw_ci"] = self._has_required_tag(plan)
        if not plan["requires_hw_ci"] and not force:
            print(f"[hwci] current HEAD does not include [{self.require_tag}]; skipping")
            return "skipped-no-tag"
        if force and not plan["requires_hw_ci"]:
            print(f"[hwci] current HEAD does not include [{self.require_tag}]; force enabled")
        return self.execute_plan(plan, sha, message)

    def _has_required_tag(self, plan):
        required = self.require_tag.strip().lower()
        if required.startswith("[") and required.endswith("]"):
            required = required[1:-1]
        return required in plan["flags"]

    def execute_plan(self, plan, sha, message):
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        run_dir = self.work_dir / f"{timestamp}-{sha[:12]}"
        run_dir.mkdir(parents=True, exist_ok=True)

        plan_path = run_dir / "ci_plan.json"
        with plan_path.open("w", encoding="utf-8") as handle:
            json.dump({"sha": sha, "message": message, "plan": plan}, handle, indent=2)

        health = collect_device_status(self.config)
        with (run_dir / "device_health.json").open("w", encoding="utf-8") as handle:
            json.dump(health, handle, indent=2)

        env = os.environ.copy()
        env.update(
            {
                "HWCI_CONFIG": str(self.config_path),
                "HWCI_RUN_DIR": str(run_dir),
                "HWCI_REPO": str(self.repo_path),
                "HWCI_SHA": sha,
                "HWCI_TARGETS": ",".join(plan["targets"]),
            }
        )

        try:
            self._run_builds(plan, run_dir, env)
            if plan["flash"]:
                self._run_device_command_stage("flash", plan, run_dir, env)
            self._run_tests(plan, run_dir, env)
        except Exception as exc:
            with (run_dir / "status.json").open("w", encoding="utf-8") as handle:
                json.dump({"status": "failed", "error": str(exc)}, handle, indent=2)
            raise

        with (run_dir / "status.json").open("w", encoding="utf-8") as handle:
            json.dump({"status": "passed"}, handle, indent=2)
        print(f"[hwci] run complete: {run_dir}")
        return "passed"

    def _run_builds(self, plan, run_dir, env):
        for target in plan["targets"]:
            device = self._enabled_device(target)
            command = device.get("build", {}).get(plan["build"])
            if not command:
                raise HardwareCiError(f"No build command configured for {target}/{plan['build']}")
            self._run_target_command("build", target, command, device, run_dir, env)

    def _run_device_command_stage(self, stage, plan, run_dir, env):
        for target in plan["targets"]:
            device = self._enabled_device(target)
            command = device.get(stage)
            if not command:
                print(f"[hwci] no {stage} command configured for {target}; skipping")
                continue
            self._run_target_command(stage, target, command, device, run_dir, env)

    def _run_tests(self, plan, run_dir, env):
        tests = self.config.get("tests", {})
        for test_name in plan["tests"]:
            test_config = tests.get(test_name)
            if not test_config:
                raise HardwareCiError(f"No test command configured for test suite: {test_name}")
            command = self._format_command(test_config["command"], run_dir, None, None)
            log_file = run_dir / f"test-{test_name}.log"
            run_shell(command, self.repo_path, log_file, env, self.dry_run)

    def _run_target_command(self, stage, target, command, device, run_dir, env):
        formatted = self._format_command(command, run_dir, target, device)
        log_file = run_dir / f"{stage}-{target}.log"
        target_env = env.copy()
        target_env.update(
            {
                "HWCI_TARGET": target,
                "HWCI_SERIAL_PORT": str(device.get("serial_port", "")),
            }
        )
        run_shell(formatted, self.repo_path, log_file, target_env, self.dry_run)

    def _format_command(self, command, run_dir, target, device):
        values = {
            "repo_path": str(self.repo_path),
            "run_dir": str(run_dir),
            "target": target or "",
            "serial_port": "",
            "usbipd_busid": "",
        }
        if device:
            values["serial_port"] = str(device.get("serial_port", ""))
            values["usbipd_busid"] = str(device.get("usbipd_busid", ""))

        formatted = command
        for key, value in values.items():
            formatted = formatted.replace("{" + key + "}", value)
        return formatted

    def _enabled_device(self, target):
        devices = self.config.get("devices", {})
        if target not in devices:
            raise HardwareCiError(f"Unknown target: {target}")
        device = devices[target]
        if not device.get("enabled", True):
            raise HardwareCiError(f"Target is disabled in config: {target}")
        return device

    def _read_state(self):
        if not self.state_file.exists():
            return {}
        with self.state_file.open("r", encoding="utf-8") as handle:
            return json.load(handle)

    def _write_state(self, state):
        self.state_file.parent.mkdir(parents=True, exist_ok=True)
        with self.state_file.open("w", encoding="utf-8") as handle:
            json.dump(state, handle, indent=2)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Windows/WSL hardware CI controller")
    parser.add_argument(
        "--config",
        default="Deployment/WindowsTestLaptop/config/hwci.yaml",
        help="Path to local hardware CI YAML config",
    )
    parser.add_argument("--poll", action="store_true", help="Poll the configured remote branch forever")
    parser.add_argument("--run-once", action="store_true", help="Run once against the current local HEAD")
    parser.add_argument("--force", action="store_true", help="Run once even if the current commit lacks the required tag")
    parser.add_argument("--dry-run", action="store_true", help="Print commands without executing them")
    parser.add_argument("--no-dry-run", action="store_true", help="Execute commands even if config has dry_run true")
    args = parser.parse_args(argv)

    dry_run_override = None
    if args.dry_run:
        dry_run_override = True
    if args.no_dry_run:
        dry_run_override = False

    try:
        config_path, config = load_config(args.config)
        controller = HardwareCiController(config_path, config, dry_run_override)

        if args.poll:
            controller.poll_forever()
            return 0

        controller.run_current_head_once(force=args.force)
        return 0
    except HardwareCiError as exc:
        print(f"[hwci] {exc}", file=sys.stderr)
        return 1
