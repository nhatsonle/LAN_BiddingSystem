#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSet>
#include <QTcpSocket> // Thư viện Socket của Qt

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  // Public method for dialogs to send commands
  void sendRequest(const QString &cmd);

  // Friend class to allow backward access if needed
  friend class ProfileDialog;

private slots:
  void onConnected();
  void onReadyRead();

  // Slot cho các nút bấm mới
  void on_btnJoin_clicked();
  void on_btnLogin_clicked();
  void on_btnRefresh_clicked(); // Nút lấy danh sách phòng
  void on_btnLeave_clicked();   // Nút rời phòng quay lại sảnh
  void on_btnCreateRoom_clicked();
  void on_btnBid_clicked();
  void on_btnBuyNow_clicked();
  void on_btnProfile_clicked();
  void on_txtSearch_textChanged(const QString &arg1);
  void on_btnOpenRegister_clicked();
  void on_btnQuickBid_clicked();
  void on_btnShowDescription_clicked();
  void on_tblRoomProducts_cellClicked(int row, int column);

  void on_cboSort_currentIndexChanged(int index);
  void on_btnSendChat_clicked();
  void on_btnLogout_clicked();

private:
  Ui::MainWindow *ui;
  QTcpSocket *m_socket; // Đối tượng quản lý kết nối
  QString m_username;   // Lưu tên người dùng hiện tại
  int m_currentRoomId = -1;
  int m_currentPriceValue = 0;
  int m_buyNowPriceValue = 0;
  QSet<int> m_ownedRoomIds;
  bool m_isCurrentRoomHost = false;
  int m_activeProductId = -1;
  QString m_activeProductDescription;

  void updateRoomActionPermissions();
  QString formatPrice(int value) const;
  QString colorizeName(const QString &name) const;
  void updateRoomInfoUI(const QString &host, const QString &leader,
                        int bidCount, const QString &participants);
};
#endif // MAINWINDOW_H
