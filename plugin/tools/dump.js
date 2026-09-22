const { chromium } = require("playwright");
(async () => { const b = await chromium.launch(); const p = await b.newPage();
  await p.goto("http://localhost:8777/vireo/", { waitUntil:"networkidle" });
  const j = await p.evaluate(()=>{ const spec={}; for(const k in S){ const s=S[k]; spec[k]={t:s.t,min:s.min,max:s.max,def:s.def,curve:s.curve,step:s.step||0,opts:s.opts||null,names:s.opts?s.opts.map(o=>fmtVal(k,o)):null,label:labelOf(k)}; }
    return JSON.stringify({spec,order:Object.keys(S),presets:PRESETS.map(p=>({cat:p.cat,name:p.name,p:p.p}))}); });
  require("fs").writeFileSync(""+__dirname+"/vireo.json", j); console.log("dumped", j.length); await b.close(); })();
