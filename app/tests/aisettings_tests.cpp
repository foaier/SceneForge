#include "../src/ai/aisettingscontroller.h"
#include "../src/ai/aiserviceprofile.h"
#include "../src/platform/windowscredentialstore.h"

#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QUuid>

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const QString& message)
{
    if (!condition) {
        throw std::runtime_error(message.toStdString());
    }
}

class MemoryCredentialStore final : public sceneforge::ai::AiCredentialStore {
public:
    bool store(const QString& profileId, const QString& secret, QString*) override
    {
        m_values.insert(profileId, secret);
        return true;
    }

    bool retrieve(const QString& profileId, QString* secret, QString*) const override
    {
        if (!secret || !m_values.contains(profileId)) {
            return false;
        }
        *secret = m_values.value(profileId);
        return true;
    }

    bool remove(const QString& profileId, QString* errorMessage) override
    {
        if (failRemoval) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("测试凭据库拒绝删除。");
            }
            return false;
        }
        m_values.remove(profileId);
        return true;
    }

    bool contains(const QString& profileId) const override
    {
        return m_values.contains(profileId);
    }

    QString value(const QString& profileId) const
    {
        return m_values.value(profileId);
    }

    bool failRemoval = false;

private:
    QMap<QString, QString> m_values;
};

QVariantMap serviceValues(const QString& name,
                         const QString& capability,
                         const QString& endpoint,
                         const QString& model)
{
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("capability"), capability},
        {QStringLiteral("protocol"), QStringLiteral("openai_compatible")},
        {QStringLiteral("endpoint"), endpoint},
        {QStringLiteral("model"), model},
        {QStringLiteral("dataHandlingNote"), QStringLiteral("测试服务的数据处理说明。")}
    };
}

void verifyProfileValidation()
{
    sceneforge::ai::AiServiceProfile profile =
        sceneforge::ai::AiServiceProfile::fromVariantMap(serviceValues(
            QStringLiteral("本地文本服务"), QStringLiteral("text"),
            QStringLiteral("http://localhost:11434/v1"), QStringLiteral("test-model")));
    QString error;
    require(profile.isValid(&error), error);
    require(profile.usesLoopbackEndpoint(),
            QStringLiteral("本机回环地址没有识别为本地服务。"));
    profile.id = QStringLiteral("invalid-id");
    require(!profile.isValid(&error),
            QStringLiteral("无效服务配置身份没有被拒绝。"));
    profile.id.clear();

    profile.endpoint = QStringLiteral("http://192.168.1.10:11434/v1");
    require(!profile.isValid(&error)
                && error.contains(QStringLiteral("HTTPS")),
            QStringLiteral("非本机 HTTP 服务没有被拒绝。"));

    profile.endpoint = QStringLiteral("https://provider.example/v1?api_key=secret");
    require(!profile.isValid(&error)
                && error.contains(QStringLiteral("查询参数")),
            QStringLiteral("服务地址中的查询参数没有被拒绝。"));
}

