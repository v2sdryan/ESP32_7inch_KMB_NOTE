#pragma once

#include <Arduino.h>

static const char WIFI_SETTINGS_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="zh-HK"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wi-Fi 管理 · BusETA</title><style>
:root{--ink:#12202b;--paper:#f4f1e8;--red:#d92b21;--line:#c8d0d2;--muted:#64737b}*{box-sizing:border-box}body{margin:0;background:var(--paper);color:var(--ink);font-family:"PingFang HK","Noto Sans TC","Microsoft JhengHei",sans-serif}.wrap{width:min(820px,100%);margin:auto;padding:18px}.head{background:var(--ink);color:#fff;border-radius:16px;padding:22px}.head h1{margin:0 0 6px}.head p{margin:0;color:#c2cdd2}.tools,.actions{display:flex;gap:10px;flex-wrap:wrap;margin:14px 0}.btn{border:0;border-radius:10px;padding:12px 16px;font-weight:800;cursor:pointer;background:var(--ink);color:#fff}.btn.red{background:var(--red)}.btn.light{background:#fff;color:var(--ink);border:1px solid var(--line)}.btn:disabled{opacity:.5}.notice{background:#fff4d8;border-left:5px solid #e19a14;padding:12px;margin:14px 0}.networks{display:grid;gap:12px}.network{background:#fff;border:1px solid var(--line);border-radius:14px;padding:16px}.network-top{display:flex;gap:10px;align-items:center;flex-wrap:wrap}.ssid{font-size:21px;font-weight:900}.badge{background:var(--red);color:#fff;border-radius:99px;padding:4px 9px;font-size:12px}.saved{color:var(--muted);font-size:13px}.row{display:grid;grid-template-columns:1fr auto;gap:8px;margin-top:12px}input{width:100%;border:1px solid var(--line);border-radius:9px;padding:11px;font:inherit}.small{padding:9px 11px}.clear{display:flex;align-items:center;gap:7px;margin-top:10px;color:var(--muted)}.clear input{width:auto}.status{min-height:24px;font-weight:700}.ok{color:#087d45}.err{color:#b51f19}.empty{background:#fff;border:1px dashed var(--line);padding:28px;text-align:center;border-radius:14px;color:var(--muted)}@media(max-width:560px){.wrap{padding:10px}.row{grid-template-columns:1fr}.tools .btn,.actions .btn{flex:1 1 45%}}
</style></head><body><main class="wrap">
<header class="head"><h1>Wi-Fi 管理</h1><p>最多儲存四個網絡；ESP32 會先嘗試「首選」，失敗後依次嘗試其他網絡。</p></header>
<div class="notice">安全提示：現有密碼不會顯示或傳送到瀏覽器。密碼欄留空會保留原有密碼。</div>
<div class="tools"><button class="btn" id="scan" disabled>掃描附近 Wi-Fi</button><button class="btn light" id="add" disabled>手動新增</button></div>
<div id="scan-note" class="status"></div><section id="networks" class="networks"></section>
<div class="actions"><button class="btn red" id="save" disabled>儲存 Wi-Fi 設定</button><button class="btn light" id="reboot">重新啟動並連線</button></div>
<div id="status" class="status">正在讀取設定…</div></main><script>
(function(){
  'use strict';
  var items=[];
  var loaded=false;
  var saving=false;
  var box=document.getElementById('networks');
  var status=document.getElementById('status');
  var scanNote=document.getElementById('scan-note');
  var saveButton=document.getElementById('save');
  var scanButton=document.getElementById('scan');
  var addButton=document.getElementById('add');

  function text(value){return String(value==null?'':value)}
  function message(value,ok){status.textContent=value;status.className='status '+(ok?'ok':'err')}
  function setLoaded(value){loaded=value;saveButton.disabled=!value;scanButton.disabled=!value;addButton.disabled=!value}
  function lockEditor(value){saving=value;saveButton.disabled=value||!loaded;scanButton.disabled=value||!loaded;addButton.disabled=value||!loaded;render()}
  function render(){
    box.textContent='';
    if(!items.length){
      var empty=document.createElement('div');empty.className='empty';
      empty.textContent=loaded?'尚未儲存 Wi-Fi。按「掃描附近 Wi-Fi」或「手動新增」。':'正在讀取 Wi-Fi 設定…';
      box.appendChild(empty);return;
    }
    items.forEach(function(item,index){
      var card=document.createElement('article');card.className='network';
      var top=document.createElement('div');top.className='network-top';
      var name=document.createElement('span');name.className='ssid';name.textContent=item.ssid;top.appendChild(name);
      if(index===0){var badge=document.createElement('span');badge.className='badge';badge.textContent='首選';top.appendChild(badge)}
      var saved=document.createElement('span');saved.className='saved';saved.textContent=item.password_configured?'已儲存密碼':'沒有儲存密碼';top.appendChild(saved);card.appendChild(top);
      var row=document.createElement('div');row.className='row';
      var pass=document.createElement('input');pass.type='password';pass.disabled=saving;pass.placeholder=item.password_configured?'輸入新密碼，或留空保留現有密碼':'輸入密碼；開放網絡可留空';pass.value=item.password||'';pass.oninput=function(){item.password=pass.value};row.appendChild(pass);
      var show=document.createElement('button');show.type='button';show.className='btn light small';show.disabled=saving;show.textContent='顯示';show.onclick=function(){pass.type=pass.type==='password'?'text':'password';show.textContent=pass.type==='password'?'顯示':'隱藏'};row.appendChild(show);card.appendChild(row);
      var clearLabel=document.createElement('label');clearLabel.className='clear';var clear=document.createElement('input');clear.type='checkbox';clear.disabled=saving;clear.checked=!!item.clear_password;clear.onchange=function(){item.clear_password=clear.checked;if(clear.checked)item.password='';render()};clearLabel.appendChild(clear);clearLabel.appendChild(document.createTextNode(' 清除已儲存密碼（只適用開放網絡）'));card.appendChild(clearLabel);
      var controls=document.createElement('div');controls.className='tools';
      function button(label,fn,disabled){var b=document.createElement('button');b.type='button';b.className='btn light small';b.textContent=label;b.onclick=fn;b.disabled=disabled||saving;controls.appendChild(b)}
      button('設為首選',function(){items.splice(index,1);items.unshift(item);render()},index===0);
      button('上移',function(){var other=items[index-1];items[index-1]=item;items[index]=other;render()},index===0);
      button('下移',function(){var other=items[index+1];items[index+1]=item;items[index]=other;render()},index===items.length-1);
      button('刪除',function(){if(items.length===1&&!confirm('刪除最後一個 Wi-Fi 後，重新啟動會進入 BusETA-Config 設定模式。確定刪除？'))return;items.splice(index,1);render()},false);
      card.appendChild(controls);box.appendChild(card);
    });
  }
  function addNetwork(ssid,secure){
    ssid=text(ssid).trim();
    if(!ssid){message('Wi-Fi 名稱不可留空。',false);return}
    if(items.length>=4){message('最多只可儲存四個 Wi-Fi。',false);return}
    if(items.some(function(item){return item.ssid===ssid})){message('呢個 Wi-Fi 已經喺清單內。',false);return}
    items.push({ssid:ssid,password:'',password_configured:false,clear_password:false,secure:secure!==false});render();
  }
  addButton.onclick=function(){var ssid=prompt('請輸入 Wi-Fi 名稱（SSID）');if(ssid!==null)addNetwork(ssid,true)};
  scanButton.onclick=function(){
    var button=this;button.disabled=true;scanNote.textContent='正在掃描…';
    function poll(){
      fetch('/api/wifi/scan').then(function(response){return response.json().then(function(data){if(!response.ok)throw new Error(data.error||'scan');return{pending:response.status===202,data:data}})}).then(function(result){
        if(result.pending){setTimeout(poll,500);return}
        var networks=result.data.networks||[];var names=networks.map(function(network){return network.ssid+(network.secure?'（有密碼）':'（開放）')}).join('\n');
        var chosen=prompt('附近 Wi-Fi：\n\n'+names+'\n\n請輸入要新增嘅 SSID：',networks.length?networks[0].ssid:'');
        if(chosen){var found=networks.filter(function(network){return network.ssid===chosen})[0];addNetwork(chosen,found?found.secure:true)}
        scanNote.textContent='已找到 '+networks.length+' 個網絡。';button.disabled=false;
      }).catch(function(error){scanNote.textContent=error.message==='scan rate limited'?'請等幾秒再掃描。':'掃描失敗，請用「手動新增」。';button.disabled=false})
    }poll();
  };
  saveButton.onclick=function(){
    if(!loaded){message('設定尚未載入，未有作出更改。',false);return}
    if(!items.length&&!confirm('清空全部 Wi-Fi 後，重新啟動會進入 BusETA-Config 設定模式。確定儲存？'))return;
    for(var i=0;i<items.length;i++){
      items[i].ssid=text(items[i].ssid).trim();var password=items[i].password||'';
      if(!items[i].ssid){message('Wi-Fi 名稱不可留空。',false);return}
      if(items[i].secure&&!items[i].password_configured&&!password){message(items[i].ssid+' 係有密碼網絡，請輸入密碼。',false);return}
      if(password&&password.length<8){message(items[i].ssid+' 密碼最少要 8 個字元。',false);return}
      if(password.length===64&&!/^[0-9a-f]+$/i.test(password)){message(items[i].ssid+' 的 64 字元密碼必須係十六進制。',false);return}
    }
    var payload=items.map(function(item){return{ssid:item.ssid,password:item.password||'',keep_password:!!item.password_configured&&!item.password&&!item.clear_password,clear_password:!!item.clear_password}});
    lockEditor(true);
    fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({wifi:{networks:payload}})}).then(function(response){return response.json().then(function(data){if(!response.ok||!data.ok)throw new Error(data.error||'save')})}).then(function(){
      return loadConfig('已儲存。按「重新啟動並連線」套用新次序。');
    }).catch(function(error){lockEditor(false);message('儲存失敗：'+error.message,false)});
  };
  document.getElementById('reboot').onclick=function(){if(!confirm('確定重新啟動 ESP32？'))return;fetch('/api/reboot',{method:'POST'}).catch(function(){});message('ESP32 正在重新啟動，請稍候再連線。',true)};
  render();
  function loadConfig(successMessage){
    return fetch('/api/config').then(function(response){if(!response.ok)throw new Error('load');return response.json()}).then(function(config){
      items=((config.wifi&&config.wifi.networks)||[]).map(function(network){return{ssid:network.ssid,password:'',password_configured:!!network.password_configured,clear_password:false,secure:!!network.password_configured}});
      setLoaded(true);saving=false;message(successMessage||'設定已載入。',true);render();
    }).catch(function(error){setLoaded(false);saving=false;message('未能讀取 Wi-Fi 設定；為安全起見已停用儲存。請重新載入頁面。',false);render();throw error});
  }
  loadConfig().catch(function(){});
})();</script></body></html>
)HTML";
