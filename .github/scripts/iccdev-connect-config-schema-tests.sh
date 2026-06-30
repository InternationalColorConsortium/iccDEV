#!/usr/bin/env bash
# Validate checked-in IccConnect JSON config files against the public schema.
#
# Copyright (c) 2026 The International Color Consortium.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

python3 - <<'PY'
import glob
import json
import numbers
import sys
from pathlib import Path


SCHEMA_PATH = Path("docs/icc-connect-config.schema.json")
CONFIG_GLOBS = [
    "docs/Testing/json-configs/*.json",
    "Testing/hybrid/config/*.json",
]


class ValidationError(Exception):
    pass


def load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def type_matches(value, schema_type):
    if schema_type == "null":
        return value is None
    if schema_type == "boolean":
        return isinstance(value, bool)
    if schema_type == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if schema_type == "number":
        return isinstance(value, numbers.Real) and not isinstance(value, bool)
    if schema_type == "string":
        return isinstance(value, str)
    if schema_type == "array":
        return isinstance(value, list)
    if schema_type == "object":
        return isinstance(value, dict)
    return False


def resolve_ref(root, ref):
    if not ref.startswith("#/"):
        raise ValidationError("unsupported ref " + ref)
    cur = root
    for part in ref[2:].split("/"):
        cur = cur[part]
    return cur


def validate_fallback(root, schema, value, path="$"):
    if "$ref" in schema:
        validate_fallback(root, resolve_ref(root, schema["$ref"]), value, path)
        return

    if "anyOf" in schema:
        if not any(validate_branch(root, branch, value, path) for branch in schema["anyOf"]):
            raise ValidationError(path + ": did not match any allowed shape")

    if "oneOf" in schema:
        matches = sum(1 for branch in schema["oneOf"] if validate_branch(root, branch, value, path))
        if matches != 1:
            raise ValidationError(path + ": did not match exactly one allowed shape")

    if "type" in schema:
        types = schema["type"] if isinstance(schema["type"], list) else [schema["type"]]
        if not any(type_matches(value, item) for item in types):
            raise ValidationError(path + ": wrong type")

    if "enum" in schema and value not in schema["enum"]:
        raise ValidationError(path + ": value is not in enum")

    if "minimum" in schema and isinstance(value, numbers.Real) and value < schema["minimum"]:
        raise ValidationError(path + ": value below minimum")

    if "maximum" in schema and isinstance(value, numbers.Real) and value > schema["maximum"]:
        raise ValidationError(path + ": value above maximum")

    if "exclusiveMinimum" in schema and isinstance(value, numbers.Real) and value <= schema["exclusiveMinimum"]:
        raise ValidationError(path + ": value below exclusive minimum")

    if isinstance(value, dict):
        for key in schema.get("required", []):
            if key not in value:
                raise ValidationError(path + ": missing required property " + key)

        props = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            for key in value:
                if key not in props:
                    raise ValidationError(path + ": unexpected property " + key)

        for key, sub_schema in props.items():
            if key in value:
                validate_fallback(root, sub_schema, value[key], path + "/" + key)

    if isinstance(value, list) and "items" in schema:
        for index, item in enumerate(value):
            validate_fallback(root, schema["items"], item, path + "/" + str(index))


def validate_branch(root, schema, value, path):
    try:
        validate_fallback(root, schema, value, path)
        return True
    except ValidationError:
        return False


def jsonschema_validator(schema):
    try:
        import jsonschema
    except ImportError:
        return None

    jsonschema.Draft202012Validator.check_schema(schema)
    return jsonschema.Draft202012Validator(schema)


def validate_document(schema, validator, data):
    if validator is not None:
        errors = sorted(validator.iter_errors(data), key=lambda err: list(err.path))
        if errors:
            err = errors[0]
            path = "/".join(str(item) for item in err.path) or "$"
            raise ValidationError(path + ": " + err.message)
    else:
        validate_fallback(schema, schema, data)


schema = load_json(SCHEMA_PATH)
validator = jsonschema_validator(schema)
engine = "jsonschema" if validator is not None else "builtin-fallback"

config_paths = []
for pattern in CONFIG_GLOBS:
    config_paths.extend(Path(path) for path in glob.glob(pattern))
config_paths = sorted(config_paths)

if not config_paths:
    print("[FAIL] no connect config JSON files found")
    sys.exit(1)

failures = []
for path in config_paths:
    try:
        validate_document(schema, validator, load_json(path))
    except Exception as exc:
        failures.append((path, str(exc)))

negative_cases = {
    "empty-object": {},
    "schema-alias-applyData": {"applyData": {}, "profileSequence": []},
    "schema-alias-applyImage": {"applyImage": {}, "profileSequence": []},
    "missing-profileSequence": {"dataFiles": {}},
    "missing-search-initial": {"dataFiles": {}, "searchApply": {"profileSequence": []}},
}

for name, data in negative_cases.items():
    try:
        validate_document(schema, validator, data)
    except ValidationError:
        continue
    failures.append((Path(name), "negative case unexpectedly validated"))

if failures:
    print("[FAIL] connect-config schema validation failed")
    for path, reason in failures[:20]:
        print("  " + str(path) + ": " + reason)
    if len(failures) > 20:
        print("  ... " + str(len(failures) - 20) + " more")
    sys.exit(1)

print("[OK] connect-config schema valid using " + engine)
print("[OK] checked configs validated: " + str(len(config_paths)))
print("[OK] negative schema shape checks rejected: " + str(len(negative_cases)))
PY
