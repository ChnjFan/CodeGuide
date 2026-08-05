import struct, sys, collections

TYPE_NAMES = {0:"F32",1:"F16",2:"Q4_0",3:"Q4_1",6:"Q5_0",7:"Q5_1",8:"Q8_0",9:"Q8_1",
10:"Q2_K",11:"Q3_K",12:"Q4_K",13:"Q5_K",14:"Q6_K",15:"Q8_K",16:"IQ2_XXS",17:"IQ2_XS",
18:"IQ3_XXS",19:"IQ1_S",20:"IQ4_NL",21:"IQ3_S",22:"IQ2_S",23:"IQ4_XS",24:"I8",25:"I16",
26:"I32",27:"I64",28:"F64",29:"IQ1_M",30:"BF16",34:"TQ1_0",35:"TQ2_0",39:"MXFP4",40:"NVFP4",41:"Q1_0",42:"Q2_0"}

def rstr(f):
    n = struct.unpack("<Q", f.read(8))[0]
    return f.read(n).decode("utf-8", "replace")

def skip_value(f, vtype):
    if vtype in (0,1,7): f.read(1)
    elif vtype in (2,3): f.read(2)
    elif vtype in (4,5,6): f.read(4)
    elif vtype in (10,11,12): f.read(8)
    elif vtype == 8: rstr(f)
    elif vtype == 9:
        et, cnt = struct.unpack("<IQ", f.read(12))
        for _ in range(cnt): skip_value(f, et)
    else: raise ValueError(f"unknown vtype {vtype}")

path = sys.argv[1]
want_meta_prefix = sys.argv[2] if len(sys.argv) > 2 else None
with open(path, "rb") as f:
    magic = f.read(4)
    assert magic == b"GGUF", magic
    ver, ntensor, nkv = struct.unpack("<IQQ", f.read(20))
    print(f"version={ver} n_tensors={ntensor} n_kv={nkv}")
    meta = {}
    for _ in range(nkv):
        key = rstr(f)
        vtype = struct.unpack("<I", f.read(4))[0]
        if want_meta_prefix and key.startswith(want_meta_prefix):
            if vtype == 8:
                print(f"KV {key} = {rstr(f)[:400]}")
                continue
            if vtype == 4:
                print(f"KV {key} = {struct.unpack('<I', f.read(4))[0]}")
                continue
        skip_value(f, vtype)
    dist = collections.Counter()
    bytes_by_type = collections.Counter()
    for _ in range(ntensor):
        name = rstr(f)
        ndim = struct.unpack("<I", f.read(4))[0]
        dims = struct.unpack(f"<{ndim}Q", f.read(8*ndim))
        t = struct.unpack("<I", f.read(4))[0]
        off = struct.unpack("<Q", f.read(8))[0]
        dist[TYPE_NAMES.get(t, str(t))] += 1
        nelem = 1
        for d in dims: nelem *= d
        bytes_by_type[TYPE_NAMES.get(t, str(t))] += nelem  # approx via elements
    print("--- tensor type distribution (count):")
    for k, v in dist.most_common():
        print(f"  {k}: {v}")
