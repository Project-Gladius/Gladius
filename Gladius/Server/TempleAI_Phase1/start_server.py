"""Start one local server using the game's loose operator settings."""
from pathlib import Path
import json,logging,os,sys,urllib.parse,urllib.request

ROOT=Path(__file__).resolve().parent
DATA=(ROOT/'../../Content/ExternalData').resolve()
os.environ['GLADIUS_EXTERNAL_DATA_DIR']=str(DATA)

def main():
 settings=json.loads((DATA/'ConnectionSettings.json').read_text(encoding='utf-8-sig'))
 endpoint=urllib.parse.urlsplit(settings.get('TempleAIBatchEndpointUrl','http://127.0.0.1:8765/temple/respond-batch'))
 if endpoint.hostname not in ('127.0.0.1','localhost','::1'):
  raise RuntimeError('The configured AI server is remote; local startup is unnecessary.')
 port=endpoint.port or 8765
 try:
  with urllib.request.urlopen(f'http://127.0.0.1:{port}/health',timeout=2) as response:
   if response.status==200:return
 except Exception:pass
 saved=ROOT/'Saved';saved.mkdir(exist_ok=True)
 # Hold the lock for the server's lifetime, including across temple visits.
 import msvcrt
 lock=(saved/f'server-{port}.lock').open('a+b')
 lock.seek(0)
 try:msvcrt.locking(lock.fileno(),msvcrt.LK_NBLCK,1)
 except OSError:return
 try:
  output=(saved/'server.log').open('a',encoding='utf-8',buffering=1)
  sys.stdout=output;sys.stderr=output
  logging.basicConfig(level=logging.INFO,stream=output,format='%(asctime)s %(levelname)s %(name)s: %(message)s',force=True)
  import uvicorn
  uvicorn.run('app.main:app',host='127.0.0.1',port=port,log_config=None,access_log=False)
 finally:
  lock.seek(0);msvcrt.locking(lock.fileno(),msvcrt.LK_UNLCK,1);lock.close()

if __name__=='__main__':main()
