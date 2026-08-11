from renderer import render_page_playwright
import json

elements = render_page_playwright("https://www.google.com/search?q=clima+km43")
print(json.dumps(elements, indent=2))
