import unittest
from pathlib import Path
import sys


APP_PATH = Path(__file__).resolve().parents[1] / "app"
sys.path.insert(0, str(APP_PATH))

from hwci.commit_tags import parse_commit_message


class CommitTagParserTests(unittest.TestCase):
    def test_defaults_when_hw_ci_requested(self):
        plan = parse_commit_message("Change control loop [hw-ci]")

        self.assertTrue(plan["requires_hw_ci"])
        self.assertEqual(plan["targets"], ["stm32", "esp32", "jetson"])
        self.assertEqual(plan["build"], "test")
        self.assertEqual(plan["tests"], ["smoke"])
        self.assertTrue(plan["flash"])

    def test_selected_targets_and_no_flash(self):
        plan = parse_commit_message(
            "Update joystick [hw-ci] [targets:stm32,esp32] [build:prod] [tests:integration] [flash:no]"
        )

        self.assertEqual(plan["targets"], ["stm32", "esp32"])
        self.assertEqual(plan["build"], "prod")
        self.assertEqual(plan["tests"], ["integration"])
        self.assertFalse(plan["flash"])

    def test_target_aliases(self):
        plan = parse_commit_message("Alias test [hw-ci] [targets:nucleo,esp,nvidia]")

        self.assertEqual(plan["targets"], ["stm32", "esp32", "jetson"])


if __name__ == "__main__":
    unittest.main()

