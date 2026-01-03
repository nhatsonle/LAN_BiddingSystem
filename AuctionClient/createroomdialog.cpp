#include "createroomdialog.h"
#include "ui_createroomdialog.h"
#include <QMessageBox>
#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>

// --- Constructor: Khớp với header ---
CreateRoomDialog::CreateRoomDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CreateRoomDialog),
    m_editingRow(-1) // Khởi tạo là -1 (Chế độ thêm mới)
{
    ui->setupUi(this);

    // --- CẤU HÌNH SPINBOX GIÁ ---

    // 1. Tăng giới hạn tối đa (Mặc định là 99 -> Sửa thành 2 tỷ)
    // Giúp bạn nhập được giá tiền lớn thoải mái
    ui->spinStartPrice->setRange(0, 2000000000);
    ui->spinBuyNow->setRange(0, 2000000000);
    ui->spinDuration->setRange(1, 1800); // tối đa 30 phút

    // 2. Chỉnh bước nhảy là 10.000 (Khi bấm nút mũi tên lên/xuống)
    ui->spinStartPrice->setSingleStep(10000);
    ui->spinBuyNow->setSingleStep(10000);
    ui->spinDuration->setSingleStep(5);

    // 3. Hiển thị dấu phẩy ngăn cách hàng nghìn cho dễ nhìn
    // Ví dụ: 10,000,000 thay vì 10000000
    // Lưu ý: Giá trị lấy về code (value()) vẫn là số nguyên bình thường
    ui->spinStartPrice->setGroupSeparatorShown(true);
    ui->spinBuyNow->setGroupSeparatorShown(true);

    // Cấu hình bảng hiển thị
    ui->tblProducts->setColumnCount(5);
    QStringList headers;
    headers << "Tên SP" << "Giá KĐ" << "Mua Ngay" << "Thời gian" << "Mô tả";
    ui->tblProducts->setHorizontalHeaderLabels(headers);

    // Tự động giãn cột cuối cho đẹp
    ui->tblProducts->horizontalHeader()->setStretchLastSection(true);
    // Chỉ cho chọn từng dòng
    ui->tblProducts->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Kết nối nút bấm
    connect(ui->btnAddProduct, &QPushButton::clicked, this, &CreateRoomDialog::on_btnAddProduct_clicked);
    connect(ui->btnEdit, &QPushButton::clicked, this, &CreateRoomDialog::on_btnEdit_clicked);     // Nút Sửa
    connect(ui->btnDelete, &QPushButton::clicked, this, &CreateRoomDialog::on_btnDelete_clicked); // Nút Xóa

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &CreateRoomDialog::onVerify);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    ui->dtStartTime->setDateTime(QDateTime::currentDateTime());
}

CreateRoomDialog::~CreateRoomDialog()
{
    delete ui;
}
// Hàm reset form tiện ích
void CreateRoomDialog::resetInputForm() {
    ui->txtName->clear();
    ui->txtDescription->clear();
    ui->spinStartPrice->setValue(0);
    ui->spinBuyNow->setValue(0);
    ui->spinDuration->setValue(30); // Giá trị mặc định
    ui->dtStartTime->setDateTime(QDateTime::currentDateTime());
    ui->txtName->setFocus();

    // Reset trạng thái về "Thêm mới"
    m_editingRow = -1;
    ui->btnAddProduct->setText("Thêm vào danh sách");

    // Mở lại các nút
    ui->btnEdit->setEnabled(true);
    ui->btnDelete->setEnabled(true);
    ui->tblProducts->setEnabled(true);
}


