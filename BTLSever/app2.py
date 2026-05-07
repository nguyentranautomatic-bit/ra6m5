from flask import Flask, render_template, jsonify, request
import datetime

app = Flask(__name__)

# Kho lưu trữ dữ liệu (tối đa 20 điểm dữ liệu)
data_store = {
    "labels": [],
    "iaq": [],
    "tvoc": [],
    "eco2": [],
    "pred": [],  # Thêm mảng này để lưu giá trị dự đoán
    "status": "CONNECTING"
}

@app.route('/')
def home():
    # Trả về giao diện index.html
    return render_template('index.html')

# --- ENDPOINT NHẬN DỮ LIỆU TỪ ESP32 ---
# Đổi route thành /api/telemetry để khớp với code ESP32
@app.route('/api/telemetry', methods=['POST'])
def update_data():
    try:
        payload = request.get_json()
        
        if not payload:
            return jsonify({"status": "error", "message": "No JSON payload"}), 400

        now = datetime.datetime.now().strftime("%H:%M:%S")
        
        # Lấy dữ liệu từ JSON gửi lên
        val_iaq = payload.get('iaq')
        val_tvoc = payload.get('tvoc')
        val_eco2 = payload.get('eco2')
        val_pred = payload.get('pred') # Lấy giá trị dự đoán từ ESP32 gửi lên
        # Cập nhật dữ liệu vào kho
        if val_iaq is not None:
            data_store["labels"].append(now)
            data_store["iaq"].append(val_iaq)
            data_store["tvoc"].append(val_tvoc)
            data_store["eco2"].append(val_eco2)
            data_store["pred"].append(val_pred) # Lưu vào kho
            data_store["status"] = "RUNNING"

            # Giữ giới hạn 20 mẫu để biểu đồ không bị quá dày
            if len(data_store["labels"]) > 20:
                for key in ["labels", "iaq", "tvoc", "eco2", "pred"]: # Cập nhật danh sách key cần pop
                    data_store[key].pop(0)
            
            print(f"[{now}] Received: IAQ={val_iaq}, TVOC={val_tvoc}, ECO2={val_eco2}")
            return jsonify({"status": "success"}), 200
        
        return jsonify({"status": "no data"}), 400

    except Exception as e:
        print(f"Error: {e}")
        return jsonify({"status": "error", "message": str(e)}), 400

# --- ENDPOINT TRẢ DỮ LIỆU CHO DASHBOARD (WEB) ---
@app.route('/get-data')
def get_data():
    return jsonify(data_store)

if __name__ == '__main__':
    # Chạy trên tất cả IP (0.0.0.0) để ESP32 có thể truy cập qua IP máy tính
    # Port 5000 (Đảm bảo code ESP32 cũng gọi đúng port này)
    app.run(debug=True, host='0.0.0.0', port=5000)