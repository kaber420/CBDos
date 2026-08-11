from bs4 import BeautifulSoup
import re

def clean_html(html_content: str) -> str:
    """Cleans HTML content, removing JS, CSS, and returning semantic structure."""
    soup = BeautifulSoup(html_content, "html.parser")
    
    # Remove script, style, meta, noscript, etc.
    for tag in soup(["script", "style", "meta", "noscript", "link", "svg", "header", "footer", "nav", "aside", "iframe", "object"]):
        tag.decompose()
        
    # Simplify the structure: just extract main content if possible, or body
    body = soup.body if soup.body else soup
    
    # DEBUG: Inject a huge text block to verify rendering
    debug_h1 = soup.new_tag("h1")
    debug_h1.string = "DEBUG: PAGINA CARGADA!"
    body.insert(0, debug_h1)
    
    return str(body)
