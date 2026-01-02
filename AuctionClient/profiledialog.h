#ifndef PROFILEDIALOG_H
#define PROFILEDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class ProfileDialog;
}

class MainWindow; // Forward declaration

class ProfileDialog : public QDialog {
  Q_OBJECT

public:
  explicit ProfileDialog(MainWindow *pw, QWidget *parent = nullptr);
  ~ProfileDialog();

  // Các hàm để MainWindow gọi cập nhật dữ liệu vào Dialog
  void updateHistory(const QString &data);
  void updateWonList(const QString &data);
  void onChangePassResult(bool success, const QString &msg);

private slots:
  void on_btnChangePass_clicked();
  void on_btnRefreshHistory_clicked();
  void on_btnRefreshWon_clicked();
  void on_btnClose_clicked();

private:
  Ui::ProfileDialog *ui;
  MainWindow *m_mainWindow; // Giữ tham chiếu để gửi lệnh qua socket của Main
};

#endif // PROFILEDIALOG_H
