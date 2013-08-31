#include <QNetworkProxy>
#include <QNetworkRequest>

#include "core/settings.h"
#include "core/defs.h"
#include "core/basenetworkaccessmanager.h"


BaseNetworkAccessManager::BaseNetworkAccessManager(QObject *parent)
  : QNetworkAccessManager(parent) {
  loadSettings();
}

BaseNetworkAccessManager::~BaseNetworkAccessManager() {
  qDebug("Destroying BaseNetworkAccessManager instance.");
}

void BaseNetworkAccessManager::loadSettings() {
  qDebug("Settings of BaseNetworkAccessManager changed.");

  QNetworkProxy new_proxy;

  // Load proxy values from settings.
  QNetworkProxy::ProxyType selected_proxy_type = static_cast<QNetworkProxy::ProxyType>(Settings::getInstance()->value(APP_CFG_PROXY,
                                                                                                                      "proxy_type",
                                                                                                                      QNetworkProxy::NoProxy).toInt());
  if (selected_proxy_type == QNetworkProxy::NoProxy) {
    // No extra setting is needed, set new proxy and exit this method.
    setProxy(QNetworkProxy::NoProxy);
    return;
  }
  Settings *settings = Settings::getInstance();

  // Custom proxy is selected, set it up.
  new_proxy.setType(selected_proxy_type);
  new_proxy.setHostName(settings->value(APP_CFG_PROXY,
                                        "host").toString());
  new_proxy.setPort(settings->value(APP_CFG_PROXY,
                                    "port", 80).toInt());
  new_proxy.setUser(settings->value(APP_CFG_PROXY,
                                    "username").toString());
  new_proxy.setPassword(settings->value(APP_CFG_PROXY,
                                        "password").toString());
  setProxy(new_proxy);
}

QNetworkReply *BaseNetworkAccessManager::createRequest(QNetworkAccessManager::Operation op,
                                                       const QNetworkRequest &request,
                                                       QIODevice *outgoingData) {
  QNetworkRequest new_request = request;

  // This rapidly speeds up loading of web sites.
  // NOTE: https://en.wikipedia.org/wiki/HTTP_pipelining
  new_request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

  // Setup custom user-agent.
  new_request.setRawHeader("User-Agent", QString(APP_USERAGENT).toLocal8Bit());

  return QNetworkAccessManager::createRequest(op, new_request, outgoingData);
}