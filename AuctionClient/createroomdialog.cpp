#include "createroomdialog.h"
#include "ui_createroomdialog.h"
#include <QMessageBox>

CreateRoomDialog::CreateRoomDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CreateRoomDialog)
{
    ui->setupUi(this);
    // Kết nối nút OK tới hàm kiểm tra của mình
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &CreateRoomDialog::onVerifyAndAccept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Set giá trị tối đa cho giá tiền (để không bị giới hạn 99 mặc định)
    ui->spinPrice->setRange(0, 2000000000);
    ui->spinPrice->setSingleStep(100000); // Mỗi lần bấm tăng 100k

    ui->spinBuyNow->setRange(0, 2000000000);
    ui->spinBuyNow->setSingleStep(100000); // Mỗi lần bấm tăng 100k

}
// --- TRIỂN KHAI HÀM KIỂM TRA ---
void CreateRoomDialog::onVerifyAndAccept() {
    int startPrice = ui->spinPrice->value();
    int buyNowPrice = ui->spinBuyNow->value(); // Giả sử tên SpinBox là spinBuyNow

    // Logic kiểm tra
    if (buyNowPrice <= startPrice) {
        QMessageBox::warning(this, "Lỗi giá trị",
                             "Giá 'Mua Ngay' phải lớn hơn giá khởi điểm!");
        return; // Dừng lại, không đóng dialog
    }

    // Nếu mọi thứ OK thì mới đóng dialog và trả về Accepted
    accept();
}
QString CreateRoomDialog::auctionDuration() const {
    return QString::number(ui->spinDuration->value());
}

CreateRoomDialog::~CreateRoomDialog()
{
    delete ui;
}

QString CreateRoomDialog::productName() const
{
    return ui->txtProductName->text().trimmed();
}

QString CreateRoomDialog::productStartingPrice() const
{
    return QString::number(ui->spinPrice->value());
}

QString CreateRoomDialog::productBuyNowPrice() const
{
    return QString::number(ui->spinBuyNow->value());
}
