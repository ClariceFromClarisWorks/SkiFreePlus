"""Package relocatable generated blitters as selectable, nonresident resources."""
from pathlib import Path
import re
b=Path(__file__).resolve().parent
for prefix,src,out in [('', 'assets.h','bank_assets.h')]:
 s=(b/src).read_text();s=re.sub(r'^extern void .*\n','',s,flags=re.M);s=re.sub(r'^static BlitFn .*\n','',s,flags=re.M)
 s=re.sub(r'(?<=,)'+prefix+r'compiled(\d+)(?=\})',lambda m:'bankFunctions['+str(int(m[1])+1)+']',s)
 (b/out).write_text(s)
 code=(b/('compact_compiled.S' if prefix else 'compiled.S')).read_text()
 table='/* GENERATED relocatable sprite bank; offsets are relative to bank_start. */\n.text\n.even\nbank_start:\n'+''.join('.long '+prefix+'compiled_%d_%d-bank_start\n'%(i,ph) for i in range(89) for ph in range(16))
 (b/('compact-bank.S' if prefix else 'original-bank.S')).write_text(table+code)
