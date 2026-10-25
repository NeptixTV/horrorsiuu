#!/usr/bin/env python3
"""Lint rules the syntax check cannot see (heuristics, errors exit 1):

  * every .h/.cpp is UTF-8 and starts with a BOM if it contains non-ASCII characters
    (MSVC reads files without BOM in the system code page and breaks TEXT("ä") literals)
  * non-ASCII characters only inside TEXT("...") / comments, not in narrow "..." literals
  * FString::Printf / UE_LOG: number of format specifiers == number of arguments,
    %s arguments must be TCHAR pointers (*Str, TEXT(), ANSI_TO_TCHAR, conditional of those)
  * usage of some engine APIs requires their header in the include closure of the .cpp
"""
import os
import re
import sys

SRC = sys.argv[1]
errors = []
warnings = []

files = []
for root, _, fs in os.walk(SRC):
    for f in fs:
        if f.endswith(('.h', '.cpp')):
            files.append(os.path.join(root, f))

contents = {}
for p in files:
    data = open(p, 'rb').read()
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError:
        errors.append('%s: not valid UTF-8' % p)
        continue
    has_bom = text.startswith('﻿')
    if has_bom:
        text = text[1:]
    contents[p] = text
    if any(ord(c) > 127 for c in text) and not has_bom:
        errors.append('%s: contains non-ASCII characters but has no UTF-8 BOM (MSVC would misread TEXT literals)' % os.path.relpath(p, SRC))


def strip_comments(t):
    t = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), t, flags=re.S)
    t = re.sub(r'//[^\n]*', '', t)
    return t


STRING = re.compile(r'(u8|u|U|L)?"((?:[^"\\\n]|\\.)*)"')

for p, text in contents.items():
    code = strip_comments(text)
    for lineno, line in enumerate(code.split('\n'), 1):
        for m in STRING.finditer(line):
            prefix_start = m.start()
            before = line[:prefix_start]
            in_text_macro = re.search(r'TEXT\s*\(\s*$', before) is not None
            if any(ord(c) > 127 for c in m.group(2)) and not in_text_macro and m.group(1) not in ('u', 'L', 'U'):
                errors.append('%s:%d: non-ASCII character in a narrow string literal (use TEXT() or \\x escapes)' % (os.path.relpath(p, SRC), lineno))


def split_args(s):
    out, depth, cur, instr = [], 0, '', False
    i = 0
    while i < len(s):
        c = s[i]
        if instr:
            cur += c
            if c == '\\':
                cur += s[i + 1]
                i += 1
            elif c == '"':
                instr = False
        elif c == '"':
            instr = True
            cur += c
        elif c in '([{':
            depth += 1
            cur += c
        elif c in ')]}':
            depth -= 1
            cur += c
        elif c == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += c
        i += 1
    if cur.strip():
        out.append(cur.strip())
    return out


def find_call_args(code, start):
    depth, instr, i = 0, False, start
    while i < len(code):
        c = code[i]
        if instr:
            if c == '\\':
                i += 1
            elif c == '"':
                instr = False
        elif c == '"':
            instr = True
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return code[start + 1:i]
        i += 1
    return None


SPEC = re.compile(r'%(?:%|[-+ #0]*(?:\*|\d+)?(?:\.(?:\*|\d+))?(?:hh|h|ll|l|z|j|t|L)?([diouxXeEfFgGcsp]))')
TCHAR_ARG = re.compile(r'^(\*|TEXT\s*\(|ANSI_TO_TCHAR|UTF8_TO_TCHAR|[A-Za-z_][A-Za-z0-9_]*\s*\?.*:|nullptr$)')

for p, text in contents.items():
    code = strip_comments(text)
    for m in re.finditer(r'\b(FString::Printf|UE_LOG)\s*\(', code):
        args_txt = find_call_args(code, m.end() - 1)
        if args_txt is None:
            continue
        args = split_args(args_txt)
        if m.group(1) == 'UE_LOG':
            args = args[2:]
        if not args:
            continue
        fm = re.match(r'TEXT\s*\(\s*"((?:[^"\\]|\\.)*)"\s*\)', args[0])
        lineno = code.count('\n', 0, m.start()) + 1
        where = '%s:%d' % (os.path.relpath(p, SRC), lineno)
        if not fm:
            errors.append('%s: %s format is not a TEXT("...") literal' % (where, m.group(1)))
            continue
        specs = [s for s in SPEC.findall(fm.group(1))]
        # '%%' yields an empty group
        specs = [s for s in specs if s]
        values = args[1:]
        if len(specs) != len(values):
            errors.append('%s: %s has %d format specifiers but %d arguments' % (where, m.group(1), len(specs), len(values)))
            continue
        # (the type of %s arguments is checked by the mock's static_assert during the syntax check)
        for s, v in zip(specs, values):
            if s in 'di' and re.search(r'\.f\b|\d\.\d*f\b', v) and 'Round' not in v and 'Floor' not in v and 'static_cast<int' not in v:
                warnings.append('%s: %%d with a float-looking argument "%s"' % (where, v))

