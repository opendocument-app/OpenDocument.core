(function() {
  function generateDiff() {
    const result = {
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
        const path = node.getAttribute('data-odr-path');
        if (path) {
          editJournal['modifiedText'][path] = node;
        }
      }
    }
  }

  const editJournal = {
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

  const odr = window.odr || {};
  odr.generateDiff = generateDiff;
  window.odr = odr;
})();