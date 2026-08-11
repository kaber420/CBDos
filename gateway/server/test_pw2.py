from renderer import render_page_playwright
import json

elements = render_page_playwright("https://google.com")
print(json.dumps(elements, indent=2))
