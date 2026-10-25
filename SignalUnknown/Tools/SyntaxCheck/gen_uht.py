#!/usr/bin/env python3
"""Mini "UHT" for the syntax check.

Generates <Name>.generated.h files for every project header with a GENERATED_BODY() so that
`clang -fsyntax-only` can parse the code against the mock engine headers. Like the real Unreal
Header Tool it declares Super/ThisClass/StaticClass() and the _Implementation/_Validate functions
of RPCs and BlueprintNativeEvents.

It also reports a few rules the real UHT / engine enforce (printed as errors, exit code 1):
  * RPCs must return void, have no non-const reference parameters and are not overloaded
  * UFUNCTIONs must not be overloaded
  * ReplicatedUsing=OnRep_X requires a UFUNCTION() void OnRep_X declared in the class
  * UPROPERTY(Replicated...) members must appear in DOREPLIFETIME in the class' .cpp
  * UPROPERTY members must not use plain 'int' / 'unsigned' / 'long' types
  * UENUM enum class used by UPROPERTY must be ': uint8'

This is NOT the real UHT and cannot prove that the real UHT accepts the code.
"""
import os
import re
import sys

SRC, OUT = sys.argv[1], sys.argv[2]
GEN = os.path.join(OUT, 'generated')
os.makedirs(GEN, exist_ok=True)
errors = []


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), text, flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    return text


def match_paren(text, start):
    """text[start] == '(' -> index of the matching ')'"""
    depth = 0
    for i in range(start, len(text)):
        c = text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i
    return -1


