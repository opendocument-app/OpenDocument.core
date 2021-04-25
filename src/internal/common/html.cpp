#include <internal/common/html.h>
#include <internal/util/string_util.h>
#include <odr/html.h>

namespace odr::internal::common {

const char *html::doctype() noexcept {
  // clang-format off
  return R"V0G0N(<!DOCTYPE html>
)V0G0N";
  // clang-format on
}

const char *html::default_headers() noexcept {
  // clang-format off
  return R"V0G0N(
<meta charset="UTF-8"/>
<base target="_blank"/>
<meta name="viewport" content="width=device-width,initial-scale=1.0,user-scalable=yes"/>
<title>odr</title>
  )V0G0N";
  // clang-format on
}

const char *html::default_style() noexcept {
  // clang-format off
  return R"V0G0N(
* {
  margin: 0px;
  position: relative;
}
body {
  padding: 5px;
}
p {
  padding: 0 !important;
}
p:after {
  content: "";
  display: inline-block;
  width: 0px;
}
span {
  margin: 0 !important;
}
span {
  white-space: pre-wrap;
}

/* https://github.com/marcelblanarik/js-keyword-highlighter/blob/dd69436bee06f8c658abe1e12e2abb35d3bf250b/index.html#L81-L82 */
mark { background: yellow; }
mark.current { background: orange; }
  )V0G0N";
  // clang-format on
}

const char *html::default_spreadsheet_style() noexcept {
  // clang-format off
  return R"V0G0N(
table {
  border-collapse: collapse;
  table-layout: fixed;
}
table td {
  vertical-align: top;
}

p {
  font-family: "Arial";
  font-size: 10pt;
}

.odr-gridlines-none table td {}
.odr-gridlines-soft table td {
  border:1px solid #C0C0C0;
}
.odr-gridlines-hard table td {
  border:1px solid #C0C0C0 !important;
}

table td.odr-value-type-float {
  text-align: right;
}
  )V0G0N";
  // clang-format on
}

