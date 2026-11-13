#!/usr/bin/env python3
"""AST checks for errors that only the real Unreal/MSVC build reports (needs `pip install libclang`).

1. MSVC C4458: "declaration of 'X' hides class member".
   Also for the parameters of UFUNCTION declarations: UnrealHeaderTool generates functions with the same
   parameter names inside the class (first seen in the real build: SUCharacter.gen.cpp, 'bInHandbrake').

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
              'NetCullDistanceSquared NetUpdateFrequency MinNetUpdateFrequency NetPriority NetTag OwnedComponents ActorHasBegunPlay '
              'bNetTemporary bNetStartup bOnlyRelevantToOwner bAlwaysRelevant bReplicateMovement bCallPreReplication bHidden bTearOff '
              'bForceNetAddressable bExchangedRoles bNetLoadOnClient bNetUseOwnerRelevancy bRelevantForNetworkReplays '
              'bRelevantForLevelBounds bReplayRewindable bAllowTickBeforeBeginPlay bAutoDestroyWhenFinished bCanBeDamaged bBlockInput '
              'bCollideWhenPlacing bFindCameraComponentWhenViewTarget bGenerateOverlapEventsDuringLevelStreaming bIgnoresOriginShifting '
              'bEnableAutoLODGeneration bIsEditorOnlyActor bActorSeamlessTraveled bReplicates bCanBeInCluster bActorInitialized '
              'bActorEnableCollision bActorIsBeingDestroyed bHasFinishedSpawning bActorIsBeingConstructed bAsyncPhysicsTickEnabled '
              'NetDriverName SpawnCollisionHandlingMethod AutoReceiveInput InputPriority LastRenderTime RayTracingGroupId '
              'OnTakeAnyDamage OnTakePointDamage OnTakeRadialDamage OnActorBeginOverlap OnActorEndOverlap OnBeginCursorOver '
              'OnEndCursorOver OnClicked OnReleased OnActorHit OnDestroyed OnEndPlay ReplicatedComponents InstanceComponents '
              'BlueprintCreatedComponents ActorLabel FolderPath bEditable bHiddenEd PivotOffset SpriteScale ActorGuid HLODLayer',
    'APawn': 'Controller PlayerState PreviousController LastHitBy ControlInputVector LastControlInputVector BaseEyeHeight '
             'AIControllerClass AutoPossessAI AutoPossessPlayer RemoteViewPitch bUseControllerRotationPitch bUseControllerRotationYaw '
             'bUseControllerRotationRoll bCanAffectNavigationGeneration bInputEnabled bProcessingOutsideWorldBounds '
             'OverrideInputComponentClass ReceiveControllerChangedDelegate',
    'ACharacter': 'Mesh CharacterMovement CapsuleComponent ArrowComponent JumpMaxCount JumpCurrentCount JumpCurrentCountPreJump '
                  'JumpMaxHoldTime JumpKeyHoldTime JumpForceTimeRemaining CrouchedEyeHeight BasedMovement ReplicatedBasedMovement '
                  'AnimRootMotionTranslationScale BaseTranslationOffset BaseRotationOffset ReplicatedMovementMode bInBaseReplication '
                  'bIsCrouched bProxyIsJumpForceApplied bPressedJump bClientUpdating bClientWasFalling bSimGravityDisabled bWasJumping '
                  'NumActorOverlapEventsCounter OnReachedJumpApex LandedDelegate MovementModeChangedDelegate OnCharacterMovementUpdated '
                  'SavedRootMotion ClientRootMotionParams RootMotionRepMoves RepRootMotion',
    'AController': 'Pawn Character PlayerState ControlRotation StartSpot TransformComponent StateName OnInstigatedAnyDamage '
                   'OnPossessedPawnChanged bAttachToPawn bIsPlayerController IgnoreMoveInput IgnoreLookInput',
    'APlayerController': 'Player MyHUD PlayerInput PlayerCameraManager PlayerCameraManagerClass NetConnection SpectatorPawn HiddenActors '
                         'HiddenPrimitiveComponents AcknowledgedPawn CheatManager CheatClass InactiveStateInputComponent '
                         'bAutoManageActiveCameraTarget TargetViewRotation SmoothTargetViewRotationSpeed bPlayerIsWaiting NetPlayerIndex '
                         'PendingSwapConnection RotationInput bShowMouseCursor bEnableClickEvents bEnableTouchEvents bEnableMouseOverEvents '
                         'bEnableTouchOverEvents bForceFeedbackEnabled bEnableMotionControls ForceFeedbackScale ClickEventKeys '
                         'DefaultMouseCursor CurrentMouseCursor DefaultClickTraceChannel CurrentClickTraceChannel HitResultTraceDistance '
                         'SeamlessTravelCount bShouldPerformFullTickWhenPaused CurrentTouchInterface bIsLocalPlayerController SpawnLocation '
                         'ClientCap ActiveForceFeedbackEffects',
    'AAIController': 'PathFollowingComponent BrainComponent PerceptionComponent Blackboard ActionsComp bStartAILogicOnPossess '
                     'bStopAILogicOnUnposses bLOSflag bSkipExtraLOSChecks bAllowStrafe bWantsPlayerState bSetControlRotationFromPawnOrientation',
    'AHUD': 'Canvas DebugCanvas PlayerOwner PostRenderedActors DebugDisplay bLostFocusPaused bShowHUD bShowDebugInfo CurrentTargetIndex '
            'bShowHitBoxDebugInfo bShowOverlays bEnableDebugTextShadow LastHUDRenderTime RenderDelta ToggledDebugCategories DebugTextList '
            'ShowDebugTargetDesiredClass ShowDebugTargetActor HitBoxMap HitBoxesOver HitBoxHits',
    'AGameModeBase': 'GameSession GameState OptionsString DefaultPlayerName GameSessionClass GameStateClass PlayerControllerClass '
                     'PlayerStateClass HUDClass DefaultPawnClass SpectatorClass ReplaySpectatorPlayerControllerClass ServerStatReplicatorClass '
                     'ServerStatReplicator bUseSeamlessTravel bStartPlayersAsSpectators bPauseable Pausers',
    'AGameStateBase': 'PlayerArray AuthorityGameMode GameModeClass SpectatorClass bReplicatedHasBegunPlay ReplicatedWorldTimeSeconds '
                      'ServerWorldTimeSecondsDelta ServerWorldTimeSecondsUpdateFrequency',
    'APlayerState': 'Score PlayerId StartTime UniqueId CompressedPing bShouldUpdateReplicatedPing bIsSpectator bOnlySpectator bIsABot '
                    'bHasBeenWelcomed bIsInactive bFromPreviousLevel bUseCustomPlayerNames EngineMessageClass ExactPing SavedNetworkAddress '
                    'OnPawnSet PawnPrivate PlayerNamePrivate OldNamePrivate SessionName',
    'UActorComponent': 'PrimaryComponentTick ComponentTags OwnerPrivate WorldPrivate AssetUserData UCSSerializationIndex bRegistered '
                       'bRenderStateCreated bPhysicsStateCreated bNetAddressable bReplicates bRenderStateDirty bRenderTransformDirty '
                       'bRenderDynamicDataDirty bAutoRegister bAllowReregistration bTickInEditor bNeverNeedsRenderUpdate bAllowConcurrentTick '
                       'bAllowAnyoneToDestroyMe bAutoActivate bIsActive bEditableWhenInherited bNavigationRelevant bCanEverAffectNavigation '
                       'bWantsInitializeComponent bIsEditorOnly bHasBeenCreated bHasBeenInitialized bIsReadyForReplication bHasBegunPlay '
                       'bIsBeingDestroyed bTickFunctionsRegistered bIsNetStartupComponent CreationMethod OnComponentActivated '
                       'OnComponentDeactivated',
    'USceneComponent': 'Bounds AttachParent AttachChildren ClientAttachedChildren AttachSocketName Mobility DetailMode PhysicsVolume '
                       'ComponentToWorld RelativeLocation RelativeRotation RelativeScale3D ComponentVelocity bComponentToWorldUpdated '
                       'bSkipUpdateOverlaps bAbsoluteLocation bAbsoluteRotation bAbsoluteScale bVisible bShouldBeAttached '
                       'bShouldSnapLocationWhenAttached bShouldSnapRotationWhenAttached bShouldSnapScaleWhenAttached '
                       'bShouldUpdatePhysicsVolume bHiddenInGame bBoundsChangeTriggersStreamingDataRebuild bUseAttachParentBound '
                       'bComputeFastLocalBounds bComputeBoundsOnceForGame bIsNotRenderAttachmentRoot bWantsOnUpdateTransform '
                       'bNetUpdateTransform bNetUpdateAttachment bVisualizeComponent PhysicsVolumeChangedDelegate TransformUpdated '
                       'WorldRotationCache RelativeRotationCache ScopedMovementStack',
    'UPrimitiveComponent': 'BodyInstance LightingChannels MoveIgnoreActors MoveIgnoreComponents CustomPrimitiveData MinDrawDistance '
                           'LDMaxDrawDistance CachedMaxDrawDistance DepthPriorityGroup ViewOwnerDepthPriorityGroup '
                           'IndirectLightingCacheQuality LightmapType bUseMaxLODAsImposter bNeverDistanceCull bAlwaysCreatePhysicsState '
                           'bGenerateOverlapEvents bMultiBodyOverlap bTraceComplexOnMove bReturnMaterialOnMove bAllowCullDistanceVolume '
                           'bVisibleInReflectionCaptures bVisibleInRealTimeSkyCaptures bVisibleInRayTracing bRenderInMainPass '
                           'bRenderInDepthPass bReceivesDecals bOwnerNoSee bOnlyOwnerSee bTreatAsBackgroundForOcclusion bUseAsOccluder '
                           'bSelectable bForceMipStreaming CastShadow bEmissiveLightSource bAffectDynamicIndirectLighting '
                           'bAffectDistanceFieldLighting bCastDynamicShadow bCastStaticShadow bCastVolumetricTranslucentShadow '
                           'bCastContactShadow bSelfShadowOnly bCastFarShadow bCastInsetShadow bCastCinematicShadow bCastHiddenShadow '
                           'bCastShadowAsTwoSided bLightAttachmentsAsGroup bIgnoreRadialImpulse bIgnoreRadialForce bApplyImpulseOnDamage '
                           'bReplicatePhysicsToAutonomousProxy AlwaysLoadOnClient AlwaysLoadOnServer bRenderCustomDepth '
                           'bVisibleInSceneCaptureOnly bHiddenInSceneCapture bHasCustomNavigableGeometry CanCharacterStepUpOn '
                           'RayTracingGroupId VisibilityId CustomDepthStencilValue CustomDepthStencilWriteMask TranslucencySortPriority '
                           'TranslucencySortDistanceOffset RuntimeVirtualTextures BoundsScale OnComponentHit OnComponentBeginOverlap '
                           'OnComponentEndOverlap OnComponentWake OnComponentSleep OnBeginCursorOver OnEndCursorOver OnClicked OnReleased '
                           'LODParentPrimitive SceneProxy OverlappingComponents',
    'UMeshComponent': 'OverrideMaterials OverlayMaterial OverlayMaterialMaxDrawDistance',
    'UProceduralMeshComponent': 'bUseComplexAsSimpleCollision bUseAsyncCooking ProcMeshBodySetup ProcMeshSections CollisionConvexElems '
                                'LocalBounds AsyncBodySetupQueue',
    'UStaticMeshComponent': 'ForcedLodModel MinLOD StaticMesh WireframeColorOverride bOverrideMinLOD bUseDefaultCollision '
                            'bReverseCulling OverriddenLightMapRes StreamingDistanceMultiplier LODData LightmassSettings',
    'USynthComponent': 'Synth AudioComponent NumChannels PreferredBufferLength SoundClass AttenuationSettings bAutoDestroy '
                       'bStopWhenOwnerDestroyed bAllowSpatialization bOverrideAttenuation bEnableBusSends bEnableBaseSubmix '
                       'bEnableSubmixSends AttenuationOverrides ConcurrencySettings ConcurrencySet SoundSubmix SoundSubmixSends '
                       'SourceEffectChain BusSends PreEffectBusSends bIsUISound bIsPreviewSound EnvelopeFollowerAttackTime '
                       'EnvelopeFollowerReleaseTime OnAudioEnvelopeValue OnAudioEnvelopeValueNative bIsSynthPlaying bIsInitialized',
    'UGameInstance': 'LocalPlayers WorldContext OnlineSession ReferencedObjects TimerManager LatentActionManager PIEMapName PIEStartTime '
                     'SubsystemCollection',
    'UGameUserSettings': 'Version FullscreenMode LastConfirmedFullscreenMode PreferredFullscreenMode FrameRateLimit ResolutionSizeX '
                         'ResolutionSizeY LastUserConfirmedResolutionSizeX LastUserConfirmedResolutionSizeY WindowPosX WindowPosY '
                         'bUseVSync bUseDynamicResolution AudioQualityLevel DesiredScreenWidth DesiredScreenHeight bUseDesiredScreenHeight '
                         'ScalabilityQuality bUseHDRDisplayOutput HDRDisplayOutputNits LastCPUBenchmarkResult LastGPUBenchmarkResult',
    'UDeveloperSettings': 'CategoryName SectionName',
    'UObject': 'ObjectFlags InternalIndex ClassPrivate NamePrivate OuterPrivate',
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


line_cache = {}


def is_ufunction(cursor):
    """True if the declaration is preceded by a UFUNCTION(...) line (the macro is empty in the mock, so look at the source)"""
    f = cursor.location.file
    if f is None:
        return False
    lines = line_cache.get(f.name)
    if lines is None:
        with open(f.name, encoding='utf-8-sig', errors='replace') as fh:
            lines = fh.read().splitlines()
        line_cache[f.name] = lines
    i = cursor.extent.start.line - 1
    if 'UFUNCTION' in lines[i]:
        return True
    i -= 1
    while i >= 0 and (not lines[i].strip() or lines[i].strip().startswith('//')):
        i -= 1
    return i >= 0 and lines[i].strip().startswith('UFUNCTION')


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
        if cur.kind not in FUNC_KINDS or not in_src(cur):
            continue
        if not cur.is_definition():
            # declaration only: for UFUNCTIONs (RPCs, events) UnrealHeaderTool generates functions with the same parameter
            # names inside the class, so a parameter named like a member fails there (C4458 in the .gen.cpp file)
            rec = cur.semantic_parent
            if rec is not None and rec.kind in (ci.CursorKind.CLASS_DECL, ci.CursorKind.STRUCT_DECL) and is_ufunction(cur):
                mem = class_members(rec.get_definition() or rec)
                for prm in cur.get_children():
                    if prm.kind == ci.CursorKind.PARM_DECL and prm.spelling in mem and in_src(prm):
                        problems.add("%s:%d: parameter '%s' of the declaration %s::%s is named like member '%s' of %s (UFUNCTION: C4458 in the generated .gen.cpp, error in Unreal builds)" % (
                            os.path.relpath(prm.location.file.name, SRC), prm.location.line, prm.spelling, rec.spelling, cur.spelling, prm.spelling, mem[prm.spelling]))
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
