import asyncio
import json
import subprocess
import time
import urllib.request
import websockets
import os

class RawBiDiEngine:
    def __init__(self):
        self.process = None
        self.ws_url = None
        
    def start(self):
        profile_dir = os.path.join(os.path.dirname(__file__), "raw_profile")
        os.makedirs(profile_dir, exist_ok=True)
        
        print("[RawBiDi] Iniciando Motor Crudo por WebSockets (Sin Playwright)...")
        
        self.process = subprocess.Popen([
            "/home/kaber420/.cache/ms-playwright/chromium-1234/chrome-linux64/chrome",
            "--headless=new",
            "--disable-gpu",
            "--no-sandbox",
            "--remote-debugging-port=9222",
            f"--user-data-dir={profile_dir}",
            "--window-size=320,480"
        ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        
        # Esperar a que levante el puerto de depuración
        for _ in range(20):
            try:
                req = urllib.request.urlopen("http://localhost:9222/json/list")
                data = json.loads(req.read())
                for page in data:
                    if page.get("type") == "page":
                        self.ws_url = page.get("webSocketDebuggerUrl")
                        if self.ws_url:
                            break
                if self.ws_url:
                    break
            except Exception:
                time.sleep(0.5)
                
        if not self.ws_url:
            raise Exception("No se pudo obtener la URL WebSocket del motor en el puerto 9222.")
            
        print(f"[RawBiDi] Conectado exitosamente por WebSocket: {self.ws_url}")

    def stop(self):
        if self.process:
            self.process.terminate()
            self.process.wait()

    async def _render_page(self, url: str):
        async with websockets.connect(self.ws_url, max_size=None) as ws:
            msg_id = 1
            
            # 1. Navegar a la URL
            await ws.send(json.dumps({
                "id": msg_id,
                "method": "Page.navigate",
                "params": {"url": url}
            }))
            msg_id += 1
            
            # Esperar a que la página cargue
            await asyncio.sleep(2.5) 
            
            # 2. Inyectar el script extractor de coordenadas matemáticas
            js_script = """
            (function() {
                const elements = [];
                let linkId = 1;
                
                function isVisible(el) {
                    const style = window.getComputedStyle(el);
                    return style.display !== 'none' && style.visibility !== 'hidden' && style.opacity !== '0';
                }
                
                const textNodes = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, null, false);
                let node;
                while (node = textNodes.nextNode()) {
                    const text = node.nodeValue.trim();
                    if (text.length === 0) continue;
                    
                    const parent = node.parentElement;
                    if (!parent || !isVisible(parent)) continue;
                    if (['SCRIPT', 'STYLE', 'NOSCRIPT'].includes(parent.tagName)) continue;
                    
                    const rect = parent.getBoundingClientRect();
                    if (rect.width === 0 || rect.height === 0) continue;
                    
                    let isLink = false;
                    let action = "";
                    let curr = parent;
                    while (curr && curr !== document.body) {
                        if (curr.tagName === 'A' && curr.href) {
                            isLink = true;
                            action = curr.href;
                            break;
                        }
                        curr = curr.parentElement;
                    }
                    
                    const w = Math.round(rect.width * 1.2);
                    const h = Math.round(rect.height);
                    
                    if (isLink) {
                        elements.push({
                            type: 'link',
                            x: Math.round(rect.left),
                            y: Math.round(rect.top),
                            w: w,
                            h: h,
                            text: text.substring(0, 50),
                            action: action,
                            link_id: linkId++
                        });
                    } else {
                        let styleFlag = 0;
                        const fw = window.getComputedStyle(parent).fontWeight;
                        if (fw === 'bold' || parseInt(fw) >= 600) styleFlag |= 1;
                        
                        elements.push({
                            type: 'text',
                            x: Math.round(rect.left),
                            y: Math.round(rect.top),
                            w: w,
                            h: h,
                            text: text.substring(0, 50),
                            style: styleFlag
                        });
                    }
                }
                
                const inputs = document.querySelectorAll('input, textarea');
                inputs.forEach(input => {
                    if (!isVisible(input)) return;
                    const rect = input.getBoundingClientRect();
                    if (rect.width === 0 || rect.height === 0) return;
                    
                    elements.push({
                        type: 'input',
                        x: Math.round(rect.left),
                        y: Math.round(rect.top),
                        w: Math.round(rect.width * 1.2),
                        h: Math.round(rect.height),
                        name: input.name || input.id || "input",
                        action: input.form ? input.form.action : "",
                        placeholder: input.placeholder || "Escribir..."
                    });
                });
                
                return elements;
            })();
            """
            
            await ws.send(json.dumps({
                "id": msg_id,
                "method": "Runtime.evaluate",
                "params": {
                    "expression": js_script,
                    "returnByValue": True
                }
            }))
            
            # Recibir la respuesta del script
            while True:
                resp = await ws.recv()
                data = json.loads(resp)
                if data.get("id") == msg_id:
                    result = data.get("result", {}).get("result", {}).get("value")
                    return result

    def render_page(self, url: str):
        return asyncio.run(self._render_page(url))

_engine = RawBiDiEngine()

def start_raw_engine():
    _engine.start()

def stop_raw_engine():
    _engine.stop()

def render_page_raw(url: str, width: int = 480, height: int = 640):
    return _engine.render_page(url)
