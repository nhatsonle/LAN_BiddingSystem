#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket> // Thư viện Socket của Qt

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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
    void on_btnHistory_clicked();
    void on_txtSearch_textChanged(const QString &arg1);

private:
    Ui::MainWindow *ui;
    QTcpSocket *m_socket; // Đối tượng quản lý kết nối
    QString m_username; // Lưu tên người dùng hiện tại
    int m_currentRoomId;
};
#endif // MAINWINDOW_H
