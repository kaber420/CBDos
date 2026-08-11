from selenium import webdriver
from selenium.webdriver.firefox.options import Options
from selenium.webdriver.firefox.service import Service
from webdriver_manager.firefox import GeckoDriverManager
import time

class FirefoxSeleniumRenderer:
    def __init__(self):
        self.driver = None
        
    def start(self):
        print("[Selenium] Iniciando motor Firefox (Anti-CAPTCHA)...")
        options = Options()
        options.add_argument("--headless")
        options.add_argument("--window-size=320,480")
        
        # Opciones para evitar detección de robots (básico)
        options.set_preference("dom.webdriver.enabled", False)
        options.set_preference("useAutomationExtension", False)

        service = Service(GeckoDriverManager().install())
        self.driver = webdriver.Firefox(service=service, options=options)
        print("[Selenium] Firefox listo.")

    def stop(self):
        if self.driver:
            self.driver.quit()

    def render_page(self, url: str):
        self.driver.get(url)
        # Esperar a que la página cargue y asiente su contenido
        time.sleep(2.5)
        
        # Mismo script de inyección matemático
        js_script = """
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
        """
        
        result = self.driver.execute_script(js_script)
        return result

_engine = FirefoxSeleniumRenderer()

def start_selenium_engine():
    _engine.start()

def stop_selenium_engine():
    _engine.stop()

def render_page_selenium(url: str):
    return _engine.render_page(url)
