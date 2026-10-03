from datetime import datetime
 
from flask import Flask, request
 
app = Flask(__name__)
 
 
@app.post("/data")
def receive():
    now = datetime.now().strftime("%H:%M:%S")
    body = request.get_data(as_text=True)
    print(f"[{now}] from {request.remote_addr}: {body}", flush=True)
    return "ok"
 
 
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
