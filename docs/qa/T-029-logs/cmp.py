import sys, zlib, struct, os
def rd(path):
    d=open(path,'rb').read(); pos=8; idat=b''
    while pos<len(d):
        ln,=struct.unpack('>I',d[pos:pos+4]); typ=d[pos+4:pos+8]; body=d[pos+8:pos+8+ln]; pos+=12+ln
        if typ==b'IHDR': w,h,bd,ct=struct.unpack('>IIBB',body[:10])
        elif typ==b'IDAT': idat+=body
    raw=zlib.decompress(idat); bpp={2:3,6:4,0:1,4:2}[ct]; st=w*bpp; rows=[]; prev=bytearray(st); p=0
    for y in range(h):
        f=raw[p]; p+=1; line=bytearray(raw[p:p+st]); p+=st
        if f==1:
            for i in range(bpp,st): line[i]=(line[i]+line[i-bpp])&255
        elif f==2:
            for i in range(st): line[i]=(line[i]+prev[i])&255
        elif f==3:
            for i in range(st):
                a=line[i-bpp] if i>=bpp else 0; line[i]=(line[i]+((a+prev[i])>>1))&255
        elif f==4:
            for i in range(st):
                a=line[i-bpp] if i>=bpp else 0; b=prev[i]; c=prev[i-bpp] if i>=bpp else 0
                pa=abs(b-c); pb=abs(a-c); pc=abs(a+b-2*c)
                pr=a if (pa<=pb and pa<=pc) else (b if pb<=pc else c)
                line[i]=(line[i]+pr)&255
        rows.append(line); prev=line
    return w,h,bpp,rows
def cmp(a,b,box):
    w,h,bpp,ra=rd(a); w2,h2,_,rb=rd(b)
    if (w,h)!=(w2,h2): return None
    x0,y0,x1,y1=box if box else (0,0,w,h)
    n=0; hist={'0':0,'1':0,'2-16':0,'>16':0}; mx=0; bb=[w,h,0,0]
    for y in range(y0,y1):
        A=ra[y];B=rb[y]
        for x in range(x0,x1):
            m=0
            for c in range(min(bpp,3)):
                dd=abs(A[x*bpp+c]-B[x*bpp+c]); m=max(m,dd)
            n+=1
            if m==0: hist['0']+=1
            elif m==1: hist['1']+=1
            elif m<=16: hist['2-16']+=1
            else:
                hist['>16']+=1
                bb=[min(bb[0],x),min(bb[1],y),max(bb[2],x),max(bb[3],y)]
            mx=max(mx,m)
    return w,h,n,hist,mx,bb
if __name__=='__main__':
    ref,cur=sys.argv[1],sys.argv[2]
    box=tuple(int(v) for v in sys.argv[3:7]) if len(sys.argv)>6 else None
    for f in sorted(os.listdir(cur)):
        if not f.endswith('.png'): continue
        r=os.path.join(ref,f)
        if not os.path.exists(r): print(f,'no ref'); continue
        res=cmp(r,os.path.join(cur,f),box)
        if res is None: print(f,'size differs'); continue
        w,h,n,hist,mx,bb=res
        print('%-34s %dx%d n=%d  same %.2f%%  d1 %.2f%%  d2-16 %.2f%%  d>16 %d  max %d  bbox>16 %s'%(f,w,h,n,100*hist['0']/n,100*hist['1']/n,100*hist['2-16']/n,hist['>16'],mx,bb if hist['>16'] else '-'))
