#pragma once

#include <Arduino.h>

static const char KMB_FAVORITES_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="zh-HK"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>九巴路線及車站</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#eef2f5;color:#15202b;font-family:-apple-system,BlinkMacSystemFont,"Noto Sans TC",sans-serif}.wrap{max-width:760px;margin:auto;padding:16px}.card{background:#fff;border-radius:14px;padding:16px;margin-bottom:14px;box-shadow:0 2px 10px #0001}h1,h2{margin:0 0 12px}.row{display:flex;gap:8px}input{min-width:0;flex:1;font-size:22px;padding:12px;border:2px solid #b8c5d0;border-radius:10px;text-transform:uppercase}button,.btn{border:0;border-radius:10px;padding:12px 16px;background:#e2231a;color:#fff;font-size:18px;font-weight:700;cursor:pointer}.dark{background:#20252b}.item{display:flex;align-items:center;gap:10px;padding:12px 4px;border-bottom:1px solid #dde3e8}.item:last-child{border-bottom:0}.main{flex:1}.route{display:inline-block;background:#e2231a;color:#fff;border-radius:7px;padding:5px 10px;font-weight:800;margin-right:8px}.muted,.status{color:#607080}.status{padding:10px 0}.error{color:#b42318}.ok{color:#18794e}.list button{font-size:16px}.remove{background:#8c1d18;padding:8px 11px}.back{display:inline-block;color:#20252b;margin-bottom:12px}.hidden{display:none}
</style></head><body><div class="wrap">
<a class="back" href="/">← 返回裝置設定</a>
<div class="card"><h1>九巴路線及車站</h1>
<div class="row"><input id="route" maxlength="6" placeholder="輸入路線，例如 1A、93K"><button id="search">搜尋</button></div>
<div id="status" class="status">先輸入巴士路線。</div><div id="directions" class="list"></div><div id="stops" class="list"></div></div>
<div class="card"><h2>已收藏車站</h2><div id="favorites"></div>
<button id="save">儲存到 ESP32</button><div id="save-status" class="status"></div></div>
</div><script>
(function(){'use strict';
var API='https://data.etabus.gov.hk/v1/transport/kmb';
var favorites=[],routeRowsPromise=null,stopMapPromise=null,currentRoute='';
function $(id){return document.getElementById(id)}
function text(tag,value,cls){var e=document.createElement(tag);e.textContent=value;if(cls)e.className=cls;return e}
function setStatus(message,isError){$('status').textContent=message;$('status').className='status'+(isError?' error':'')}
function getJson(url){return fetch(url).then(function(r){if(!r.ok)throw new Error('HTTP '+r.status);return r.json()})}
function routeRows(){if(!routeRowsPromise)routeRowsPromise=getJson(API+'/route').then(function(x){return x.data||[]}).catch(function(error){routeRowsPromise=null;throw error});return routeRowsPromise}
function stopMap(){if(!stopMapPromise)stopMapPromise=getJson(API+'/stop').then(function(x){var map={};(x.data||[]).forEach(function(s){if(s&&s.stop)map[s.stop]=s.name_tc||s.stop});return map}).catch(function(error){stopMapPromise=null;throw error});return stopMapPromise}
function normalizeRoute(value){return String(value||'').trim().toUpperCase()}
function renderFavorites(){var box=$('favorites');box.textContent='';if(!favorites.length){box.appendChild(text('p','尚未收藏車站。','muted'));return}favorites.forEach(function(f,index){var row=text('div','', 'item');var main=text('div','', 'main');var top=text('div','');top.appendChild(text('span',f.route,'route'));top.appendChild(text('span','往 '+f.destination_tc));main.appendChild(top);main.appendChild(text('div',f.stop_name_tc,'muted'));var remove=text('button','刪除','remove');remove.type='button';remove.onclick=function(){favorites.splice(index,1);renderFavorites()};row.appendChild(main);row.appendChild(remove);box.appendChild(row)})}
function addFavorite(direction,stop){if(favorites.length>=8){setStatus('最多只可收藏八個車站。',true);return}var duplicate=favorites.some(function(f){return f.route===currentRoute&&f.direction===direction.bound&&String(f.service_type)===String(direction.service_type)&&f.stop_id===stop.stop});if(duplicate){setStatus('呢個車站已經收藏。',true);return}favorites.push({route:currentRoute,direction:direction.bound,service_type:parseInt(direction.service_type,10),destination_tc:direction.dest_tc,stop_id:stop.stop,stop_name_tc:stop.name_tc||stop.stop});renderFavorites();setStatus('已加入 '+stop.name_tc+'；記得按「儲存到 ESP32」。',false)}
function showStops(direction){$('stops').textContent='';setStatus('正在載入車站…',false);var endpoint=direction.bound==='I'?'inbound':'outbound';Promise.all([getJson(API+'/route-stop/'+encodeURIComponent(currentRoute)+'/'+endpoint+'/'+direction.service_type),stopMap()]).then(function(values){var rows=(values[0]&&values[0].data)||[],names=values[1];$('directions').textContent='';$('stops').textContent='';if(!rows.length)throw new Error('empty');rows.sort(function(a,b){return parseInt(a.seq,10)-parseInt(b.seq,10)});rows.forEach(function(s){s.name_tc=names[s.stop]||s.stop;var row=text('div','', 'item');var main=text('div',(s.seq||'')+'. '+s.name_tc,'main');var add=text('button','加入');add.type='button';add.onclick=function(){addFavorite(direction,s)};row.appendChild(main);row.appendChild(add);$('stops').appendChild(row)});setStatus('請選擇車站。',false)}).catch(function(){setStatus('車站載入失敗，請檢查網絡後重試。',true)})}
function search(){currentRoute=normalizeRoute($('route').value);$('directions').textContent='';$('stops').textContent='';if(!/^[0-9A-Z]{1,6}$/.test(currentRoute)){setStatus('請輸入有效九巴路線。',true);return}setStatus('正在搜尋 '+currentRoute+'…',false);routeRows().then(function(rows){var matches=rows.filter(function(r){return normalizeRoute(r.route)===currentRoute&&r.dest_tc&&(/^[IO]$/.test(r.bound))});if(!matches.length)throw new Error('not-found');setStatus('請選擇方向。',false);matches.forEach(function(direction){var row=text('div','', 'item');var main=text('div','往 '+direction.dest_tc,'main');main.appendChild(text('div','方向 '+direction.bound+' · 服務類別 '+direction.service_type,'muted'));var choose=text('button','選擇');choose.type='button';choose.onclick=function(){showStops(direction)};row.appendChild(main);row.appendChild(choose);$('directions').appendChild(row)})}).catch(function(error){if(error&&error.message==='not-found')setStatus('搵唔到路線 '+currentRoute+'，請確認路線編號。',true);else setStatus('未能連接九巴資料服務，請檢查上網後重試。',true)})}
$('search').onclick=search;$('route').onkeydown=function(e){if(e.key==='Enter'){e.preventDefault();search()}};
$('save').onclick=function(){var button=this;button.disabled=true;$('save-status').textContent='正在儲存…';fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({kmb_favorites:favorites,kmb_defaults_initialized:true})}).then(function(r){return r.json().then(function(body){if(!r.ok||!body.ok)throw new Error(body.error||'save');return body})}).then(function(){$('save-status').textContent='已儲存；巴士 ETA 將會自動更新。';$('save-status').className='status ok'}).catch(function(e){$('save-status').textContent='儲存失敗：'+e.message;$('save-status').className='status error'}).then(function(){button.disabled=false})};
getJson('/api/config').then(function(cfg){favorites=Array.isArray(cfg.kmb_favorites)?cfg.kmb_favorites:[];renderFavorites()}).catch(function(){renderFavorites();$('save-status').textContent='未能讀取現有收藏。';$('save-status').className='status error'});
})();
</script></body></html>
)HTML";
