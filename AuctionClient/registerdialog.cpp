#include "registerdialog.h"
#include "ui_registerdialog.h"
#include <QMessageBox>

RegisterDialog::RegisterDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    setWindowTitle("Đăng ký tài khoản");

    // Kết nối nút OK với hàm kiểm tra của mình thay vì đóng luôn
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &RegisterDialog::onVerify);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

QString RegisterDialog::getUsername() const {
    return ui->txtUser->text().trimmed();
}

QString RegisterDialog::getPassword() const {
    return ui->txtPass->text();
}

void RegisterDialog::onVerify() {
    if (ui->txtUser->text().isEmpty() || ui->txtPass->text().isEmpty()) {
        QMessageBox::warning(this, "Lỗi", "Vui lòng nhập đầy đủ thông tin!");
        return;
    }

    if (ui->txtPass->text() != ui->txtConfirmPass->text()) {
        QMessageBox::warning(this, "Lỗi", "Mật khẩu xác nhận không khớp!");
        return;
    }

    accept(); // Đóng dialog và trả về QDialog::Accepted
}
