from pathlib import Path
import sys
src=Path(sys.argv[1]);out=Path(sys.argv[2]);s=(src/'App.r').read_text().replace('#include "Icon.r"',(src/'Icon.r').read_text())
for n,name in [(128,'original')]:
 b=(out/(name+'-bank.bin')).read_bytes();s+="\ndata 'BLIT' (%d) {\n"%n+''.join('$"'+b[i:i+32].hex()+'"\n' for i in range(0,len(b),32))+'};\n'
(out/'AppResources.r').write_text(s)
