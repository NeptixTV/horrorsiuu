#!/usr/bin/env python3
"""AST checks for errors that only the real Unreal/MSVC build reports (needs `pip install libclang`).

1. MSVC C4458: "declaration of 'X' hides class member".

Unreal builds treat shadowing warnings as errors. clang does not warn when a local variable or a
parameter of a member function has the same name as a data member of the class or of a base class,
MSVC does (the classic: `AController* Controller = GetController();` inside a pawn). This walks the
clang AST (libclang, same include setup as check.sh) and reports every such case.

Engine base classes in the mock do not declare all their (private) members, so ENGINE_MEMBERS adds
the names known from the engine.

2. Unity builds: Unreal compiles several .cpp files of a module as ONE translation unit. Names with
internal linkage (anonymous namespace, file-level `static`) that exist in two .cpp files then collide
("redefinition"), and a file-level `using namespace` leaks into the following files.

Usage: ast_check.py <Source/SignalUnknown> <syntax out dir> <mock dir>
"""
import os
import sys

try:
    import clang.cindex as ci
except ImportError:
    print('ast_check: python package "libclang" missing (pip install libclang) - skipped')
    sys.exit(0)

SRC, OUT, MOCK = (os.path.abspath(a) for a in sys.argv[1:4])
ARGS = ['-x', 'c++', '-std=c++20', '-I' + SRC, '-I' + os.path.join(OUT, 'generated'), '-I' + os.path.join(OUT, 'ue'), '-I' + MOCK]


def system_includes():
    """libclang does not know the system include paths: ask the installed clang++ for them"""
    import subprocess
    try:
        res = subprocess.run(['clang++', '-E', '-x', 'c++', '-std=c++20', '-v', os.devnull], capture_output=True, text=True, check=False)
    except OSError:
        return []
    paths, inside = [], False
    for line in res.stderr.splitlines():
        if line.startswith('#include <...> search starts here'):
            inside = True
        elif line.startswith('End of search list'):
            break
        elif inside:
            paths.append('-isystem' + line.strip())
    return paths


ARGS += system_includes()

ENGINE_MEMBERS = {
    'AActor': 'Owner Instigator Role RemoteRole Children RootComponent InputComponent Tags Layers PrimaryActorTick '
              'NetDormancy ParentComponent ReplicatedMovement AttachmentReplication InitialLifeSpan CustomTimeDilation '
              'NetCullDistanceSquared NetUpdateFrequency NetPriority NetTag OwnedComponents ActorHasBegunPlay',
    'APawn': 'Controller PlayerState PreviousController LastHitBy ControlInputVector LastControlInputVector BaseEyeHeight '
             'AIControllerClass AutoPossessAI AutoPossessPlayer RemoteViewPitch',
    'ACharacter': 'Mesh CharacterMovement CapsuleComponent ArrowComponent JumpMaxCount JumpCurrentCount JumpMaxHoldTime '
                  'CrouchedEyeHeight BasedMovement',
    'AController': 'Pawn Character PlayerState ControlRotation StartSpot TransformComponent StateName',
    'APlayerController': 'Player MyHUD PlayerInput PlayerCameraManager NetConnection SpectatorPawn HiddenActors '
                         'AcknowledgedPawn CheatManager InactiveStateInputComponent',
    'AAIController': 'PathFollowingComponent BrainComponent PerceptionComponent Blackboard',
    'AHUD': 'Canvas DebugCanvas PlayerOwner PostRenderedActors DebugDisplay',
    'AGameModeBase': 'GameSession GameState OptionsString DefaultPlayerName',
    'AGameStateBase': 'PlayerArray AuthorityGameMode GameModeClass SpectatorClass',
    'APlayerState': 'Score PlayerId StartTime UniqueId CompressedPing',
    'UActorComponent': 'PrimaryComponentTick ComponentTags OwnerPrivate WorldPrivate AssetUserData',
    'USceneComponent': 'Bounds AttachParent AttachChildren AttachSocketName Mobility PhysicsVolume ComponentToWorld '
                       'RelativeLocation RelativeRotation RelativeScale3D ComponentVelocity',
    'UPrimitiveComponent': 'BodyInstance LightingChannels MoveIgnoreActors MoveIgnoreComponents CustomPrimitiveData',
    'USynthComponent': 'Synth AudioComponent NumChannels PreferredBufferLength SoundClass AttenuationSettings',
    'UGameInstance': 'LocalPlayers WorldContext OnlineSession TimerManager LatentActionManager',
    'UGameUserSettings': 'Version FullscreenMode FrameRateLimit ResolutionSizeX ResolutionSizeY',
}
ENGINE_MEMBERS = {k: set(v.split()) for k, v in ENGINE_MEMBERS.items()}

