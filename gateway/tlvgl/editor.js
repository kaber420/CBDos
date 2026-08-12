const editor = document.getElementById('html-editor');
const screenInner = document.getElementById('screen-inner');
let selectedNode = null; // Reference to the DOM node in the parsed HTML doc
let htmlDoc = null; // The DOMParser document representation of the textarea

let currentW = 240;
let currentH = 320;

document.getElementById('res-select').addEventListener('change', e => {
  const [w, h] = e.target.value.split('x');
  currentW = parseInt(w);
  currentH = parseInt(h);
  document.getElementById('res-label').textContent = `${currentW} × ${currentH}`;
  document.getElementById('screen-frame').style.width = currentW + 'px';
  document.getElementById('screen-frame').style.height = currentH + 'px';
});

// Init parser
function updateDocFromEditor() {
  const parser = new DOMParser();
  htmlDoc = parser.parseFromString(`<body>${editor.value}</body>`, 'text/html');
  renderDoc();
}

function updateEditorFromDoc() {
  editor.value = htmlDoc.body.innerHTML.trim().replace(/></g, '>\n<');
  renderDoc();
}

function renderDoc() {
  screenInner.innerHTML = '';
  const scale = 1; // 1:1 pixel perfect for easier dragging

  function walk(node, parentX=0, parentY=0) {
    if (node.nodeType !== 1) return; // Only element nodes
    
    // Parse inline styles
    const isPanel = node.tagName.toLowerCase() === 'div';
    const isChart = node.tagName.toLowerCase() === 'chart';
    
    const x = (parseInt(style.left) || 0) + parentX;
    const y = (parseInt(style.top) || 0) + parentY;
    const w = parseInt(style.width) || (isPanel ? 100 : (isChart ? 180 : 80));
    const h = parseInt(style.height) || (isPanel ? 100 : (isChart ? 100 : 30));
    
    const el = document.createElement('div');
    el.className = 'lv-el';
    el.style.left = x + 'px';
    el.style.top = y + 'px';
    el.style.width = w + 'px';
    el.style.height = h + 'px';

    const tag = node.tagName.toLowerCase();
    
    if (tag === 'div') { el.classList.add('lv-panel'); }
    else if (tag === 'chart') {
      el.classList.add('lv-chart');
      const cType = node.getAttribute('type') || 'line';
      const cVals = node.getAttribute('values') || '18,19,21,24';
      el.innerHTML = `<span style="pointer-events:none">📊 ${cType.toUpperCase()} CHART<br>[${cVals}]</span>`;
    }
    else if (tag === 'button' || tag === 'a') { el.classList.add('lv-btn'); el.textContent = node.textContent; }
    else if (tag === 'input') { el.classList.add('lv-input'); el.textContent = node.placeholder || 'Input'; }
    else if (tag === 'i') { 
      el.classList.add('lv-icon'); 
      const cls = node.className;
      if (cls.includes('audio')) el.textContent = '\uf001';
else if (cls.includes('video')) el.textContent = '\uf008';
else if (cls.includes('list')) el.textContent = '\uf00b';
else if (cls.includes('check')) el.textContent = '\uf00c';
else if (cls.includes('times')) el.textContent = '\uf00d';
else if (cls.includes('power-off')) el.textContent = '\uf011';
else if (cls.includes('cog')) el.textContent = '\uf013';
else if (cls.includes('home')) el.textContent = '\uf015';
else if (cls.includes('download')) el.textContent = '\uf019';
else if (cls.includes('hdd')) el.textContent = '\uf01c';
else if (cls.includes('sync')) el.textContent = '\uf021';
else if (cls.includes('volume-mute')) el.textContent = '\uf026';
else if (cls.includes('volume-down')) el.textContent = '\uf027';
else if (cls.includes('volume-up')) el.textContent = '\uf028';
else if (cls.includes('image')) el.textContent = '\uf03e';
else if (cls.includes('tint')) el.textContent = '\uf043';
else if (cls.includes('edit')) el.textContent = '\uf044';
else if (cls.includes('step-backward')) el.textContent = '\uf048';
else if (cls.includes('play')) el.textContent = '\uf04b';
else if (cls.includes('pause')) el.textContent = '\uf04c';
else if (cls.includes('stop')) el.textContent = '\uf04d';
else if (cls.includes('step-forward')) el.textContent = '\uf051';
else if (cls.includes('eject')) el.textContent = '\uf052';
else if (cls.includes('chevron-left')) el.textContent = '\uf053';
else if (cls.includes('chevron-right')) el.textContent = '\uf054';
else if (cls.includes('plus')) el.textContent = '\uf067';
else if (cls.includes('minus')) el.textContent = '\uf068';
else if (cls.includes('eye-slash')) el.textContent = '\uf070';
else if (cls.includes('eye')) el.textContent = '\uf06e';
else if (cls.includes('exclamation-triangle')) el.textContent = '\uf071';
else if (cls.includes('random')) el.textContent = '\uf074';
else if (cls.includes('chevron-up')) el.textContent = '\uf077';
else if (cls.includes('chevron-down')) el.textContent = '\uf078';
else if (cls.includes('redo')) el.textContent = '\uf01e';
else if (cls.includes('folder')) el.textContent = '\uf07b';
else if (cls.includes('upload')) el.textContent = '\uf093';
else if (cls.includes('phone')) el.textContent = '\uf095';
else if (cls.includes('cut')) el.textContent = '\uf0c4';
else if (cls.includes('copy')) el.textContent = '\uf0c5';
else if (cls.includes('save')) el.textContent = '\uf0c7';
else if (cls.includes('bars')) el.textContent = '\uf0c9';
else if (cls.includes('envelope')) el.textContent = '\uf0e0';
else if (cls.includes('bolt')) el.textContent = '\uf0e7';
else if (cls.includes('bell')) el.textContent = '\uf0f3';
else if (cls.includes('keyboard')) el.textContent = '\uf11c';
else if (cls.includes('map-marker-alt')) el.textContent = '\uf3c5';
else if (cls.includes('wifi')) el.textContent = '\uf1eb';
else if (cls.includes('battery-full')) el.textContent = '\uf240';
else if (cls.includes('battery-three-quarters')) el.textContent = '\uf241';
else if (cls.includes('battery-half')) el.textContent = '\uf242';
else if (cls.includes('battery-quarter')) el.textContent = '\uf243';
else if (cls.includes('battery-empty')) el.textContent = '\uf244';
else if (cls.includes('usb')) el.textContent = '\uf287';
else if (cls.includes('bluetooth')) el.textContent = '\uf293';
else if (cls.includes('trash')) el.textContent = '\uf1f8';
else if (cls.includes('backspace')) el.textContent = '\uf55a';
else if (cls.includes('sd-card')) el.textContent = '\uf7c2';
else if (cls.includes('user')) el.textContent = '\uf007';
else if (cls.includes('thermometer-half')) el.textContent = '\uf2c9';
      else el.textContent = '\uf013'; // cog default
    }
    else { el.classList.add('lv-label'); if(tag==='h1') el.classList.add('h1'); el.textContent = node.textContent; }

    // Interactivity
    
    // Resize Handle
    const resizer = document.createElement('div');
    resizer.className = 'resize-handle';
    resizer.addEventListener('mousedown', e => {
      e.stopPropagation(); // prevent drag element
      selectElement(node, el, x, y, w, h);
      
      const startX = e.clientX, startY = e.clientY;
      const initialW = parseInt(node.style.width) || el.offsetWidth;
      const initialH = parseInt(node.style.height) || el.offsetHeight;

      function onMouseMove(ev) {
        const dw = ev.clientX - startX;
        const dh = ev.clientY - startY;
        const newW = Math.max(10, initialW + dw);
        const newH = Math.max(10, initialH + dh);
        
        node.style.width = newW + 'px';
        node.style.height = newH + 'px';
        el.style.width = newW + 'px';
        el.style.height = newH + 'px';
        
        // update inputs in panel
        document.getElementById('prop-w').value = newW;
        document.getElementById('prop-h').value = newH;
      }

      function onMouseUp() {
        window.removeEventListener('mousemove', onMouseMove);
        window.removeEventListener('mouseup', onMouseUp);
        updateEditorFromDoc();
      }

      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
    });
    el.appendChild(resizer);

    el.addEventListener('mousedown', e => {
      e.stopPropagation();
      selectElement(node, el, x, y, w, h);
      
      const startX = e.clientX, startY = e.clientY;
      const initialLeft = parseInt(style.left) || 0;
      const initialTop = parseInt(style.top) || 0;

      function onMouseMove(ev) {
        const dx = ev.clientX - startX;
        const dy = ev.clientY - startY;
        style.left = (initialLeft + dx) + 'px';
        style.top = (initialTop + dy) + 'px';
        style.position = 'absolute';
        
        el.style.left = (x + dx) + 'px';
        el.style.top = (y + dy) + 'px';
      }

      function onMouseUp() {
        window.removeEventListener('mousemove', onMouseMove);
        window.removeEventListener('mouseup', onMouseUp);
        updateEditorFromDoc(); // save changes
      }

      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
    });

    screenInner.appendChild(el);

    // Render children
    Array.from(node.children).forEach(child => walk(child, isPanel ? x : parentX, isPanel ? y : parentY));
  }
  
  Array.from(htmlDoc.body.children).forEach(child => walk(child));
}

