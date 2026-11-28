#include "../src/project/projectcontroller.h"
#include "../src/ai/aisettingscontroller.h"
#include "../src/ai/aigenerationcontroller.h"
#include "../src/platform/windowscredentialstore.h"

#include <QCoreApplication>
#include <QNetworkProxy>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

#include <memory>

class ProjectUiTestSetup final : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE void applicationAvailable()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy(
            QNetworkProxy::HttpProxy,
            QStringLiteral("offline-test.sceneforge.invalid"),
            1));
        QCoreApplication::setOrganizationName(QStringLiteral("SceneForgeTests"));
        QCoreApplication::setApplicationName(QStringLiteral("ProjectUiTests"));
        QQuickStyle::setStyle(QStringLiteral("Basic"));

        m_libraryDirectory = std::make_unique<QTemporaryDir>();
        if (!m_libraryDirectory->isValid()) {
            qFatal("无法创建界面测试资料库目录。");
        }

        m_controller = std::make_unique<sceneforge::project::ProjectController>();
        m_credentialStore = std::make_unique<sceneforge::platform::WindowsCredentialStore>();
        m_aiSettingsController = std::make_unique<sceneforge::ai::AiSettingsController>(
            m_credentialStore.get());
        m_aiGenerationController = std::make_unique<sceneforge::ai::AiGenerationController>(
            m_controller.get(), m_aiSettingsController.get(), m_credentialStore.get());
        QString errorMessage;
        if (!m_controller->setLibraryRoot(m_libraryDirectory->path())) {
            errorMessage = m_controller->errorMessage();
            qFatal("无法初始化界面测试资料库：%s", qPrintable(errorMessage));
        }
    }

    Q_INVOKABLE void qmlEngineAvailable(QQmlEngine* engine)
    {
        engine->rootContext()->setContextProperty("projectController", m_controller.get());
        engine->rootContext()->setContextProperty("aiSettingsController",
                                                  m_aiSettingsController.get());
        engine->rootContext()->setContextProperty("aiGenerationController",
                                                  m_aiGenerationController.get());
    }

private:
    std::unique_ptr<QTemporaryDir> m_libraryDirectory;
    std::unique_ptr<sceneforge::project::ProjectController> m_controller;
    std::unique_ptr<sceneforge::platform::WindowsCredentialStore> m_credentialStore;
    std::unique_ptr<sceneforge::ai::AiSettingsController> m_aiSettingsController;
    std::unique_ptr<sceneforge::ai::AiGenerationController> m_aiGenerationController;
};

QUICK_TEST_MAIN_WITH_SETUP(SceneForgeProjectUi, ProjectUiTestSetup)

#include "projectuitests.moc"