void verifySettingsAndCredentialSeparation()
{
    QTemporaryDir settingsDirectory;
    require(settingsDirectory.isValid(), QStringLiteral("无法创建 AI 设置测试目录。"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDirectory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
                       settingsDirectory.path());

    MemoryCredentialStore credentials;
    QString textServiceId;
    QString imageServiceId;
    {
        sceneforge::ai::AiSettingsController controller(&credentials);
        textServiceId = controller.upsertProfile(serviceValues(
            QStringLiteral("本地文本"), QStringLiteral("text"),
            QStringLiteral("http://127.0.0.1:11434/v1"), QStringLiteral("story-model")));
        imageServiceId = controller.upsertProfile(serviceValues(
            QStringLiteral("云端图片"), QStringLiteral("image"),
            QStringLiteral("https://images.example/v1"), QStringLiteral("image-model")));
        require(!textServiceId.isEmpty() && !imageServiceId.isEmpty(),
                controller.errorMessage());
        require(controller.setDefaultService(QStringLiteral("text"), textServiceId),
                controller.errorMessage());
        require(controller.setDefaultService(QStringLiteral("image"), imageServiceId),
                controller.errorMessage());
        require(!controller.setDefaultService(QStringLiteral("text"), imageServiceId),
                QStringLiteral("错误能力的服务被设为默认。"));

        const QString secret = QStringLiteral("test-secret-never-in-settings");
        require(controller.saveCredential(imageServiceId, secret), controller.errorMessage());
        require(credentials.value(imageServiceId) == secret,
                QStringLiteral("凭据没有写入凭据库接口。"));
        QString recoveredSecret;
        require(credentials.retrieve(imageServiceId, &recoveredSecret, nullptr)
                    && recoveredSecret == secret,
                QStringLiteral("凭据没有通过凭据库接口恢复。"));
        require(controller.hasCredential(imageServiceId),
                QStringLiteral("服务配置没有反映凭据是否已保存。"));
    }

    QSettings settings;
    settings.sync();
    QFile settingsFile(settings.fileName());
    require(settingsFile.open(QIODevice::ReadOnly),
            QStringLiteral("无法读取测试设置文件。"));
    require(!settingsFile.readAll().contains("test-secret-never-in-settings"),
            QStringLiteral("API 凭据意外写入普通设置文件。"));
    settingsFile.close();

    {
        sceneforge::ai::AiSettingsController reloaded(&credentials);
        require(reloaded.profiles().size() == 2,
                QStringLiteral("服务配置重启后没有恢复。"));
        require(reloaded.defaultTextServiceId() == textServiceId
                    && reloaded.defaultImageServiceId() == imageServiceId,
                QStringLiteral("按能力设置的默认服务没有持久化。"));
        require(reloaded.profiles().at(0).toMap().value(QStringLiteral("isDefault")).toBool()
                    && reloaded.profiles().at(1).toMap()
                           .value(QStringLiteral("isDefault")).toBool(),
                QStringLiteral("服务列表没有标记默认路由。"));
        require(reloaded.profiles().at(0).toMap()
                    .value(QStringLiteral("networkScope")).toString()
                    == QStringLiteral("local"),
                QStringLiteral("本地服务范围没有显示。"));
        require(reloaded.profiles().at(1).toMap()
                    .value(QStringLiteral("networkScope")).toString()
                    == QStringLiteral("external"),
                QStringLiteral("外部服务范围没有显示。"));
        require(reloaded.isServiceAuthorized(textServiceId)
                    && !reloaded.isServiceAuthorized(imageServiceId),
                QStringLiteral("本地/外部服务的初始授权状态错误。"));
        require(reloaded.authorizeExternalService(imageServiceId),
                reloaded.errorMessage());
        require(reloaded.isServiceAuthorized(imageServiceId),
                QStringLiteral("外部服务授权没有保存。"));

        QVariantMap updatedImageProfile = reloaded.profiles().at(1).toMap();
        updatedImageProfile.insert(QStringLiteral("endpoint"),
                                   QStringLiteral("https://images-alt.example/v1"));
        updatedImageProfile.insert(QStringLiteral("dataHandlingNote"),
                                   QStringLiteral("更新后的服务数据说明。"));
        require(reloaded.upsertProfile(updatedImageProfile) == imageServiceId,
                reloaded.errorMessage());
        require(!reloaded.isServiceAuthorized(imageServiceId),
                QStringLiteral("服务地址或处理说明变更后旧授权仍有效。"));
        require(reloaded.authorizeExternalService(imageServiceId),
                reloaded.errorMessage());
        require(reloaded.revokeServiceAuthorization(imageServiceId),
                reloaded.errorMessage());
        require(!reloaded.isServiceAuthorized(imageServiceId),
                QStringLiteral("撤销服务授权后状态仍为已授权。"));

        credentials.failRemoval = true;
        require(!reloaded.removeProfile(imageServiceId)
                    && reloaded.defaultImageServiceId() == imageServiceId
                    && reloaded.profiles().size() == 2,
                QStringLiteral("删除凭据失败后，服务配置未能保留。"));
        credentials.failRemoval = false;
        require(reloaded.removeProfile(imageServiceId), reloaded.errorMessage());
        require(!credentials.contains(imageServiceId)
                    && reloaded.defaultImageServiceId().isEmpty(),
                QStringLiteral("删除服务后凭据或默认路由仍被保留。"));
    }
}

void verifyWindowsCredentialStore()
{
#if defined(Q_OS_WIN)
    sceneforge::platform::WindowsCredentialStore credentials;
    const QString profileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString expectedSecret = QStringLiteral("synthetic-credential-store-test-value");
    QString error;
    require(credentials.store(profileId, expectedSecret, &error), error);

    QString recoveredSecret;
    if (!credentials.retrieve(profileId, &recoveredSecret, &error)) {
        credentials.remove(profileId, nullptr);
        require(false, error);
    }
    if (recoveredSecret != expectedSecret) {
        credentials.remove(profileId, nullptr);
        require(false, QStringLiteral("Windows 凭据管理器返回的测试凭据不匹配。"));
    }
    require(credentials.remove(profileId, &error), error);
    require(!credentials.contains(profileId),
            QStringLiteral("测试凭据删除后仍存在于 Windows 凭据管理器。"));
#endif
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SceneForgeAiTests"));
    QCoreApplication::setApplicationName(QStringLiteral("AiSettingsTests"));
    try {
        verifyProfileValidation();
        verifySettingsAndCredentialSeparation();
        verifyWindowsCredentialStore();
        std::cout << "PASS: AI service profiles, routing, locality, and credential separation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
