#include <internal/html/common.h>
#include <internal/util/string_util.h>
#include <iomanip>
#include <odr/html.h>
#include <odr/style.h>
#include <sstream>

namespace odr::internal {

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
  margin: 0;
  position: relative;
}
body {
  padding: 5px;
}

x-p {
  display: block;
  font-size: 0;
}
x-s {
  display: inline;
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
td {
  vertical-align: bottom;
  text-overflow: ellipsis;
  height: inherit;
}

x-p {
  font-family: "Arial";
  font-size: 10pt;
}
td x-p {
  height: inherit;
}

.odr-gridlines-none table td {}
.odr-gridlines-soft table td {
  border-top: 1px solid #C0C0C0;
  border-left: 1px solid #C0C0C0;
}
.odr-gridlines-hard table td {
  border: 1px solid #C0C0C0 !important;
}

table td.odr-value-type-float {
  text-align: right;
}
)V0G0N";
  // clang-format on
}

const char *html::default_script() noexcept {
  // clang-format off
  return
    R"V0G0N(!function(e,t){"object"==typeof exports&&"object"==typeof module?module.exports=t():"function"==typeof define&&define.amd?define([],t):"object"==typeof exports?exports.odr=t():e.odr=t()}(self,(function(){return(()=>{var e={813:function(e){e.exports=function(){"use strict";var e="function"==typeof Symbol&&"symbol"==typeof Symbol.iterator?function(e){return typeof e}:function(e){return e&&"function"==typeof Symbol&&e.constructor===Symbol&&e!==Symbol.prototype?"symbol":typeof e},t=function(e,t){if(!(e instanceof t))throw new TypeError("Cannot call a class as a function")},n=function(){function e(e,t){for(var n=0;n<t.length;n++){var r=t[n];r.enumerable=r.enumerable||!1,r.configurable=!0,"value"in r&&(r.writable=!0),Object.defineProperty(e,r.key,r)}}return function(t,n,r){return n&&e(t.prototype,n),r&&e(t,r),t}}(),r=Object.assign||function(e){for(var t=1;t<arguments.length;t++){var n=arguments[t];for(var r in n)Object.prototype.hasOwnProperty.call(n,r)&&(e[r]=n[r])}return e},o=function(){function e(n){var r=!(arguments.length>1&&void 0!==arguments[1])||arguments[1],o=arguments.length>2&&void 0!==arguments[2]?arguments[2]:[],i=arguments.length>3&&void 0!==arguments[3]?arguments[3]:5e3;t(this,e),this.ctx=n,this.iframes=r,this.exclude=o,this.iframesTimeout=i}return n(e,[{key:"getContexts",value:function(){var e=[];return(void 0!==this.ctx&&this.ctx?NodeList.prototype.isPrototypeOf(this.ctx)?Array.prototype.slice.call(this.ctx):Array.isArray(this.ctx)?this.ctx:"string"==typeof this.ctx?Array.prototype.slice.call(document.querySelectorAll(this.ctx)):[this.ctx]:[]).forEach((function(t){var n=e.filter((function(e){return e.contains(t)})).length>0;-1!==e.indexOf(t)||n||e.push(t)})),e}},{key:"getIframeContents",value:function(e,t){var n=arguments.length>2&&void 0!==arguments[2]?arguments[2]:function(){},r=void 0;try{var o=e.contentWindow;if(r=o.document,!o||!r)throw new Error("iframe inaccessible")}catch(e){n()}r&&t(r)}},{key:"isIframeBlank",value:function(e){var t="about:blank",n=e.getAttribute("src").trim();return e.contentW)V0G0N"
    R"V0G0N(indow.location.href===t&&n!==t&&n}},{key:"observeIframeLoad",value:function(e,t,n){var r=this,o=!1,i=null,a=function a(){if(!o){o=!0,clearTimeout(i);try{r.isIframeBlank(e)||(e.removeEventListener("load",a),r.getIframeContents(e,t,n))}catch(e){n()}}};e.addEventListener("load",a),i=setTimeout(a,this.iframesTimeout)}},{key:"onIframeReady",value:function(e,t,n){try{"complete"===e.contentWindow.document.readyState?this.isIframeBlank(e)?this.observeIframeLoad(e,t,n):this.getIframeContents(e,t,n):this.observeIframeLoad(e,t,n)}catch(e){n()}}},{key:"waitForIframes",value:function(e,t){var n=this,r=0;this.forEachIframe(e,(function(){return!0}),(function(e){r++,n.waitForIframes(e.querySelector("html"),(function(){--r||t()}))}),(function(e){e||t()}))}},{key:"forEachIframe",value:function(t,n,r){var o=this,i=arguments.length>3&&void 0!==arguments[3]?arguments[3]:function(){},a=t.querySelectorAll("iframe"),s=a.length,c=0;a=Array.prototype.slice.call(a);var u=function(){--s<=0&&i(c)};s||u(),a.forEach((function(t){e.matches(t,o.exclude)?u():o.onIframeReady(t,(function(e){n(t)&&(c++,r(e)),u()}),u)}))}},{key:"createIterator",value:function(e,t,n){return document.createNodeIterator(e,t,n,!1)}},{key:"createInstanceOnIframe",value:function(t){return new e(t.querySelector("html"),this.iframes)}},{key:"compareNodeIframe",value:function(e,t,n){if(e.compareDocumentPosition(n)&Node.DOCUMENT_POSITION_PRECEDING){if(null===t)return!0;if(t.compareDocumentPosition(n)&Node.DOCUMENT_POSITION_FOLLOWING)return!0}return!1}},{key:"getIteratorNode",value:function(e){var t=e.previousNode();return{prevNode:t,node:(null===t||e.nextNode())&&e.nextNode()}}},{key:"checkIframeFilter",value:function(e,t,n,r){var o=!1,i=!1;return r.forEach((function(e,t){e.val===n&&(o=t,i=e.handled)})),this.compareNodeIframe(e,t,n)?(!1!==o||i?!1===o||i||(r[o].handled=!0):r.push({val:n,handled:!0}),!0):(!1===o&&r.push({val:n,handled:!1}),!1)}},{key:"handleOpenIframes",value:function(e,t,n,r){var o=this;e.forEach((function(e){e.handled||o.getIframeContents(e.val,(function(e){o.)V0G0N"
    R"V0G0N(createInstanceOnIframe(e).forEachNode(t,n,r)}))}))}},{key:"iterateThroughNodes",value:function(e,t,n,r,o){for(var i=this,a=this.createIterator(t,e,r),s=[],c=[],u=void 0,l=void 0;h=void 0,h=i.getIteratorNode(a),l=h.prevNode,u=h.node;)this.iframes&&this.forEachIframe(t,(function(e){return i.checkIframeFilter(u,l,e,s)}),(function(t){i.createInstanceOnIframe(t).forEachNode(e,(function(e){return c.push(e)}),r)})),c.push(u);var h;c.forEach((function(e){n(e)})),this.iframes&&this.handleOpenIframes(s,e,n,r),o()}},{key:"forEachNode",value:function(e,t,n){var r=this,o=arguments.length>3&&void 0!==arguments[3]?arguments[3]:function(){},i=this.getContexts(),a=i.length;a||o(),i.forEach((function(i){var s=function(){r.iterateThroughNodes(e,i,t,n,(function(){--a<=0&&o()}))};r.iframes?r.waitForIframes(i,s):s()}))}}],[{key:"matches",value:function(e,t){var n="string"==typeof t?[t]:t,r=e.matches||e.matchesSelector||e.msMatchesSelector||e.mozMatchesSelector||e.oMatchesSelector||e.webkitMatchesSelector;if(r){var o=!1;return n.every((function(t){return!r.call(e,t)||(o=!0,!1)})),o}return!1}}]),e}(),i=function(){function i(e){t(this,i),this.ctx=e,this.ie=!1;var n=window.navigator.userAgent;(n.indexOf("MSIE")>-1||n.indexOf("Trident")>-1)&&(this.ie=!0)}return n(i,[{key:"log",value:function(t){var n=arguments.length>1&&void 0!==arguments[1]?arguments[1]:"debug",r=this.opt.log;this.opt.debug&&"object"===(void 0===r?"undefined":e(r))&&"function"==typeof r[n]&&r[n]("mark.js: "+t)}},{key:"escapeStr",value:function(e){return e.replace(/[\-\[\]\/\{\}\(\)\*\+\?\.\\\^\$\|]/g,"\\$&")}},{key:"createRegExp",value:function(e){return"disabled"!==this.opt.wildcards&&(e=this.setupWildcardsRegExp(e)),e=this.escapeStr(e),Object.keys(this.opt.synonyms).length&&(e=this.createSynonymsRegExp(e)),(this.opt.ignoreJoiners||this.opt.ignorePunctuation.length)&&(e=this.setupIgnoreJoinersRegExp(e)),this.opt.diacritics&&(e=this.createDiacriticsRegExp(e)),e=this.createMergedBlanksRegExp(e),(this.opt.ignoreJoiners||this.opt.ignorePunctuation.length)&&(e=this.createJoin)V0G0N"
    R"V0G0N(ersRegExp(e)),"disabled"!==this.opt.wildcards&&(e=this.createWildcardsRegExp(e)),this.createAccuracyRegExp(e)}},{key:"createSynonymsRegExp",value:function(e){var t=this.opt.synonyms,n=this.opt.caseSensitive?"":"i",r=this.opt.ignoreJoiners||this.opt.ignorePunctuation.length?"\0":"";for(var o in t)if(t.hasOwnProperty(o)){var i=t[o],a="disabled"!==this.opt.wildcards?this.setupWildcardsRegExp(o):this.escapeStr(o),s="disabled"!==this.opt.wildcards?this.setupWildcardsRegExp(i):this.escapeStr(i);""!==a&&""!==s&&(e=e.replace(new RegExp("("+this.escapeStr(a)+"|"+this.escapeStr(s)+")","gm"+n),r+"("+this.processSynomyms(a)+"|"+this.processSynomyms(s)+")"+r))}return e}},{key:"processSynomyms",value:function(e){return(this.opt.ignoreJoiners||this.opt.ignorePunctuation.length)&&(e=this.setupIgnoreJoinersRegExp(e)),e}},{key:"setupWildcardsRegExp",value:function(e){return(e=e.replace(/(?:\\)*\?/g,(function(e){return"\\"===e.charAt(0)?"?":""}))).replace(/(?:\\)*\*/g,(function(e){return"\\"===e.charAt(0)?"*":""}))}},{key:"createWildcardsRegExp",value:function(e){var t="withSpaces"===this.opt.wildcards;return e.replace(/\u0001/g,t?"[\\S\\s]?":"\\S?").replace(/\u0002/g,t?"[\\S\\s]*?":"\\S*")}},{key:"setupIgnoreJoinersRegExp",value:function(e){return e.replace(/[^(|)\\]/g,(function(e,t,n){var r=n.charAt(t+1);return/[(|)\\]/.test(r)||""===r?e:e+"\0"}))}},{key:"createJoinersRegExp",value:function(e){var t=[],n=this.opt.ignorePunctuation;return Array.isArray(n)&&n.length&&t.push(this.escapeStr(n.join(""))),this.opt.ignoreJoiners&&t.push("\\u00ad\\u200b\\u200c\\u200d"),t.length?e.split(/\u0000+/).join("["+t.join("")+"]*"):e}},{key:"createDiacriticsRegExp",value:function(e){var t=this.opt.caseSensitive?"":"i",n=this.opt.caseSensitive?["aàáảãạăằắẳẵặâầấẩẫậäåāą","AÀÁẢÃẠĂẰẮẲẴẶÂẦẤẨẪẬÄÅĀĄ","cçćč","CÇĆČ","dđď","DĐĎ","eèéẻẽẹêềếểễệëěēę","EÈÉẺẼẸÊỀẾỂỄỆËĚĒĘ","iìíỉĩịîïī","IÌÍỈĨỊÎÏĪ","lł","LŁ","nñňń","NÑŇŃ","oòóỏõọôồốổỗộơởỡớờợöøō","OÒÓỎÕỌÔỒỐỔỖỘƠỞỠỚỜỢÖØŌ","rř","RŘ","sšśșş","SŠŚȘŞ","tťțţ","TŤȚŢ","uùúủũụưừứửữựûüůū","UÙÚỦŨỤƯỪỨỬỮỰÛÜŮŪ","yýỳỷỹỵÿ","YÝỲỶỸỴŸ")V0G0N"
    R"V0G0N(,"zžżź","ZŽŻŹ"]:["aàáảãạăằắẳẵặâầấẩẫậäåāąAÀÁẢÃẠĂẰẮẲẴẶÂẦẤẨẪẬÄÅĀĄ","cçćčCÇĆČ","dđďDĐĎ","eèéẻẽẹêềếểễệëěēęEÈÉẺẼẸÊỀẾỂỄỆËĚĒĘ","iìíỉĩịîïīIÌÍỈĨỊÎÏĪ","lłLŁ","nñňńNÑŇŃ","oòóỏõọôồốổỗộơởỡớờợöøōOÒÓỎÕỌÔỒỐỔỖỘƠỞỠỚỜỢÖØŌ","rřRŘ","sšśșşSŠŚȘŞ","tťțţTŤȚŢ","uùúủũụưừứửữựûüůūUÙÚỦŨỤƯỪỨỬỮỰÛÜŮŪ","yýỳỷỹỵÿYÝỲỶỸỴŸ","zžżźZŽŻŹ"],r=[];return e.split("").forEach((function(o){n.every((function(n){if(-1!==n.indexOf(o)){if(r.indexOf(n)>-1)return!1;e=e.replace(new RegExp("["+n+"]","gm"+t),"["+n+"]"),r.push(n)}return!0}))})),e}},{key:"createMergedBlanksRegExp",value:function(e){return e.replace(/[\s]+/gim,"[\\s]+")}},{key:"createAccuracyRegExp",value:function(e){var t=this,n=this.opt.accuracy,r="string"==typeof n?n:n.value,o="string"==typeof n?[]:n.limiters,i="";switch(o.forEach((function(e){i+="|"+t.escapeStr(e)})),r){default:return"()("+e+")";case"complementary":return"()([^"+(i="\\s"+(i||this.escapeStr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~¡¿")))+"]*"+e+"[^"+i+"]*)";case"exactly":return"(^|\\s"+i+")("+e+")(?=$|\\s"+i+")"}}},{key:"getSeparatedKeywords",value:function(e){var t=this,n=[];return e.forEach((function(e){t.opt.separateWordSearch?e.split(" ").forEach((function(e){e.trim()&&-1===n.indexOf(e)&&n.push(e)})):e.trim()&&-1===n.indexOf(e)&&n.push(e)})),{keywords:n.sort((function(e,t){return t.length-e.length})),length:n.length}}},{key:"isNumeric",value:function(e){return Number(parseFloat(e))==e}},{key:"checkRanges",value:function(e){var t=this;if(!Array.isArray(e)||"[object Object]"!==Object.prototype.toString.call(e[0]))return this.log("markRanges() will only accept an array of objects"),this.opt.noMatch(e),[];var n=[],r=0;return e.sort((function(e,t){return e.start-t.start})).forEach((function(e){var o=t.callNoMatchOnInvalidRanges(e,r),i=o.start,a=o.end;o.valid&&(e.start=i,e.length=a-i,n.push(e),r=a)})),n}},{key:"callNoMatchOnInvalidRanges",value:function(e,t){var n=void 0,r=void 0,o=!1;return e&&void 0!==e.start?(r=(n=parseInt(e.start,10))+parseInt(e.length,10),this.isNumeric(e.start)&&this.isNumeric(e.length)&&r-t>0&&r-n>0?o=!0:(this.log("Ignori)V0G0N"
    R"V0G0N(ng invalid or overlapping range: "+JSON.stringify(e)),this.opt.noMatch(e))):(this.log("Ignoring invalid range: "+JSON.stringify(e)),this.opt.noMatch(e)),{start:n,end:r,valid:o}}},{key:"checkWhitespaceRanges",value:function(e,t,n){var r=void 0,o=!0,i=n.length,a=t-i,s=parseInt(e.start,10)-a;return(r=(s=s>i?i:s)+parseInt(e.length,10))>i&&(r=i,this.log("End range automatically set to the max value of "+i)),s<0||r-s<0||s>i||r>i?(o=!1,this.log("Invalid range: "+JSON.stringify(e)),this.opt.noMatch(e)):""===n.substring(s,r).replace(/\s+/g,"")&&(o=!1,this.log("Skipping whitespace only range: "+JSON.stringify(e)),this.opt.noMatch(e)),{start:s,end:r,valid:o}}},{key:"getTextNodes",value:function(e){var t=this,n="",r=[];this.iterator.forEachNode(NodeFilter.SHOW_TEXT,(function(e){r.push({start:n.length,end:(n+=e.textContent).length,node:e})}),(function(e){return t.matchesExclude(e.parentNode)?NodeFilter.FILTER_REJECT:NodeFilter.FILTER_ACCEPT}),(function(){e({value:n,nodes:r})}))}},{key:"matchesExclude",value:function(e){return o.matches(e,this.opt.exclude.concat(["script","style","title","head","html"]))}},{key:"wrapRangeInTextNode",value:function(e,t,n){var r=this.opt.element?this.opt.element:"mark",o=e.splitText(t),i=o.splitText(n-t),a=document.createElement(r);return a.setAttribute("data-markjs","true"),this.opt.className&&a.setAttribute("class",this.opt.className),a.textContent=o.textContent,o.parentNode.replaceChild(a,o),i}},{key:"wrapRangeInMappedTextNode",value:function(e,t,n,r,o){var i=this;e.nodes.every((function(a,s){var c=e.nodes[s+1];if(void 0===c||c.start>t){if(!r(a.node))return!1;var u=t-a.start,l=(n>a.end?a.end:n)-a.start,h=e.value.substr(0,a.start),f=e.value.substr(l+a.start);if(a.node=i.wrapRangeInTextNode(a.node,u,l),e.value=h+f,e.nodes.forEach((function(t,n){n>=s&&(e.nodes[n].start>0&&n!==s&&(e.nodes[n].start-=l),e.nodes[n].end-=l)})),n-=l,o(a.node.previousSibling,a.start),!(n>a.end))return!1;t=a.end}return!0}))}},{key:"wrapMatches",value:function(e,t,n,r,o){var i=this,a=0===t?0:t+1;this.getTextNodes((functi)V0G0N"
    R"V0G0N(on(t){t.nodes.forEach((function(t){t=t.node;for(var o=void 0;null!==(o=e.exec(t.textContent))&&""!==o[a];)if(n(o[a],t)){var s=o.index;if(0!==a)for(var c=1;c<a;c++)s+=o[c].length;t=i.wrapRangeInTextNode(t,s,s+o[a].length),r(t.previousSibling),e.lastIndex=0}})),o()}))}},{key:"wrapMatchesAcrossElements",value:function(e,t,n,r,o){var i=this,a=0===t?0:t+1;this.getTextNodes((function(t){for(var s=void 0;null!==(s=e.exec(t.value))&&""!==s[a];){var c=s.index;if(0!==a)for(var u=1;u<a;u++)c+=s[u].length;var l=c+s[a].length;i.wrapRangeInMappedTextNode(t,c,l,(function(e){return n(s[a],e)}),(function(t,n){e.lastIndex=n,r(t)}))}o()}))}},{key:"wrapRangeFromIndex",value:function(e,t,n,r){var o=this;this.getTextNodes((function(i){var a=i.value.length;e.forEach((function(e,r){var s=o.checkWhitespaceRanges(e,a,i.value),c=s.start,u=s.end;s.valid&&o.wrapRangeInMappedTextNode(i,c,u,(function(n){return t(n,e,i.value.substring(c,u),r)}),(function(t){n(t,e)}))})),r()}))}},{key:"unwrapMatches",value:function(e){for(var t=e.parentNode,n=document.createDocumentFragment();e.firstChild;)n.appendChild(e.removeChild(e.firstChild));t.replaceChild(n,e),this.ie?this.normalizeTextNode(t):t.normalize()}},{key:"normalizeTextNode",value:function(e){if(e){if(3===e.nodeType)for(;e.nextSibling&&3===e.nextSibling.nodeType;)e.nodeValue+=e.nextSibling.nodeValue,e.parentNode.removeChild(e.nextSibling);else this.normalizeTextNode(e.firstChild);this.normalizeTextNode(e.nextSibling)}}},{key:"markRegExp",value:function(e,t){var n=this;this.opt=t,this.log('Searching with expression "'+e+'"');var r=0,o="wrapMatches";this.opt.acrossElements&&(o="wrapMatchesAcrossElements"),this[o](e,this.opt.ignoreGroups,(function(e,t){return n.opt.filter(t,e,r)}),(function(e){r++,n.opt.each(e)}),(function(){0===r&&n.opt.noMatch(e),n.opt.done(r)}))}},{key:"mark",value:function(e,t){var n=this;this.opt=t;var r=0,o="wrapMatches",i=this.getSeparatedKeywords("string"==typeof e?[e]:e),a=i.keywords,s=i.length,c=this.opt.caseSensitive?"":"i";this.opt.acrossElements&&(o="wrapMatchesAcrossE)V0G0N"
    R"V0G0N(lements"),0===s?this.opt.done(r):function e(t){var i=new RegExp(n.createRegExp(t),"gm"+c),u=0;n.log('Searching with expression "'+i+'"'),n[o](i,1,(function(e,o){return n.opt.filter(o,t,r,u)}),(function(e){u++,r++,n.opt.each(e)}),(function(){0===u&&n.opt.noMatch(t),a[s-1]===t?n.opt.done(r):e(a[a.indexOf(t)+1])}))}(a[0])}},{key:"markRanges",value:function(e,t){var n=this;this.opt=t;var r=0,o=this.checkRanges(e);o&&o.length?(this.log("Starting to mark with the following ranges: "+JSON.stringify(o)),this.wrapRangeFromIndex(o,(function(e,t,r,o){return n.opt.filter(e,t,r,o)}),(function(e,t){r++,n.opt.each(e,t)}),(function(){n.opt.done(r)}))):this.opt.done(r)}},{key:"unmark",value:function(e){var t=this;this.opt=e;var n=this.opt.element?this.opt.element:"*";n+="[data-markjs]",this.opt.className&&(n+="."+this.opt.className),this.log('Removal selector "'+n+'"'),this.iterator.forEachNode(NodeFilter.SHOW_ELEMENT,(function(e){t.unwrapMatches(e)}),(function(e){var r=o.matches(e,n),i=t.matchesExclude(e);return!r||i?NodeFilter.FILTER_REJECT:NodeFilter.FILTER_ACCEPT}),this.opt.done)}},{key:"opt",set:function(e){this._opt=r({},{element:"",className:"",exclude:[],iframes:!1,iframesTimeout:5e3,separateWordSearch:!0,diacritics:!0,synonyms:{},accuracy:"partially",acrossElements:!1,caseSensitive:!1,ignoreJoiners:!1,ignoreGroups:0,ignorePunctuation:[],wildcards:"disabled",each:function(){},noMatch:function(){},filter:function(){return!0},done:function(){},debug:!1,log:window.console},e)},get:function(){return this._opt}},{key:"iterator",get:function(){return new o(this.ctx,this.opt.iframes,this.opt.exclude,this.opt.iframesTimeout)}}]),i}();return function(e){var t=this,n=new i(e);return this.mark=function(e,r){return n.mark(e,r),t},this.markRegExp=function(e,r){return n.markRegExp(e,r),t},this.markRanges=function(e,r){return n.markRanges(e,r),t},this.unmark=function(e){return n.unmark(e),t},this}}()}},t={};function n(r){var o=t[r];if(void 0!==o)return o.exports;var i=t[r]={exports:{}};return e[r].call(i.exports,i,i.exports,n),i.exports)V0G0N"
    R"V0G0N(}n.n=e=>{var t=e&&e.__esModule?()=>e.default:()=>e;return n.d(t,{a:t}),t},n.d=(e,t)=>{for(var r in t)n.o(t,r)&&!n.o(e,r)&&Object.defineProperty(e,r,{enumerable:!0,get:t[r]})},n.o=(e,t)=>Object.prototype.hasOwnProperty.call(e,t),n.r=e=>{"undefined"!=typeof Symbol&&Symbol.toStringTag&&Object.defineProperty(e,Symbol.toStringTag,{value:"Module"}),Object.defineProperty(e,"__esModule",{value:!0})};var r={};return(()=>{"use strict";n.r(r),n.d(r,{error_illegal_edit_new_line:()=>e,generateDiff:()=>o,onError:()=>t,resetSearch:()=>y,search:()=>p,searchNext:()=>g,searchPrevious:()=>v});const e={code:1,message:"new line not supported by this document"};let t=(e,t)=>{console.error(`error ${e} message ${t}`)};function o(){const e={modifiedText:{}};for(let[t,n]of Object.entries(i.modifiedText))e.modifiedText[t]=n.innerText;return JSON.stringify(e)}const i={modifiedText:{}};new MutationObserver((function(e,t){for(let t of e)if("characterData"===t.type){const e=t.target.parentNode,n=e.getAttribute("data-odr-path");n&&(i.modifiedText[n]=e)}})).observe(document.body,{attributes:!1,childList:!0,subtree:!0,characterData:!0}),document.addEventListener("keydown",(n=>{"Enter"===n.key&&(n.preventDefault(),t(e.code,e.message))}));var a=n(813),s=n.n(a);const c=document.body,u=new(s())(c),l=" current";let h,f,d;function p(e){u.unmark(),u.mark(e,{element:"mark",className:"highlight",diacritics:!0}),h=0,f=c.getElementsByTagName("mark"),f[0]&&(f[0].className+=l)}function g(){!isNaN(h)&&f.length>0&&(d=!0,h++,m())}function v(){!isNaN(h)&&f.length>0&&(d=!1,--h,m())}function m(){let e=f[h-1],t=f[h+1];d?(e.className-=l,h>f.length-1&&(h=0),f[h].className+=l,window.scroll(0,x(f[h]))):(t.className-=l,h<0&&(h=f.length-1),f[h].className+=l,window.scroll(0,x(f[h])))}function y(){u.unmark()}function x(e){let t=0;if(e.offsetParent){do{t+=e.offsetTop}while(e=e.offsetParent);return[t]}t=0}})(),r})()}));)V0G0N"
  ;
  // clang-format on
}

