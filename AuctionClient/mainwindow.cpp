#include "mainwindow.h"
#include "createroomdialog.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QTimer>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Khởi tạo Socket
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);

    // Mặc định hiển thị trang Login (Index 0)
    ui->stackedWidget->setCurrentIndex(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// --- XỬ LÝ NÚT BẤM ---

void MainWindow::on_btnBid_clicked()
{
    // 1. Lấy giá hiện tại từ giao diện (bóc tách từ chuỗi hiển thị hoặc lưu biến riêng)

    int currentPrice = ui->lblCurrentPrice->text().toInt();
    int bidAmount = ui->txtBidAmount->text().toInt();

    // 2. Kiểm tra quy tắc +10.000
    if (bidAmount < currentPrice + 10000) {
        QMessageBox::warning(this, "Không hợp lệ",
                             "Giá đấu phải cao hơn giá hiện tại ít nhất 10.000 VND!");
        return; // Dừng lại ngay, KHÔNG gửi gì lên Server cả
    }

    // 3. Nếu OK thì mới gửi
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        QString msg = QString("BID|%1|%2\n").arg(m_currentRoomId).arg(bidAmount);
        m_socket->write(msg.toUtf8());
    }
}

void MainWindow::on_btnLogin_clicked()
{
    // 1. Kết nối tới Server (nếu chưa)
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        m_socket->connectToHost("127.0.0.1", 8080); // Hardcode IP cho nhanh, hoặc lấy từ ô nhập
        if (!m_socket->waitForConnected(3000)) {
            QMessageBox::critical(this, "Lỗi", "Không thể kết nối Server!");
            return;
        }
    }

    // 2. Gửi lệnh Login
    QString user = ui->txtUser->text();
    QString pass = ui->txtPass->text();

    if (user.isEmpty()) return;

    m_username = user; // Lưu lại
    QString command = "LOGIN|" + user + "|" + pass + "\n";
    m_socket->write(command.toUtf8());
}

void MainWindow::on_btnRefresh_clicked()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "Client: Sending LIST_ROOMS..."; // <--- THÊM DÒNG NÀY

        // KIỂM TRA KỸ: Có ký tự xuống dòng \n ở cuối không?
        m_socket->write("LIST_ROOMS\n");

        // Nếu thiếu \n, Server (dùng getline hoặc buffer) có thể đợi mãi ký tự kết thúc dòng
    } else {
        qDebug() << "Client: Socket is NOT connected. State:" << m_socket->state();
        QMessageBox::warning(this, "Lỗi", "Mất kết nối với Server!");
    }
}
void MainWindow::on_btnLeave_clicked()
{
    // 1. Gửi thông báo cho Server biết mình rút lui
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        QString cmd = QString("LEAVE_ROOM|%1\n").arg(m_currentRoomId);
        m_socket->write(cmd.toUtf8());
    }

    // 2. Xử lý giao diện (UI)
    ui->stackedWidget->setCurrentIndex(1); // Quay về trang Lobby

    // Xóa log chat cũ để lần sau vào phòng khác không bị lẫn lộn
    ui->txtRoomLog->clear();

    // (Tùy chọn) Reset ID phòng hiện tại
    m_currentRoomId = -1;

    // (Tùy chọn) Refresh lại danh sách phòng ngoài sảnh để cập nhật giá mới nhất
    on_btnRefresh_clicked();
}

void MainWindow::on_btnCreateRoom_clicked()
{
    CreateRoomDialog dlg(this);

    if (dlg.exec() == QDialog::Accepted) {

        QString productName = dlg.productName();
        QString productStartingPrice = dlg.productStartingPrice();
        QString buyNow = dlg.productBuyNowPrice();
        QString duration = dlg.auctionDuration();

        // Validate tối thiểu
        if (productName.isEmpty()) {
            QMessageBox::warning(this, "Lỗi", "Tên phòng không được trống");
            return;
        }

        // Build message theo protocol
        QString message = QString("CREATE_ROOM|%1|%2|%3|%4\n")
                              .arg(productName)
                              .arg(productStartingPrice)
                              .arg(buyNow)
                              .arg(duration);

        // Gửi qua socket
        m_socket->write(message.toUtf8());
    }
}

void MainWindow::on_btnJoin_clicked(){
    // Lấy item đang được chọn
    QListWidgetItem *current = ui->listRooms->currentItem();
    if (!current) {
        QMessageBox::warning(this, "Chú ý", "Vui lòng chọn một phòng để tham gia!");
        return;
    }

    // Lấy ID từ dữ liệu ẩn
    int roomId = current->data(Qt::UserRole).toInt();

    // Gửi lệnh JOIN
    m_socket->write(QString("JOIN_ROOM|%1\n").arg(roomId).toUtf8());
}

