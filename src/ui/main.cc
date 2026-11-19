#include "search_controller.hh"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  app.setOrganizationName("LazyFinder");
  app.setApplicationName("LazyFinder");

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

  return app.exec();
}
