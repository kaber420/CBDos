from playwright.sync_api import sync_playwright

def render_page_playwright(url: str, width: int = 320, height: int = 240) -> list:
    elements = []
    try:
        with sync_playwright() as p:
            browser = p.chromium.launch(headless=True)
            context = browser.new_context(viewport={'width': width, 'height': height})
            page = context.new_page()
            
            page.goto(url, wait_until='domcontentloaded', timeout=15000)
            page.wait_for_timeout(1000) # Give dynamic JS time to render (e.g. Google search box)
            
            # Script to extract visible bounding boxes
            script = """
            () => {
                const elems = [];
                function isVisible(el) {
                    const style = window.getComputedStyle(el);
                    return style.display !== 'none' && style.visibility !== 'hidden' && style.opacity !== '0';
                }
                
                // Texts
                const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, null, false);
                let node;
                while(node = walker.nextNode()) {
                    const text = node.nodeValue.trim();
                    if (text.length === 0) continue;
                    const parent = node.parentElement;
                    if (!parent || !isVisible(parent)) continue;
                    
                    const tag = parent.tagName.toLowerCase();
                    if (['script', 'style', 'noscript', 'button', 'a'].includes(tag)) continue;
                    
                    const range = document.createRange();
                    range.selectNodeContents(node);
                    const rects = range.getClientRects();
                    if (rects.length === 0) continue;
                    const rect = rects[0];
                    if (rect.width === 0 || rect.height === 0) continue;
                    
                    const style = window.getComputedStyle(parent);
                    let textStyle = 0; // Normal
                    if (['h1', 'h2', 'h3'].includes(tag) || style.fontWeight === 'bold' || parseInt(style.fontWeight) >= 600) {
                        textStyle = 1; // Bold
                    }
                    
                    elems.push({
                        type: 'text',
                        x: Math.round(rect.x + window.scrollX),
                        y: Math.round(rect.y + window.scrollY),
                        w: Math.round(rect.width),
                        h: Math.round(rect.height),
                        text: text,
                        style: textStyle
                    });
                }
                
                // Links and Buttons
                let linkId = 1;
                document.querySelectorAll('a, button').forEach(el => {
                    if (!isVisible(el)) return;
                    const rect = el.getBoundingClientRect();
                    if (rect.width === 0 || rect.height === 0) return;
                    
                    let text = el.innerText.trim();
                    if (!text) text = el.value || el.getAttribute('aria-label') || "Link";
                    if (!text) return;
                    
                    let action = "";
                    if (el.tagName.toLowerCase() === 'a') action = el.href;
                    
                    elems.push({
                        type: 'link',
                        x: Math.round(rect.x + window.scrollX),
                        y: Math.round(rect.y + window.scrollY),
                        w: Math.round(rect.width),
                        h: Math.round(rect.height),
                        text: text,
                        action: action,
                        link_id: linkId++
                    });
                });
                
                // Inputs
                document.querySelectorAll('input, textarea').forEach(el => {
                    if (!isVisible(el)) return;
                    const tag = el.tagName.toLowerCase();
                    const type = el.type ? el.type.toLowerCase() : 'text';
                    if (tag === 'input' && !['text', 'search', 'email', 'password', 'url'].includes(type)) return;
                    
                    const rect = el.getBoundingClientRect();
                    if (rect.width === 0 || rect.height === 0) return;
                    
                    let action = "";
                    const form = el.closest('form');
                    if (form) action = form.getAttribute('action') || "";
                    
                    let placeholder = el.placeholder || el.title || "Input";
                    
                    elems.push({
                        type: 'input',
                        x: Math.round(rect.x + window.scrollX),
                        y: Math.round(rect.y + window.scrollY),
                        w: Math.round(rect.width),
                        h: Math.round(rect.height),
                        name: el.name || "",
                        action: action,
                        placeholder: placeholder
                    });
                });
                
                return elems;
            }
            """
            elements = page.evaluate(script)
            browser.close()
            return elements
    except Exception as e:
        print(f"Error rendering page with Playwright: {e}")
        return []

def render_page(url: str, use_playwright: bool = True, width: int = 320, height: int = 240) -> list:
    if use_playwright:
        return render_page_playwright(url, width=width, height=height)
    else:
        # TODO: Implement Raw BiDi Renderer
        return []
