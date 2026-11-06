#!/usr/bin/env python3
# Copyright (c) 2024 MUSIC Tribe
#
# This file is part of Kick.
#
# Kick is free software: you can redistribute it and/or modify it under the
# terms of the GNU Affero General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# Kick is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
# more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with Kick. If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Validates JSON lines against a small subset of JSON Schema.

Supported keywords: type (object, string, number, integer, boolean, array), enum, required, properties, additionalProperties
(false only), minimum, maximum, oneOf. Anything else in the schema is ignored, including title and description.
That is the part docs/gesture-event.schema.json uses, so the check needs no third-party package.

Usage: validate_events.py SCHEMA [FILE]       reads stdin when FILE is missing or "-"
Exit status: 0 when every line is valid, 1 when a line is not, 2 for a usage or schema error.
"""
import json
import sys


def check(schema, value, path):
    """Returns a list of error strings, empty when value matches schema."""
    errors = []
    if "oneOf" in schema:
        matches = [s for s in schema["oneOf"] if not check(s, value, path)]
        if len(matches) != 1:
            return [f"{path}: matches {len(matches)} of {len(schema['oneOf'])} oneOf branches (need exactly 1)"]
    t = schema.get("type")
    if t is not None:
        ok = {
            "object": isinstance(value, dict),
            "string": isinstance(value, str),
            "boolean": isinstance(value, bool),
            "array": isinstance(value, list),
            "number": isinstance(value, (int, float)) and not isinstance(value, bool),
            "integer": isinstance(value, int) and not isinstance(value, bool),
        }.get(t)
        if ok is None:
            raise ValueError(f"unsupported type {t!r}")
        if not ok:
            return [f"{path}: expected {t}"]
    if "enum" in schema and value not in schema["enum"]:
        errors.append(f"{path}: {value!r} is not one of {schema['enum']}")
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        if "minimum" in schema and value < schema["minimum"]:
            errors.append(f"{path}: {value} is below {schema['minimum']}")
        if "maximum" in schema and value > schema["maximum"]:
            errors.append(f"{path}: {value} is above {schema['maximum']}")
    if isinstance(value, dict):
        for key in schema.get("required", []):
            if key not in value:
                errors.append(f"{path}: missing {key!r}")
        props = schema.get("properties", {})
        for key, sub in props.items():
            if key in value:
                errors += check(sub, value[key], f"{path}.{key}")
        if schema.get("additionalProperties") is False:
            for key in value:
                if key not in props:
                    errors.append(f"{path}: unexpected property {key!r}")
    return errors


def main(argv):
    if len(argv) not in (1, 2):
        print(__doc__, file=sys.stderr)
        return 2
    try:
        with open(argv[0]) as f:
            schema = json.load(f)
    except (OSError, ValueError) as e:
        print(f"schema: {e}", file=sys.stderr)
        return 2
    src = sys.stdin if len(argv) == 1 or argv[1] == "-" else open(argv[1])
    bad = 0
    count = 0
    for n, line in enumerate(src, 1):
        line = line.strip()
        if not line:
            continue
        count += 1
        try:
            value = json.loads(line)
            errors = check(schema, value, "$")
        except ValueError as e:
            errors = [f"not valid JSON or schema: {e}"]
        for err in errors:
            print(f"line {n}: {err}")
        bad += 1 if errors else 0
    print(f"{count} lines, {bad} invalid")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
