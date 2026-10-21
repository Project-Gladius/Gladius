/* Shared combat engine. The browser, both workers and Node tests run this exact code. */
function createEngine(){
'use strict';
const zones=['left','center','right'], names=['공격 1','공격 2','방어','회피·좌','회피·중','회피·우'];
const clone=x=>JSON.parse(JSON.stringify(x)), sum=a=>a.reduce((x,y)=>x+y,0), key=x=>Number(x.toFixed(8));
function assert(ok,msg){if(!ok)throw Error(msg);}
function number(v,min,max,label,int=false){assert(typeof v==='number'&&Number.isFinite(v)&&v>=min&&v<=max&&(!int||Number.isInteger(v)),label+' 범위: '+min+'~'+max+(int?' 정수':''));}
function str(v,label){assert(typeof v==='string'&&v.trim().length>0&&v.length<=200,label+'을 확인하세요.');}
function unique(list,label){assert(Array.isArray(list)&&list.length>0,label+'이 비어 있습니다.');const ids=new Set();list.forEach(x=>{assert(/^[A-Za-z0-9_]+$/.test(x.id),label+' ID는 영문·숫자·밑줄만 사용합니다.');assert(!ids.has(x.id),label+' ID 중복: '+x.id);ids.add(x.id);str(x.name,label+' 이름');});return ids;}
// External state schema stays separate from the simulator's combat representation.
function importBoss(source){
 if(!source?.states)return source;
 assert(source.schemaVersion===2,'보스 상태 JSON 버전은 2여야 합니다.');
 assert(Array.isArray(source.states)&&source.states.length>0,'보스 상태가 비어 있습니다.');
 const ids=new Set();
 const actions=source.states.map(s=>{
  str(s.id,'상태 ID');assert(/^[A-Za-z0-9_]+$/.test(s.id)&&!ids.has(s.id),'잘못되었거나 중복된 상태 ID: '+s.id);ids.add(s.id);
  number(s.totalDamage,0,100,'총 대미지',true);str(s.telegraphText,'예고 텍스트');
  const t=s.target;assert(t&&['none','fixed','randomOne','selectOne'].includes(t.mode)&&Array.isArray(t.zones),'타격 구역 형식 오류: '+s.id);
  assert(t.zones.every(z=>zones.includes(z))&&new Set(t.zones).size===t.zones.length,'타격 구역 오류: '+s.id);
  assert(t.mode==='none'?t.zones.length===0&&s.totalDamage===0:t.zones.length>0,'공격/비공격 구역 오류: '+s.id);
  for(const z of zones)assert(['vulnerable','normal','resistant'].includes(s.vulnerabilityByZone?.[z]),'구역별 취약도 오류: '+s.id);
  return {id:s.id,name:s.name||s.id,totalDamage:s.totalDamage,target:clone(t),vulnerabilityByZone:clone(s.vulnerabilityByZone),hitCount:1,
   damageByZone:Object.fromEntries(zones.map(z=>[z,t.zones.includes(z)?s.totalDamage:0])),attributeByZone:Object.fromEntries(zones.map(z=>[z,'physical'])),
   resistanceByZone:Object.fromEntries(zones.map(z=>{const v=s.vulnerabilityByZone[z]==='resistant'?'enduring':s.vulnerabilityByZone[z];return [z,{physical:v,magical:v}];})),telegraph:{text:s.telegraphText,motionId:''},attackMotionId:''};
 });
 assert(Array.isArray(source.patterns)&&source.patterns.length>0,'패턴이 비어 있습니다.');
 const patterns=source.patterns.map((p,i)=>{str(p.name,'패턴 이름');assert(Array.isArray(p.sequence)&&p.sequence.length>0&&p.sequence.every(id=>ids.has(id)),'패턴 상태 참조 오류: '+p.name);assert(p.turns===p.sequence.length,'패턴 턴 수와 상태 수가 다릅니다: '+p.name);return {id:'pattern_'+i,name:p.name,steps:clone(p.sequence)};});
 const config=source.simulation||{}, preset=config.preset||{};
 const patternId=i=>{assert(Number.isInteger(i)&&patterns[i],'시뮬레이션 패턴 번호 오류');return patterns[i].id;};
 return {schemaVersion:1,kind:'gladius-boss-patterns',revision:1,name:source.name||'보스',maxHp:config.maxHp??100,
  encounter:clone(config.encounter||{turns:10,patternFit:'remainingTurns'}),actions,patterns,
  selection:{mode:'weightedRandom',allowImmediateRepeat:config.allowImmediateRepeat??true,pool:patterns.map((p,i)=>({patternId:p.id,weight:config.weights?.[i]??1}))},
  preset:{mode:preset.mode||'random',seed:preset.seed??20261018,customPatterns:(preset.customPatternIndices||[0]).map(patternId),directActions:clone(preset.directActions||patterns[0].steps)},
  stateFormat:{schemaVersion:2,damageRule:source.damageRule}};
}
function exportBoss(b){
 if(!b.stateFormat)return clone(b);
 const out={schemaVersion:2,name:b.name,states:b.actions.map(a=>({id:a.id,...(a.name!==a.id?{name:a.name}:{}),totalDamage:a.totalDamage,target:clone(a.target),telegraphText:a.telegraph.text,vulnerabilityByZone:clone(a.vulnerabilityByZone)})),
  patterns:b.patterns.map(p=>({name:p.name,turns:p.steps.length,sequence:clone(p.steps)}))};
 if(b.stateFormat.damageRule)out.damageRule=b.stateFormat.damageRule;
 out.simulation={maxHp:b.maxHp,encounter:clone(b.encounter),allowImmediateRepeat:b.selection.allowImmediateRepeat,weights:b.patterns.map(p=>b.selection.pool.find(x=>x.patternId===p.id)?.weight??0),preset:{mode:b.preset.mode,seed:b.preset.seed,customPatternIndices:b.preset.customPatterns.map(id=>b.patterns.findIndex(p=>p.id===id)),directActions:clone(b.preset.directActions)}};
 return out;
}
function syncState(a){
 if(!a.target||!a.vulnerabilityByZone)return;
 a.hitCount=1;
 for(const z of zones){a.damageByZone[z]=a.target.zones.includes(z)?a.totalDamage:0;const v=a.vulnerabilityByZone[z]==='resistant'?'enduring':a.vulnerabilityByZone[z];a.resistanceByZone[z]={physical:v,magical:v};}
}
function attackVariants(a){
 if(!a.target||!['randomOne','selectOne'].includes(a.target.mode))return [a];
 return a.target.zones.map(z=>({...a,damageByZone:Object.fromEntries(zones.map(k=>[k,k===z?(a.totalDamage??a.damageByZone[k]):0]))}));
}
function validate(data){
 if(data.bosses?.states)data.bosses=importBoss(data.bosses);
 const {abilities:a,bosses:b,enhancements:e}=data;
 if(b.stateFormat)importBoss(exportBoss(b));
 b.actions?.forEach(syncState);
 for(const [x,kind] of [[a,'abilities'],[b,'boss-patterns'],[e,'enhancements']]){assert(x&&x.schemaVersion===1&&x.kind==='gladius-'+kind,'지원하지 않는 JSON 형식: '+kind);number(x.revision,1,1e9,'개정 번호',true);}
 const r=a.rules;assert(r,'전투 규칙이 없습니다.');
 number(r.attackFeverGain,0,100,'공격 피버');number(r.defenseFeverGain,0,100,'방어 피버');number(r.feverDamageCoefficient,0,20,'피버 피해 계수');number(r.evadeThreshold,1,100,'연속 회피 기준',true);number(r.evadeReduction,0,1,'회피 감소율');
 assert(['everyAfter','eachN'].includes(r.evadeRepeat),'피버 반복 방식 오류');assert(['floor','round','none'].includes(r.feverRounding),'피버 소수 처리 오류');assert(typeof r.allowStayEvade==='boolean','현재 위치 회피는 true/false입니다.');
 if(r.feverCap!==null)number(r.feverCap,0,10000,'피버 상한');
 assert(r.attackDamageRounding==='roundHalfUp'&&r.defenseDamageRounding==='ceil','공격 피해 반올림 / 방어 피해 올림 규칙을 유지하세요.');
 assert(JSON.stringify(r.turnOrder)===JSON.stringify(['move','playerAttack','bossAttack','feverAndDeath']),'턴 처리 순서가 다릅니다.');
 for(const k of ['enduring','normal','vulnerable'])number(r.resistanceMultipliers?.[k],0,10,'내성 배율');
 number(a.initial?.fever,0,10000,'시작 피버');number(a.initial?.evadeStreak,0,100,'시작 연속 회피',true);assert(zones.includes(a.initial?.zone),'시작 구역 오류');
 unique(a.characters,'캐릭터');assert(a.characters.some(c=>c.id==='gladius')&&a.characters.some(c=>c.id==='sera'),'글라디우스와 세라가 필요합니다.');
 a.characters.forEach(c=>{number(c.baseHp,1,1000,c.name+' HP',true);number(c.defenseDamageRatio,0,1,c.name+' 방어 시 피해 비율');assert(c.skills?.length===2,'캐릭터마다 공격 2개가 필요합니다.');unique(c.skills,'공격');c.skills.forEach(s=>{number(s.baseDamage,0,10000,'공격력');number(s.critChance,0,1,'치명타 확률');number(s.critBonus,0,10,'치명타 추가 피해');assert(['physical','magical'].includes(s.attribute),'공격 속성 오류');});});
 for(const k of ['hp','damage','critChance','critBonus']){number(e.perLevel?.[k],0,k.startsWith('crit')?1:100,'강화 증가량',k==='hp');number(e.initialLevels?.[k],0,100,'강화 단계',true);}number(e.critChanceCap,0,1,'치명타 상한');
 for(const a of b.actions||[]){if(a.target&&!a.vulnerabilityByZone){assert(['randomOne','selectOne'].includes(a.target.mode),'지원하지 않는 선택 타격 방식');assert(Array.isArray(a.target.zones)&&a.target.zones.length>0&&a.target.zones.every(z=>zones.includes(z))&&new Set(a.target.zones).size===a.target.zones.length,'선택 타격 후보 구역 오류');}}
 const ids=unique(b.actions,'보스 공격'),pids=unique(b.patterns,'보스 패턴');assert(b.actions.length<=100&&b.patterns.length<=100,'공격과 패턴은 각각 최대 100개입니다.');number(b.maxHp,1,1e9,'보스 HP');number(b.encounter?.turns,1,10,'전투 구간 턴 수',true);assert(b.encounter.patternFit==='remainingTurns','남은 턴에 맞춘 패턴 선택이 필요합니다.');
 for(const x of b.actions){number(x.hitCount,1,10,'타격 횟수',true);for(const z of zones){number(x.damageByZone?.[z],0,100,'보스 피해',true);assert(['physical','magical'].includes(x.attributeByZone?.[z]),'보스 공격 속성 오류');for(const at of ['physical','magical'])assert(['enduring','normal','vulnerable'].includes(x.resistanceByZone?.[z]?.[at]),'구역별 내성을 모두 입력하세요.');}}
 for(const p of b.patterns){assert(Array.isArray(p.steps)&&p.steps.length>=1&&p.steps.length<=4,'패턴은 1~4개의 공격으로 구성됩니다: '+p.name);p.steps.forEach(id=>assert(ids.has(id),'없는 공격 ID: '+id));}
 assert(b.selection?.mode==='weightedRandom'&&typeof b.selection.allowImmediateRepeat==='boolean','패턴 선택 규칙 오류');assert(Array.isArray(b.selection.pool)&&b.selection.pool.length>0,'패턴 가중치가 없습니다.');const poolIds=new Set();b.selection.pool.forEach(x=>{assert(pids.has(x.patternId),'없는 패턴 ID: '+x.patternId);assert(!poolIds.has(x.patternId),'가중치 패턴 중복');poolIds.add(x.patternId);number(x.weight,0,10000,'패턴 가중치');});
 // Check every reachable boundary, including states that a lucky sample may not visit.
 const visited=new Set();function fit(left,prev){if(left===0)return;const k=left+':'+prev;if(visited.has(k))return;visited.add(k);const choices=candidates(b,left,prev);assert(choices.length>0,'남은 '+left+'턴에 선택 가능한 패턴이 없습니다 (직전: '+(prev||'없음')+').');choices.forEach(p=>fit(left-p.steps.length,p.id));}if(b.preset?.mode==='random')fit(b.encounter.turns,null);
 assert(['random','custom','direct'].includes(b.preset?.mode),'보스 실행 모드 오류');number(b.preset.seed,0,4294967295,'난수 시드',true);
 assert(Array.isArray(b.preset.directActions)&&b.preset.directActions.length>=1&&b.preset.directActions.length<=10,'직접 지정 공격은 1~10턴입니다.');b.preset.directActions.forEach(id=>assert(ids.has(id),'직접 지정에 없는 공격 ID: '+id));
 assert(Array.isArray(b.preset.customPatterns)&&b.preset.customPatterns.length>=1,'사용자 지정 패턴 순서가 비어 있습니다.');b.preset.customPatterns.forEach(id=>assert(pids.has(id),'사용자 지정에 없는 패턴 ID: '+id));if(b.preset.mode==='custom')assert(b.preset.customPatterns.reduce((n,id)=>n+b.patterns.find(p=>p.id===id).steps.length,0)<=10,'사용자 지정 패턴은 총 10턴 이하로 구성하세요.');
 if(a.simulationDefaults){number(a.simulationDefaults.initialLines,1,50,'최초 라인',true);number(a.simulationDefaults.additionalLines,1,50,'추가 라인',true);assert(typeof a.simulationDefaults.uniqueDamage==='boolean','중복 생략 값 오류');for(const p of a.simulationDefaults.probabilityByTurn||[]){for(const k of ['pa','pd','pe'])number(p[k],0,1,'피격 확률');number(p.damage,0,100,'확률 모드 피해',true);for(const at of ['physical','magical'])assert(['enduring','normal','vulnerable'].includes(p[at]),'확률 모드 내성 오류');}}
 return data;
}
function rng(seed){let a=seed>>>0;return()=>{a=(a+0x6D2B79F5)>>>0;let t=a;t=Math.imul(t^t>>>15,t|1);t^=t+Math.imul(t^t>>>7,t|61);return((t^t>>>14)>>>0)/4294967296;};}
function candidates(b,left,prev){
 const memo=new Map();
 function choices(n,last){return b.selection.pool.filter(x=>x.weight>0&&(b.selection.allowImmediateRepeat||x.patternId!==last)).map(x=>({...b.patterns.find(p=>p.id===x.patternId),weight:x.weight})).filter(p=>p.steps.length<=n);}
 function fits(n,last){if(n===0)return true;const k=n+':'+last;if(memo.has(k))return memo.get(k);const ok=choices(n,last).some(p=>fits(n-p.steps.length,p.id));memo.set(k,ok);return ok;}
 return choices(left,prev).filter(p=>fits(left-p.steps.length,p.id));
}
function schedule(b,seed){
 b=importBoss(b);seed=seed??b.preset.seed;
 const mode=b.preset.mode, out=[],patterns=[],r=rng(seed);const add=(id,pid)=>{const a=b.actions.find(a=>a.id===id),variants=attackVariants(a),chosen=variants.length===1?variants[0]:variants[Math.floor(r()*variants.length)];out.push({...clone(chosen),patternId:pid});};
 if(mode==='direct')b.preset.directActions.forEach(id=>add(id,null));
 else if(mode==='custom')b.preset.customPatterns.forEach(id=>{const p=b.patterns.find(x=>x.id===id);assert(p,'사용자 지정 패턴을 찾을 수 없습니다.');patterns.push(id);p.steps.forEach(a=>add(a,id));});
 else {let left=b.encounter.turns,prev=null;while(left>0){const pool=candidates(b,left,prev);assert(pool.length,'남은 턴에 맞는 패턴이 없습니다.');let v=r()*sum(pool.map(p=>p.weight)),p=pool[pool.length-1];for(const x of pool){v-=x.weight;if(v<0){p=x;break;}}patterns.push(p.id);p.steps.forEach(id=>add(id,p.id));left-=p.steps.length;prev=p.id;}}
 return {turns:out,patterns,seed};
}
function profile(data,id,levels=data.enhancements.initialLevels){const c=clone(data.abilities.characters.find(x=>x.id===id));assert(c,'캐릭터를 찾을 수 없습니다.');const e=data.enhancements;for(const k of Object.keys(e.initialLevels))number(levels[k],0,100,'강화 단계',true);c.maxHp=c.baseHp+levels.hp*e.perLevel.hp;c.skills=c.skills.map(s=>({...s,baseDamage:s.baseDamage+levels.damage*e.perLevel.damage,critChance:Math.min(e.critChanceCap,s.critChance+levels.critChance*e.perLevel.critChance),critBonus:s.critBonus+levels.critBonus*e.perLevel.critBonus}));return c;}
function initial(data,p,over={}){const a=data.abilities.initial;const s={h:over.hp??p.maxHp,f:over.fever??a.fever,e:over.evadeStreak??a.evadeStreak,z:zones.indexOf(over.zone??a.zone),damage:0};number(s.h,1,10000,'시작 HP',true);number(s.f,0,10000,'시작 피버');number(s.e,0,100,'연속 회피',true);assert(s.z>=0,'시작 위치 오류');return s;}
function actions(s,r,mode='position'){return mode==='probability'?[0,1,2,3]:[0,1,2,...[0,1,2].filter(z=>r.allowStayEvade||z!==s.z).map(z=>z+3)];}
function fever(s,a,r){let e=a>=3?s.e+1:0,f=s.f;if(a<2)f+=r.attackFeverGain;else if(a===2)f+=r.defenseFeverGain;else if(r.evadeRepeat==='everyAfter'?e>=r.evadeThreshold:e%r.evadeThreshold===0){f*=1-r.evadeReduction;f=r.feverRounding==='floor'?Math.floor(f):r.feverRounding==='round'?Math.round(f):key(f);}return {f:Math.max(0,r.feverCap===null?f:Math.min(f,r.feverCap)),e:r.evadeRepeat==='everyAfter'?Math.min(e,r.evadeThreshold):e%r.evadeThreshold};}
function attackDamage(skill,f,res,r,crit){return Math.max(0,Math.round((skill.baseDamage+f*r.feverDamageCoefficient)*r.resistanceMultipliers[res]*(crit?1+skill.critBonus:1)));}
function expectedDamage(skill,f,res,r){return attackDamage(skill,f,res,r,false)*(1-skill.critChance)+attackDamage(skill,f,res,r,true)*skill.critChance;}
// The same transition drives exact search and sampled combat. Reward uses pre-update fever.
function transition(s,a,b,p,r,{mode='position',probability,crit=null,hit=true}={}){
 assert(actions(s,r,mode).includes(a),'허용되지 않는 회피 목적지입니다.');
 const z=mode==='position'&&a>=3?a-3:s.z,zone=zones[z],sk=a<2?p.skills[a]:null,at=mode==='probability'?probability:b.resistanceByZone[zone];
 const damage=sk?(crit===null?expectedDamage(sk,s.f,at[sk.attribute],r):attackDamage(sk,s.f,at[sk.attribute],r,crit)):0;
 const base=mode==='probability'?probability.damage:b.damageByZone[zone],hits=mode==='probability'?1:b.hitCount;
 const loss=hit?(a===2?Math.ceil(base*p.defenseDamageRatio):base)*hits:0;
 const fe=fever(s,a,r);return {s:{h:Math.max(0,s.h-loss),z,...fe,damage:(s.damage||0)+damage},damage,loss};
}
function trace(data,p,turns,line,start,mode='position',probs=[]){let s=clone(start),hp=new Map([[s.h,1]]),damage=0;const rows=[];for(let t=0;t<line.length;t++){const a=line[t],pr=probs[t]||{pa:.5,pd:.5,pe:.15,damage:2,physical:'normal',magical:'normal'},before=clone(s),b=transition(s,a,turns[t],p,data.abilities.rules,{mode,probability:pr}),prob=mode==='position'?1:a<2?pr.pa:a===2?pr.pd:pr.pe,newhp=new Map();const surv=sum([...hp.values()]);for(const [h,w]of hp){if(prob<1)newhp.set(h,(newhp.get(h)||0)+w*(1-prob));if(h>b.loss)newhp.set(h-b.loss,(newhp.get(h-b.loss)||0)+w*prob);}hp=newhp;s=b.s;damage+=b.damage;rows.push({t:t+1,a,boss:turns[t].name,before,after:clone(s),damage:surv>0?b.damage:0,survival:sum([...hp.values()]),hp:sum([...hp].map(([h,w])=>h*w))/(sum([...hp.values()])||1),loss:b.loss});}
 return {rows,damage,survival:sum([...hp.values()]),endHp:sum([...hp].map(([h,w])=>h*w))/(sum([...hp.values()])||1)};}
const compare=(a,b)=>key(b.damage)-key(a.damage)||b.survival-a.survival||b.endHp-a.endHp||a.actions.join('').localeCompare(b.actions.join(''));
function search(data,opts,progress=()=>{}){
 validate(data);const p=profile(data,opts.character,opts.levels),r=data.abilities.rules,turns=schedule(data.bosses,opts.seed).turns,start=initial(data,p,opts.start),k=opts.count||10,prefix=opts.prefix||[],mode=opts.mode||'position',probs=opts.probabilities||data.abilities.simulationDefaults.probabilityByTurn;number(k,1,50,'라인 수',true);assert(prefix.length<=turns.length,'고정 행동이 전투보다 깁니다.');
 let visits=0;const pick=list=>{list.sort(compare);if(opts.unique){const seen=new Set();list=list.filter(x=>{const d=key(x.damage);if(seen.has(d))return false;seen.add(d);return true;});}return list.slice(0,k);};
 let lines;
 if(mode==='position'){
  let state=start,base=0;for(let t=0;t<prefix.length;t++){assert(state.h>0,'고정 행동 중 사망했습니다.');const b=transition(state,prefix[t],turns[t],p,r);state=b.s;base+=b.damage;}
  const memo=new Map();function solve(t,s){if(s.h<=0)return [];if(t===turns.length)return[{damage:0,survival:1,endHp:s.h,actions:[]}];const kk=[t,s.h,s.f,s.e,s.z].join(',');if(memo.has(kk))return memo.get(kk);if(++visits>1500000)throw Error('탐색 상태가 너무 많습니다. 턴 수나 피버 범위를 줄여 주세요.');if(visits%5000===0)progress({visited:visits});const result=[];for(const a of actions(s,r)){const b=transition(s,a,turns[t],p,r);for(const line of solve(t+1,b.s))result.push({...line,damage:line.damage+b.damage,actions:[a,...line.actions]});}const best=pick(result);memo.set(kk,best);return best;}
  lines=solve(prefix.length,state).map(l=>({...l,damage:l.damage+base,actions:[...prefix,...l.actions]}));
 }else{
  lines=[];function visit(t,s,hp,damage,aa){if(++visits%20000===0)progress({visited:visits});if(!hp.size)return;if(t===turns.length){const survival=sum([...hp.values()]),endHp=sum([...hp].map(([h,w])=>h*w))/survival;lines=pick([...lines,{actions:aa,damage,survival,endHp}]);return;}
   const pr=probs[t]||{pa:.5,pd:.5,pe:.15,damage:2,physical:'normal',magical:'normal'};
   for(const a of t<prefix.length?[prefix[t]]:actions(s,r,mode)){const b=transition(s,a,turns[t],p,r,{mode,probability:pr}),q=a<2?pr.pa:a===2?pr.pd:pr.pe,next=new Map();for(const [h,w]of hp){if(q<1)next.set(h,(next.get(h)||0)+w*(1-q));if(h>b.loss)next.set(h-b.loss,(next.get(h-b.loss)||0)+w*q);}visit(t+1,b.s,next,damage+b.damage,[...aa,a]);}}
  visit(0,start,new Map([[start.h,1]]),0,[]);
 }
 return {lines:lines.map(l=>({...l,trace:trace(data,p,turns,l.actions,start,mode,probs).rows})),visited:visits,turns,start,character:p.name,mode};
}
function statistics(data,opts,progress=()=>{}){
 const p=profile(data,opts.character,opts.levels),r=data.abilities.rules,turns=schedule(data.bosses,opts.seed).turns,s=initial(data,p,opts.start),base=r.allowStayEvade?6:5,total=base**turns.length;
 let states=new Map([[JSON.stringify([s.h,s.f,s.e,s.z,0]),1]]),dead=new Map();
 for(let t=0;t<turns.length;t++){const next=new Map();for(const [ks,count]of states){const [h,f,e,z,d]=JSON.parse(ks);for(const a of actions({z},r)){const b=transition({h,f,e,z},a,turns[t],p,r),nd=key(d+b.damage);if(b.s.h<=0)dead.set(nd,(dead.get(nd)||0)+count*base**(turns.length-t-1));else {const nk=JSON.stringify([b.s.h,b.s.f,b.s.e,b.s.z,nd]);next.set(nk,(next.get(nk)||0)+count);}}}states=next;assert(states.size<=2000000,'전체 분포 상태가 너무 많습니다. 턴 수를 줄여 주세요.');progress({fraction:(t+1)/turns.length,states:states.size});}
 const alive=new Map();for(const [ks,n]of states){const d=JSON.parse(ks)[4];alive.set(d,(alive.get(d)||0)+n);}const living=sum([...alive.values()]);let max=0;for(const d of alive.keys())max=Math.max(max,d);for(const d of dead.keys())max=Math.max(max,d);const bins=Array.from({length:24},(_,i)=>({lo:max*i/24,hi:max*(i+1)/24,alive:0,dead:0}));for(const [map,field]of [[alive,'alive'],[dead,'dead']])for(const [d,n]of map)bins[Math.min(23,Math.floor(d/(max||1)*24))][field]+=n;
 return {total,living,survival:living/total,bins};
}
// Belief over pattern state: recognition reveals the current attack only. Hidden future
// steps are never passed to the decision function; belief is filtered by observations.
function initialBelief(){return [{queue:[],prev:null,w:1}];}
function expandBelief(b,belief,left){const out=[];for(const s of belief){if(s.queue.length){out.push({...s,id:s.queue[0]});continue;}const pool=candidates(b,left,s.prev),total=sum(pool.map(p=>p.weight));for(const p of pool)out.push({queue:p.steps,prev:p.id,id:p.steps[0],w:s.w*p.weight/total});}return out;}
function normalizeBelief(belief){const map=new Map();for(const s of belief){const k=s.prev+'|'+s.queue.join(',');if(map.has(k))map.get(k).w+=s.w;else map.set(k,{...s});}const total=sum([...map.values()].map(s=>s.w));return [...map.values()].map(s=>({...s,w:s.w/total}));}
function nextBelief(expanded){return normalizeBelief(expanded.map(s=>(({attack,...rest})=>({...rest,queue:rest.queue.slice(1)}))(s)));}
function planner(data,player,policy='optimal'){
 const p=profile(data,player.character,player.levels),r=data.abilities.rules,b=data.bosses,q=player.q/100,threshold=p.maxHp*player.threshold/100,memo=new Map();let nodes=0;
 const costs=(h,nh)=>player.hpCost*(Math.max(0,threshold-nh)-Math.max(0,threshold-h))+(nh<=0?player.deathCost:0);
 const bkey=belief=>belief.map(x=>x.prev+':'+x.queue.join('.')+':'+x.w.toFixed(6)).sort().join(';');
 function value(s,left,belief,depth){if(s.h<=0||left<=0||depth<=0)return 0;const kk=[s.h,s.f,s.e,s.z,left,depth,bkey(belief)].join('|');if(memo.has(kk))return memo.get(kk);assert(++nodes<=1000000,'AI 최적화 상태가 너무 많습니다. 예측 깊이나 패턴 수를 줄여 주세요.');const ex=expandBelief(b,belief,left),ids=[...new Set(ex.map(x=>x.id))];let v=0;if(q<1)v+=(1-q)*rankExpanded(s,left,ex,depth)[0].v;if(q>0)for(const id of ids){const sub=ex.filter(x=>x.id===id),w=sum(sub.map(x=>x.w));v+=q*w*rankExpanded(s,left,sub.map(x=>({...x,w:x.w/w})),depth)[0].v;}memo.set(kk,v);return v;}
 const future=ex=>normalizeBelief(nextBelief(ex).map(x=>b.selection.allowImmediateRepeat?{...x,prev:null}:x));
 function rankExpanded(s,left,ex,depth){const after=future(ex);return actions(s,r).map(a=>{let v=0;for(const item of ex){const variants=item.attack?[item.attack]:attackVariants(b.actions.find(x=>x.id===item.id));for(const attack of variants){const tr=transition(s,a,attack,p,r);v+=item.w/variants.length*(policy==='defense'?(a===2?1:0):policy==='attack'?(a<2?tr.damage:-1):policy==='evade'?(a>=3?-tr.loss:-1e6):tr.damage-costs(s.h,tr.s.h)+(tr.s.h>0?value(tr.s,left-1,after,depth-1):0));}}return {a,v};}).sort((x,y)=>y.v-x.v||x.a-y.a);}
 return {rank(s,left,belief,knownAttack){const knownId=typeof knownAttack==='string'?knownAttack:knownAttack?.id;let ex=expandBelief(b,belief,left);if(knownId){const sub=ex.filter(x=>x.id===knownId);if(sub.length)ex=normalizeBelief(sub).map(x=>({...x,id:x.queue[0]}));}if(knownAttack&&typeof knownAttack==='object')ex=ex.map(x=>({...x,attack:knownAttack}));const ranked=rankExpanded(s,left,ex,Math.min(left,player.lookahead||10));return {ranked,belief:future(ex)};},p};
}
function simulate(data,opts,progress=()=>{}){
 validate(data);const seq=opts.sequences||1,n=opts.iterations||1,players=opts.players,r=data.abilities.rules,b=data.bosses;number(seq,1,100,'시퀀스',true);number(n,1,50000,'반복 횟수',true);assert(players.length>0&&players.length<=30,'AI 인원은 1~30명입니다.');
 players.forEach(p=>{number(p.q,0,100,'판독 확률');number(p.threshold,0,100,'HP 임계치');number(p.hpCost,0,1000,'HP 비용');number(p.deathCost,0,10000,'사망 비용');number(p.lookahead,1,10,'예측 턴',true);});
 const planCache=new Map(),plans=players.map(p=>{const k=JSON.stringify([p.character,p.levels,p.q,p.threshold,p.hpCost,p.deathCost,p.lookahead]);if(!planCache.has(k))planCache.set(k,planner(data,p,opts.policy||'optimal'));return planCache.get(k);}),fixedRanks=new Map(),by=players.map(()=>({dead:0,damage:0,hp:0,counts:[0,0,0]})),hist=Array(players.length+1).fill(0),logs=[],teamDamages=[];let lastStates,lastDeathTurns=[],patternCounts={};
 for(let iter=0;iter<n;iter++){const random=rng((opts.seed+Math.imul(iter+1,2654435761))>>>0),states=players.map((p,i)=>({...initial(data,plans[i].p,{zone:p.zone}),counts:[0,0,0],deadTurn:null}));let beliefs=players.map(initialBelief),globalTurn=0;
  for(let cycle=0;cycle<seq&&states.some(s=>s.h>0);cycle++){const sch=schedule(b,(opts.seed+Math.imul(iter,374761393)+Math.imul(cycle,668265263))>>>0);beliefs=players.map(initialBelief);sch.patterns.forEach(id=>patternCounts[id]=(patternCounts[id]||0)+1);
   // Fixed schedules are known inputs for deterministic policy; q still hides the current attack.
   const fixed=b.preset.mode!=='random';
   for(let t=0;t<sch.turns.length&&states.some(s=>s.h>0);t++){globalTurn++;const attack=sch.turns[t],rows=[];for(let i=0;i<players.length;i++){const s=states[i],pl=players[i];if(s.h<=0)continue;const recognized=random()<pl.q/100;let rank;
    if(fixed){const fk=[cycle,JSON.stringify(sch.turns.map(x=>x.damageByZone)),i,t,s.h,s.f,s.e,s.z,recognized].join(',');rank=fixedRanks.get(fk);if(!rank){rank=rankFixed(data,pl,s,sch.turns,t,recognized,opts.policy||'optimal');fixedRanks.set(fk,rank);}
    }else {const result=plans[i].rank(s,sch.turns.length-t,beliefs[i],recognized?attack:null);rank=result.ranked;beliefs[i]=result.belief;}
    const a=rank[0].a,crit=a<2&&random()<plans[i].p.skills[a].critChance,before=clone(s),tr=transition(s,a,attack,plans[i].p,r,{crit});Object.assign(s,tr.s);s.counts[a<2?0:a===2?1:2]++;if(s.h<=0)s.deadTurn=globalTurn;if(n===1)rows.push({i,name:pl.name,a,recognized,crit,damage:tr.damage,loss:tr.loss,before,after:clone(s),score:rank[0].v});
   }if(n===1)logs.push({turn:globalTurn,cycle:cycle+1,attack:clone(attack),rows});}
  }
  const dead=states.filter(s=>s.h<=0).length;hist[dead]++;teamDamages.push(sum(states.map(s=>s.damage)));states.forEach((s,i)=>{by[i].dead+=s.h<=0?1:0;by[i].damage+=s.damage;by[i].hp+=s.h;s.counts.forEach((v,j)=>by[i].counts[j]+=v);if(s.deadTurn)lastDeathTurns.push(s.deadTurn);});lastStates=states;if(iter%20===0)progress({fraction:(iter+1)/n});
 }
 const mean=sum(teamDamages)/n,se=Math.sqrt(sum(teamDamages.map(x=>(x-mean)**2))/Math.max(1,n-1)/n);return {iterations:n,sequences:seq,hist,by:by.map(x=>({...x,dead:x.dead/n,damage:x.damage/n,hp:x.hp/n})),survival:1-sum(by.map(x=>x.dead))/(n*players.length),damage:mean,damageCI:[mean-1.96*se,mean+1.96*se],logs,states:lastStates,deathTurn:lastDeathTurns.length?sum(lastDeathTurns)/lastDeathTurns.length:null,patternCounts};
}
function rankFixed(data,pl,start,turns,index,recognized,policy){const p=profile(data,pl.character,pl.levels),r=data.abilities.rules,threshold=p.maxHp*pl.threshold/100,memo=new Map();function val(s,t,depth){if(s.h<=0||t>=turns.length||depth<=0)return 0;const kk=[s.h,s.f,s.e,s.z,t,depth].join();if(memo.has(kk))return memo.get(kk);const v=rank(s,t,depth,true)[0].v;memo.set(kk,v);return v;}function rank(s,t,depth,known){const pool=known?[turns[t]]:turns;return actions(s,r).map(a=>{let v=0;for(const attack of pool){const tr=transition(s,a,attack,p,r),cost=pl.hpCost*(Math.max(0,threshold-tr.s.h)-Math.max(0,threshold-s.h))+(tr.s.h<=0?pl.deathCost:0);v+=(policy==='attack'?(a<2?tr.damage:-1):policy==='defense'?(a===2?1:0):policy==='evade'?(a>=3?-tr.loss:-1e6):tr.damage-cost+val(tr.s,t+1,depth-1))/pool.length;}return {a,v};}).sort((x,y)=>y.v-x.v||x.a-y.a);}return rank(start,index,Math.min(pl.lookahead,turns.length-index),recognized);}
return {zones,names,clone,importBoss,exportBoss,syncState,validate,rng,candidates,schedule,profile,initial,actions,fever,attackDamage,expectedDamage,transition,trace,search,statistics,simulate,planner};
}
if(typeof module!=='undefined')module.exports=createEngine;
