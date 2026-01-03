-- Sample data for auction system
PRAGMA foreign_keys = ON;

-- Users (passwords in plain text per current code)
INSERT INTO users (username, password) VALUES
('admin', '123456'),
('user1', '123456'),
('user2', '123456'),
('user3', '123456');

-- Rooms (created_by maps to users above)
INSERT INTO rooms (name, status, created_by, start_time) VALUES
('Room Demo 1', 'OPEN', 1, CURRENT_TIMESTAMP),
('Room Demo 2', 'OPEN', 2, CURRENT_TIMESTAMP),
('Room Demo 3', 'OPEN', 3, CURRENT_TIMESTAMP),
('Room Demo 4', 'OPEN', 4, CURRENT_TIMESTAMP);

-- Products (status defaults to WAITING)
INSERT INTO products (room_id, name, start_price, buy_now_price, duration, status, description) VALUES
(1, 'SP1-1', 100000, 500000, 120, 'WAITING', 'Dong ho thong minh nhap khau'),
(1, 'SP1-2', 200000, 600000, 150, 'WAITING', 'May anh mini chong rung cho choi'),
(1, 'SP1-3', 300000, 700000, 180, 'WAITING', 'Loa bluetooth cho am thanh truong phong'),
(2, 'SP2-1', 150000, 550000, 180, 'WAITING', 'Laptop cho sinh vien nho gon mau trang'),
(2, 'SP2-2', 250000, 700000, 200, 'WAITING', 'May anh mirrorless chup chup sang tao'),
(2, 'SP2-3', 350000, 800000, 210, 'WAITING', 'Thiet bi gaming he thong tan nhiet tot'),
(3, 'SP3-1', 120000, 520000, 140, 'WAITING', 'Tay cam choi game co den led'),
(3, 'SP3-2', 180000, 620000, 160, 'WAITING', 'Pin du phong dung luu ngay dem'),
(3, 'SP3-3', 260000, 720000, 190, 'WAITING', 'May loc khong khi mini cho van phong'),
(3, 'SP3-4', 320000, 820000, 220, 'WAITING', 'Chuot khong day co dau nhay nhe'),
(4, 'SP4-1', 110000, 510000, 130, 'WAITING', 'Tai nghe khong day choi nhac chat luong cao'),
(4, 'SP4-2', 170000, 610000, 155, 'WAITING', 'Balo di du lich chong nuoc gon nhe'),
(4, 'SP4-3', 240000, 710000, 185, 'WAITING', 'Ban phim co dien thoai thong minh phu hop'),
(4, 'SP4-4', 280000, 810000, 210, 'WAITING', 'Camera an ninh do giai cao ket noi wifi'),
(4, 'SP4-5', 340000, 910000, 230, 'WAITING', 'Bong den thong minh dieu khien tu xa');
