function download(filename, text) {
  const element = document.createElement('a');
  element.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(text));
  element.setAttribute('download', filename);
  element.style.display = 'none';
  document.body.appendChild(element);
  element.click();
  document.body.removeChild(element);
}

window.addEventListener('keydown', function(event) {
  if ((event.ctrlKey || event.metaKey) && (event.key === 's')) {
    event.preventDefault();
    download('test.json', generateDiff());
  }
});

function generateDiff() {
  let result = {
    modifiedText: {},
  };
  for (let [k, v] of Object.entries(editJournal['modifiedText'])) {
    result['modifiedText'][k] = v.innerText;
  }
  return JSON.stringify(result);
}

function mutation(mutations, observer) {
  for(let m of mutations) {
    if (m.type === 'characterData') {
       const node = m.target.parentNode;
       const id = node.getAttribute('data-odr-cid');
       if (id) {
         editJournal['modifiedText'][parseInt(id)] = node;
       }
    } else {
      console.log(m);
    }
  }
}

let editJournal = {
  modifiedText: {},
};

const observer = new MutationObserver(mutation);
const config = {
  attributes: false,
  childList: true,
  subtree: true,
  characterData: true,
};
observer.observe(document.body, config);
