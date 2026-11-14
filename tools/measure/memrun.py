# Runs a command, samples its memory 4x/s, and reports peak memory per phase.
# anon = memory the program truly holds; file = model file pages the OS can drop.
import sys,subprocess,time,json,os,re
out=sys.argv[1]; os.makedirs(out,exist_ok=True)
log=open(out+'/log.txt','wb')
p=subprocess.Popen(sys.argv[2:],stdout=log,stderr=subprocess.STDOUT)
marks=[('encode_first_stage completed','vae_encode'),('get_learned_condition completed','encoder'),
       ('sampling completed','denoise'),('decode_first_stage completed','vae_decode')]
phase_i=0; pos=0; t0=time.time()
peaks={}; cur={'anon':0,'file':0,'rss':0}; overall=dict(cur)
def read():
    try:
        s=open(f'/proc/{p.pid}/status').read()
        g=lambda k:int(re.search(k+r':\s+(\d+)',s).group(1))/1024
        return g('RssAnon'),g('RssFile')+g('RssShmem'),g('VmRSS')
    except Exception: return None
while p.poll() is None:
    r=read()
    if r:
        for k,v in zip(('anon','file','rss'),r):
            cur[k]=max(cur[k],v); overall[k]=max(overall[k],v)
    with open(out+'/log.txt','rb') as f:
        f.seek(pos); chunk=f.read(); pos+=len(chunk)
    while phase_i<len(marks) and marks[phase_i][0].encode() in chunk:
        peaks[marks[phase_i][1]]={k:round(v) for k,v in cur.items()}; cur={'anon':0,'file':0,'rss':0}; phase_i+=1
    time.sleep(0.25)
txt=open(out+'/log.txt',errors='replace').read()
tm=lambda pat:(lambda m: float(m[-1]) if m else None)(re.findall(pat,txt))
res={'exit':p.returncode,'wall_s':round(time.time()-t0,1),
     'encoder_s':tm(r'get_learned_condition completed, taking ([\d.]+)s'),
     'denoise_s':tm(r'sampling completed, taking ([\d.]+)s'),
     'vae_decode_s':tm(r'decode_first_stage completed, taking ([\d.]+)s'),
     'peak_mb':{k:round(v) for k,v in overall.items()},'peak_by_phase_mb':peaks}
json.dump(res,open(out+'/mem.json','w'),indent=1); print(json.dumps(res))