// Properties Panel
function selectElement(node, uiEl, x, y, w, h) {
  document.querySelectorAll('.lv-el').forEach(e => e.classList.remove('selected'));
  uiEl.classList.add('selected');
  selectedNode = node;
  
  document.getElementById('props-container').style.display = 'block';
  document.getElementById('prop-text').value = node.textContent || '';
  document.getElementById('prop-x').value = parseInt(node.style.left) || 0;
  document.getElementById('prop-y').value = parseInt(node.style.top) || 0;
  document.getElementById('prop-w').value = parseInt(node.style.width) || 0;
  document.getElementById('prop-h').value = parseInt(node.style.height) || 0;
  
  const classGroup = document.getElementById('prop-class-group');
  if(node.tagName.toLowerCase() === 'i') {
    classGroup.style.display = 'block';
    document.getElementById('prop-class').value = node.className || '';
  } else {
    classGroup.style.display = 'none';
  }
}

document.getElementById('prop-apply').addEventListener('click', () => {
  if(!selectedNode) return;
  selectedNode.textContent = document.getElementById('prop-text').value;
  selectedNode.style.left = document.getElementById('prop-x').value + 'px';
  selectedNode.style.top = document.getElementById('prop-y').value + 'px';
  
  if(selectedNode.tagName.toLowerCase() === 'i') {
    selectedNode.className = document.getElementById('prop-class').value;
  }
  const pw = document.getElementById('prop-w').value;
  const ph = document.getElementById('prop-h').value;
  if(pw > 0) selectedNode.style.width = pw + 'px';
  if(ph > 0) selectedNode.style.height = ph + 'px';
  selectedNode.style.position = 'absolute';
  updateEditorFromDoc();
});

