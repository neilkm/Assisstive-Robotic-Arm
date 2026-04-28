import json
import os
from pathlib import Path

import pytest


@pytest.mark.smoke
def test_hardware_ci_environment_is_present():
    assert os.environ.get("HWCI_RUN_DIR")
    assert os.environ.get("HWCI_REPO")
    assert os.environ.get("HWCI_TARGETS")


@pytest.mark.smoke
def test_device_health_report_exists():
    run_dir = Path(os.environ["HWCI_RUN_DIR"])
    report = run_dir / "device_health.json"

    assert report.exists()
    health = json.loads(report.read_text(encoding="utf-8"))
    assert isinstance(health, dict)


@pytest.mark.smoke
def test_serial_devices_when_required():
    if os.environ.get("HWCI_REQUIRE_SERIAL", "false").lower() not in {"1", "true", "yes"}:
        pytest.skip("Set HWCI_REQUIRE_SERIAL=true to require serial devices in smoke tests")

    run_dir = Path(os.environ["HWCI_RUN_DIR"])
    health = json.loads((run_dir / "device_health.json").read_text(encoding="utf-8"))
    missing = [
        name
        for name, status in health.items()
        if status.get("enabled") and not status.get("serial_present")
    ]

    assert not missing, f"Missing configured serial devices: {', '.join(missing)}"

