#include "node.h"
#include <QDebug>

Node::Node(const QString &id, quint16 port, QObject *parent)
    : QObject(parent)
    , m_nodeId(id)
    , m_address(QHostAddress::LocalHost)
    , m_port(port)
    , m_status(Status::Offline)
    , m_tcpAddress(QHostAddress::LocalHost)
    , m_isShuttingDown(false)
{
    qDebug() << "Created node with ID:" << m_nodeId << "on port" << m_port;
}

Node::~Node()
{
    qDebug() << "Destroying node:" << m_nodeId;
}

bool Node::initialize()
{
    m_status = Status::Online;

    if (m_tcpAddress.isNull()) {
        m_tcpAddress = m_address;
    }

    qDebug() << "Node" << m_nodeId << "initialized";
    return true;
}

void Node::shutdown()
{
    qDebug() << "Node" << "\"" + m_nodeId + "\"" << "is shutting down...";

    if (!m_isShuttingDown) {
        prepareShutdown();
    }

    m_status = Status::Offline;
    QCoreApplication::processEvents();
}

bool Node::prepareShutdown()
{
    qDebug() << "Node" << "\"" + m_nodeId + "\"" << "preparing for shutdown...";
    m_isShuttingDown = true;
    return true;
}
