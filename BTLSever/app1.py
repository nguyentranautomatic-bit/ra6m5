from flask import Flask, render_template, jsonify, request
import datetime
import csv
import os

app = Flask(__name__)

# Tên file để lưu dữ liệu huấn luyện
TRAIN_DATA_FILE = 'data_raw.csv'

# Kiểm tra và tạo file CSV nếu chưa có, ghi tiêu đề cột
if not os.path.exists(TRAIN_DATA_FILE):
    with open(TRAIN_DATA_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Timestamp', 'iaq', 'tvoc', 'eco2'])

# Kho lưu trữ dữ liệu hiển thị (tối đa 20 điểm)
data_store = {
    "labels": [],
    "iaq": [],
    "tvoc": [],
    "eco2": [],
    "status": "CONNECTING"
}

@app.route('/')
def home():
    return render_template('index.html')

@app.route('/api/telemetry', methods=['POST'])
def update_data():
    try:
        payload = request.get_json()
        if not payload:
            return jsonify({"status": "error", "message": "No JSON payload"}), 400

        now_time = datetime.datetime.now().strftime("%H:%M:%S")
        full_ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        val_iaq = payload.get('iaq')
        val_tvoc = payload.get('tvoc')
        val_eco2 = payload.get('eco2')

        if val_iaq is not None:
            # 1. LƯU DỮ LIỆU VÀO CSV ĐỂ HUẤN LUYỆN AI sau này
            with open(TRAIN_DATA_FILE, 'a', newline='') as f:
                writer = csv.writer(f)
                writer.writerow([full_ts, val_iaq, val_tvoc, val_eco2])

            # 2. Cập nhật vào kho dữ liệu hiển thị Web
            data_store["labels"].append(now_time)
            data_store["iaq"].append(val_iaq)
            data_store["tvoc"].append(val_tvoc)
            data_store["eco2"].append(val_eco2)
            data_store["status"] = "RUNNING"

            if len(data_store["labels"]) > 20:
                for key in ["labels", "iaq", "tvoc", "eco2"]:
                    data_store[key].pop(0)
            
            print(f"[{now_time}] Saved to CSV & Dashboard: IAQ={val_iaq}")
            return jsonify({"status": "success"}), 200
        
        return jsonify({"status": "no data"}), 400

    except Exception as e:
        print(f"Error: {e}")
        return jsonify({"status": "error", "message": str(e)}), 400

@app.route('/get-data')
def get_data():
    return jsonify(data_store)

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)