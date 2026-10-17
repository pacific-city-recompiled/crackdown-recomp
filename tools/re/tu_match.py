# Match functions between two rexglue codegen outputs (disc vs title update) and map guest addresses.
# Usage: OUT=out/tu-probe/tu-map.json python -I tools/re/tu_match.py crackdown/generated/default out/tu-probe/proj/generated/default crackdown/src crackdown/config/midasm_hooks.toml crackdown/config/functions.toml
import sys, os, re, hashlib, collections, bisect, json, difflib
HEX = re.compile(r'0x[0-9a-fA-F]+|-?\b\d+\b')
def parse(d):
    funcs = {}; cur = None
    for fn in sorted(os.listdir(d)):
        if not (fn.startswith('crackdown_recomp') and fn.endswith('.cpp')): continue
        for line in open(os.path.join(d, fn), encoding='utf-8', errors='replace'):
            m = re.match(r'DEFINE_REX_FUNC\(sub_([0-9A-F]{8})\)', line)
            if m: cur = int(m.group(1), 16); funcs[cur] = []; continue
            if cur is not None and line.startswith('\t// '): funcs[cur].append(line[4:].strip())
    return funcs
def calls(v): return [int(m.group(1), 16) for s in v for m in [re.match(r'bl 0x([0-9a-f]+)$', s)] if m]
def k_exact(v): return hashlib.md5('\n'.join(re.sub(r'0x8[0-9a-f]{7}', 'A', s) for s in v).encode()).hexdigest()
def k_shape(v): return hashlib.md5('\n'.join(HEX.sub('#', s) for s in v).encode()).hexdigest()
def k_ops(v): return hashlib.md5(' '.join(s.split(' ')[0] for s in v).encode()).hexdigest()
def data_refs(v):
    # lis rX,hi ; then addi/lwz/stw/lfs... rY,lo(rX) or addi rY,rX,lo -> (index, address)
    hi = {}; out = []
    for i, s in enumerate(v):
        m = re.match(r'lis (r\d+),(-?\d+)$', s)
        if m: hi[m.group(1)] = int(m.group(2)) & 0xFFFF; continue
        m = re.match(r'addi (r\d+),(r\d+),(-?\d+)$', s) or re.match(r'\w+ [rf]?\d+,(-?\d+)\((r\d+)\)$', s)
        if m:
            if s.startswith('addi'): reg, lo = m.group(2), int(m.group(3))
            else: lo, reg = int(m.group(1)), m.group(2)
            if reg in hi:
                a = ((hi[reg] << 16) + lo) & 0xFFFFFFFF
                if 0x82000000 <= a < 0x84000000: out.append((i, a))
    return out
base = parse(sys.argv[1]); tu = parse(sys.argv[2])
mapping = {}; method = {}
def unique_pass(keyf, cand_b, cand_t):
    bk = collections.defaultdict(list); tk = collections.defaultdict(list)
    for a in cand_b: bk[keyf(base[a])].append(a)
    for a in cand_t: tk[keyf(tu[a])].append(a)
    n = 0
    for k, la in bk.items():
        lb = tk.get(k, [])
        if len(la) == 1 and len(lb) == 1: mapping[la[0]] = lb[0]; method[la[0]] = keyf.__name__[2:]; n += 1
    return n
for name, kf in (('exact', k_exact), ('shape', k_shape), ('opcodes', k_ops)):
    used = set(mapping.values())
    n = unique_pass(kf, [a for a in base if a not in mapping], [a for a in tu if a not in used])
    print(f'{name}: +{n} (total {len(mapping)})')
