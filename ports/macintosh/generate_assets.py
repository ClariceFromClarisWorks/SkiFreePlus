"""Generate monochrome masks, 16 phases, sparse words and 68000 sprite code.
Requires Pillow and pefile. Only reads RT_BITMAP resources; never runs the EXE.
"""
from pathlib import Path
import sys,struct,io,json,hashlib
import pefile
from PIL import Image,ImageDraw,ImageFont
p=Path(__file__).resolve().parent
exe=Path(sys.argv[1]); pe=pefile.PE(str(exe)); ids=list(range(1,90)); names=['sprite'+str(n) for n in ids]; imgs={}
for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
 if t.id!=2:continue
 for r in t.directory.entries:
  if r.id not in ids:continue
  d=r.directory.entries[0].data.struct;b=pe.get_data(d.OffsetToData,d.Size)
  sz,w,h,_,bpp=struct.unpack_from('<IiiHH',b);colors=struct.unpack_from('<I',b,32)[0] or (1<<bpp if bpp<=8 else 0)
  bmp=b'BM'+struct.pack('<IHHI',14+len(b),0,0,14+sz+4*colors)+b
  imgs[r.id]=Image.open(io.BytesIO(bmp)).convert('RGB')
# Put a 2x native Chicago Apple glyph before the unmodified wordmark.
# The renderer compensates for the 20-pixel prefix; credits retain their position.
logo_prefix=20
mark=json.loads((p/'apple-mark.json').read_text())['rows']
old_logo=imgs[53];logo=Image.new('RGB',(old_logo.width+logo_prefix,old_logo.height),'white');logo.paste(old_logo,(logo_prefix,0))
for my,row in enumerate(mark):
 for mx,bit in enumerate(row):
  if bit=='1':
   for dy in range(2):
    for dx in range(2):logo.putpixel((mx*2+dx,my*2+3+dy),(0,0,0))
