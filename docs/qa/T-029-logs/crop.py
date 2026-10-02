import sys, zlib, struct
sys.path.insert(0,'.')
from cmp import rd
def wpng(path,w,h,rows):
    raw=b''.join(b'\x00'+bytes(r) for r in rows)
    def ch(t,d): 
        c=struct.pack('>I',len(d))+t+d
        return c+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
    open(path,'wb').write(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+ch(b'IDAT',zlib.compress(raw))+ch(b'IEND',b''))
ref,cur,out,x0,y0,x1,y1,z=sys.argv[1],sys.argv[2],sys.argv[3],*map(int,sys.argv[4:9])
_,_,bpa,ra=rd(ref); _,_,bpb,rb=rd(cur)
W=(x1-x0)*z; H=(y1-y0)*z; gap=8
rows=[]
for y in range(y0,y1):
    line=bytearray()
    for panel in range(3):
        for x in range(x0,x1):
            if panel==0: px=ra[y][x*bpa:x*bpa+3]
            elif panel==1: px=rb[y][x*bpb:x*bpb+3]
            else:
                d=[abs(ra[y][x*bpa+c]-rb[y][x*bpb+c]) for c in range(3)]
                m=min(255,max(d)*4); px=bytes([m,m,m])
            line+=bytes(px)*z
        if panel<2: line+=bytes([255,0,255])*gap
    for _ in range(z): rows.append(line)
wpng(out,W*3+gap*2,H,rows)