# includes: API usage -> required engine header somewhere in the include closure
REQUIRES = [
    (r'GetCapsuleComponent\(\)->', 'Components/CapsuleComponent.h'),
    (r'GetCharacterMovement\(\)->', 'GameFramework/CharacterMovementComponent.h'),
    (r'\bTActorIterator<', 'EngineUtils.h'),
    (r'\bDOREPLIFETIME', 'Net/UnrealNetwork.h'),
    (r'GetTimerManager\(\)\.|GetWorldTimerManager\(\)\.', 'TimerManager.h'),
    (r'\bUGameplayStatics::', 'Kismet/GameplayStatics.h'),
    (r'\bUKismetSystemLibrary::', 'Kismet/KismetSystemLibrary.h'),
    (r'\bUKismetRenderingLibrary::', 'Kismet/KismetRenderingLibrary.h'),
    (r'\bGEngine\b', 'Engine/Engine.h'),
    (r'\bFCanvasTextItem\b|\bFCanvasTileItem\b', 'CanvasItem.h'),
    (r'\bCanvas->', 'Engine/Canvas.h'),
    (r'GetWorld\(\)->(LineTrace|Sweep|Overlap|SpawnActor|GetTimeSeconds|GetGameState|GetAuthGameMode|GetFirstPlayerController|GetSubsystem)', 'Engine/World.h'),
    (r'\bUCameraComponent\b.*->|GetFirstPersonCamera\(\)->', 'Camera/CameraComponent.h'),
    (r'\bUBoxComponent>\s*\(|\bUBoxComponent\*.*->SetBoxExtent|->SetBoxExtent\(', 'Components/BoxComponent.h'),
    (r'NewObject<UStaticMeshComponent>|CreateDefaultSubobject<UStaticMeshComponent>', 'Components/StaticMeshComponent.h'),
    (r'NewObject<UTextRenderComponent>|CreateDefaultSubobject<UTextRenderComponent>', 'Components/TextRenderComponent.h'),
    (r'NewObject<UPointLightComponent>|CreateDefaultSubobject<UPointLightComponent>', 'Components/PointLightComponent.h'),
    (r'NewObject<USpotLightComponent>|CreateDefaultSubobject<USpotLightComponent>', 'Components/SpotLightComponent.h'),
    (r'NewObject<USceneCaptureComponent2D>', 'Components/SceneCaptureComponent2D.h'),
    (r'NewObject<UCableComponent>', 'CableComponent.h'),
    (r'\bAAIController\b', 'AIController.h'),
    (r'\bFSlateApplication::', 'Framework/Application/SlateApplication.h'),
    (r'\bFCoreStyle::', 'Styling/CoreStyle.h'),
    (r'\bUEnhancedInputComponent\b', 'EnhancedInputComponent.h'),
    (r'\bUEnhancedInputLocalPlayerSubsystem\b', 'EnhancedInputSubsystems.h'),
    (r'\bFApp::', 'Misc/App.h'),
    (r'\bFPaths::', 'Misc/Paths.h'),
    (r'\bFFileHelper::', 'Misc/FileHelper.h'),
    (r'\bIFileManager::', 'HAL/FileManager.h'),
    (r'\bFAutoConsoleVariableRef\b|\bFAutoConsoleCommand', 'HAL/IConsoleManager.h'),
]

include_re = re.compile(r'#include\s+"([^"]+)"')
by_rel = {os.path.relpath(p, SRC).replace(os.sep, '/'): p for p in contents}


def closure(path, seen):
    if path in seen:
        return
    seen.add(path)
    for inc in include_re.findall(contents.get(path, '')):
        target = by_rel.get(inc)
        if target is None:
            local = os.path.join(os.path.dirname(path), inc)
            if local in contents:
                target = local
        if target:
            closure(target, seen)
        else:
            seen.add('ENGINE:' + inc)


for p, text in contents.items():
    if not p.endswith('.cpp'):
        continue
    seen = set()
    closure(p, seen)
    engine = {s[7:] for s in seen if s.startswith('ENGINE:')}
    code = strip_comments(text)
    for pattern, header in REQUIRES:
        if re.search(pattern, code) and header not in engine:
            errors.append('%s: uses an API from <%s> but the header is not included (directly or through project headers)' % (os.path.relpath(p, SRC), header))

for w in warnings:
    print('LINT WARNING: ' + w)
for e in errors:
    print('LINT ERROR: ' + e)
print('lint: %d files, %d errors, %d warnings' % (len(contents), len(errors), len(warnings)))
sys.exit(1 if errors else 0)