imgs[53]=logo
(p/'logo_layout.h').write_text('#define LOGO_PREFIX 20\n')
# Preserve the original logo shapes; coloured strokes map to solid black.
c=['/* Generated from SkiFree 1.04 RT_BITMAP resources. */'];a=['/* GENERATED sprite blitters: 16 horizontal phases per sprite. */','.text','.even'];info=[]
preview=Image.new('RGB',(8*240,12*220),'#eeeeee');dr=ImageDraw.Draw(preview)
for k,(rid,name) in enumerate(zip(ids,names)):
 im=imgs[rid];w,h=im.size;words=(w+30)//16
 # White source is transparent, as in the SDL port. Other pixels opaque.
 # Semantic, sprite-local patterns: stable when the sprite moves.
 category="ramp" if rid==52 else "snow" if rid in (27,47,48) else "hazard" if rid in (45,46,49,50,51,83,84,85,87,88,89) else "character_or_marker"
 black_count=0
 m=[];b=[];mono=Image.new('RGB',(w,h),'#cccccc')
 for y in range(h):
  mr=[0]*words;br=[0]*words
  for x in range(w):
   rgb=im.getpixel((x,y));opaque=rgb!=(255,255,255)
   black=(299*rgb[0]+587*rgb[1]+114*rgb[2])<160000
   if rid==53:black=True
   elif rid in (25,26):black=not (2<=x<=9 and 2<=y<=9 and rgb==(0,0,0))
   elif rid in range(57,64):black=max(rgb)<80
   elif category=="ramp":black=not (h//2-1<=y<=h//2)
   elif category=="snow":black=(x+2*y)%4==0
   elif rid in (49,51,87,88,89):
    # Keep animation frames consistent: dark foliage, pale branch highlights,
    # bright-green accents and an intact trunk, all from original colour regions.
    edge=any(nx<0 or ny<0 or nx>=w or ny>=h or im.getpixel((nx,ny))==(255,255,255) for nx,ny in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)))
    if rgb[1]>rgb[0] and rgb[1]>rgb[2]:black=edge or ((x+2*y)%4!=0 if rgb[1]<255 else (x+2*y)%4==0)
    elif rgb[0]>0 and rgb[1]>0:black=y>=h-10 and abs(x-w//2)<=max(2,w//6)
    else:black=True
   elif category=="hazard":
    edge=any(nx<0 or ny<0 or nx>=w or ny>=h or im.getpixel((nx,ny))==(255,255,255) for nx,ny in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)))
    # Keep source branch/shadow detail; dither the brighter foliage only.
    dark=max(rgb)<80
    black=edge or dark or (x+y)%2==0
   if opaque:
    black_count+=int(black)
    mr[x//16]|=1<<(15-x%16)
    if black:br[x//16]|=1<<(15-x%16)
    mono.putpixel((x,y),(0,0,0) if black else (255,255,255))
  m.append(mr);b.append(br)
 px=(k%8)*240;py=(k//8)*220
 dr.text((px+2,py+2),name+' / '+category,fill='black');preview.paste(im,(px+4,py+18));preview.paste(mono.resize((w*2,h*2),Image.Resampling.NEAREST),(px+4,py+86))
 if rid in (27,47,48,52):assert black_count>0, (rid,'invisible terrain')
 for tag,arr in [('mask',m),('bits',b)]:c.append(f'static const U16 {tag}{k}[]={{'+','.join(hex(v) for row in arr for v in row)+'};')
 ptrs=[];spans=[];counts=[];fns=[]
 for ph in range(16):
  rows=[];sparse=[];fn=f'compiled_{k}_{ph}';fns.append(fn)
  a += [f'.globl {fn}',f'{fn}:','move.l 4(%sp),%a0']
  for y in range(h):
   row=[]
   for j in range(words):
    mask=(m[y][j]>>ph)|(((m[y][j-1]<<(16-ph))&65535) if ph and j else 0)
    bits=(b[y][j]>>ph)|(((b[y][j-1]<<(16-ph))&65535) if ph and j else 0)
    keep=mask^65535;rows += [keep,bits];row.append((mask,bits))
    if mask:sparse.append((y,j*2,keep,bits))
   def emit(mask,bits,j,size):
    full=65535 if size=='w' else 4294967295;keep=mask^full;operand=f'{y*64+j*2}(%a0)'
    if not mask:return []
    if not keep:return [f'move.{size} #{bits},{operand}']
    if keep==(bits^full):return [f'ori.{size} #{bits},{operand}']
    return [f'andi.{size} #{keep},{operand}']+([f'ori.{size} #{bits},{operand}'] if bits else [])
   j=0
   while j<words:
    one=emit(*row[j],j,'w')
    if j+1<words:
     two=emit(*row[j+1],j+1,'w');pair=emit((row[j][0]<<16)|row[j+1][0],(row[j][1]<<16)|row[j+1][1],j,'l')
     # Immediate longword instructions cost 8 bytes versus 6 per word.
     # Never extend a transfer into padding beyond the screen row.
     if row[j][0] and row[j+1][0] and len(pair)*8 < (len(one)+len(two))*6:
      a+=pair;j+=2;continue
    a+=one;j+=1
   # Fixed 64-byte stride: no per-row pointer updates.
  a.append('rts');c.append(f'extern void {fn}(U8*,long);')
  pn=f'phase{k}_{ph}';sn=f'span{k}_{ph}';ptrs.append(pn);spans.append(sn);counts.append(str(len(sparse)))
  pass # Shifted masks are generated on demand for clipped sprites.
  pass # Sparse-word candidate excluded from playable memory footprint.
 pass


 c.append(f'static BlitFn compiled{k}[16]={{'+','.join(fns)+'};')
 info.append(dict(category=category,black_pixels=black_count,resource_id=rid,name=name,width=w,height=h,words=words,phase_data_bytes=0,baseline_phase_data_bytes=16*h*words*4,source_data_bytes=h*words*4,sparse_data_bytes=sum(map(int,counts))*8))
c.append('static GfxSprite art[]={ {0},'+','.join('{'+f'{v["width"]},{v["height"]},{v["words"]},mask{k},bits{k},0,compiled{k}'+'}' for k,v in enumerate(info))+'};')
p.joinpath('assets.h').write_text('\n'.join(c)+'\n');p.joinpath('compiled.S').write_text('\n'.join(a)+'\n');preview.save(p/'art-preview.png')
p.joinpath('assets.json').write_text(json.dumps({'source':'https://ski.ihoc.net/ski32.exe','sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'conversion':'white colour key; white-striped ramps; colour-region foliage with solid contours/trunks and white branch highlights; quarter-density snow; threshold 160 characters/markers; prefixed 2x Chicago Apple glyph','sprites':info},indent=2))

# Original ICONSKI monochrome RT_ICON, with its original transparency mask.
icon_data=None
for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
 if t.id==3:
  for r in t.directory.entries:
   if r.id==1:
    z=r.directory.entries[0].data.struct;icon_data=pe.get_data(z.OffsetToData,z.Size)
assert icon_data is not None
assert struct.unpack_from('<iiHH',icon_data,4)==(32,64,1,1)
assert icon_data[40:48]==bytes([0,0,0,0,255,255,255,0])
ib=bytearray(128);mask=bytearray(128);icon=Image.new('RGB',(32,32),'white')
for y in range(32):
 for j in range(4):
  src=(31-y)*4+j;dst=y*4+j
  mask[dst]=icon_data[48+128+src]^255
  ib[dst]=(icon_data[48+src]^255)&mask[dst]
 for x in range(32):
  if ib[y*4+x//8]&(128>>(x%8)):icon.putpixel((x,y),(0,0,0))
bundle=b'SKFR'+struct.pack('>hh',0,1)+b'ICN#'+struct.pack('>hhh',0,0,128)+b'FREF'+struct.pack('>hhh',0,0,128)
def rez(typ,num,data):return "data '"+typ+"' ("+str(num)+") { $\""+data.hex()+"\" };\n"
p.joinpath('Icon.r').write_text(rez('ICN#',128,ib+mask)+rez('BNDL',128,bundle)+rez('FREF',128,b'APPL'+struct.pack('>h',0)+b'\0')+rez('SKFR',0,b'\7SkiFree'))
icon.save(p/'finder-icon-preview.png')
