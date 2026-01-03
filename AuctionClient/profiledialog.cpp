#include "profiledialog.h"
#include "mainwindow.h"
#include "ui_profiledialog.h"
#include <QMessageBox>

ProfileDialog::ProfileDialog(MainWindow *pw, QWidget *parent)
    : QDialog(parent), ui(new Ui::ProfileDialog), m_mainWindow(pw) {
  ui->setupUi(this);

  // Auto load data when opened
  on_btnRefreshHistory_clicked();
  on_btnRefreshWon_clicked();
}

ProfileDialog::~ProfileDialog() { delete ui; }

void ProfileDialog::on_btnClose_clicked() { this->close(); }

void ProfileDialog::on_btnChangePass_clicked() {
  QString oldP = ui->txtOldPass->text();
  QString newP = ui->txtNewPass->text();
  QString conf = ui->txtConfirmPass->text();

  if (oldP.isEmpty() || newP.isEmpty()) {
    QMessageBox::warning(this, "Lỗi", "Vui lòng nhập đầy đủ thông tin.");
    return;
  }

  if (newP != conf) {
    QMessageBox::warning(this, "Lỗi", "Mật khẩu xác nhận không khớp.");
    return;
  }

  // Gửi lệnh qua MainWindow
  m_mainWindow->sendRequest("CHANGE_PASS|" + oldP + "|" + newP + "\n");
}

void ProfileDialog::on_btnRefreshHistory_clicked() {
  m_mainWindow->sendRequest("GET_HISTORY\n");
}

void ProfileDialog::on_btnRefreshWon_clicked() {
  m_mainWindow->sendRequest("GET_WON\n");
}

// --- XỬ LÝ DỮ LIỆU TỪ SERVER TRẢ VỀ ---

void ProfileDialog::onChangePassResult(bool success, const QString &msg) {
  if (success) {
    QMessageBox::information(this, "Thành công", msg);
    ui->txtOldPass->clear();
    ui->txtNewPass->clear();
    ui->txtConfirmPass->clear();
  } else {
    QMessageBox::critical(this, "Lỗi", msg);
  }
}

void ProfileDialog::updateHistory(const QString &data) {
  ui->tableHistory->setRowCount(0);
  // Format item: Name:Price:Winner;...
  QStringList items = data.split(';', Qt::SkipEmptyParts);

  // Set headers if needed (though UI handles it)
  // ui->tableHistory->setHorizontalHeaderLabels({"Vật phẩm", "Giá cuối", "Người
  // thắng"});

  for (const QString &item : items) {
    QStringList details = item.split(':');
    if (details.size() >= 3) {
      int row = ui->tableHistory->rowCount();
      ui->tableHistory->insertRow(row);

      ui->tableHistory->setItem(row, 0,
                                new QTableWidgetItem(details[0])); // Name
      ui->tableHistory->setItem(row, 1,
                                new QTableWidgetItem(details[1])); // Price
      ui->tableHistory->setItem(row, 2,
                                new QTableWidgetItem(details[2])); // Winner
    }
  }
}

void ProfileDialog::updateWonList(const QString &data) {
  ui->tableWon->setRowCount(0);
  // Format: Item:Price:Time;...
  QStringList items = data.split(';', Qt::SkipEmptyParts);

  for (const QString &item : items) {
    QStringList details = item.split(':');
    if (details.size() >= 3) {
      int row = ui->tableWon->rowCount();
      ui->tableWon->insertRow(row);

      ui->tableWon->setItem(row, 0, new QTableWidgetItem(details[0])); // Item
      ui->tableWon->setItem(row, 1, new QTableWidgetItem(details[1])); // Price
      ui->tableWon->setItem(row, 2, new QTableWidgetItem(details[2])); // Time
    }
  }
}
