const assert=require('node:assert/strict'),fs=require('fs'),E=require('./engine.js')();
const read=p=>JSON.parse(fs.readFileSync(p,'utf8'));
const deployed=fs.existsSync(__dirname+'/../Data/abilities.json'),dataRoot=deployed?__dirname+'/../Data':__dirname+'/data';
const raw=read(deployed?dataRoot+'/bosses.json':__dirname+'/../Balancing/bosses.json');
const data={abilities:read(dataRoot+'/abilities.json'),bosses:structuredClone(raw),enhancements:read(dataRoot+'/enhancements.json')};
E.validate(data);
assert.equal(data.bosses.actions.length,13);assert.equal(data.bosses.patterns.length,16);
const round=E.exportBoss(data.bosses);assert.deepEqual(round.states,raw.states);assert.deepEqual(round.patterns,raw.patterns);
for(const k of ['kind','bossId','zoneIds','actions'])assert.ok(!(k in round));
assert.ok(!JSON.stringify(round).includes('motionId'));
const reloaded=E.importBoss(round);assert.deepEqual(E.exportBoss(reloaded),round);
for(let seed=0;seed<1000;seed++){const s=E.schedule(data.bosses,seed);assert.equal(s.turns.length,10);assert.deepEqual(s,E.schedule(data.bosses,seed));for(const t of s.turns){const source=raw.states.find(x=>x.id===t.id);assert.equal(t.hitCount,1);assert.equal(Object.values(t.damageByZone).filter(x=>x>0).length,source.target.mode==='randomOne'?1:source.target.zones.length);}}
const b=data.bosses,p=E.profile(data,'gladius'),r=data.abilities.rules;
const all=b.actions.find(x=>x.id==='AllStrike');
for(let z=0;z<3;z++)assert.equal(E.transition({h:7,f:0,e:0,z},0,all,p,r).loss,2);
const left=b.actions.find(x=>x.id==='SingleStrikeLeft');assert.equal(left.resistanceByZone.left.physical,'vulnerable');assert.equal(left.resistanceByZone.right.magical,'enduring');
for(const id of ['Defense','Defenseless'])for(let z=0;z<3;z++)assert.equal(E.transition({h:7,f:0,e:0,z},0,b.actions.find(x=>x.id===id),p,r).loss,0);
const changed=structuredClone(data);changed.bosses.actions[0].totalDamage=3;changed.bosses.actions[0].vulnerabilityByZone.left='normal';E.validate(changed);assert.equal(changed.bosses.actions[0].damageByZone.left,3);assert.equal(changed.bosses.actions[0].resistanceByZone.left.magical,'normal');
for(const edit of [b=>b.patterns[0].turns++,b=>b.patterns[0].sequence[0]='missing',b=>b.states[0].vulnerabilityByZone.left='bad',b=>b.states[0].target.zones=['bad']]){const bad=structuredClone(raw);edit(bad);assert.throws(()=>E.importBoss(bad));}
const short=structuredClone(data);short.bosses.preset.mode='custom';short.bosses.preset.customPatterns=[short.bosses.patterns[14].id];assert.equal(E.search(short,{character:'gladius',count:2}).turns.length,2);
const result=E.simulate(data,{seed:42,iterations:2,sequences:1,players:[{name:'AI',character:'gladius',zone:'left',q:70,threshold:20,hpCost:12,deathCost:30,lookahead:2}]});assert.equal(result.hist.reduce((a,b)=>a+b),2);assert.ok(Number.isFinite(result.damage));
console.log('PASS state JSON import/export, 1000 seeded schedules, Fury single-zone hits, vulnerabilities, damage, non-attacks, edits, invalid inputs, exact search and AI simulation');
