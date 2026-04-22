import re


TAG_PATTERN = re.compile(r"\[([A-Za-z0-9_-]+)(?::([^\]]+))?\]")
DEFAULT_TARGETS = ["stm32", "esp32", "jetson"]
TARGET_ALIASES = {
    "all": DEFAULT_TARGETS,
    "stm": ["stm32"],
    "nucleo": ["stm32"],
    "stm32": ["stm32"],
    "esp": ["esp32"],
    "esp32": ["esp32"],
    "nvidia": ["jetson"],
    "jetson": ["jetson"],
}
FALSE_VALUES = {"false", "no", "0", "off", "skip"}


def _split_csv(value):
    return [item.strip().lower() for item in value.split(",") if item.strip()]


def _expand_targets(values):
    targets = []
    for value in values:
        expanded = TARGET_ALIASES.get(value)
        if expanded is None:
            targets.append(value)
            continue
        targets.extend(expanded)

    deduped = []
    for target in targets:
        if target not in deduped:
            deduped.append(target)
    return deduped


def parse_commit_message(message):
    tags = {}
    flags = set()

    for key, value in TAG_PATTERN.findall(message or ""):
        key = key.lower()
        if value == "":
            flags.add(key)
        else:
            tags[key] = value.strip()

    targets = DEFAULT_TARGETS
    if "targets" in tags:
        targets = _expand_targets(_split_csv(tags["targets"]))

    tests = ["smoke"]
    if "tests" in tags:
        tests = _split_csv(tags["tests"])

    build = tags.get("build", "test").lower()

    flash = True
    if "flash" in tags and tags["flash"].lower() in FALSE_VALUES:
        flash = False

    return {
        "raw_tags": tags,
        "flags": sorted(flags),
        "requires_hw_ci": "hw-ci" in flags,
        "targets": targets,
        "build": build,
        "tests": tests,
        "flash": flash,
    }