def match_brace(text, start):
    depth = 0
    for i in range(start, len(text)):
        c = text[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return i
    return len(text)


def split_params(params):
    out, depth, cur = [], 0, ''
    for c in params:
        if c in '<([':
            depth += 1
        elif c in '>)]':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


def strip_default(p):
    depth = 0
    for i, c in enumerate(p):
        if c in '<([':
            depth += 1
        elif c in '>)]':
            depth -= 1
        elif c == '=' and depth == 0:
            return p[:i].strip()
    return p


CLASS_RE = re.compile(r'\b(UCLASS|USTRUCT|UINTERFACE)\s*\(')
DECL_RE = re.compile(r'\b(class|struct)\s+(?:[A-Z_]+_API\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:final\s*)?(?::\s*([^{]*))?\{')

all_classes = {}  # name -> info (for cross checks)
headers = []
for root, _, files in os.walk(SRC):
    for f in files:
        if f.endswith('.h'):
            headers.append(os.path.join(root, f))

uenums = {}
for path in sorted(headers):
    raw = open(path, encoding='utf-8-sig').read()
    text = strip_comments(raw)
    for m in re.finditer(r'UENUM\s*\([^)]*\)\s*enum\s+class\s+([A-Za-z0-9_]+)\s*(:\s*([A-Za-z0-9_]+))?', text):
        uenums[m.group(1)] = m.group(3)

for path in sorted(headers):
    raw = open(path, encoding='utf-8-sig').read()
    text = strip_comments(raw)
    base = os.path.splitext(os.path.basename(path))[0]
    if '%s.generated.h' % base not in raw:
        continue
    file_id = 'FID_' + re.sub(r'[^A-Za-z0-9]', '_', os.path.relpath(path, SRC))
    lines = []
    lines.append('#undef CURRENT_FILE_ID')
    lines.append('#define CURRENT_FILE_ID %s' % file_id)

    # every GENERATED_BODY with its enclosing declaration
    for gm in re.finditer(r'GENERATED_(?:U?CLASS_|USTRUCT_)?BODY\s*\(\s*\)', text):
        line_no = text.count('\n', 0, gm.start()) + 1
        # nearest class/struct declaration before it
        decl = None
        for dm in DECL_RE.finditer(text, 0, gm.start()):
            body_end = match_brace(text, dm.end() - 1)
            if body_end > gm.start():
                decl = dm
        if not decl:
            errors.append('%s:%d: GENERATED_BODY outside of a class' % (path, line_no))
            continue
        kind_kw, name, bases = decl.group(1), decl.group(2), (decl.group(3) or '').strip()
        prefix = text[max(0, decl.start() - 400):decl.start()]
        macro = None
        mm = list(CLASS_RE.finditer(prefix))
        if mm:
            close = match_paren(prefix, mm[-1].end() - 1)
            if close >= 0 and prefix[close + 1:].strip() == '':
                macro = mm[-1].group(1)
        first_base = None
        if bases:
            b0 = split_params(bases)[0]
            first_base = re.sub(r'^(public|protected|private)\s+', '', b0).strip()
        body_start = decl.end() - 1
        body_end = match_brace(text, body_start)
        body = text[body_start:body_end]

        decls = []
        if macro == 'USTRUCT' or (kind_kw == 'struct' and macro is None):
            decls.append('public: static UScriptStruct* StaticStruct();')
            if first_base:
                decls.append('typedef %s Super;' % first_base)
            decls.append('typedef %s ThisClass; public:' % name)
        elif macro is None and name.startswith('I'):
            uname = 'U' + name[1:]
            decls.append('protected: virtual ~%s() {} public: typedef %s UClassType; typedef %s ThisClass; private:' % (name, uname, name))
        else:
            decls.append('public: typedef %s Super; typedef %s ThisClass; static UClass* StaticClass();' % (first_base or 'UObject', name))
            if macro == 'UINTERFACE':
                decls.append('%s(const FObjectInitializer& ObjectInitializer = FObjectInitializer());' % name)

            # UFUNCTIONs: RPC / native event declarations + rules
            seen_funcs = {}
            for fm in re.finditer(r'UFUNCTION\s*\(', body):
                close = match_paren(body, fm.end() - 1)
                spec = body[fm.end():close]
                rest = body[close + 1:]
                semi = rest.find(';')
                brace = rest.find('{')
                end = semi if (brace < 0 or (semi >= 0 and semi < brace)) else brace
                sig = ' '.join(rest[:end].split())
                sm = re.match(r'(?:virtual\s+|static\s+)*(.+?)\s*\b([A-Za-z_][A-Za-z0-9_]*)\s*\((.*)\)\s*(const)?\s*(override)?\s*$', sig)
                fline = line_no
                if not sm:
                    errors.append('%s: cannot parse UFUNCTION declaration: %s' % (path, sig))
                    continue
                ret, fname, params, const = sm.group(1).strip(), sm.group(2), sm.group(3), sm.group(4)
                if fname in seen_funcs:
                    errors.append('%s: UFUNCTION %s::%s is overloaded (UHT does not allow this)' % (path, name, fname))
                seen_funcs[fname] = True
                plist = [strip_default(p) for p in split_params(params)]
                is_rpc = re.search(r'\b(Server|Client|NetMulticast)\b', spec) is not None
                native = re.search(r'\bBlueprintNativeEvent\b', spec) is not None
                if is_rpc:
                    if ret != 'void':
                        errors.append('%s: RPC %s::%s must return void' % (path, name, fname))
                    if not re.search(r'\b(Reliable|Unreliable)\b', spec):
                        errors.append('%s: RPC %s::%s needs Reliable or Unreliable' % (path, name, fname))
                    for p in plist:
                        if '&' in p and not p.startswith('const'):
                            errors.append('%s: RPC %s::%s has a non-const reference parameter: %s' % (path, name, fname, p))
                if is_rpc or native:
                    decls.append('virtual %s %s_Implementation(%s)%s;' % (ret, fname, ', '.join(plist), ' const' if const else ''))
                if re.search(r'\bWithValidation\b', spec):
                    decls.append('virtual bool %s_Validate(%s)%s;' % (fname, ', '.join(plist), ' const' if const else ''))

            # UPROPERTY rules
            ufuncs = set(seen_funcs)
            for pm in re.finditer(r'UPROPERTY\s*\(', body):
                close = match_paren(body, pm.end() - 1)
                spec = body[pm.end():close]
                rest = body[close + 1:]
                semi = rest.find(';')
                decl_txt = ' '.join(rest[:semi].split())
                decl_txt = strip_default(decl_txt)
                dm2 = re.match(r'(.+?)\s*\b([A-Za-z_][A-Za-z0-9_]*)\s*(:\s*\d+)?\s*(\[[^\]]*\])?$', decl_txt)
                if not dm2:
                    errors.append('%s: cannot parse UPROPERTY: %s' % (path, decl_txt))
                    continue
                ptype, pname = dm2.group(1).strip(), dm2.group(2)
                if re.search(r'(^|[<\s,])(int|unsigned|long|short)(\s|$|[>,*])', ptype):
                    errors.append('%s: UPROPERTY %s::%s uses a non-UE integer type "%s" (use int32/uint8/...)' % (path, name, pname, ptype))
                for en, under in uenums.items():
                    if re.search(r'\b%s\b' % en, ptype) and under != 'uint8':
                        errors.append('%s: UPROPERTY %s::%s uses UENUM %s which is not ": uint8"' % (path, name, pname, en))
                rm = re.search(r'ReplicatedUsing\s*=\s*([A-Za-z0-9_]+)', spec)
                if rm and rm.group(1) not in ufuncs:
                    errors.append('%s: %s::%s ReplicatedUsing=%s but no UFUNCTION with that name' % (path, name, pname, rm.group(1)))
                if re.search(r'\bReplicated(Using)?\b', spec):
                    cpp = os.path.splitext(path)[0] + '.cpp'
                    found = False
                    if os.path.exists(cpp):
                        ctext = open(cpp, encoding='utf-8-sig').read()
                        found = re.search(r'DOREPLIFETIME\w*\s*\(\s*%s\s*,\s*%s\s*[,)]' % (name, pname), ctext) is not None
                    if not found:
                        errors.append('%s: replicated property %s::%s is not registered with DOREPLIFETIME' % (path, name, pname))
            # replicated properties need GetLifetimeReplicatedProps
            if re.search(r'UPROPERTY\s*\([^)]*\bReplicated', body) and 'GetLifetimeReplicatedProps' not in body:
                errors.append('%s: %s has replicated properties but does not override GetLifetimeReplicatedProps' % (path, name))
            decls.append('private:')

        lines.append('#define %s_%d_GENERATED_BODY %s' % (file_id, line_no, ' '.join(decls)))

    with open(os.path.join(GEN, base + '.generated.h'), 'w') as fh:
        fh.write('\n'.join(lines) + '\n')

# AddDynamic handlers must be UFUNCTIONs
for root, _, files in os.walk(SRC):
    for f in files:
        if not f.endswith('.cpp'):
            continue
        cpp = os.path.join(root, f)
        ctext = strip_comments(open(cpp, encoding='utf-8-sig').read())
        for am in re.finditer(r'(AddDynamic|AddUniqueDynamic|BindDynamic)\s*\(\s*this\s*,\s*&\s*([A-Za-z0-9_]+)::([A-Za-z0-9_]+)\s*\)', ctext):
            cls, fn = am.group(2), am.group(3)
            hdr = os.path.splitext(cpp)[0] + '.h'
            ok = False
            if os.path.exists(hdr):
                htext = strip_comments(open(hdr, encoding='utf-8-sig').read())
                ok = re.search(r'UFUNCTION\s*\([^)]*\)\s*(virtual\s+)?void\s+%s\s*\(' % fn, htext) is not None
            if not ok:
                errors.append('%s: %s::%s is bound with %s but is not declared as UFUNCTION() in %s' % (cpp, cls, fn, am.group(1), os.path.basename(hdr)))

for e in errors:
    print('UHT-CHECK ERROR: ' + e)
print('gen_uht: %d generated headers, %d rule violations' % (len(os.listdir(GEN)), len(errors)))
sys.exit(1 if errors else 0)
