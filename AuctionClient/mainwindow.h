#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSet>
#include <QTcpSocket> // Qt Socket
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct ProductInfo;
class QPushButton;
class QTableWidget;
class QWidget;

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
  void onRoomStartTimeout();

  void on_cboSort_currentIndexChanged(int index);
  void on_btnSendChat_clicked();
  void on_btnLogout_clicked();
  void showMyRoomsView();
  void reloadMyRooms();
  void editSelectedRoom();
  void stopSelectedRoom();
  void backToLobbyFromMyRooms();
  void myRoomsSelectionChanged();

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

  QTimer *m_roomStartTimer;
  QDateTime m_roomStartTime;
  bool m_roomHasStartTime = false;
  bool m_roomStartReached = true;
  qint64 m_roomServerEpoch = 0;
  QElapsedTimer m_roomServerElapsed;
  QWidget *m_pageMyRooms = nullptr;
  QTableWidget *m_tblMyRooms = nullptr;
  QPushButton *m_btnReloadMyRooms = nullptr;
  QPushButton *m_btnEditMyRoom = nullptr;
  QPushButton *m_btnStopMyRoom = nullptr;
  QPushButton *m_btnBackToLobby = nullptr;
  QPushButton *m_btnMyRoomsButton = nullptr;
  qint64 m_myRoomsServerEpoch = 0;
  QElapsedTimer m_myRoomsElapsed;
  QTimer *m_myRoomsGateTimer = nullptr;

  void resetRoomStartState();
  void updateRoomStartState(bool force = false);

  void updateRoomActionPermissions();
  QString formatPrice(int value) const;
  QString colorizeName(const QString &name) const;
  void updateRoomInfoUI(const QString &host, const QString &leader,
                        int bidCount, const QString &participants);
  void requestMyRooms();
  void populateMyRoomsTable(const QString &payload);
  QString buildProductPayload(const std::vector<ProductInfo> &products) const;
  qint64 myRoomsNowEpoch() const;
  bool canEditMyRoomRow(int row, QString *outReason = nullptr) const;
  bool canStopMyRoomRow(int row) const;
};
#endif // MAINWINDOW_H
