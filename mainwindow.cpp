#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "MessagePackage.h"
#include <QMessageBox>
#include <QJsonDocument>
#include <QDataStream>
#include <QJsonObject>
#include <QJsonArray>

#include "ChatMessageBox.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    socket=new QTcpSocket();
    soundEffect=new QSoundEffect(this);
    connect(socket,&QTcpSocket::readyRead,this,&MainWindow::ReadData);
    this->ui->chat_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->ui->chat_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->ui->textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->ui->textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(ui->modules_list,&QListWidget::currentRowChanged,ui->stackedWidget,&QStackedWidget::setCurrentIndex);
    connect(ui->friends_list,&QListWidget::currentRowChanged,this,&MainWindow::SelectedFriendTarget);
    connect(ui->but_send,&QPushButton::clicked,this,&MainWindow::OnButSend);
}

bool MainWindow::login(QString user, QString pwd)
{
    socket->disconnected();
    socket->connectToHost("127.0.0.1",2344);
    if(!socket->waitForConnected(2000))
    {
        QMessageBox::information(nullptr,"提示","无法连接服务器请检查网络设置");
        return false;
    }
    WriteToServer(MessagePackage::LoginPackage(user,pwd));
    if(socket->waitForBytesWritten(5000))
    {
        if(socket->waitForReadyRead(5000))
        {
            if(result_login)
                this->user=user;
            else QMessageBox::information(this,"提示","账号不存在或密码错误");
            return result_login;
        }
    }
    return true;
}

void MainWindow::ReadData()
{
    data_buffer.append(socket->readAll());
    int rt_len=1;
    while(rt_len>0)
    {
        rt_len= HandleReveData(data_buffer);
        if(rt_len>0)
            data_buffer.remove(0,rt_len+4);
    }
}

int MainWindow::HandleReveData(QByteArray &buffer)
{
    QByteArray bd;
//    raw=socket->readAll();
    QDataStream stream(&buffer,QIODevice::ReadOnly);
    int data_length;
    if(buffer.size()<4)
        return 0;
    stream>>bd;
    data_length=bd.size();
    if(!bd.isEmpty())
    {
        //ui->textEdit->setText(ui->textEdit->toPlainText()+tr(buffer));
        QJsonDocument doc=QJsonDocument::fromJson(bd);
        QJsonObject obj=doc.object();
        QString head=obj["head"].toString();
        QJsonArray tags=obj["tags"].toArray();
        //QMessageBox::information(nullptr,"Message",head);
        if(head=="login_response")
        {
            if(tags.isEmpty())
                return data_length;
            QString result=tags[0].toString();
            if(result=="finished")
            {
                result_login = true;
                //WriteToServer(MessagePackage::GetInfoPackage());
            }else result_login= false;
        }else if(head=="userinfo")
        {
            //QMessageBox::information(nullptr,"User Info","User Info");
            QJsonArray array=obj["data"].toArray();
            //QMessageBox::information(nullptr,"Count",QString::number(array.count()));
            auto a=obj["data"].toArray().first();
            QJsonDocument doc_data=QJsonDocument::fromJson(a.toString().toUtf8());

            QJsonObject info=doc_data.object();
            if(!info.isEmpty())
            {
                QJsonArray arr=info["friends"].toArray();
                for(auto i : arr)
                {
                    this->frineds.push_back(i.toString());
                    this->ui->friends_list->addItem(i.toString());
                }
            }
        }else if(head=="schat")
        {
//            QMessageBox::information(nullptr,"Msg",buffer);
            QJsonArray data=obj["data"].toArray();
            if(tags[0].toString()=="private")
            {
                QString target=tags[1].toString();
                history[target].push_back(Message::Create(target,false,data[0].toString()));
                if(now_target==target)
                {
                    ChatMessageBox::Create(QPixmap("://images/icon_user.png"),false,data[0].toString(),this->ui->chat_list);
                    this->ui->chat_list->scrollToBottom();
                }
                soundEffect->setSource(QUrl::fromLocalFile("://sound/message_tips.wav"));
                soundEffect->play();
            }
        }
        return data_length;
    }
    return -1;
}

void MainWindow::SelectedFriendTarget(int row)
{
    if(now_target==frineds[row])
        return;
    this->now_target=frineds[row];
    this->ui->label_target->setText(now_target);
    this->ui->chat_list->clear();
    std::vector<Message*> his_target=history[now_target];
    for(Message *i : his_target)
    {
        ChatMessageBox::Create(QPixmap("://images/icon_user.png"),i->type,i->text,this->ui->chat_list);
    }
    this->ui->chat_list->scrollToBottom();
}

void MainWindow::OnButSend()
{
    QString text=this->ui->textEdit->toPlainText();
    ChatMessageBox::Create(QPixmap("://images/icon_user.png"), true,text,this->ui->chat_list);
    this->ui->textEdit->clear();
    Message *msg=Message::Create(now_target,true,text);
    history[now_target].push_back(msg);
    this->ui->chat_list->scrollToBottom();
   // QMessageBox::information(nullptr,"Msg",MessagePackage::ChatPackage(msg,now_target));
    QByteArray bf=MessagePackage::ChatPackage(msg,now_target);
    WriteToServer(bf);
}

void MainWindow::WriteToServer(QByteArray bf)
{
    QByteArray data;
    QDataStream stream(&data,QIODevice::WriteOnly);
    stream<<bf;
    socket->write(data);
}

MainWindow::~MainWindow()
{
    delete ui;
}

