#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // --- BẮT ĐẦU ĐOẠN STYLE SHEET ---
    a.setStyleSheet(
        // 1. Cấu hình chung cho toàn bộ App
        "QWidget { "
        "   font-family: 'Segoe UI', 'Arial'; "
        "   font-size: 14px; "
        "   color: #333333; "
        "   background-color: #f4f7f6; " // Màu nền xám nhẹ hiện đại
        "} "

        // 2. Nút bấm (QPushButton)
        "QPushButton { "
        "   background-color: #3498db; " // Màu xanh dương chủ đạo
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "        // Bo tròn góc
        "   font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "   background-color: #2980b9; " // Màu khi di chuột vào (đậm hơn)
        "} "
        "QPushButton:pressed { "
        "   background-color: #1c6ea4; " // Màu khi nhấn xuống
        "} "
        "QPushButton:disabled { "
        "   background-color: #bdc3c7; " // Màu xám khi bị vô hiệu hóa
        "   color: #7f8c8d; "
        "} "

        // 3. Ô nhập liệu (QLineEdit, QSpinBox)
        "QLineEdit, QSpinBox { "
        "   background-color: white; "
        "   border: 1px solid #bdc3c7; "
        "   border-radius: 4px; "
        "   selection-background-color: #3498db; "
        "} "
        "QLineEdit:focus, QSpinBox:focus { "
        "   border: 2px solid #3498db; " // Viền xanh khi đang nhập
        "} "

        // 4. Danh sách (QListWidget, QTableWidget)
        "QListWidget, QTableWidget { "
        "   background-color: white; "
        "   border: 1px solid #bdc3c7; "
        "   border-radius: 4px; "
        "   padding: 5px; "
        "} "
        "QListWidget::item { "
        "   padding: 10px; "
        "   margin-bottom: 5px; "
        "   border-bottom: 1px solid #ecf0f1; "
        "} "
        "QListWidget::item:selected { "
        "   background-color: #d6eaf8; " // Màu nền khi chọn dòng
        "   color: #2c3e50; "
        "   border-left: 5px solid #3498db; " // Điểm nhấn bên trái
        "} "

        // 5. GroupBox (Khung gom nhóm)
        "QGroupBox { "
        "   font-weight: bold; "
        "   border: 1px solid #bdc3c7; "
        "   border-radius: 6px; "
        "   margin-top: 10px; "
        "   background-color: white; "
        "} "
        "QGroupBox::title { "
        "   subcontrol-origin: margin; "
        "   left: 10px; "
        "   padding: 0 5px; "
        "} "

        // 6. LCD Number (Đồng hồ đếm ngược)
        "QLCDNumber { "
        "   background-color: #2c3e50; " // Nền đen
        "   color: #e74c3c; "            // Chữ đỏ (bạn set color trong code rồi thì cái này là fallback)
        "   border: 2px solid #34495e; "
        "   border-radius: 4px; "
        "} "

        // 7. Label tiêu đề (Nếu bạn đặt objectName là lblTitle)
        "QLabel#lblTitle { "
        "   font-size: 20px; "
        "   font-weight: bold; "
        "   color: #2c3e50; "
        "} "
        );
    // --- KẾT THÚC STYLE SHEET ---

    MainWindow w;
    w.show();
    return a.exec();
}
