#include "MainDlg.h"
#include <QTcpServer>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>

MainDlg::MainDlg(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	m_labelStatus = new QLabel(U16("δ����"));
	m_labelNatType = new QLabel(U16(" "));
	m_labelUpnp = new QLabel(U16(" "));

	m_labelStatus->setMinimumWidth(60);
	m_labelNatType->setMinimumWidth(60);
	m_labelUpnp->setMinimumWidth(60);

	ui.statusBar->addPermanentWidget(m_labelUpnp);
	ui.statusBar->addPermanentWidget(m_labelNatType);
	ui.statusBar->addPermanentWidget(m_labelStatus);

	ui.btnTunnel->setEnabled(false);

	m_model = new QStandardItemModel(ui.tableView);
	ui.tableView->setModel(m_model);
	m_model->setHorizontalHeaderLabels(U16("tunnelId,�Է��û���,�Է�IP��ַ,״̬,����").split(","));
	ui.tableView->setColumnHidden(0, true);
	ui.tableView->setColumnWidth(1, 80);
	ui.tableView->setColumnWidth(2, 80);
	ui.tableView->setColumnWidth(3, 80);
	ui.tableView->setColumnWidth(4, 80);

	connect(&m_client, SIGNAL(connected()), this, SLOT(connected()));
	connect(&m_client, SIGNAL(disconnected()), this, SLOT(disconnected()));
	connect(&m_client, SIGNAL(logined()), this, SLOT(logined()));
	connect(&m_client, SIGNAL(loginFailed(QString)), this, SLOT(loginFailed(QString)));
	connect(&m_client, SIGNAL(natTypeConfirmed(NatType)), this, SLOT(natTypeConfirmed(NatType)));
	connect(&m_client, SIGNAL(firewallWarning()), this, SLOT(firewallWarning()));

	connect(&m_upnpPortMapper, SIGNAL(discoverFinished(bool)), this, SLOT(onUpnpDiscoverFinished(bool)));
	connect(&m_upnpPortMapper, SIGNAL(queryExternalAddressFinished(QHostAddress,bool,QString)), this, SLOT(onUpnpQueryExternalAddressFinished(QHostAddress, bool, QString)));


	connect(ui.editLocalPassword, SIGNAL(textChanged(const QString &)), this, SLOT(onEditLocalPasswordChanged()));
	connect(ui.btnTunnel, SIGNAL(clicked()), this, SLOT(onBtnTunnel()));
	connect(&m_client, SIGNAL(replyTryTunneling(QString, bool, bool, QString)), this, SLOT(onReplyTryTunneling(QString, bool, bool, QString)));
	connect(&m_client, SIGNAL(replyReadyTunneling(int, int, QString)), this, SLOT(onReplyReadyTunneling(int, int, QString)));
	connect(&m_client, SIGNAL(tunnelStarted(int, QString, QHostAddress)), this, SLOT(onTunnelStarted(int, QString, QHostAddress)));
	connect(&m_client, SIGNAL(tunnelHandShaked(int)), this, SLOT(onTunnelHandShaked(int)));
	connect(&m_client, SIGNAL(tunnelData(int, QByteArray)), this, SLOT(onTunnelData(int, QByteArray)));
	connect(&m_client, SIGNAL(tunnelClosed(int)), this, SLOT(onTunnelClosed(int)));

	leadWindowsFirewallPopup();

	QSettings setting("NatTunnelClient.ini", QSettings::IniFormat);
	const QHostAddress serverAddress = QHostAddress(setting.value("Server/Address").toString());
	const int serverPort = setting.value("Server/Port").toInt();

	const QString userName = setting.value("Client/UserName").toString();
	const QString password = setting.value("Client/Password").toString();
	const QString localPassword = setting.value("Client/LocalPassword").toString();

	ui.editUserName->setText(userName);
	ui.editLocalPassword->setText(localPassword);

	m_client.setUserInfo(userName, password);
	m_client.setServerInfo(serverAddress, serverPort);
	m_client.start();
}

MainDlg::~MainDlg()
{
	m_client.stop();
}

void MainDlg::leadWindowsFirewallPopup()
{
#if defined(Q_OS_WIN)
	// ����һ��TcpServer������Windows����ǽ�ᵯ����ʾ�������û�������������
	// ����������������Udp�ղ���δ������ַ���������ݰ�
	static bool invoked = false;
	if (invoked)
		return;
	QTcpServer tcpServer;
	tcpServer.listen(QHostAddress::Any, 0);
	invoked = true;
#endif
}

void MainDlg::closeEvent(QCloseEvent *event)
{
	QSettings setting("NatTunnelClient.ini", QSettings::IniFormat);
	setting.setValue("Client/LocalPassword", ui.editLocalPassword->text());
}
void MainDlg::connected()
{
	m_labelStatus->setText(U16("���ڵ�¼"));

	const QHostAddress localAddress = m_client.getLocalAddress();
	if (isNatAddress(localAddress))
	{
		m_upnpPortMapper.open(m_client.getLocalAddress());
		m_upnpPortMapper.discover();
		m_labelUpnp->setText(U16("���ڼ��upnp֧��"));
	}
	else
	{
		m_labelUpnp->setText(U16("��ǰ���绷������upnp"));
	}
}