std::string html::body_attributes(const HtmlConfig &config) noexcept {
  std::string result;

  result += "class=\"";
  switch (config.spreadsheet_gridlines) {
  case HtmlTableGridlines::soft:
    result += "odr-gridlines-soft";
    break;
  case HtmlTableGridlines::hard:
    result += "odr-gridlines-hard";
    break;
  case HtmlTableGridlines::none:
  default:
    result += "odr-gridlines-none";
    break;
  }
  result += "\"";

  return result;
}

std::string html::escape_text(std::string text) noexcept {
  if (text.empty()) {
    return text;
  }

  util::string::replace_all(text, "&", "&amp;");
  util::string::replace_all(text, "<", "&lt;");
  util::string::replace_all(text, ">", "&gt;");

  if (text.front() == ' ') {
    text = "&nbsp;" + text.substr(1);
  }
  if (text.back() == ' ') {
    text = text.substr(0, text.length() - 1) + "&nbsp;";
  }
  util::string::replace_all(text, "  ", " &nbsp;");

  // TODO `&emsp;` is not a tab
  util::string::replace_all(text, "\t", "&emsp;");

  return text;
}

std::string html::color(const Color &color) noexcept {
  std::stringstream ss;
  ss << "#";
  ss << std::setw(6) << std::setfill('0') << std::hex << color.rgb();
  return ss.str();
}

} // namespace odr::internal
