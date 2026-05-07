import pandas as pd
from sklearn.linear_model import LinearRegression

# 1. Đọc dữ liệu thô bạn vừa thu thập
df = pd.read_csv('zmod4410_data.csv')

# 2. TỰ TẠO CỘT TARGET (Dịch chuyển cột IAQ lên 10 dòng)
# Giá trị mục tiêu của hiện tại là giá trị IAQ của 10 phút sau
df['Target_IAQ_10m'] = df['iaq'].shift(-10)

# 3. Tạo các cột độ lệch (Tiền xử lý) 
df['iaq_diff'] = df['iaq'].diff().fillna(0)
df['tvoc_diff'] = df['tvoc'].diff().fillna(0)

# 4. Xóa các dòng cuối cùng (vì 10 dòng cuối không có dữ liệu tương lai để dự đoán)
df = df.dropna(subset=['Target_IAQ_10m'])

# 5. Chọn đầu vào (Features) và đầu ra (Target)
X = df[['iaq', 'iaq_diff', 'tvoc_diff']]
y = df['Target_IAQ_10m']

# 6. Huấn luyện mô hình
model = LinearRegression()
model.fit(X, y)

print("--- KẾT QUẢ THÀNH CÔNG ---")
print(f"W1 (iaq_current): {model.coef_[0]:.6f}")
print(f"W2 (iaq_diff): {model.coef_[1]:.6f}")
print(f"W3 (tvoc_diff): {model.coef_[2]:.6f}")
print(f"Bias: {model.intercept_:.6f}")