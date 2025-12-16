#include "createroomdialog.h"
#include "ui_createroomdialog.h"

CreateRoomDialog::CreateRoomDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CreateRoomDialog)
{
    ui->setupUi(this);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Set giá trị tối đa cho giá tiền (để không bị giới hạn 99 mặc định)
    ui->spinPrice->setRange(0, 2000000000);
    ui->spinPrice->setSingleStep(100000); // Mỗi lần bấm tăng 100k
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

