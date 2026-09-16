#!/usr/bin/env python3
"""Rank address-independent opcode similarities; candidates are NOT hook addresses."""
import argparse
import difflib
import json
import struct
from pathlib import Path

def functions(report, exe):
    report=json.loads(report.read_text()); data=exe.read_bytes()[2048:]; result={}
    for f in report['functions']:
        if f['is_data']:continue
        start=f['addr']-report['load_address']; size=f['end']-f['addr']
        words=struct.unpack_from(f'<{size//4}I', data, start)
        # Preserve registers and SPECIAL/GTE operations; discard relocated addresses.
        words=tuple(w if w>>26 in (0,18) else w&0xfc000000 if w>>26 in (2,3) else w&0xffff0000 for w in words)
        result[f['addr']]=(f,words)
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference',type=Path,required=True);args=p.parse_args()
    root=Path(__file__).resolve().parents[1]
    old=functions(root/'diagnostics/reference-analysis/analysis.json',args.reference/'disc/SCUS-943.00')
    new=functions(root/'diagnostics/main-analysis/analysis.json',root/'disc/files/RIDGE.EXE')
    candidates={}
    for addr in [0x800143cc,0x800195b0,0x8001bd80,0x80020bb0,0x8002b520,0x8002e0bc,0x80041538,0x800348e8,0x80034f78,0x80035f28]:
        if addr not in old:continue
        f,a=old[addr]; matches=[]
        for target,(g,b) in new.items():
            if min(len(a),len(b))/max(len(a),len(b))<.4:continue
            matcher=difflib.SequenceMatcher(None,a,b,autojunk=False)
            if matcher.quick_ratio()<.45:continue
            matches.append((round(matcher.ratio(),3),f'{target:08X}',g['size']))
        candidates[f'{addr:08X}']=sorted(matches,reverse=True)[:5]
    print(json.dumps(candidates,indent=2))
if __name__=='__main__':main()