void MainDlg::disconnected()
{
	m_labelStatus->setText(U16("�Ͽ�"));
	m_labelNatType->clear();
	m_labelUpnp->clear();
	ui.btnTunnel->setEnabled(false);
}

void MainDlg::logined()
{
	m_labelStatus->setText(U16("��¼�ɹ�"));
	m_labelNatType->setText(U16("���ڼ��NAT����"));
}

void MainDlg::loginFailed(QString msg)
{
	m_labelStatus->setText(msg);
}

void MainDlg::natTypeConfirmed(NatType natType)
{
	m_labelNatType->setText(getNatDescription(natType));
	ui.btnTunnel->setEnabled(true);
}

void MainDlg::firewallWarning()
{
	ui.statusBar->showMessage(U16("��ǰ���ڹ������������Ƿ���ǽ���������˱�����"));
}

void MainDlg::onUpnpDiscoverFinished(bool ok)
{
	if (ok)
	{
		m_client.setUpnpAvailable(true);
		m_labelUpnp->setText(U16("upnp����"));
		m_upnpPortMapper.queryExternalAddress();
	}
	else
	{
		m_client.setUpnpAvailable(false);
		m_labelUpnp->setText(U16("upnp������"));
	}
}

void MainDlg::onUpnpQueryExternalAddressFinished(QHostAddress address, bool ok, QString errorString)
{
	if (ok)
	{
		if (!isSameHostAddress(address, m_client.getLocalPublicAddress()))
			ui.statusBar->showMessage(U16("�������˷��ص�IP��ַ %1 ��upnp���صĵ�ַ %2 ��ͬ").arg(m_client.getLocalPublicAddress().toString()).arg(address.toString()));
	}else
	{
		ui.statusBar->showMessage(U16("upnp��ȡ������ַʧ��"));
	}
}

void MainDlg::onEditLocalPasswordChanged()
{
	m_client.setLocalPassword(ui.editLocalPassword->text());
}

void MainDlg::onBtnTunnel()
{
	const QString peerUserName = ui.comboBoxPeerUserName->currentText();
	if (peerUserName.isEmpty())
		return;

	m_client.tryTunneling(peerUserName);
	ui.btnTunnel->setEnabled(false);
}

void MainDlg::onReplyTryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason)
{
	if (!canTunnel)
	{
		QMessageBox::warning(this, U16("����ʧ��"), failReason);
		ui.btnTunnel->setEnabled(true);
		return;
	}
	const QString peerLocalPassword = QInputDialog::getText(this, U16("����"), U16("���� %1 �ı�������").arg(peerUserName), QLineEdit::Password);
	if (peerLocalPassword.isNull())
	{
		ui.btnTunnel->setEnabled(true);
		return;
	}

	m_client.readyTunneling(peerUserName, peerLocalPassword, needUpnp);
}

void MainDlg::onReplyReadyTunneling(int requestId, int tunnelId, QString peerUserName)
{
	if (tunnelId == 0)
	{
		QMessageBox::warning(this, U16("����ʧ��"), U16("����ʧ��"));
		return;
	}

	ui.btnTunnel->setEnabled(true);
	updateTableRow(tunnelId, peerUserName, "", U16("׼����"));
}

void MainDlg::onTunnelStarted(int tunnelId, QString peerUserName, QHostAddress peerAddress)
{
	updateTableRow(tunnelId, peerUserName, peerAddress.toString(), U16("��ʼ����"));
}

void MainDlg::onTunnelHandShaked(int tunnelId)
{
	updateTableRow(tunnelId, QString(), QString(), U16("���ӳɹ�"));
}

void MainDlg::onTunnelData(int tunnelId, QByteArray package)
{

}

void MainDlg::onTunnelClosed(int tunnelId)
{
	deleteTableRow(tunnelId);
}

void MainDlg::updateTableRow(int tunnelId, QString peerUsername, QString peerAddress, QString status)
{
	const QString key = QString::number(tunnelId);
	QList<QStandardItem*> lstItem = m_model->findItems(key);
	if (lstItem.isEmpty())
	{
		lstItem << new QStandardItem(key) << new QStandardItem(peerUsername)
			<< new QStandardItem(peerAddress) << new QStandardItem(status) << new QStandardItem();
		m_model->appendRow(lstItem);
		QPushButton * btnCloseConnection = new QPushButton(U16("�Ͽ�"));
		btnCloseConnection->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
		ui.tableView->setIndexWidget(m_model->index(m_model->rowCount() - 1, m_model->columnCount() - 1), btnCloseConnection);
	}
	else
	{
		const int row = lstItem.at(0)->row();
		if(!peerUsername.isNull())
			m_model->item(row, 1)->setText(peerUsername);
		if(!peerAddress.isNull())
			m_model->item(row, 2)->setText(peerAddress);
		if(!status.isNull())
			m_model->item(row, 3)->setText(status);
	}
}

void MainDlg::deleteTableRow(int tunnelId)
{
	const QString key = QString::number(tunnelId);
	QList<QStandardItem*> lstItem = m_model->findItems(key);
	if (lstItem.isEmpty())
		return;
	m_model->removeRow(lstItem.at(0)->row());
}