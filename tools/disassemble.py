#!/usr/bin/env python3
"""Print original executable instructions for local reverse engineering."""
import argparse,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'psxrecomp/tools'))
from disasm_helper import dis
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('address',type=lambda s:int(s,16));p.add_argument('count',type=int,nargs='?',default=64);a=p.parse_args();data=(ROOT/'disc/files/RIDGE.EXE').read_bytes()[2048:]
 for address in range(a.address,a.address+4*a.count,4):
  word=struct.unpack_from('<I',data,address-0x80010000)[0];print(f'{address:08X} {word:08X} {dis(address,word)}')
