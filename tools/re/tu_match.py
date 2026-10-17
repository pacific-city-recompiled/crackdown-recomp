# Match functions between two rexglue codegen outputs (disc vs title update) and map guest addresses.
# Usage: OUT=out/tu-probe/tu-map.json python -I tools/re/tu_match.py crackdown/generated/default out/tu-probe/proj/generated/default crackdown/src crackdown/config/midasm_hooks.toml crackdown/config/functions.toml
import sys, os, re, hashlib, collections, bisect, json
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
mapping = {}
def unique_pass(keyf, cand_b, cand_t):
    bk = collections.defaultdict(list); tk = collections.defaultdict(list)
    for a in cand_b: bk[keyf(base[a])].append(a)
    for a in cand_t: tk[keyf(tu[a])].append(a)
    n = 0
    for k, la in bk.items():
        lb = tk.get(k, [])
        if len(la) == 1 and len(lb) == 1: mapping[la[0]] = lb[0]; n += 1
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
                mapping[x] = y; used.add(y); added += 1
    print(f'callgraph pass {it}: +{added} (total {len(mapping)})')
    if not added: break
print(f'matched {len(mapping)} of {len(base)} disc functions ({100*len(mapping)/len(base):.1f}%)')
# data address map from aligned lis/addi pairs in matched functions with equal shape
votes = collections.defaultdict(collections.Counter)
for a, b in mapping.items():
    if len(base[a]) != len(tu[b]): continue
    ra, rb = dict(data_refs(base[a])), dict(data_refs(tu[b]))
    for i, x in ra.items():
        if i in rb: votes[x][rb[i]] += 1
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
        if f in mapping and len(base[f]) == len(tu[mapping[f]]):
            t = mapping[f] + off; kind = 'code'; stats['code mapped'] += 1
        elif f in mapping:
            t = None; kind = f'code (function matched at {mapping[f]:08X}, length differs)'; stats['code needs review'] += 1
        else:
            t = None; kind = 'code (function unmatched)'; stats['code unmatched'] += 1
    else:
        t = datamap.get(a); kind = 'data'; stats['data mapped' if t else 'data unmatched'] += 1
    rows.append({'disc': f'{a:08X}', 'update': f'{t:08X}' if t else None, 'kind': kind, 'used_in': sorted(addrs[a])})
print(dict(stats))
json.dump({'functions': {f'{a:08X}': f'{b:08X}' for a, b in sorted(mapping.items())}, 'hooks': rows}, open(os.environ.get('OUT', 'tu-map.json'), 'w'), indent=1)