// --- Logic nút Thêm sản phẩm vào bảng ---
void CreateRoomDialog::on_btnAddProduct_clicked() {
    // 1. Lấy dữ liệu
    QString name = ui->txtName->text().trimmed();
    QString description = ui->txtDescription->text().trimmed();
    int start = ui->spinStartPrice->value();
    int buyNow = ui->spinBuyNow->value();
    int duration = ui->spinDuration->value();

    // 2. Validate (Giống cũ)
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Lỗi", "Vui lòng nhập tên sản phẩm!");
        return;
    }
    if (buyNow <= start) {
        QMessageBox::warning(this, "Lỗi", "Giá mua ngay phải lớn hơn giá khởi điểm!");
        return;
    }
    if (name.contains(';') || name.contains(',') || name.contains('|')) {
        QMessageBox::warning(this, "Lỗi", "Tên không được chứa ký tự đặc biệt (; , |)");
        return;
    }
    if (description.contains(';') || description.contains(',') || description.contains('|')) {
        QMessageBox::warning(this, "Lỗi", "Mô tả không được chứa ký tự đặc biệt (; , |)");
        return;
    }
    if (!description.isEmpty()) {
        QStringList words =
            description.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (words.size() > 50) {
            QMessageBox::warning(this, "Lỗi", "Mô tả tối đa 50 chữ.");
            return;
        }
    }

    // --- LOGIC MỚI: KIỂM TRA CHẾ ĐỘ ---

    if (m_editingRow == -1) {
        // === TRƯỜNG HỢP 1: THÊM MỚI ===
        m_products.push_back({name, description, start, buyNow, duration});

        int row = ui->tblProducts->rowCount();
        ui->tblProducts->insertRow(row);
        ui->tblProducts->setItem(row, 0, new QTableWidgetItem(name));
        ui->tblProducts->setItem(row, 1, new QTableWidgetItem(QString::number(start)));
        ui->tblProducts->setItem(row, 2, new QTableWidgetItem(QString::number(buyNow)));
        ui->tblProducts->setItem(row, 3, new QTableWidgetItem(QString::number(duration)));
        ui->tblProducts->setItem(row, 4, new QTableWidgetItem(description));
    }
    else {
        // === TRƯỜNG HỢP 2: CẬP NHẬT (LƯU SỬA) ===
        // 1. Cập nhật vào vector
        m_products[m_editingRow] = {name, description, start, buyNow, duration};

        // 2. Cập nhật lại Table Widget
        ui->tblProducts->item(m_editingRow, 0)->setText(name);
        ui->tblProducts->item(m_editingRow, 1)->setText(QString::number(start));
        ui->tblProducts->item(m_editingRow, 2)->setText(QString::number(buyNow));
        ui->tblProducts->item(m_editingRow, 3)->setText(QString::number(duration));
        ui->tblProducts->item(m_editingRow, 4)->setText(description);

        QMessageBox::information(this, "Thành công", "Đã cập nhật thông tin sản phẩm!");
    }

    // 4. Dọn dẹp form sau khi xong
    resetInputForm();
}

// --- Logic nút OK (Tạo phòng) ---
void CreateRoomDialog::onVerify() {
    if (ui->txtRoomName->text().isEmpty()) {
        QMessageBox::warning(this, "Lỗi", "Chưa nhập tên Phòng đấu giá!");
        return;
    }
    if (ui->txtRoomName->text().contains('|') || ui->txtRoomName->text().contains('\n') ||
        ui->txtRoomName->text().contains('\r')) {
        QMessageBox::warning(this, "Lỗi", "Tên phòng không được chứa ký tự '|'.");
        return;
    }
    if (m_products.empty()) {
        QMessageBox::warning(this, "Lỗi", "Phòng đấu giá phải có ít nhất 1 sản phẩm!");
        return;
    }
    accept(); // Đóng dialog và trả về kết quả OK
}

// --- HÀM 1: XỬ LÝ NÚT SỬA ---
void CreateRoomDialog::on_btnEdit_clicked() {
    int row = ui->tblProducts->currentRow();

    // Kiểm tra xem có dòng nào được chọn không
    if (row < 0 || row >= (int)m_products.size()) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng chọn một sản phẩm để sửa!");
        return;
    }

    // Load dữ liệu từ Vector lên các ô nhập liệu
    ProductInfo p = m_products[row];
    ui->txtName->setText(p.name);
    ui->txtDescription->setText(p.description);
    ui->spinStartPrice->setValue(p.startPrice);
    ui->spinBuyNow->setValue(p.buyNowPrice);
    ui->spinDuration->setValue(p.duration);

    // Chuyển sang chế độ chỉnh sửa
    m_editingRow = row;
    ui->btnAddProduct->setText("Lưu thay đổi"); // Đổi tên nút cho người dùng biết

    // (Tùy chọn) Disable nút Edit/Delete để tránh lỗi logic khi đang sửa
    ui->btnEdit->setEnabled(false);
    ui->btnDelete->setEnabled(false);
    ui->tblProducts->setEnabled(false); // Khóa bảng không cho chọn dòng khác
}

// --- HÀM 2: XỬ LÝ NÚT XÓA ---
void CreateRoomDialog::on_btnDelete_clicked() {
    int row = ui->tblProducts->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng chọn một sản phẩm để xóa!");
        return;
    }

    // Hỏi xác nhận cho chắc
    if (QMessageBox::question(this, "Xác nhận", "Bạn có chắc muốn xóa sản phẩm này?") == QMessageBox::Yes) {
        // 1. Xóa khỏi Vector
        m_products.erase(m_products.begin() + row);

        // 2. Xóa khỏi UI Table
        ui->tblProducts->removeRow(row);

        // 3. Reset form nếu cần
        resetInputForm();
    }
}

// --- Các hàm Getter để lấy dữ liệu ra ---
QString CreateRoomDialog::getRoomName() const {
    return ui->txtRoomName->text();
}

QString CreateRoomDialog::getStartTimeString() const {
    return ui->dtStartTime->dateTime().toString("yyyy-MM-dd HH:mm:ss");
}

std::vector<ProductInfo> CreateRoomDialog::getProductList() const {
    return m_products;
}
