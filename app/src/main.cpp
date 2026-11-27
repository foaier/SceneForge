#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QQmlApplicationEngine engine;

    engine.loadFromModule("SceneForge", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return application.exec();
}
