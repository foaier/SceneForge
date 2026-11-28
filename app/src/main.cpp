#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QDebug>

#include "ai/aisettingscontroller.h"
#include "ai/aigenerationcontroller.h"
#include "platform/windowscredentialstore.h"
#include "project/projectcontroller.h"

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SceneForge"));
    QCoreApplication::setApplicationName(QStringLiteral("SceneForge"));
    QQmlApplicationEngine engine;
    sceneforge::project::ProjectController projectController;
    sceneforge::platform::WindowsCredentialStore credentialStore;
    sceneforge::ai::AiSettingsController aiSettingsController(&credentialStore);
    sceneforge::ai::AiGenerationController aiGenerationController(
        &projectController, &aiSettingsController, &credentialStore);

    engine.rootContext()->setContextProperty("projectController", &projectController);
    engine.rootContext()->setContextProperty("aiSettingsController", &aiSettingsController);
    engine.rootContext()->setContextProperty("aiGenerationController",
                                             &aiGenerationController);
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &application,
                     [](const QList<QQmlError>& warnings) {
                         for (const QQmlError& warning : warnings) {
                             qCritical().noquote() << warning.toString();
                         }
                     });

    engine.loadFromModule("SceneForge", "Main");

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Unable to load the main application window.";
        return -1;
    }

    return application.exec();
}
