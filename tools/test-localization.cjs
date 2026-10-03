/* Exercise text-node/attribute switching without taking control of a browser. */
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const root=path.resolve(__dirname,'..'),dictionary=JSON.parse(fs.readFileSync(root+'/assets/browser-translations.json','utf8'));
const keys=Object.keys(dictionary);
const textNodes=keys.map(key=>({nodeValue:' '+dictionary[key][0]+' ',parentElement:{closest:()=>null}}));
const placeholder={attrs:{placeholder:dictionary['예: 둥근 얼굴의 하얀 유령. 보라색 모자, 짧은 팔, 단순하고 선명한 윤곽.'][0]},getAttribute(name){return this.attrs[name]},setAttribute(name,value){this.attrs[name]=value}};
const document={documentElement:{lang:''},title:'',createTreeWalker(){let index=0;return {nextNode:()=>textNodes[index++]||null}},querySelectorAll(){return [placeholder]}};
const context=vm.createContext({document,NodeFilter:{SHOW_TEXT:4}});
vm.runInContext(fs.readFileSync(root+'/assets/localization.js','utf8'),context);
for(const language of [0,1,2,3,4,0,4,2,1,0]){
 vm.runInContext(`applyUiLanguage(${language})`,context);
 for(const key of keys)assert.equal(vm.runInContext(`t(${JSON.stringify(key)})`,context),dictionary[key][language]);
 // English aliases may share a key; ensure every visible node matches its resolved translation.
 const aliases=new Map(Object.entries(dictionary).map(([key,v])=>[v[0].trim(),key]));
 textNodes.forEach((node,index)=>assert.equal(node.nodeValue.trim(),dictionary[aliases.get(dictionary[keys[index]][0].trim())][language].trim()));
 assert.equal(placeholder.attrs.placeholder,dictionary['예: 둥근 얼굴의 하얀 유령. 보라색 모자, 짧은 팔, 단순하고 선명한 윤곽.'][language]);
 const prompt=vm.runInContext(`makeLocalizedPrompt('Test character','Motion',200,'energetic')`,context);
 assert(prompt.includes('128×128')&&prompt.includes('84×84')&&prompt.includes('32×32')&&prompt.includes('30×30')&&prompt.includes('1MiB')&&prompt.includes('pet_01.png')&&prompt.includes('pet_08.png'));
 assert(!prompt.includes('{character}'));
}
const html=fs.readFileSync(root+'/assets/settings.html','utf8');
for(const match of html.matchAll(/<script>([\s\S]*?)<\/script>/g))new vm.Script(match[1]);
assert(html.indexOf('id="languageSection"')<html.indexOf('<header>'));
assert(html.includes("request('language',{language:wanted})"));
// Every dynamic translation key must exist, including keys added for feedback messages.
for(const match of html.matchAll(/\bt\(("(?:[^"\\]|\\.)*"|'[^']*')\)/g)){
 const key=match[1][0]==='"'?JSON.parse(match[1]):match[1].slice(1,-1);
 assert(dictionary[key],'Missing dynamic key: '+key);
}
const c=fs.readFileSync(root+'/src/CodexPetLite.c','utf8');
assert(c.indexOf('AppendMenuW(root,MF_POPUP,(UINT_PTR)languages')<c.indexOf('_snwprintf(title,80,tr(TXT_status)'));
console.log('PASS: five languages, repeated DOM switching, translated attributes/prompts, topmost selectors, browser action wiring, script syntax');
