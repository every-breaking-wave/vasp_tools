#include <QApplication>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include "RemoteClient.h"

#define VASP_SERVER_IP  "10.0.16.21"
#define VASP_SERVER_PORT  12345

int main(int argc, char *argv[]) {
    try {
        QApplication app(argc, argv);

        boost::asio::io_context io_context;

        RemoteClient client(io_context, VASP_SERVER_IP, VASP_SERVER_PORT);

        QMessageBox msgBox;
        msgBox.setText("choose your action");
        msgBox.setInformativeText("File or Command?");
        
        QPushButton *fileButton = msgBox.addButton("send folder", QMessageBox::ActionRole);
        QPushButton *cmdButton = msgBox.addButton("do compute", QMessageBox::ActionRole);
        msgBox.addButton(QMessageBox::Cancel);

        msgBox.exec();

        if (msgBox.clickedButton() == (QAbstractButton *)fileButton) {
            QString fileName = QFileDialog::getOpenFileName(nullptr, "Open File", QDir::homePath());
            if (!fileName.isEmpty()) {
                client.send_file(fileName.toStdString());
            }
            
        } else if (msgBox.clickedButton() == (QAbstractButton *)cmdButton) {
            bool ok;
            QString command = QInputDialog::getText(nullptr, "do compute", "input command:",
                                                    QLineEdit::Normal, QString(), &ok);
            if (ok && !command.isEmpty()) {
                client.send_command(command.toStdString());
            }
        }

        io_context.run();

    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}