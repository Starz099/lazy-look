#include "config.hh"
#include "search_controller.hh"

#include <QApplication>
#include <QCommandLineParser>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QMenu>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSystemTrayIcon>
#include <iostream>
#include <unistd.h>

int main(int argc, char *argv[])
{
  // Filter benign Qt platform abstraction warnings on Wayland/Portal
  QLoggingCategory::setFilterRules("qt.qpa.services.warning=false\nqt.core.qobject.connect=false");

  QApplication app(argc, argv);
  app.setOrganizationName("LazyFinder");
  app.setApplicationName("LazyFinder");
  app.setApplicationVersion("0.1.0");
  app.setDesktopFileName("lazyfinder");
  app.setWindowIcon(QIcon::fromTheme("lazyfinder", QIcon::fromTheme("system-search")));
  app.setQuitOnLastWindowClosed(false);

  QCommandLineParser parser;
  parser.setApplicationDescription("LazyFinder - Instant AI Desktop Search");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption toggleOption("toggle", "Toggle LazyFinder window visibility.");
  QCommandLineOption showOption("show", "Show and focus LazyFinder window.");
  QCommandLineOption hideOption("hide", "Hide LazyFinder window.");
  QCommandLineOption quitOption("quit", "Quit the running LazyFinder daemon.");
  QCommandLineOption daemonOption("daemon", "Start in background daemon mode.");
  QCommandLineOption enableAutostartOption("enable-autostart",
                                           "Enable LazyFinder autostart on system login.");
  QCommandLineOption disableAutostartOption("disable-autostart",
                                            "Disable LazyFinder autostart on system login.");

  parser.addOption(toggleOption);
  parser.addOption(showOption);
  parser.addOption(hideOption);
  parser.addOption(quitOption);
  parser.addOption(daemonOption);
  parser.addOption(enableAutostartOption);
  parser.addOption(disableAutostartOption);

  parser.process(app);

  if (parser.isSet(enableAutostartOption)) {
    lazyfinder::Config::instance().load();
    lazyfinder::Config::instance().set_launch_on_startup(true);
    std::cout << "LazyFinder autostart on login enabled (~/.config/autostart/lazyfinder.desktop)."
              << std::endl;
    return 0;
  }
  if (parser.isSet(disableAutostartOption)) {
    lazyfinder::Config::instance().load();
    lazyfinder::Config::instance().set_launch_on_startup(false);
    std::cout << "LazyFinder autostart on login disabled." << std::endl;
    return 0;
  }

  const QString serverName = "lazyfinder_ipc_" + QString::number(getuid());

  // Check if another instance is already running
  {
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(250)) {
      QString cmd = "TOGGLE";
      if (parser.isSet(showOption)) {
        cmd = "SHOW";
      }
      else if (parser.isSet(hideOption)) {
        cmd = "HIDE";
      }
      else if (parser.isSet(quitOption)) {
        cmd = "QUIT";
      }
      const char *token = std::getenv("XDG_ACTIVATION_TOKEN");
      if (!token) {
        token = std::getenv("DESKTOP_STARTUP_ID");
      }
      if (token && *token) {
        cmd += " ";
        cmd += QString::fromUtf8(token);
      }
      cmd += "\n";
      socket.write(cmd.toUtf8());
      socket.flush();
      socket.waitForBytesWritten(250);
      return 0;
    }
  }

  // If the app is NOT running, do NOT launch on --toggle, --hide, or --quit!
  // The global hotkey only works when the app has been started by user or autostart.
  if (parser.isSet(toggleOption) || parser.isSet(hideOption) || parser.isSet(quitOption)) {
    return 0;
  }

  // Register GNOME global shortcut now that LazyFinder is running
  lazyfinder::Config::instance().load();
  lazyfinder::Config::register_gnome_shortcut(lazyfinder::Config::instance().global_shortcut());

  // Automatically unregister GNOME shortcut when app exits
  QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
    lazyfinder::Config::unregister_gnome_shortcut();
  });

  // Remove any stale socket file from a prior crash
  QLocalServer::removeServer(serverName);

  auto server = new QLocalServer(&app);
  if (!server->listen(serverName)) {
    qWarning("Failed to start IPC server: %s", qPrintable(server->errorString()));
  }

  QQuickStyle::setStyle("Basic");

  lazyfinder::SearchController controller;

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("searchController", &controller);

  QObject::connect(
      &engine,
      &QQmlApplicationEngine::objectCreationFailed,
      &app,
      []() { QCoreApplication::exit(-1); },
      Qt::QueuedConnection);

  engine.loadFromModule("LazyFinder", "Main");

  QQuickWindow *window = nullptr;
  const auto rootObjects = engine.rootObjects();
  if (!rootObjects.isEmpty()) {
    window = qobject_cast<QQuickWindow *>(rootObjects.first());
  }

  if (window) {
    if (parser.isSet(daemonOption)) {
      window->setVisible(false);
    }
    else {
      window->setVisible(true);
      window->raise();
      window->requestActivate();
    }
  }

  // Set up System Tray presence
  QSystemTrayIcon trayIcon;
  QIcon searchIcon = QIcon::fromTheme("lazyfinder", QIcon::fromTheme("system-search"));
  if (searchIcon.isNull()) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#3584e4"), 2));
    p.drawEllipse(4, 4, 11, 11);
    p.drawLine(13, 13, 19, 19);
    searchIcon = QIcon(pixmap);
  }
  trayIcon.setIcon(searchIcon);
  trayIcon.setToolTip("LazyFinder (Ctrl+Shift+Space)");

  QMenu trayMenu;
  auto openAction = trayMenu.addAction("Open LazyFinder");
  auto settingsAction = trayMenu.addAction("Preferences...");
  trayMenu.addSeparator();
  auto quitAction = trayMenu.addAction("Quit LazyFinder");

  trayIcon.setContextMenu(&trayMenu);
  trayIcon.show();

  QObject::connect(openAction, &QAction::triggered, [window]() {
    if (window) {
      window->setVisible(true);
      window->raise();
      window->requestActivate();
    }
  });

  QObject::connect(settingsAction, &QAction::triggered, [window]() {
    if (window) {
      window->setProperty("settingsOpen", true);
      window->setVisible(true);
      window->raise();
      window->requestActivate();
    }
  });

  QObject::connect(quitAction, &QAction::triggered, [&app]() {
    lazyfinder::Config::unregister_gnome_shortcut();
    app.quit();
  });

  QObject::connect(
      &trayIcon, &QSystemTrayIcon::activated, [window](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
          if (window) {
            if (window->isVisible() && window->isActive()) {
              window->setVisible(false);
            }
            else {
              window->setVisible(true);
              window->raise();
              window->requestActivate();
            }
          }
        }
      });

  // Handle IPC commands
  QObject::connect(server, &QLocalServer::newConnection, [server, window, &app]() {
    while (server->hasPendingConnections()) {
      QLocalSocket *client = server->nextPendingConnection();
      QObject::connect(client, &QLocalSocket::readyRead, [client, window, &app]() {
        const QByteArray data = client->readAll();
        const QString fullMsg = QString::fromUtf8(data).trimmed();
        const QStringList parts = fullMsg.split(' ', Qt::SkipEmptyParts);
        const QString cmd = parts.value(0);
        const QString token = parts.value(1);
        if (!token.isEmpty()) {
          qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
        }

        if (cmd == "TOGGLE") {
          if (window) {
            if (window->isVisible() && window->isActive()) {
              window->setVisible(false);
            }
            else {
              window->setVisible(true);
              window->raise();
              window->requestActivate();
            }
          }
        }
        else if (cmd == "SHOW") {
          if (window) {
            window->setVisible(true);
            window->raise();
            window->requestActivate();
          }
        }
        else if (cmd == "HIDE") {
          if (window) {
            window->setVisible(false);
          }
        }
        else if (cmd == "QUIT") {
          lazyfinder::Config::unregister_gnome_shortcut();
          app.quit();
        }
        client->disconnectFromServer();
      });
      QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
    }
  });

  return app.exec();
}
