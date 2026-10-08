FROM python:3.13-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY server.py .

ENV PYTHONUNBUFFERED=1

EXPOSE 5000

# gunicorn is a production web server that runs the Flask app
CMD ["gunicorn", "--bind", "0.0.0.0:5000", "server:app"]