const char *html::default_script() noexcept {
  // clang-format off
  return R"V0G0N(
  // babel from `resources/edit.js`
  "use strict";function _createForOfIteratorHelper(a){if("undefined"==typeof Symbol||null==a[Symbol.iterator]){if(Array.isArray(a)||(a=_unsupportedIterableToArray(a))){var b=0,c=function(){};return{s:c,n:function n(){return b>=a.length?{done:!0}:{done:!1,value:a[b++]}},e:function e(a){throw a},f:c}}throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.")}var d,e,f=!0,g=!1;return{s:function s(){d=a[Symbol.iterator]()},n:function n(){var a=d.next();return f=a.done,a},e:function e(a){g=!0,e=a},f:function f(){try{f||null==d["return"]||d["return"]()}finally{if(g)throw e}}}}function _slicedToArray(a,b){return _arrayWithHoles(a)||_iterableToArrayLimit(a,b)||_unsupportedIterableToArray(a,b)||_nonIterableRest()}function _nonIterableRest(){throw new TypeError("Invalid attempt to destructure non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.")}function _unsupportedIterableToArray(a,b){if(a){if("string"==typeof a)return _arrayLikeToArray(a,b);var c=Object.prototype.to_string.call(a).slice(8,-1);return"Object"===c&&a.constructor&&(c=a.constructor.name),"Map"===c||"Set"===c?Array.from(a):"Arguments"===c||/^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(c)?_arrayLikeToArray(a,b):void 0}}function _arrayLikeToArray(a,b){(null==b||b>a.length)&&(b=a.length);for(var c=0,d=Array(b);c<b;c++)d[c]=a[c];return d}function _iterableToArrayLimit(a,b){if("undefined"!=typeof Symbol&&Symbol.iterator in Object(a)){var c=[],d=!0,e=!1,f=void 0;try{for(var g,h=a[Symbol.iterator]();!(d=(g=h.next()).done)&&(c.push(g.value),!(b&&c.length===b));d=!0);}catch(a){e=!0,f=a}finally{try{d||null==h["return"]||h["return"]()}finally{if(e)throw f}}return c}}function _arrayWithHoles(a){if(Array.isArray(a))return a}(function(){function a(a,b){var c=document.createElement("a");c.setAttribute("href","data:text/plain;charset=utf-8,"+encodeURIComponent(b)),c.setAttribute("download",a),c.style.display="none",document.body.appendChild(c),c.click(),document.body.removeChild(c)}function b(){for(var a={modifiedText:{}},b=0,d=Object.entries(c.modifiedText);b<d.length;b++){var e=_slicedToArray(d[b],2),f=e[0],g=e[1];a.modifiedText[f]=g.innerText}return JSON.stringify(a)}window.addEventListener("keydown",function(c){(c.ctrlKey||c.metaKey)&&"s"===c.key&&(c.preventDefault(),a("test.json",b()))});var c={modifiedText:{}},d=new MutationObserver(function(a){var b,d=_createForOfIteratorHelper(a);try{for(d.s();!(b=d.n()).done;){var g=b.value;if("characterData"===g.type){var e=g.target.parentNode,f=e.getAttribute("data-odr-cid");f&&(c.modifiedText[parseInt(f)]=e)}else console.log(g)}}catch(a){d.e(a)}finally{d.f()}});d.observe(document.body,{attributes:!1,childList:!0,subtree:!0,characterData:!0});var e=window.odr||{};e.download=a,e.generateDiff=b,window.odr=e})();
  // https://cdnjs.cloudflare.com/ajax/libs/mark.js/8.11.1/mark.es6.min.js
  !function(e,t){"object"==typeof exports&&"undefined"!=typeof module?module.exports=t():"function"==typeof define&&define.amd?define(t):e.Mark=t()}(this,function(){"use strict";class e{constructor(e,t=!0,s=[],r=5e3){this.ctx=e,this.iframes=t,this.exclude=s,this.iframesTimeout=r}static matches(e,t){const s="string"==typeof t?[t]:t,r=e.matches||e.matchesSelector||e.msMatchesSelector||e.mozMatchesSelector||e.oMatchesSelector||e.webkitMatchesSelector;if(r){let t=!1;return s.every(s=>!r.call(e,s)||(t=!0,!1)),t}return!1}getContexts(){let e,t=[];return(e=void 0!==this.ctx&&this.ctx?NodeList.prototype.isPrototypeOf(this.ctx)?Array.prototype.slice.call(this.ctx):Array.isArray(this.ctx)?this.ctx:"string"==typeof this.ctx?Array.prototype.slice.call(document.querySelectorAll(this.ctx)):[this.ctx]:[]).forEach(e=>{const s=t.filter(t=>t.contains(e)).length>0;-1!==t.indexOf(e)||s||t.push(e)}),t}getIframeContents(e,t,s=(()=>{})){let r;try{const t=e.contentWindow;if(r=t.document,!t||!r)throw new Error("iframe inaccessible")}catch(e){s()}r&&t(r)}isIframeBlank(e){const t="about:blank",s=e.getAttribute("src").trim(),r=e.contentWindow.location.href;return r===t&&s!==t&&s}observeIframeLoad(e,t,s){let r=!1,i=null;const o=()=>{if(!r){r=!0,clearTimeout(i);try{this.isIframeBlank(e)||(e.removeEventListener("load",o),this.getIframeContents(e,t,s))}catch(e){s()}}};e.addEventListener("load",o),i=setTimeout(o,this.iframesTimeout)}onIframeReady(e,t,s){try{"complete"===e.contentWindow.document.readyState?this.isIframeBlank(e)?this.observeIframeLoad(e,t,s):this.getIframeContents(e,t,s):this.observeIframeLoad(e,t,s)}catch(e){s()}}waitForIframes(e,t){let s=0;this.forEachIframe(e,()=>!0,e=>{s++,this.waitForIframes(e.querySelector("html"),()=>{--s||t()})},e=>{e||t()})}forEachIframe(t,s,r,i=(()=>{})){let o=t.querySelectorAll("iframe"),n=o.length,a=0;o=Array.prototype.slice.call(o);const c=()=>{--n<=0&&i(a)};n||c(),o.forEach(t=>{e.matches(t,this.exclude)?c():this.onIframeReady(t,e=>{s(t)&&(a++,r(e)),c()},c)})}createIterator(e,t,s){return document.createNodeIterator(e,t,s,!1)}createInstanceOnIframe(t){return new e(t.querySelector("html"),this.iframes)}compareNodeIframe(e,t,s){const r=e.compareDocumentPosition(s),i=Node.DOCUMENT_POSITION_PRECEDING;if(r&i){if(null===t)return!0;if(t.compareDocumentPosition(s)&Node.DOCUMENT_POSITION_FOLLOWING)return!0}return!1}getIteratorNode(e){const t=e.previousNode();let s;return{prevNode:t,node:s=null===t?e.nextNode():e.nextNode()&&e.nextNode()}}checkIframeFilter(e,t,s,r){let i=!1,o=!1;return r.forEach((e,t)=>{e.val===s&&(i=t,o=e.handled)}),this.compareNodeIframe(e,t,s)?(!1!==i||o?!1===i||o||(r[i].handled=!0):r.push({val:s,handled:!0}),!0):(!1===i&&r.push({val:s,handled:!1}),!1)}handleOpenIframes(e,t,s,r){e.forEach(e=>{e.handled||this.getIframeContents(e.val,e=>{this.createInstanceOnIframe(e).forEachNode(t,s,r)})})}iterateThroughNodes(e,t,s,r,i){const o=this.createIterator(t,e,r);let n,a,c=[],h=[],l=()=>(({prevNode:a,node:n}=this.getIteratorNode(o)),n);for(;l();)this.iframes&&this.forEachIframe(t,e=>this.checkIframeFilter(n,a,e,c),t=>{this.createInstanceOnIframe(t).forEachNode(e,e=>h.push(e),r)}),h.push(n);h.forEach(e=>{s(e)}),this.iframes&&this.handleOpenIframes(c,e,s,r),i()}forEachNode(e,t,s,r=(()=>{})){const i=this.getContexts();let o=i.length;o||r(),i.forEach(i=>{const n=()=>{this.iterateThroughNodes(e,i,t,s,()=>{--o<=0&&r()})};this.iframes?this.waitForIframes(i,n):n()})}}class t{constructor(e){this.ctx=e,this.ie=!1;const t=window.navigator.userAgent;(t.indexOf("MSIE")>-1||t.indexOf("Trident")>-1)&&(this.ie=!0)}set opt(e){this._opt=Object.assign({},{element:"",className:"",exclude:[],iframes:!1,iframesTimeout:5e3,separateWordSearch:!0,diacritics:!0,synonyms:{},accuracy:"partially",acrossElements:!1,caseSensitive:!1,ignoreJoiners:!1,ignoreGroups:0,ignorePunctuation:[],wildcards:"disabled",each:()=>{},noMatch:()=>{},filter:()=>!0,done:()=>{},debug:!1,log:window.console},e)}get opt(){return this._opt}get iterator(){return new e(this.ctx,this.opt.iframes,this.opt.exclude,this.opt.iframesTimeout)}log(e,t="debug"){const s=this.opt.log;this.opt.debug&&"object"==typeof s&&"function"==typeof s[t]&&s[t](`mark.js: ${e}`)}escapeStr(e){return e.replace(/[\-\[\]\/\{\}\(\)\*\+\?\.\\\^\$\|]/g,"\\$&")}createRegExp(e){return"disabled"!==this.opt.wildcards&&(e=this.setupWildcardsRegExp(e)),e=this.escapeStr(e),Object.keys(this.opt.synonyms).length&&(e=this.createSynonymsRegExp(e)),(this.opt.ignoreJoiners||this.opt.ignorePunctuation.length)&&(e=this.setupIgnoreJoinersRegExp(e)),this.opt.diacritics&&(e=this.createDiacriticsRegExp(e)),e=this.createMergedBlanksRegExp(e),(this.opt.ignoreJoiners||this.opt.ignorePunctuation.length)&&(e=this.createJoinersRegExp(e)),"disabled"!==this.opt.wildcards&&(e=this.createWildcardsRegExp(e)),e=this.createAccuracyRegExp(e)}createSynonymsRegExp(e){const t=this.opt.synonyms,s=this.opt.caseSensitive?"":"i",r=this.opt.ignoreJoiners||this.opt.ignorePunctuation.length?"\0":"";for(let i in t)if(t.hasOwnProperty(i)){const o=t[i],n="disabled"!==this.opt.wildcards?this.setupWildcardsRegExp(i):this.escapeStr(i),a="disabled"!==this.opt.wildcards?this.setupWildcardsRegExp(o):this.escapeStr(o);""!==n&&""!==a&&(e=e.replace(new RegExp(`(${this.escapeStr(n)}|${this.escapeStr(a)})`,`gm${s}`),r+`(${this.processSynomyms(n)}|`+`${this.processSynomyms(a)})`+r))}return e}processSynomyms(e){return(this.opt.ignoreJoiners||this.opt.ignorePunctuation.length)&&(e=this.setupIgnoreJoinersRegExp(e)),e}setupWildcardsRegExp(e){return(e=e.replace(/(?:\\)*\?/g,e=>"\\"===e.charAt(0)?"?":"")).replace(/(?:\\)*\*/g,e=>"\\"===e.charAt(0)?"*":"")}createWildcardsRegExp(e){let t="withSpaces"===this.opt.wildcards;return e.replace(/\u0001/g,t?"[\\S\\s]?":"\\S?").replace(/\u0002/g,t?"[\\S\\s]*?":"\\S*")}setupIgnoreJoinersRegExp(e){return e.replace(/[^(|)\\]/g,(e,t,s)=>{let r=s.charAt(t+1);return/[(|)\\]/.test(r)||""===r?e:e+"\0"})}createJoinersRegExp(e){let t=[];const s=this.opt.ignorePunctuation;return Array.isArray(s)&&s.length&&t.push(this.escapeStr(s.join(""))),this.opt.ignoreJoiners&&t.push("\\u00ad\\u200b\\u200c\\u200d"),t.length?e.split(/\u0000+/).join(`[${t.join("")}]*`):e}createDiacriticsRegExp(e){const t=this.opt.caseSensitive?"":"i",s=this.opt.caseSensitive?["aàáảãạăằắẳẵặâầấẩẫậäåāą","AÀÁẢÃẠĂẰẮẲẴẶÂẦẤẨẪẬÄÅĀĄ","cçćč","CÇĆČ","dđď","DĐĎ","eèéẻẽẹêềếểễệëěēę","EÈÉẺẼẸÊỀẾỂỄỆËĚĒĘ","iìíỉĩịîïī","IÌÍỈĨỊÎÏĪ","lł","LŁ","nñňń","NÑŇŃ","oòóỏõọôồốổỗộơởỡớờợöøō","OÒÓỎÕỌÔỒỐỔỖỘƠỞỠỚỜỢÖØŌ","rř","RŘ","sšśșş","SŠŚȘŞ","tťțţ","TŤȚŢ","uùúủũụưừứửữựûüůū","UÙÚỦŨỤƯỪỨỬỮỰÛÜŮŪ","yýỳỷỹỵÿ","YÝỲỶỸỴŸ","zžżź","ZŽŻŹ"]:["aàáảãạăằắẳẵặâầấẩẫậäåāąAÀÁẢÃẠĂẰẮẲẴẶÂẦẤẨẪẬÄÅĀĄ","cçćčCÇĆČ","dđďDĐĎ","eèéẻẽẹêềếểễệëěēęEÈÉẺẼẸÊỀẾỂỄỆËĚĒĘ","iìíỉĩịîïīIÌÍỈĨỊÎÏĪ","lłLŁ","nñňńNÑŇŃ","oòóỏõọôồốổỗộơởỡớờợöøōOÒÓỎÕỌÔỒỐỔỖỘƠỞỠỚỜỢÖØŌ","rřRŘ","sšśșşSŠŚȘŞ","tťțţTŤȚŢ","uùúủũụưừứửữựûüůūUÙÚỦŨỤƯỪỨỬỮỰÛÜŮŪ","yýỳỷỹỵÿYÝỲỶỸỴŸ","zžżźZŽŻŹ"];let r=[];return e.split("").forEach(i=>{s.every(s=>{if(-1!==s.indexOf(i)){if(r.indexOf(s)>-1)return!1;e=e.replace(new RegExp(`[${s}]`,`gm${t}`),`[${s}]`),r.push(s)}return!0})}),e}createMergedBlanksRegExp(e){return e.replace(/[\s]+/gim,"[\\s]+")}createAccuracyRegExp(e){const t="!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~¡¿";let s=this.opt.accuracy,r="string"==typeof s?s:s.value,i="string"==typeof s?[]:s.limiters,o="";switch(i.forEach(e=>{o+=`|${this.escapeStr(e)}`}),r){case"partially":default:return`()(${e})`;case"complementary":return`()([^${o="\\s"+(o||this.escapeStr(t))}]*${e}[^${o}]*)`;case"exactly":return`(^|\\s${o})(${e})(?=$|\\s${o})`}}getSeparatedKeywords(e){let t=[];return e.forEach(e=>{this.opt.separateWordSearch?e.split(" ").forEach(e=>{e.trim()&&-1===t.indexOf(e)&&t.push(e)}):e.trim()&&-1===t.indexOf(e)&&t.push(e)}),{keywords:t.sort((e,t)=>t.length-e.length),length:t.length}}isNumeric(e){return Number(parseFloat(e))==e}checkRanges(e){if(!Array.isArray(e)||"[object Object]"!==Object.prototype.to_string.call(e[0]))return this.log("markRanges() will only accept an array of objects"),this.opt.noMatch(e),[];const t=[];let s=0;return e.sort((e,t)=>e.start-t.start).forEach(e=>{let{start:r,end:i,valid:o}=this.callNoMatchOnInvalidRanges(e,s);o&&(e.start=r,e.length=i-r,t.push(e),s=i)}),t}callNoMatchOnInvalidRanges(e,t){let s,r,i=!1;return e&&void 0!==e.start?(r=(s=parseInt(e.start,10))+parseInt(e.length,10),this.isNumeric(e.start)&&this.isNumeric(e.length)&&r-t>0&&r-s>0?i=!0:(this.log("Ignoring invalid or overlapping range: "+`${JSON.stringify(e)}`),this.opt.noMatch(e))):(this.log(`Ignoring invalid range: ${JSON.stringify(e)}`),this.opt.noMatch(e)),{start:s,end:r,valid:i}}checkWhitespaceRanges(e,t,s){let r,i=!0,o=s.length,n=t-o,a=parseInt(e.start,10)-n;return(r=(a=a>o?o:a)+parseInt(e.length,10))>o&&(r=o,this.log(`End range automatically set to the max value of ${o}`)),a<0||r-a<0||a>o||r>o?(i=!1,this.log(`Invalid range: ${JSON.stringify(e)}`),this.opt.noMatch(e)):""===s.substring(a,r).replace(/\s+/g,"")&&(i=!1,this.log("Skipping whitespace only range: "+JSON.stringify(e)),this.opt.noMatch(e)),{start:a,end:r,valid:i}}getTextNodes(e){let t="",s=[];this.iterator.forEachNode(NodeFilter.SHOW_TEXT,e=>{s.push({start:t.length,end:(t+=e.textContent).length,node:e})},e=>this.matchesExclude(e.parentNode)?NodeFilter.FILTER_REJECT:NodeFilter.FILTER_ACCEPT,()=>{e({value:t,nodes:s})})}matchesExclude(t){return e.matches(t,this.opt.exclude.concat(["script","style","title","head","html"]))}wrapRangeInTextNode(e,t,s){const r=this.opt.element?this.opt.element:"mark",i=e.splitText(t),o=i.splitText(s-t);let n=document.createElement(r);return n.setAttribute("data-markjs","true"),this.opt.className&&n.setAttribute("class",this.opt.className),n.textContent=i.textContent,i.parentNode.replaceChild(n,i),o}wrapRangeInMappedTextNode(e,t,s,r,i){e.nodes.every((o,n)=>{const a=e.nodes[n+1];if(void 0===a||a.start>t){if(!r(o.node))return!1;const a=t-o.start,c=(s>o.end?o.end:s)-o.start,h=e.value.substr(0,o.start),l=e.value.substr(c+o.start);if(o.node=this.wrapRangeInTextNode(o.node,a,c),e.value=h+l,e.nodes.forEach((t,s)=>{s>=n&&(e.nodes[s].start>0&&s!==n&&(e.nodes[s].start-=c),e.nodes[s].end-=c)}),s-=c,i(o.node.previous_sibling,o.start),!(s>o.end))return!1;t=o.end}return!0})}wrapMatches(e,t,s,r,i){const o=0===t?0:t+1;this.getTextNodes(t=>{t.nodes.forEach(t=>{let i;for(t=t.node;null!==(i=e.exec(t.textContent))&&""!==i[o];){if(!s(i[o],t))continue;let n=i.index;if(0!==o)for(let e=1;e<o;e++)n+=i[e].length;t=this.wrapRangeInTextNode(t,n,n+i[o].length),r(t.previous_sibling),e.lastIndex=0}}),i()})}wrapMatchesAcrossElements(e,t,s,r,i){const o=0===t?0:t+1;this.getTextNodes(t=>{let n;for(;null!==(n=e.exec(t.value))&&""!==n[o];){let i=n.index;if(0!==o)for(let e=1;e<o;e++)i+=n[e].length;const a=i+n[o].length;this.wrapRangeInMappedTextNode(t,i,a,e=>s(n[o],e),(t,s)=>{e.lastIndex=s,r(t)})}i()})}wrapRangeFromIndex(e,t,s,r){this.getTextNodes(i=>{const o=i.value.length;e.forEach((e,r)=>{let{start:n,end:a,valid:c}=this.checkWhitespaceRanges(e,o,i.value);c&&this.wrapRangeInMappedTextNode(i,n,a,s=>t(s,e,i.value.substring(n,a),r),t=>{s(t,e)})}),r()})}unwrapMatches(e){const t=e.parentNode;let s=document.createDocumentFragment();for(;e.firstChild;)s.appendChild(e.removeChild(e.firstChild));t.replaceChild(s,e),this.ie?this.normalizeTextNode(t):t.normalize()}normalizeTextNode(e){if(e){if(3===e.nodeType)for(;e.next_sibling&&3===e.next_sibling.nodeType;)e.nodeValue+=e.next_sibling.nodeValue,e.parentNode.removeChild(e.next_sibling);else this.normalizeTextNode(e.firstChild);this.normalizeTextNode(e.next_sibling)}}markRegExp(e,t){this.opt=t,this.log(`Searching with expression "${e}"`);let s=0,r="wrapMatches";const i=e=>{s++,this.opt.each(e)};this.opt.acrossElements&&(r="wrapMatchesAcrossElements"),this[r](e,this.opt.ignoreGroups,(e,t)=>this.opt.filter(t,e,s),i,()=>{0===s&&this.opt.noMatch(e),this.opt.done(s)})}mark(e,t){this.opt=t;let s=0,r="wrapMatches";const{keywords:i,length:o}=this.getSeparatedKeywords("string"==typeof e?[e]:e),n=this.opt.caseSensitive?"":"i",a=e=>{let t=new RegExp(this.createRegExp(e),`gm${n}`),c=0;this.log(`Searching with expression "${t}"`),this[r](t,1,(t,r)=>this.opt.filter(r,e,s,c),e=>{c++,s++,this.opt.each(e)},()=>{0===c&&this.opt.noMatch(e),i[o-1]===e?this.opt.done(s):a(i[i.indexOf(e)+1])})};this.opt.acrossElements&&(r="wrapMatchesAcrossElements"),0===o?this.opt.done(s):a(i[0])}markRanges(e,t){this.opt=t;let s=0,r=this.checkRanges(e);r&&r.length?(this.log("Starting to mark with the following ranges: "+JSON.stringify(r)),this.wrapRangeFromIndex(r,(e,t,s,r)=>this.opt.filter(e,t,s,r),(e,t)=>{s++,this.opt.each(e,t)},()=>{this.opt.done(s)})):this.opt.done(s)}unmark(t){this.opt=t;let s=this.opt.element?this.opt.element:"*";s+="[data-markjs]",this.opt.className&&(s+=`.${this.opt.className}`),this.log(`Removal selector "${s}"`),this.iterator.forEachNode(NodeFilter.SHOW_ELEMENT,e=>{this.unwrapMatches(e)},t=>{const r=e.matches(t,s),i=this.matchesExclude(t);return!r||i?NodeFilter.FILTER_REJECT:NodeFilter.FILTER_ACCEPT},this.opt.done)}}return function(e){const s=new t(e);return this.mark=((e,t)=>(s.mark(e,t),this)),this.markRegExp=((e,t)=>(s.markRegExp(e,t),this)),this.markRanges=((e,t)=>(s.markRanges(e,t),this)),this.unmark=(e=>(s.unmark(e),this)),this}});
  // from resources/search.js
  "use strict";// https://github.com/marcelblanarik/js-keyword-highlighter/blob/dd69436bee06f8c658abe1e12e2abb35d3bf250b/index.js
  (function(){function a(){var a=d[c-1],f=d[c+1];e?(a.className-=" current",c>d.length-1&&(c=0),d[c].className+=" current",window.scroll(0,b(d[c]))):(f.className-=" current",0>c&&(c=d.length-1),d[c].className+=" current",window.scroll(0,b(d[c])))}function b(a){var b=0;if(a.offsetParent){do b+=a.offsetTop;while(a=a.offsetParent);return[b]}b=0}var c,d,e,f=document.body,g=new Mark(f),h=function(a){g.unmark(),g.mark(a,{element:"mark",className:"highlight",diacritics:!0}),c=0,d=f.getElementsByTagName("mark"),d[0]?d[0].className+=" current":null},i=function(){!isNaN(c)&&0<d.length&&(e=!0,c++,a())},j=function(){!isNaN(c)&&0<d.length&&(e=!1,--c,a())},k=window.odr||{};k.search=h,k.searchNext=i,k.searchPrevious=j,k.resetSearch=function reset(){g.unmark()},window.odr=k})();
    )V0G0N";
  // clang-format on
}

std::string html::body_attributes(const HtmlConfig &config) noexcept {
  std::string result;

  result += "class=\"";
  switch (config.table_gridlines) {
  case HtmlTableGridlines::SOFT:
    result += "odr-gridlines-soft";
    break;
  case HtmlTableGridlines::HARD:
    result += "odr-gridlines-hard";
    break;
  case HtmlTableGridlines::NONE:
  default:
    result += "odr-gridlines-none";
    break;
  }
  result += "\"";

  return result;
}

std::string html::escape_text(std::string text) noexcept {
  util::string::replace_all(text, "&", "&amp;");
  util::string::replace_all(text, "<", "&lt;");
  util::string::replace_all(text, ">", "&gt;");

  return text;
}

} // namespace odr::internal::common
