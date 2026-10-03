const fs=require('fs'),path=require('path'),{spawn}=require('child_process'),assert=require('assert');
const root=path.resolve(__dirname,'..'),folder=path.join(root,'build/settings-http-test');
const child=spawn(path.join(root,'build/serve-settings-test.exe'),[],{cwd:root,windowsHide:true,stdio:'ignore'});
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
(async()=>{
 try{
  let config;for(let i=0;i<100;i++){await sleep(50);if(fs.existsSync(folder+'/connection.json')){config=JSON.parse(fs.readFileSync(folder+'/connection.json'));break;}}
  assert(config,'Test settings connection did not start');
  const url=`http://127.0.0.1:${config.port}/${config.key}/`;
  // Copy only the two static settings resources into the isolated test directory.
  for(const name of ['settings.html','localization.js'])fs.copyFileSync(root+'/assets/'+name,folder+'/'+name);
  const get=route=>fetch(url+route);
  const post=(route,body,key=config.key)=>fetch(url+'api/'+route,{method:'POST',headers:{'X-Pet-Key':key,'Content-Type':'application/json',Origin:`http://127.0.0.1:${config.port}`},body:JSON.stringify(body)});
  let response=await get('');assert.equal(response.status,200);assert((await response.text()).includes('id="languageSection"'));
  response=await get('localization.js');assert.equal(response.status,200);assert(response.headers.get('content-type').startsWith('text/javascript'));assert((await response.text()).includes('Guardar idioma'));
  assert.equal((await(await get('api/state')).json()).language,0);
  for(let language=0;language<5;language++){
   response=await post('language',{language});assert.equal(response.status,200);assert.equal((await response.json()).language,language);
   response=await post('settings',{badgeColor:2,animation:1,customIcon:0});assert.equal(response.status,200);assert.equal((await response.json()).language,language);
   assert(fs.readFileSync(folder+'/pet-settings.txt','utf8').includes(`language=${language}`));
  }
  for(const language of [-1,5,1.5,'ko'])assert.equal((await post('language',{language})).status,400);
  assert.equal((await post('language',{language:0},'wrong-key')).status,403);
  assert.equal((await(await get('api/state')).json()).language,4);
  assert.equal((await post('language',{language:0})).status,200);
  console.log('PASS: live loopback HTTP page/bundle, five language changes, saved preferences, invalid IDs, authenticated mutations');
 }finally{
  child.kill();await sleep(200);
  // Only this dedicated fixed test folder is removed; no installed settings are touched.
  if(path.dirname(path.resolve(folder))!==path.join(root,'build'))throw Error('Test cleanup escaped build folder');
  fs.rmSync(folder,{recursive:true,force:true});
 }
})().catch(error=>{console.error(error.message);process.exitCode=1});
