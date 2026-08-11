from renderer_raw import start_raw_engine, stop_raw_engine, render_page_raw
import json

start_raw_engine()
try:
    elements = render_page_raw("https://www.google.com")
    print(json.dumps(elements, indent=2))
finally:
    stop_raw_engine()
