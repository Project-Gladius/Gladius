const fs=require('fs'),assert=require('node:assert/strict'),vm=require('vm'),E=require('./engine.js')();
const root=__dirname+'/../';const data=Object.fromEntries(['abilities','bosses','enhancements'].map(k=>[k,JSON.parse(fs.readFileSync(root+'Data/'+k+'.json','utf8'))]));
E.validate(data);assert.equal(data.bosses.actions.length,13);assert.equal(data.bosses.patterns.length,22);assert.deepEqual(E.exportBoss(data.bosses),data.bosses);
for(let seed=0;seed<1000;seed++){const x=E.schedule(data.bosses,seed);assert.equal(x.turns.length,10);for(const a of x.turns){if(a.id==='Fury'){assert.equal(Object.values(a.damageByZone).filter(v=>v>0).length,1);assert.equal(Object.values(a.damageByZone).reduce((x,y)=>x+y),1);}for(const z of E.zones)assert.equal(a.resistanceByZone[z].magical,'normal');}}
const bad=structuredClone(data);bad.bosses.actions.find(a=>a.id==='Fury').target.zones=['bad'];assert.throws(()=>E.validate(bad));
const html=fs.readFileSync(root+'Gladius 통합 시뮬레이터.html','utf8');const scripts=[...html.matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)].map(m=>m[1]);for(const s of scripts)new vm.Script(s);const c={};vm.createContext(c);vm.runInContext(scripts.find(s=>s.includes('const EMBEDDED ='))+';this.data=EMBEDDED',c);assert.deepEqual(JSON.parse(JSON.stringify(c.data)),data);
const other=fs.readFileSync(root+'Gladius 시뮬레이터.html','utf8');const os=[...other.matchAll(/<script([^>]*)>([\s\S]*?)<\/script>/g)].filter(m=>!m[1].includes('application/json')).map(m=>m[2]);for(const s of os)new vm.Script(s);
const ctx={window:{},ju:structuredClone,ye:x=>x,Qu:()=>({probability:{},zones:{}}),ne:E.zones};vm.createContext(ctx);vm.runInContext(os.find(s=>s.includes('window.GladiusStateEngine=createEngine()')),ctx);const start=other.indexOf('function jp('),end=other.indexOf('function zn(',start);vm.runInContext(other.slice(start,end),ctx);const boss=structuredClone(data.bosses);boss.preset.mode='direct';boss.preset.directActions=['Fury','Fury'];const result=ctx.jp(boss,{},42);for(const t of result.config.turns)assert.equal(Object.values(t.zones).filter(z=>z.damage>0).length,1);
console.log('PASS v1 roundtrip, 1000 complete schedules, Fury single-zone, separate resistances, invalid targets, current embedded JSON, both HTML syntax and standalone import');

// Match the game random-mode guard and candidate rules at every remaining turn.
assert.ok(data.bosses.selection.allowImmediateRepeat);
for(let left=1;left<=10;left++)assert.ok(data.bosses.selection.pool.some(w=>w.weight>0&&data.bosses.patterns.find(p=>p.id===w.patternId).steps.length<=left));
assert.deepEqual(data.bosses.patterns.slice(16).map(p=>p.steps),[['SingleStrikeLeft'],['SingleStrikeCenter'],['SingleStrikeRight'],['LeftMiddle'],['RightMiddle'],['LeftRight']]);
for(const p of data.bosses.patterns.slice(16))assert.ok(data.bosses.selection.pool.some(w=>w.patternId===p.id&&w.weight===1));
console.log('PASS six single-turn patterns and game candidate availability for every remaining turn 1..10');
