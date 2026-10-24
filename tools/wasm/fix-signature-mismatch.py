#!/usr/bin/env python3
"""fix-signature-mismatch.py: give wasm-ld's signature-mismatch stubs a body.

The period code calls functions with other argument counts and return types
than they were defined with (MSC's cdecl allowed it; x86 tolerates it). For
each such call wasm-ld links in a stub, "signature_mismatch:NAME", that traps.
This rewrites every stub into an adapter: it passes the caller's arguments on
(dropping extras, adding zeros for missing ones, converting i32/i64), calls
the real NAME, and adjusts the result the same way. Missing arguments were
stack garbage on DOS; zero stands in for them.

Runs on wasm-ld's output, before Binaryen (Asyncify sees real calls).
Usage: fix-signature-mismatch.py MODULE.wasm
"""
import sys

I32, I64, F32, F64 = 0x7F, 0x7E, 0x7D, 0x7C
ZERO = {I32: b"\x41\x00", I64: b"\x42\x00", F32: b"\x43" + bytes(4), F64: b"\x44" + bytes(8)}
CONVERT = {(I32, I64): b"\xAC", (I64, I32): b"\xA7"}   # i64.extend_i32_s, i32.wrap_i64


def uleb(data, pos):
    result = shift = 0
    while True:
        b = data[pos]
        pos += 1
        result |= (b & 0x7F) << shift
        shift += 7
        if not b & 0x80:
            return result, pos


def enc_uleb(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        out.append(b | (0x80 if n else 0))
        if not n:
            return bytes(out)


def name_at(data, pos):
    n, pos = uleb(data, pos)
    return data[pos:pos + n].decode("utf-8", "replace"), pos + n


def sections(data):
    pos = 8
    while pos < len(data):
        sid = data[pos]
        size, body = uleb(data, pos + 1)
        yield sid, pos, body, body + size
        pos = body + size


def parse_types(data, pos):
    count, pos = uleb(data, pos)
    types = []
    for _ in range(count):
        assert data[pos] == 0x60, "only function types are expected"
        pos += 1
        n, pos = uleb(data, pos)
        params = list(data[pos:pos + n])
        pos += n
        n, pos = uleb(data, pos)
        results = list(data[pos:pos + n])
        pos += n
        types.append((params, results))
    return types


def imported_funcs(data, pos):
    count, pos = uleb(data, pos)
    funcs = []
    for _ in range(count):
        _, pos = name_at(data, pos)
        _, pos = name_at(data, pos)
        kind = data[pos]
        pos += 1
        if kind == 0:                              # function: type index
            t, pos = uleb(data, pos)
            funcs.append(t)
        elif kind == 1:                            # table: reftype, limits
            pos += 1
            flags, pos = uleb(data, pos)
            _, pos = uleb(data, pos)
            if flags & 1:
                _, pos = uleb(data, pos)
        elif kind == 2:                            # memory: limits
            flags, pos = uleb(data, pos)
            _, pos = uleb(data, pos)
            if flags & 1:
                _, pos = uleb(data, pos)
        elif kind == 3:                            # global: valtype, mut
            pos += 2
        elif kind == 4:                            # tag: attribute, type
            pos += 1
            _, pos = uleb(data, pos)
        else:
            raise SystemExit(f"unknown import kind {kind}")
    return funcs


def function_names(data, pos, end):
    pos += 0
    _, pos = name_at(data, pos)                    # "name"
    names = {}
    while pos < end:
        sub = data[pos]
        size, body = uleb(data, pos + 1)
        if sub == 1:
            count, p = uleb(data, body)
            for _ in range(count):
                idx, p = uleb(data, p)
                names[idx], p = name_at(data, p)
        pos = body + size
    return names


def adapter(caller, callee, target):
    (cp, cr), (tp, tr) = caller, callee
    code = bytearray(b"\x00")                      # no locals
    for i, t in enumerate(tp):
        if i < len(cp):
            code += b"\x20" + enc_uleb(i)          # local.get i
            if cp[i] != t:
                if (cp[i], t) not in CONVERT:
                    code += b"\x1A" + ZERO[t]      # drop, use zero
                else:
                    code += CONVERT[(cp[i], t)]
        else:
            code += ZERO[t]
    code += b"\x10" + enc_uleb(target)             # call
    if tr and not cr:
        code += b"\x1A"
    elif cr and not tr:
        code += ZERO[cr[0]]
    elif cr and tr and cr[0] != tr[0]:
        code += CONVERT.get((tr[0], cr[0]), b"\x1A" + ZERO[cr[0]])
    code += b"\x0B"
    return enc_uleb(len(code)) + bytes(code)


def main(path):
    data = bytearray(open(path, "rb").read())
    types, imports, func_types, names, code_sec = [], [], [], {}, None
    for sid, start, body, end in sections(data):
        if sid == 1:
            types = parse_types(data, body)
        elif sid == 2:
            imports = imported_funcs(data, body)
        elif sid == 3:
            count, p = uleb(data, body)
            for _ in range(count):
                t, p = uleb(data, p)
                func_types.append(t)
        elif sid == 10:
            code_sec = (start, body, end)
        elif sid == 0 and name_at(data, body)[0] == "name":
            names = function_names(data, body, end)
    if not names:
        raise SystemExit(f"{path}: no name section; cannot find signature-mismatch stubs")
    all_types = imports + func_types
    by_name = {}
    for idx, n in names.items():
        if not n.startswith("signature_mismatch:"):
            by_name.setdefault(n, idx)

    start, body, end = code_sec
    count, p = uleb(data, body)
    bodies = []
    for _ in range(count):
        size, q = uleb(data, p)
        bodies.append(bytes(data[p:q + size]))
        p = q + size
    fixed = 0
    for idx, n in names.items():
        if not n.startswith("signature_mismatch:") or idx < len(imports):
            continue
        target = by_name.get(n.split(":", 1)[1])
        if target is None:
            raise SystemExit(f"{path}: no function for stub {n}")
        bodies[idx - len(imports)] = adapter(types[all_types[idx]], types[all_types[target]], target)
        fixed += 1
    payload = enc_uleb(count) + b"".join(bodies)
    section = b"\x0A" + enc_uleb(len(payload)) + payload
    data[start:end] = section
    open(path, "wb").write(data)
    print(f"fix-signature-mismatch: {fixed} stub(s) adapted in {path}", file=sys.stderr)


if __name__ == "__main__":
    main(sys.argv[1])