void MainWindow::on_btnBuyNow_clicked()
{
    // Hiển thị hộp thoại xác nhận cho chắc ăn
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Xác nhận", "Bạn có chắc muốn mua ngay sản phẩm này?",
                                  QMessageBox::Yes|QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // Gửi lệnh
        m_socket->write(QString("BUY_NOW|%1\n").arg(m_currentRoomId).toUtf8());
    }
}

void MainWindow::on_btnHistory_clicked() {
    m_socket->write("VIEW_HISTORY\n");
}




// --- XỬ LÝ MẠNG (CORE) ---

void MainWindow::onConnected()
{
    // Có thể hiện thông báo nhỏ ở thanh status bar
    statusBar()->showMessage("Đã kết nối Server");
}



void MainWindow::onReadyRead()
{
    while (m_socket->canReadLine()) {
        QString line = QString::fromUtf8(m_socket->readLine()).trimmed();
        qDebug() << "Server msg:" << line; // <-- Nhìn vào đây xem có thấy NEW_BID không?
        // --- PHÂN TÍCH GÓI TIN TỪ SERVER ---

        // 1. Phản hồi Đăng nhập
        if (line.startsWith("OK|LOGIN_SUCCESS")) {
            // Chuyển sang trang Lobby (Index 1)
            ui->stackedWidget->setCurrentIndex(1);

            // Tự động lấy danh sách phòng luôn
            on_btnRefresh_clicked();
        }
        else if (line.startsWith("ERR|WRONG_PASS")) {
            QMessageBox::warning(this, "Lỗi", "Sai mật khẩu!");
        }

        // 2. Phản hồi Danh sách phòng
        // Server gửi: OK|LIST|1:Laptop:1500;2:Phone:500;
        else if (line.startsWith("OK|LIST")) {
            ui->listRooms->clear();

            QString data = line.section('|', 2, 2); // Lấy phần sau OK|LIST|
            QStringList rooms = data.split(';', Qt::SkipEmptyParts);

            for (const QString &r : rooms) {
                QStringList parts = r.split(':'); // 1:Laptop:1500
                if (parts.size() >= 3) {
                    QString display = "Phòng " + parts[0] + ": " + parts[1] + " - Giá: " + parts[2];
                    QListWidgetItem *item = new QListWidgetItem(display);
                    // LƯU ID VÀO DỮ LIỆU ẨN CỦA ITEM
                    item->setData(Qt::UserRole, parts[0].toInt());

                    ui->listRooms->addItem(item);
                }
            }
        }
        else if(line.startsWith("OK|ROOM_CREATED")){
            //Server replies: OK|ROOM_CREATED|5 (5 is the new room's ID)
            QMessageBox::information(this, "Thành công", "Đã tạo phòng đấu giá mới");
            // Tự động refresh lại list để thấy phòng mình vừa tạo
            on_btnRefresh_clicked();
        }
        // 1. XỬ LÝ VÀO PHÒNG THÀNH CÔNG
        // Server: OK|JOINED|<id>|<name>|<price>
        else if (line.startsWith("OK|JOINED")) {
            QStringList parts = line.split('|');

            // Server gửi: OK|JOINED|ID|Name|CurrentPrice|BuyNowPrice
            // Tổng cộng là 6 phần (index 0 đến 5)
            if (parts.size() >= 6) {
                m_currentRoomId = parts[2].toInt();
                QString name = parts[3];
                QString price = parts[4];
                QString buyNow = parts[5]; // <-- Lấy giá mua ngay

                // Cập nhật UI
                ui->lblRoomName->setText("Đang đấu giá: " + name);
                ui->lblCurrentPrice->setText(price);

                // --- HIỂN THỊ GIÁ MUA NGAY ---
                ui->lblBuyNowPrice->setText("Giá mua ngay: " + buyNow);
                // -----------------------------

                ui->txtRoomLog->clear();
                ui->txtRoomLog->append("--- Bắt đầu phiên đấu giá ---");

                ui->stackedWidget->setCurrentIndex(2);
            }
        }

        // 2. XỬ LÝ KHI CÓ NGƯỜI RA GIÁ (BROADCAST)
        // Server: NEW_BID|<price>|<user_socket>
        else if (line.startsWith("NEW_BID")) {
            // In ra chuỗi thô để xem có ký tự lạ không
            qDebug() << "1. Raw string received:" << line;

            QStringList parts = line.split('|');
            qDebug() << "2. Split size:" << parts.size(); // Phải >= 3 mới đúng

            if (parts.size() >= 3) {
                QString newPrice = parts[1];
                QString userSocket = parts[2];

                qDebug() << "3. New Price extracted:" << newPrice;

                // --- CẬP NHẬT GIAO DIỆN ---

                // Cố tình update cứng để test xem Label có chết không
                // Nếu dòng này chạy mà giao diện không đổi -> Label bị lỗi kết nối UI
                ui->lblCurrentPrice->setText(newPrice);

                ui->txtRoomLog->append("User " + userSocket + " trả giá: " + newPrice);

                qDebug() << "4. UI Updated successfully!";
            } else {
                qDebug() << "ERROR: Split size wrong! Check protocol separator.";
            }
        }
        // 1. XỬ LÝ ĐỒNG HỒ ĐẾM NGƯỢC
        // Server: TIME_UPDATE|299
        else if (line.startsWith("TIME_UPDATE")) {
            int seconds = line.section('|', 1, 1).toInt();

            // Hiển thị lên LCD Number hoặc Label
            ui->lcdTimer->display(seconds);

            // Đổi màu nếu sắp hết giờ (< 30s)
            if (seconds < 30) {
                ui->lcdTimer->setStyleSheet("color: red;");
            } else {
                ui->lcdTimer->setStyleSheet("color: black;"); // Mặc định
            }
        }

        // 2. XỬ LÝ KẾT THÚC PHIÊN (SOLD)
        // Server: SOLD|<price>|<winner_socket>
        else if (line.startsWith("SOLD")) {
            QString price = line.section('|', 1, 1);
            QString winner = line.section('|', 2, 2);

            ui->lcdTimer->display(0);
            ui->txtRoomLog->append("--- KẾT THÚC ---");
            ui->txtRoomLog->append("Sản phẩm đã thuộc về User " + winner + " với giá " + price);

            QMessageBox::information(this, "Kết thúc", "Sản phẩm đã được bán!");

            // Disable các nút để không bấm được nữa
            ui->btnBid->setEnabled(false);
        }
        else if (line.startsWith("ERR|INVALID_PRICE_CONFIG")) {
            QMessageBox::critical(this, "Lỗi tạo phòng",
                                  "Server từ chối: Giá mua ngay phải lớn hơn giá khởi điểm.");
        }
        else if (line.startsWith("ERR|PRICE_TOO_LOW")) {
            QMessageBox::warning(this, "Đấu giá thất bại",
                                 "Giá bạn đặt không hợp lệ!\n\n"
                                 "Quy tắc: Bạn phải đặt giá cao hơn giá hiện tại ít nhất 10.000 VND.");

            // (Tùy chọn) Reset lại ô nhập liệu cho sạch
            ui->txtBidAmount->clear();
            ui->txtBidAmount->setFocus();
        }
        else if (line.startsWith("ERR|LOGIN_FAILED")) {
            QMessageBox::critical(this, "Đăng nhập thất bại",
                                  "Tài khoản hoặc mật khẩu không chính xác!\n"
                                  "Vui lòng kiểm tra lại hoặc Đăng ký mới.");
        }
        else if (line.startsWith("OK|HISTORY")) {
            QString data = line.section('|', 2, 2); // Lấy phần dữ liệu sau OK|HISTORY|
            QStringList items = data.split(';', Qt::SkipEmptyParts);

            QString displayMsg = "=== CÁC PHIÊN ĐẤU GIÁ ĐÃ KẾT THÚC ===\n\n";
            for (const QString& item : items) {
                QStringList details = item.split(':');
                if (details.size() >= 3) {
                    displayMsg += QString("Vật phẩm: %1\nGiá chốt: %2 VND\nNgười thắng: %3\n----------------\n")
                                      .arg(details[0], details[1], details[2]);
                }
            }

            if (items.isEmpty()) displayMsg += "(Chưa có dữ liệu)";

            QMessageBox::information(this, "Lịch sử đấu giá", displayMsg);
        }

        // 3. XỬ LÝ KHÔNG AI MUA (CLOSED)
        else if (line.startsWith("CLOSED")) {
            ui->lcdTimer->display(0);
            ui->txtRoomLog->append("--- KẾT THÚC ---");
            ui->txtRoomLog->append("Không có ai đấu giá. Phiên bị hủy.");
            ui->btnBid->setEnabled(false);
        }

}
}



void MainWindow::on_txtSearch_textChanged(const QString &arg1)
{
    // Duyệt qua tất cả các dòng trong List danh sách phòng
    for(int i = 0; i < ui->listRooms->count(); ++i)
    {
        QListWidgetItem* item = ui->listRooms->item(i);
        QString roomText = item->text();
        // roomText có dạng: "Phòng 1: iPhone 15 - Giá: 2000"

        // Kiểm tra xem text có chứa từ khóa không (Case Insensitive - Không phân biệt hoa thường)
        if(roomText.contains(arg1, Qt::CaseInsensitive)) {
            item->setHidden(false); // Hiện
        } else {
            item->setHidden(true);  // Ẩn
        }
    }
}

