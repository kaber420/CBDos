import requests

def fetch_html(url: str) -> str:
    """Fetches real HTML from the given URL."""
    try:
        # Add basic headers to simulate a real browser request
        headers = {
            "User-Agent": "AlternetGateway/1.0"
        }
        # default to http if no scheme
        if not url.startswith("http://") and not url.startswith("https://"):
            url = "http://" + url
            
        response = requests.get(url, headers=headers, timeout=10)
        response.raise_for_status()
        return response.text
    except Exception as e:
        print(f"Error fetching URL: {url} -> {e}")
        # Return a simple error HTML if it fails
        return f"<body><div><h1>Error</h1><p>Failed to fetch {url}</p></div></body>"
