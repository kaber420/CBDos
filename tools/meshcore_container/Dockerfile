FROM python:3.11-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY mesh_framing.py mesh_protocol.py mesh_node.py cli.py ./
RUN chmod +x cli.py

ENTRYPOINT ["python3", "-u", "cli.py"]
