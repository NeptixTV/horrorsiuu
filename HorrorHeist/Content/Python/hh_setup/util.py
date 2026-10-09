"""Shared helpers for the content setup (runs inside the Unreal Editor)."""
import json
import os
import re

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, 'SourceArt')
ROOT = '/Game/HorrorHeist'

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

ERRORS = []


def log(msg):
    unreal.log('[HH Setup] ' + str(msg))


def warn(msg):
    ERRORS.append(str(msg))
    unreal.log_warning('[HH Setup] ' + str(msg))


def load_json(*rel):
    path = os.path.join(SOURCE, *rel)
    with open(path, 'r', encoding='utf-8') as fh:
        return json.load(fh)


def ensure_dir(path):
    if not EAL.does_directory_exist(path):
        EAL.make_directory(path)


# ---------------------------------------------------------------------------------------------
# Asset index (short name -> asset), refreshed after each import step.

_INDEX = {}


def refresh_index():
    _INDEX.clear()
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path(unreal.Name(ROOT), recursive=True):
        _INDEX[str(data.asset_name)] = data


def remember(obj):
    if obj is not None:
        _INDEX[obj.get_name()] = obj


def find(name, required=True):
    """Load one of our assets by its short name (e.g. 'SM_Workbench')."""
    if not name:
        return None
    entry = _INDEX.get(name)
    if entry is None:
        refresh_index()
        entry = _INDEX.get(name)
    if entry is None:
        if required:
            warn('Missing asset: ' + name)
        return None
    if isinstance(entry, unreal.AssetData):
        obj = entry.get_asset()
        _INDEX[name] = obj
        return obj
    return entry


def create_or_load(name, folder, cls, factory):
    path = f'{folder}/{name}'
    ensure_dir(folder)
    if EAL.does_asset_exist(path):
        obj = EAL.load_asset(path)
        if obj is not None and isinstance(obj, cls):
            remember(obj)
            return obj
        EAL.delete_asset(path)
    obj = AT.create_asset(name, folder, cls, factory)
    remember(obj)
    return obj


# ---------------------------------------------------------------------------------------------
# Property names & values

def snake(name, is_bool=False):
    """C++ property name -> Unreal Python name ('bUseTemperature' -> 'use_temperature')."""
    if re.match(r'^b[A-Z]', name):
        name = name[1:]
    s = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1_\2', name)
    s = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', s)
    return s.lower()


def upper_snake(value):
    s = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1_\2', value)
    s = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', s)
    return s.upper()


def enum_value(enum_name, value):
    cls = getattr(unreal, enum_name[1:] if enum_name.startswith('E') else enum_name, None)
    if cls is None:
        cls = getattr(unreal, enum_name)
    for candidate in (upper_snake(value), value.upper(), value):
        if hasattr(cls, candidate):
            return getattr(cls, candidate)
    raise AttributeError(f'{enum_name}.{value}')


def colour(values, alpha=1.0):
    v = list(values) + [alpha] * (4 - len(values))
    return unreal.LinearColor(float(v[0]), float(v[1]), float(v[2]), float(v[3]))


def convert(value, current=None):
    """Layout/catalogue JSON value -> Unreal value (uses the current value's type as a hint)."""
    if isinstance(value, dict):
        if 'asset' in value:
            return find(value['asset'])
        if 'enum' in value:
            return enum_value(value['enum'], value['value'])
        if 'text' in value:
            return unreal.Text(value['text'])
    if isinstance(value, list):
        if isinstance(current, unreal.Vector2D) and len(value) == 2:
            return unreal.Vector2D(float(value[0]), float(value[1]))
        if isinstance(current, unreal.Vector) and len(value) == 3:
            return unreal.Vector(*[float(x) for x in value])
        if isinstance(current, unreal.LinearColor):
            return colour(value)
        if isinstance(current, unreal.Color):
            c = colour(value)
            return unreal.Color(r=int(c.r * 255), g=int(c.g * 255), b=int(c.b * 255), a=255)
        return [convert(v) for v in value]
    if isinstance(current, unreal.Text) and isinstance(value, str):
        return unreal.Text(value)
    if isinstance(current, float) and isinstance(value, (int, float)):
        return float(value)
    if isinstance(current, int) and not isinstance(current, bool) and isinstance(value, float):
        return int(value)
    return value


def set_prop(obj, key, value):
    """Set a property given its C++ name; logs instead of raising."""
    name = snake(key)
    try:
        current = obj.get_editor_property(name)
    except Exception:
        current = None
    try:
        obj.set_editor_property(name, convert(value, current))
        return True
    except Exception as exc:  # noqa: BLE001
        warn(f'{obj.get_name()}.{name}: {exc}')
        return False


def save_all():
    EAL.save_directory(ROOT, only_if_is_dirty=True, recursive=True)