document.getElementById('prop-delete').addEventListener('click', () => {
  if(!selectedNode) return;
  selectedNode.remove();
  selectedNode = null;
  document.getElementById('props-container').style.display = 'none';
  updateEditorFromDoc();
});

// Drag & Drop Create
document.querySelectorAll('.tool-btn').forEach(btn => {
  btn.addEventListener('dragstart', e => { e.dataTransfer.setData('tag', btn.dataset.tag); });
});

screenInner.addEventListener('dragover', e => { e.preventDefault(); screenInner.classList.add('drag-over'); });
screenInner.addEventListener('dragleave', () => screenInner.classList.remove('drag-over'));
screenInner.addEventListener('drop', e => {
  e.preventDefault(); screenInner.classList.remove('drag-over');
  const tag = e.dataTransfer.getData('tag');
  if(!tag) return;

  const rect = screenInner.getBoundingClientRect();
  const x = Math.round(e.clientX - rect.left);
  const y = Math.round(e.clientY - rect.top);

  const newNode = htmlDoc.createElement(tag);
  newNode.style.position = 'absolute';
  newNode.style.left = x + 'px';
  newNode.style.top = y + 'px';
  
  if(tag === 'div') { newNode.className="bento"; newNode.style.width="100px"; newNode.style.height="100px"; }
  else if(tag === 'button') { newNode.style.width="80px"; newNode.textContent = "Botón"; }
  else if(tag === 'h1') { newNode.textContent = "Título"; }
  else if(tag === 'p') { newNode.textContent = "Texto"; }
  else if(tag === 'input') { newNode.style.width="120px"; newNode.placeholder = "Input"; }
  else if(tag === 'i') { newNode.className = "fa fa-home"; }
  else if(tag === 'chart') { newNode.setAttribute('type', 'line'); newNode.setAttribute('values', '10,25,18,30,42,35'); newNode.style.width="180px"; newNode.style.height="100px"; }

  htmlDoc.body.appendChild(newNode);
  updateEditorFromDoc();
});

editor.addEventListener('input', updateDocFromEditor);
updateDocFromEditor();
