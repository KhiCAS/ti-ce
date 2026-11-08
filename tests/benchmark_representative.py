from pathlib import Path
import subprocess,json,hashlib,time,statistics,concurrent.futures,argparse
ap=argparse.ArgumentParser(description="Run the frozen representative KhiCAS CAS suite, one clean CEmu boot per case.")
ap.add_argument('--name',default='khicas')
ap.add_argument('--runner',type=Path,required=True)
ap.add_argument('--rom',type=Path,required=True,help='app-signature-disabled clean CE ROM; copied, never modified')
ap.add_argument('--app-bin',type=Path,required=True,help='pre-relocated raw _KhiCAS.bin from BENCHMARK=YES')
ap.add_argument('--cases',type=Path,default=Path(__file__).with_name('representative_cases.json'))
ap.add_argument('--out',type=Path,required=True)
ap.add_argument('--ids',default='all',help='comma-separated zero-based case IDs, or all')
ap.add_argument('--jobs',type=int,default=2)
ap.add_argument('--limit-seconds',type=int,default=120,help='emulated seconds without progress before stopping a case')
args=ap.parse_args();name=args.name
cases=json.loads(args.cases.read_text())['cases']
ids=list(range(len(cases))) if args.ids=='all' else list(map(int,args.ids.split(',')))
assert args.jobs>0 and args.limit_seconds>0 and all(0<=i<len(cases) for i in ids)
out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
runner=args.runner.resolve();raw=args.app_bin.read_bytes();marker=b'KHICAS_SUITE_SELECT=000'
assert raw.count(marker)==1,'Expected exactly one selector marker in the benchmark app'
pos=raw.index(marker)+len(marker)-3;clean=args.rom.read_bytes()
assert len(clean)==0x400000,'Expected a 4 MiB CE ROM'
# Remove any existing apps from the disposable copy before placing our first-slot app.
clean=bytearray(clean);end=0x3b0000;removed_apps=[]
while True:
 size=int.from_bytes(clean[end-3:end],'little');start=end-3-size
 if size<0x12a or start<0 or clean[start:start+2]!=b'\x81\x0f':break
 removed_apps.append({'start':start,'bytes':size+3});end=start
if removed_apps:clean[end:0x3b0000]=b'\xff'*(0x3b0000-end)
clean=bytes(clean)

def run(n):
 c=cases[n];d=out/f'{n:02d}-{c["name"]}';d.mkdir(exist_ok=True);b=bytearray(raw);b[pos:pos+3]=f'{n:03d}'.encode();rom=bytearray(clean);start=0x3b0000-3-len(b);rom[start:start+len(b)]=b;rom[0x3b0000-3:0x3b0000]=len(b).to_bytes(3,'little');rp=d/'input.rom';rp.write_bytes(rom)
 z={'id':n,'name':c['name'],'category':c['category'],'expression':c['expression'],'patched_app_sha256':hashlib.sha256(b).hexdigest(),'rom_sha256':hashlib.sha256(rom).hexdigest(),'status':'error'};transcript=[];lp=d/'emulator.log';proc=None
 try:
  with lp.open('w') as log:
   proc=subprocess.Popen([str(runner),'--rom',str(rp)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log,text=True,bufsize=1)
   ready=proc.stdout.readline().strip();assert ready.startswith('CEMU_HEADLESS_READY'),ready
   def command(s):
    proc.stdin.write(s+'\n');proc.stdin.flush();reply=proc.stdout.readline().strip();transcript.append([s,reply]);assert reply.startswith('OK'),(s,reply)
   command('run 3000');command('run 3000');command('keys clear,delay:300,2nd,delay:200,cos,delay:1000,2,delay:2000,enter,delay:1500')
   elapsed=0;phase=None;phase_start=0
   for _ in range(args.limit_seconds*6):
    command('run 1000');elapsed+=1000;s=lp.read_text();starts=[l for l in s.splitlines() if l.startswith('KHICAS_V2_START|')];samples=[l for l in s.splitlines() if l.startswith('KHICAS_V2_SAMPLE|')];p=(starts[-1] if starts else 'startup',len(samples))
    if p!=phase:phase=p;phase_start=elapsed
    if 'KHICAS_V2_DONE' in s:break
    if elapsed-phase_start>=args.limit_seconds*1000:raise TimeoutError(f'No progress for {args.limit_seconds} emulated seconds; phase {phase}')
    if 'NMI reset' in s or 'Reset caused' in s:raise RuntimeError('Calculator reset')
   else:raise TimeoutError('Case exceeded total deadline')
   command('quit');proc.wait(timeout=10)
  s=lp.read_text();samples=[];check=None;plot=None
  for l in s.splitlines():
   if l.startswith('KHICAS_V2_SAMPLE|'):
    _,cid,rep,p,e,f,v=l.split('|',6);assert int(cid)==n;samples.append(dict(repeat=int(rep),parse_ticks=int(p),eval_ticks=int(e),print_ticks=int(f),result=v))
   if l.startswith('KHICAS_V2_CHECK|'):
    _,cid,v=l.split('|');assert int(cid)==n;check=int(v)==1
   if l.startswith('KHICAS_V2_PLOT|'):plot=list(map(int,l.split('|')[2:]))
  assert [s['repeat'] for s in samples]==[0,1,2,3],samples
  assert len({s['result'] for s in samples})==1,'unstable result'
  z.update(samples=samples,validation_passed=check,result=samples[0]['result'],plot_fingerprint=plot)
  for k in ['parse_ticks','eval_ticks','print_ticks']:z[k]=statistics.median(s[k] for s in samples[1:])
  z['total_ticks']=sum(z[k] for k in ['parse_ticks','eval_ticks','print_ticks']);z['eval_seconds']=z['eval_ticks']/32768;z['seconds']=z['total_ticks']/32768;z['status']='passed' if check else 'validation_failed'
 except Exception as e:
  z['error']=str(e)
  if lp.exists():
   lines=lp.read_text().splitlines();health=[l for l in lines if l.startswith('KHICAS_V2_HEALTH|')]
   if health:z['health']=list(map(int,health[-1].split('|')[2:]));z['status']='out_of_memory' if z['health'][0] else 'interrupted'
   partial=[]
   for l in lines:
    if l.startswith('KHICAS_V2_SAMPLE|'):
     _,cid,rep,p,e,f,v=l.split('|',6);partial.append(dict(repeat=int(rep),parse_ticks=int(p),eval_ticks=int(e),print_ticks=int(f),result=v))
   if partial:z['partial_samples']=partial

 finally:
  if proc and proc.poll() is None:
   proc.kill();proc.wait()
  (d/'protocol.json').write_text(json.dumps(transcript,indent=2)+'\n');(d/'result.json').write_text(json.dumps(z,indent=2)+'\n');rp.unlink(missing_ok=True)
 print(name,n,c['name'],z['status'],f"{z.get('eval_seconds',0):.3f}s",z.get('error',''),flush=True);return z
with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:res=list(ex.map(run,ids))
p=out/'results.json';previous=json.loads(p.read_text())['cases'] if p.exists() else {};previous.update({str(z['id']):z for z in res});p.write_text(json.dumps({'name':name,'clock_hz':32768,'cases':previous,'app_bin_sha256':hashlib.sha256(raw).hexdigest(),'cases_sha256':hashlib.sha256(args.cases.read_bytes()).hexdigest()},indent=2)+'\n');print(name,'finished',len(res),'cases',flush=True)

raise SystemExit(0 if all(z['status']=='passed' for z in res) else 1)