# call-graph propagation: same-length callee lists of matched pairs pair up positionally
for it in range(6):
    used = set(mapping.values()); added = 0
    for a, b in list(mapping.items()):
        ca, cb = calls(base[a]), calls(tu[b])
        if len(ca) != len(cb): continue
        for x, y in zip(ca, cb):
            if x in base and y in tu and x not in mapping and y not in used and len(base[x]) and abs(len(base[x]) - len(tu[y])) <= max(4, len(base[x]) // 10):
                mapping[x] = y; method[x] = 'callgraph'; used.add(y); added += 1
    print(f'callgraph pass {it}: +{added} (total {len(mapping)})')
    if not added: break
# Neighbor + fuzzy pass: an unmatched disc function lies between two matched neighbors, so its update
# counterpart lies between their images; pick the unmatched update function there with the best diff ratio.
def shape(v): return [HEX.sub('#', x) for x in v]
bstarts = sorted(base); tstarts = sorted(tu)
for it in range(3):
    used = set(mapping.values()); added = 0
    for i, a in enumerate(bstarts):
        if a in mapping or len(base[a]) < 4: continue
        lo = next((mapping[bstarts[j]] for j in range(i - 1, max(-1, i - 50), -1) if bstarts[j] in mapping), None)
        hi = next((mapping[bstarts[j]] for j in range(i + 1, min(len(bstarts), i + 50)) if bstarts[j] in mapping), None)
        if lo is None or hi is None or hi <= lo: continue
        cands = [t for t in tstarts[bisect.bisect_right(tstarts, lo):bisect.bisect_left(tstarts, hi)] if t not in used]
        sa = shape(base[a]); best = (0, None)
        for t in cands[:40]:
            sm = difflib.SequenceMatcher(None, sa, shape(tu[t]), autojunk=False)
            if sm.real_quick_ratio() <= best[0] or sm.quick_ratio() <= best[0]: continue
            r = sm.ratio()
            if r > best[0]: best = (r, t)
        if best[1] is not None and best[0] >= 0.6:
            mapping[a] = best[1]; method[a] = f'fuzzy {best[0]:.2f}'; used.add(best[1]); added += 1
    print(f'fuzzy pass {it}: +{added} (total {len(mapping)})')
    if not added: break
print(f'matched {len(mapping)} of {len(base)} disc functions ({100*len(mapping)/len(base):.1f}%)')
def align(a, b):
    sm = difflib.SequenceMatcher(None, shape(base[a]), shape(tu[b]), autojunk=False); m = {}
    for blk in sm.get_matching_blocks():
        for k in range(blk.size): m[blk.a + k] = blk.b + k
    return m
# data address map from aligned lis/addi pairs in matched functions with equal shape
votes = collections.defaultdict(collections.Counter)
for a, b in mapping.items():
    al = {i: i for i in range(len(base[a]))} if len(base[a]) == len(tu[b]) else align(a, b)
    ra, rb = dict(data_refs(base[a])), dict(data_refs(tu[b]))
    for i, x in ra.items():
        if al.get(i) in rb: votes[x][rb[al[i]]] += 1
datamap = {x: c.most_common(1)[0][0] for x, c in votes.items()}
print(f'data addresses mapped: {len(datamap)}')
starts = sorted(base)
addrs = {}
for p in sys.argv[3:]:
    walk = os.walk(p) if os.path.isdir(p) else [(os.path.dirname(p), [], [os.path.basename(p)])]
    for root, _, files in walk:
        for f in files:
            if f.endswith(('.cpp', '.h', '.toml')):
                path = os.path.join(root, f)
                for m in re.finditer(r'0x(8[23][0-9A-Fa-f]{6})\b', open(path, encoding='utf-8', errors='replace').read()):
                    addrs.setdefault(int(m.group(1), 16), set()).add(os.path.relpath(path))
rows = []; stats = collections.Counter()
for a in sorted(addrs):
    i = bisect.bisect_right(starts, a) - 1
    if i >= 0 and a < starts[i] + 4 * len(base[starts[i]]):
        f = starts[i]; off = a - f
        al = (({i: i for i in range(len(base[f]))} if len(base[f]) == len(tu[mapping[f]]) else align(f, mapping[f]))
              if f in mapping else {})
        if f in mapping and off // 4 in al:
            t = mapping[f] + 4 * al[off // 4]
            review = len(base[f]) < 8 or not method[f].startswith(('exact', 'shape'))
            kind = f'code ({method[f]}, {len(base[f])} instr{", review" if review else ""})'
            stats['code mapped, review' if review else 'code mapped'] += 1
        elif f in mapping:
            t = None; kind = f'code (function matched at {mapping[f]:08X}, instruction changed)'; stats['code needs review'] += 1
        else:
            t = None; kind = 'code (function unmatched)'; stats['code unmatched'] += 1
    else:
        t = datamap.get(a); kind = 'data'; stats['data mapped' if t else 'data unmatched'] += 1
    rows.append({'disc': f'{a:08X}', 'update': f'{t:08X}' if t else None, 'kind': kind, 'used_in': sorted(addrs[a])})
print(dict(stats))
json.dump({'functions': {f'{a:08X}': f'{b:08X}' for a, b in sorted(mapping.items())}, 'hooks': rows}, open(os.environ.get('OUT', 'tu-map.json'), 'w'), indent=1)