FUNC_KINDS = {ci.CursorKind.CXX_METHOD, ci.CursorKind.CONSTRUCTOR, ci.CursorKind.DESTRUCTOR}
member_cache = {}


def in_src(cursor):
    f = cursor.location.file
    return f is not None and os.path.abspath(f.name).startswith(SRC + os.sep)


def class_members(record):
    """data member name -> declaring class name, for the record and all of its bases"""
    key = record.get_usr() or record.spelling
    if key in member_cache:
        return member_cache[key]
    out = {}
    member_cache[key] = out
    for name in ENGINE_MEMBERS.get(record.spelling, ()):
        out.setdefault(name, record.spelling)
    for c in record.get_children():
        if c.kind in (ci.CursorKind.FIELD_DECL, ci.CursorKind.VAR_DECL):
            out.setdefault(c.spelling, record.spelling)
        elif c.kind == ci.CursorKind.CXX_BASE_SPECIFIER:
            base = c.type.get_declaration()
            if base is not None and base.kind != ci.CursorKind.NO_DECL_FOUND:
                defn = base.get_definition() or base
                for n, owner in class_members(defn).items():
                    out.setdefault(n, owner)
    return out


def walk_decls(cursor, found):
    for c in cursor.get_children():
        if c.kind in (ci.CursorKind.CLASS_DECL, ci.CursorKind.STRUCT_DECL):
            continue  # local classes have their own scope
        if c.kind in (ci.CursorKind.VAR_DECL, ci.CursorKind.PARM_DECL) and c.spelling:
            found.append(c)
        walk_decls(c, found)


index = ci.Index.create()
problems = set()
internal_names = {}  # name -> set of .cpp files that define it with internal linkage
files = sorted(os.path.join(r, f) for r, _, fs in os.walk(SRC) for f in fs if f.endswith('.cpp'))
for path in files:
    tu = index.parse(path, args=ARGS)
    errs = [d for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error]
    if errs:
        print('ast_check: %s does not parse (%s) - run check.sh first' % (os.path.relpath(path, SRC), errs[0].spelling))
        continue
    # --- unity build: internal names at file scope of this .cpp
    main = os.path.abspath(path)
    for top in tu.cursor.get_children():
        if top.location.file is None or os.path.abspath(top.location.file.name) != main:
            continue
        if top.kind == ci.CursorKind.NAMESPACE and top.spelling == '':
            for c in top.get_children():
                if c.spelling:
                    internal_names.setdefault(c.spelling, set()).add(os.path.relpath(path, SRC))
        elif top.kind in (ci.CursorKind.VAR_DECL, ci.CursorKind.FUNCTION_DECL) and top.storage_class == ci.StorageClass.STATIC:
            internal_names.setdefault(top.spelling, set()).add(os.path.relpath(path, SRC))
        elif top.kind == ci.CursorKind.USING_DIRECTIVE:
            problems.add('%s:%d: file-level "using namespace" leaks into other files in unity builds' % (os.path.relpath(path, SRC), top.location.line))
    for cur in tu.cursor.walk_preorder():
        if cur.kind not in FUNC_KINDS or not cur.is_definition() or not in_src(cur):
            continue
        record = cur.semantic_parent
        if record is None or record.kind not in (ci.CursorKind.CLASS_DECL, ci.CursorKind.STRUCT_DECL):
            continue
        members = class_members(record.get_definition() or record)
        decls = []
        walk_decls(cur, decls)
        for d in decls:
            if d.spelling in members and in_src(d):
                loc = d.location
                problems.add('%s:%d: %s \'%s\' in %s::%s hides member \'%s\' of %s (MSVC C4458, error in Unreal builds)' % (
                    os.path.relpath(loc.file.name, SRC), loc.line, 'parameter' if d.kind == ci.CursorKind.PARM_DECL else 'local',
                    d.spelling, record.spelling, cur.spelling, d.spelling, members[d.spelling]))

for name, where in sorted(internal_names.items()):
    if len(where) > 1:
        problems.add("unity build: '%s' is defined with internal linkage in %s (redefinition when Unreal merges these files)" % (name, ', '.join(sorted(where))))

for p in sorted(problems):
    print('AST ERROR: ' + p)
print('ast check: %d files, %d problems' % (len(files), len(problems)))
sys.exit(1 if problems else 0)
