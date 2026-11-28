#include "../src/ai/aitextserviceadapter.h"
#include "../src/ai/aisettingscontroller.h"
#include "../src/ai/aigenerationcontroller.h"
#include "../src/project/projectcontroller.h"
#include "../src/project/aibibleupdatecandidateapplier.h"
#include "../src/project/scriptcandidatecodec.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
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

QString bibleFingerprintFromMap(const QVariantMap& values)
{
    sceneforge::project::BibleItemDraft item;
    item.id = values.value(QStringLiteral("id")).toString();
    item.itemType = values.value(QStringLiteral("itemType")).toString();
    item.name = values.value(QStringLiteral("name")).toString();
    item.description = values.value(QStringLiteral("description")).toString();
    item.contentStatus = values.value(QStringLiteral("contentStatus")).toString();
    item.certaintyStatus = values.value(QStringLiteral("certaintyStatus")).toString();
    item.details = values.value(QStringLiteral("details")).toMap();
    return sceneforge::project::AiBibleUpdateCandidateApplier::fingerprint(item);
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
        const auto found = m_values.constFind(profileId);
        if (found == m_values.cend()) {
            return false;
        }
        *secret = *found;
        return true;
    }
    bool remove(const QString& profileId, QString*) override
    {
        return m_values.remove(profileId) > 0;
    }
    bool contains(const QString& profileId) const override
    {
        return m_values.contains(profileId);
    }

private:
    QHash<QString, QString> m_values;
};

class MockHttpServer final : public QObject {
    Q_OBJECT

public:
    MockHttpServer(int statusCode, QByteArray responseBody, bool holdResponse = false)
        : m_statusCode(statusCode),
          m_responseBody(std::move(responseBody)),
          m_holdResponse(holdResponse)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            m_socket = m_server.nextPendingConnection();
            connect(m_socket, &QTcpSocket::readyRead, this, [this] {
                m_request.append(m_socket->readAll());
                captureCompleteRequest();
            });
        });
        require(m_server.listen(QHostAddress::LocalHost, 0),
                QStringLiteral("无法启动本机模拟 HTTP 服务。"));
    }

    QUrl endpoint() const
    {
        QUrl url;
        url.setScheme(QStringLiteral("http"));
        url.setHost(QStringLiteral("127.0.0.1"));
        url.setPort(m_server.serverPort());
        url.setPath(QStringLiteral("/v1"));
        return url;
    }

    QByteArray request() const
    {
        return m_request;
    }

signals:
    void requestCaptured();

private:
    void captureCompleteRequest()
    {
        const qsizetype headerEnd = m_request.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }

        const QByteArray headers = m_request.left(headerEnd);
        const QRegularExpression contentLengthPattern(
            QStringLiteral("(?:^|\\r\\n)Content-Length:\\s*(\\d+)"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = contentLengthPattern.match(
            QString::fromLatin1(headers));
        if (!match.hasMatch()) {
            return;
        }
        const qsizetype expectedLength = match.captured(1).toLongLong();
        if (m_request.size() < headerEnd + 4 + expectedLength || m_requestCaptured) {
            return;
        }

        m_requestCaptured = true;
        emit requestCaptured();
        if (!m_holdResponse) {
            sendResponse();
        }
    }

    void sendResponse()
    {
        if (!m_socket || m_responseSent) {
            return;
        }
        m_responseSent = true;
        const QByteArray reason = m_statusCode == 200 ? QByteArrayLiteral("OK")
                                                       : QByteArrayLiteral("Error");
        const QByteArray response = QByteArrayLiteral("HTTP/1.1 ")
                                    + QByteArray::number(m_statusCode) + ' ' + reason
                                    + QByteArrayLiteral("\r\nContent-Type: application/json\r\n")
                                    + QByteArrayLiteral("Content-Length: ")
                                    + QByteArray::number(m_responseBody.size())
                                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n")
                                    + m_responseBody;
        m_socket->write(response);
        m_socket->flush();
    }

    QTcpServer m_server;
    QTcpSocket* m_socket = nullptr;
    QByteArray m_request;
    QByteArray m_responseBody;
    int m_statusCode = 200;
    bool m_holdResponse = false;
    bool m_requestCaptured = false;
    bool m_responseSent = false;
};

sceneforge::ai::AiServiceProfile profileFor(const MockHttpServer& server)
{
    sceneforge::ai::AiServiceProfile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.name = QStringLiteral("本机模拟文本服务");
    profile.capability = QStringLiteral("text");
    profile.protocol = QStringLiteral("openai_compatible");
    profile.endpoint = server.endpoint().toString();
    profile.model = QStringLiteral("test-model");
    profile.dataHandlingNote = QStringLiteral("本机模拟服务，不发送至外部网络。");
    return profile;
}

void verifyStructuredResponseAndRequestScope()
{
    const QByteArray responseBody = R"({
        "id": "test-response",
        "choices": [{"message": {"role": "assistant", "content": "{\"synopsis\":\"收到一封迟到的信。\"}"}}],
        "usage": {"prompt_tokens": 12, "completion_tokens": 5}
    })";
    MockHttpServer server(200, responseBody);
    sceneforge::ai::AiTextServiceAdapter adapter;
    const QString secret = QStringLiteral("synthetic-test-token");
    const QString userContent = QStringLiteral("请根据一句创意整理故事梗概。");
    QString validationError;
    const QString taskId = adapter.submit(profileFor(server), secret,
                                          QStringLiteral("仅返回 JSON 对象。"),
                                          userContent, true, 1, &validationError);
    require(!taskId.isEmpty(), validationError);

    bool completed = false;
    QString generatedText;
    QVariantMap usage;
    QEventLoop wait;
    QObject::connect(&adapter, &sceneforge::ai::AiTextServiceAdapter::completionReady,
                     &wait, [&](const QString& resultTaskId,
                                const QString& text,
                                const QVariantMap& resultUsage) {
        if (resultTaskId != taskId) {
            return;
        }
        completed = true;
        generatedText = text;
        usage = resultUsage;
        wait.quit();
    });
    QObject::connect(&adapter, &sceneforge::ai::AiTextServiceAdapter::requestFailed,
                     &wait, [&](const QString& resultTaskId,
                                const QString& category,
                                const QString& message) {
        if (resultTaskId == taskId) {
            wait.exit(1);
            std::cerr << "Unexpected adapter failure (" << category.toStdString()
                      << "): " << message.toStdString() << '\n';
        }
    });
    QTimer::singleShot(5000, &wait, &QEventLoop::quit);
    wait.exec();

    require(completed, QStringLiteral("模拟服务没有返回结构化文本候选。"));
    require(generatedText.contains(QStringLiteral("迟到的信")),
            QStringLiteral("文本候选内容解析错误。"));
    require(usage.value(QStringLiteral("prompt_tokens")).toInt() == 12,
            QStringLiteral("服务用量信息没有随结果返回。"));
    require(server.request().startsWith("POST /v1/chat/completions "),
            QStringLiteral("请求路径没有追加 OpenAI 兼容文本接口。"));
    require(server.request().contains("Authorization: Bearer synthetic-test-token"),
            QStringLiteral("请求没有使用系统凭据接口提供的服务密钥。"));

    const qsizetype bodyStart = server.request().indexOf("\r\n\r\n") + 4;
    const QJsonDocument requestDocument = QJsonDocument::fromJson(
        server.request().mid(bodyStart));
    const QJsonObject requestObject = requestDocument.object();
    require(requestObject.value(QStringLiteral("model")).toString()
                == QStringLiteral("test-model"),
            QStringLiteral("请求中没有选择当前服务的模型。"));
    require(requestObject.value(QStringLiteral("response_format")).toObject()
                .value(QStringLiteral("type")).toString() == QStringLiteral("json_object"),
            QStringLiteral("结构化生成没有请求 JSON 对象响应。"));
    require(requestObject.value(QStringLiteral("messages")).toArray().size() == 2,
            QStringLiteral("系统指令和用户内容没有按消息范围组装。"));
    require(!requestObject.contains(QStringLiteral("n")),
            QStringLiteral("单候选请求不应发送多候选参数。"));
}

void verifyMultipleCandidateResponse()
{
    const QByteArray responseBody = R"({
        "choices": [
            {"message": {"role": "assistant", "content": "版本甲"}},
            {"message": {"role": "assistant", "content": "版本乙"}}
        ],
        "usage": {"completion_tokens": 30}
    })";
    MockHttpServer server(200, responseBody);
    sceneforge::ai::AiTextServiceAdapter adapter;
    QString validationError;
    const QString taskId = adapter.submit(profileFor(server), QStringLiteral("test-token"),
                                          QString(), QStringLiteral("生成三个版本"),
                                          false, 3, &validationError);
    require(!taskId.isEmpty(), validationError);

    QStringList receivedCandidates;
    QEventLoop wait;
    QObject::connect(&adapter, &sceneforge::ai::AiTextServiceAdapter::candidatesReady,
                     &wait, [&](const QString& resultTaskId,
                                const QStringList& candidates,
                                const QVariantMap&) {
        if (resultTaskId == taskId) {
            receivedCandidates = candidates;
            wait.quit();
        }
    });
    QTimer::singleShot(5000, &wait, &QEventLoop::quit);
    wait.exec();
    require(receivedCandidates == QStringList{QStringLiteral("版本甲"),
                                               QStringLiteral("版本乙")},
            QStringLiteral("服务少于请求数返回候选时，已返回内容没有完整保留。"));

    const qsizetype bodyStart = server.request().indexOf("\r\n\r\n") + 4;
    const QJsonObject request = QJsonDocument::fromJson(
        server.request().mid(bodyStart)).object();
    require(request.value(QStringLiteral("n")).toInt() == 3,
            QStringLiteral("请求没有向兼容服务声明需要三个候选。"));
}

void verifyAuthenticationFailure()
{
    MockHttpServer server(401, R"({"error":{"message":"invalid test credential"}})");
    sceneforge::ai::AiTextServiceAdapter adapter;
    QString validationError;
    const QString taskId = adapter.submit(profileFor(server), QStringLiteral("synthetic-token"),
                                          QString(), QStringLiteral("测试认证失败"),
                                          false, 1, &validationError);
    require(!taskId.isEmpty(), validationError);

    QString errorCategory;
    QEventLoop wait;
    QObject::connect(&adapter, &sceneforge::ai::AiTextServiceAdapter::requestFailed,
                     &wait, [&](const QString& resultTaskId,
                                const QString& category,
                                const QString&) {
        if (resultTaskId == taskId) {
            errorCategory = category;
            wait.quit();
        }
    });
    QTimer::singleShot(5000, &wait, &QEventLoop::quit);
    wait.exec();
    require(errorCategory == QStringLiteral("authentication_error"),
            QStringLiteral("HTTP 401 没有分类为认证失败。"));
}

void verifyCancellation()
{
    MockHttpServer server(200, R"({})", true);
    sceneforge::ai::AiTextServiceAdapter adapter;
    QString validationError;
    const QString taskId = adapter.submit(profileFor(server), QStringLiteral("synthetic-token"),
                                          QString(), QStringLiteral("测试取消"),
                                          false, 1, &validationError);
    require(!taskId.isEmpty(), validationError);

    bool cancelAccepted = false;
    QString errorCategory;
    QEventLoop wait;
    QObject::connect(&server, &MockHttpServer::requestCaptured, &wait, [&] {
        cancelAccepted = adapter.cancel(taskId);
    });
    QObject::connect(&adapter, &sceneforge::ai::AiTextServiceAdapter::requestFailed,
                     &wait, [&](const QString& resultTaskId,
                                const QString& category,
                                const QString&) {
        if (resultTaskId == taskId) {
            errorCategory = category;
            wait.quit();
        }
    });
    QTimer::singleShot(5000, &wait, &QEventLoop::quit);
    wait.exec();
    require(cancelAccepted && errorCategory == QStringLiteral("cancelled"),
            QStringLiteral("执行中的请求不能取消或没有返回取消状态。"));
}

void verifyStructuredScriptAnalysisPreview()
{
    MockHttpServer server(200, QByteArrayLiteral("{}"));
    QTemporaryDir temporaryDirectory;
    require(temporaryDirectory.isValid(), QStringLiteral("无法创建结构化分析测试目录。"));

    MemoryCredentialStore credentialStore;
    sceneforge::ai::AiSettingsController settingsController(&credentialStore);
    const sceneforge::ai::AiServiceProfile profile = profileFor(server);
    const QString profileId = settingsController.upsertProfile(profile.toVariantMap(false));
    require(!profileId.isEmpty(), QStringLiteral("分析服务配置失败：")
                                      + settingsController.errorMessage());
    require(settingsController.saveCredential(profileId, QStringLiteral("test-only-secret")),
            QStringLiteral("分析凭据保存失败：") + settingsController.errorMessage());

    sceneforge::project::ProjectController projectController;
    require(projectController.setLibraryRoot(
                QDir(temporaryDirectory.path()).filePath(QStringLiteral("作品库"))),
            QStringLiteral("设置分析测试作品库失败：") + projectController.errorMessage());
    require(projectController.createProject(QStringLiteral("结构化分析测试"), QString(),
                                            QStringLiteral("series")),
            QStringLiteral("创建分析测试作品失败：") + projectController.errorMessage());
    const QVariantList scriptBlocks = projectController.scriptBlocks();
    require(!scriptBlocks.isEmpty(), QStringLiteral("分析测试作品没有初始剧本段落。"));
    const QString blockId = scriptBlocks.first().toMap().value(QStringLiteral("id")).toString();
    const QString originalText = QStringLiteral("顾遥把旧钥匙放回抽屉。");
    require(projectController.updateScriptBlock(blockId, QStringLiteral("action"), originalText),
            QStringLiteral("写入分析测试剧本失败：") + projectController.errorMessage());
    require(projectController.createEpisode(QStringLiteral("第二集")),
            QStringLiteral("创建第二集分析样例失败：") + projectController.errorMessage());
    const QVariantList secondEpisodeBlocks = projectController.scriptBlocks();
    require(!secondEpisodeBlocks.isEmpty(), QStringLiteral("第二集没有初始剧本段落。"));
    const QString secondBlockId = secondEpisodeBlocks.first().toMap()
                                      .value(QStringLiteral("id")).toString();
    const QString secondEpisodeText = QStringLiteral("顾遥收到姐姐发来的语音。");
    require(projectController.updateScriptBlock(secondBlockId, QStringLiteral("dialogue"),
                                                secondEpisodeText),
            QStringLiteral("写入第二集分析剧本失败：") + projectController.errorMessage());

    sceneforge::ai::AiGenerationController generationController(
        &projectController, &settingsController, &credentialStore);
    const QString taskId = generationController.prepareTask(
        QStringLiteral("script_analysis"), QStringLiteral("script_block"), blockId,
        profileId, false, false, true, QStringLiteral("检查连续性。"));
    require(!taskId.isEmpty(), generationController.errorMessage());
    const QVariantMap task = projectController.aiTask(taskId);
    const QString requestText = task.value(QStringLiteral("requestText")).toString();
    const QString systemInstruction = task.value(QStringLiteral("systemInstruction")).toString();
    const QJsonObject analysisManifest = QJsonDocument::fromJson(
        task.value(QStringLiteral("contextManifestJson")).toString().toUtf8()).object();
    require(requestText.contains(QStringLiteral("可引用剧本段落清单"))
                && requestText.contains(QStringLiteral("ID: ") + blockId)
                && requestText.contains(originalText)
                && analysisManifest.value(QStringLiteral("analysisCategoryVersion")).toInt() == 1
                && systemInstruction.contains(QStringLiteral("sceneforge.script-analysis.v1"))
                && systemInstruction.contains(QStringLiteral("sourceId"))
                && systemInstruction.contains(QStringLiteral("theme"))
                && systemInstruction.contains(QStringLiteral("character_goal"))
                && systemInstruction.contains(QStringLiteral("conflict"))
                && systemInstruction.contains(QStringLiteral("pacing"))
                && systemInstruction.contains(QStringLiteral("target_duration"))
                && server.request().isEmpty(),
            QStringLiteral("结构化分析预览未包含稳定段落引用和创作建议维度，或预览时提前发送了请求。"));

    const QString projectAnalysisTaskId = generationController.prepareTask(
        QStringLiteral("script_analysis"), QStringLiteral("project"),
        projectController.currentProjectId(), profileId, false, false, true,
        QStringLiteral("检查全剧连续性。"));
    require(!projectAnalysisTaskId.isEmpty(), generationController.errorMessage());
    const QVariantMap projectAnalysisTask = projectController.aiTask(projectAnalysisTaskId);
    const QString projectAnalysisRequest = projectAnalysisTask
                                               .value(QStringLiteral("requestText")).toString();
    require(projectAnalysisRequest.contains(QStringLiteral("ID: ") + blockId)
                && projectAnalysisRequest.contains(QStringLiteral("ID: ") + secondBlockId)
                && projectAnalysisRequest.contains(originalText)
                && projectAnalysisRequest.contains(secondEpisodeText)
                && projectAnalysisTask.value(QStringLiteral("targetType")).toString()
                       == QStringLiteral("project"),
            QStringLiteral("全剧分析预览未覆盖多集剧本及其稳定段落引用。"));
}

void verifyProjectScriptGenerationFlow()
{
    QTemporaryDir temporaryDirectory;
    require(temporaryDirectory.isValid(), QStringLiteral("无法创建全剧生成测试目录。"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(temporaryDirectory.path()), controller.errorMessage());
    require(controller.createProject(QStringLiteral("全剧生成"), {}, QStringLiteral("series")),
            controller.errorMessage());
    const QString firstEpisodeId = controller.currentEpisodeId();
    const QString firstOriginal = QStringLiteral("第一集正式剧本。 ");
    require(controller.updateScriptBlock(
                controller.scriptBlocks().first().toMap().value(QStringLiteral("id")).toString(),
                QStringLiteral("action"), firstOriginal), controller.errorMessage());
    require(controller.createEpisode(QStringLiteral("第二集")), controller.errorMessage());
    const QString secondEpisodeId = controller.currentEpisodeId();
    const QString secondOriginal = QStringLiteral("第二集正式剧本。");
    require(controller.updateScriptBlock(
                controller.scriptBlocks().first().toMap().value(QStringLiteral("id")).toString(),
                QStringLiteral("action"), secondOriginal), controller.errorMessage());

    const auto candidateFor = [&controller](const QString& suffix) {
        QJsonArray episodes;
        for (const QVariant& value : controller.episodes()) {
            const QVariantMap episode = value.toMap();
            episodes.append(QJsonObject{
                {QStringLiteral("episodeId"), episode.value(QStringLiteral("id")).toString()},
                {QStringLiteral("title"), episode.value(QStringLiteral("title")).toString()},
                {QStringLiteral("text"), episode.value(QStringLiteral("title")).toString() + suffix}
            });
        }
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {QStringLiteral("format"), QStringLiteral("sceneforge.project-script.v1")},
            {QStringLiteral("episodes"), episodes}
        }).toJson(QJsonDocument::Compact));
    };
    const QStringList candidates{candidateFor(QStringLiteral("方案一")),
                                 candidateFor(QStringLiteral("方案二")),
                                 candidateFor(QStringLiteral("方案三"))};
    QJsonArray choices;
    for (const QString& candidate : candidates) {
        choices.append(QJsonObject{{QStringLiteral("message"), QJsonObject{
            {QStringLiteral("content"), candidate}}}});
    }
    MockHttpServer server(200, QJsonDocument(QJsonObject{
        {QStringLiteral("choices"), choices}}).toJson(QJsonDocument::Compact));
    MemoryCredentialStore credentialStore;
    sceneforge::ai::AiSettingsController settings(&credentialStore);
    const QString profileId = settings.upsertProfile(profileFor(server).toVariantMap(false));
    require(!profileId.isEmpty(), settings.errorMessage());
    require(settings.saveCredential(profileId, QStringLiteral("test-only-secret")),
            settings.errorMessage());
    sceneforge::ai::AiGenerationController generation(&controller, &settings, &credentialStore);
    const auto prepare = [&] (int count) {
        const QString id = generation.prepareTask(
            QStringLiteral("script_generate"), QStringLiteral("project"),
            controller.currentProjectId(), profileId, false, false, false,
            QStringLiteral("为各集生成连贯的候选剧本。"), count);
        require(!id.isEmpty(), generation.errorMessage());
        return id;
    };
    const QString taskId = prepare(3);
    const QVariantMap prepared = controller.aiTask(taskId);
    require(server.request().isEmpty()
                && prepared.value(QStringLiteral("requestText")).toString().contains(firstEpisodeId)
                && prepared.value(QStringLiteral("requestText")).toString().contains(secondEpisodeId)
                && !prepared.value(QStringLiteral("requestText")).toString().contains(firstOriginal)
                && prepared.value(QStringLiteral("systemInstruction")).toString()
                       .contains(QStringLiteral("sceneforge.project-script.v1")),
            QStringLiteral("全剧请求未显示固定身份清单，泄漏未选择正文，或预览时提前发送。"));
    QEventLoop wait;
    QObject::connect(&controller, &sceneforge::project::ProjectController::aiGenerationTasksChanged,
                     &wait, [&] {
        if (controller.aiTask(taskId).value(QStringLiteral("status")).toString()
            == QStringLiteral("succeeded")) {
            wait.quit();
        }
    });
    require(generation.submitPreparedTask(taskId), generation.errorMessage());
    QTimer::singleShot(5000, &wait, &QEventLoop::quit);
    wait.exec();
    require(controller.aiTask(taskId).value(QStringLiteral("status")).toString()
                == QStringLiteral("succeeded"), QStringLiteral("全剧模拟生成未成功返回。"));
    const QJsonObject request = QJsonDocument::fromJson(
        server.request().mid(server.request().indexOf("\r\n\r\n") + 4)).object();
    require(request.value(QStringLiteral("n")).toInt() == 3
                && request.value(QStringLiteral("response_format")).toObject()
                       .value(QStringLiteral("type")).toString() == QStringLiteral("json_object"),
            QStringLiteral("全剧请求未发送候选数或结构化响应要求。"));
    QStringList returned;
    int selected = -1;
    require(sceneforge::project::decodeScriptCandidates(
                controller.aiTask(taskId).value(QStringLiteral("candidateText")).toString(),
                &returned, &selected) && returned == candidates,
            QStringLiteral("全剧多个方案没有被完整保存。"));
    require(controller.scriptVersions().isEmpty() && controller.scriptText() == secondOriginal,
            QStringLiteral("全剧生成返回时提前写入版本或正式剧本。"));
    QString encoded;
    require(sceneforge::project::encodeScriptCandidates(returned, 1, &encoded),
            QStringLiteral("无法选择全剧第二方案。"));
    require(controller.updateAiTaskCandidate(taskId, encoded), controller.errorMessage());
    const QString path = controller.currentProjectPath();
    require(controller.closeProject(), controller.errorMessage());
    require(controller.openProject(path), controller.errorMessage());
    const bool accepted = controller.acceptAiTask(taskId);
    require(accepted, QStringLiteral("采纳全剧候选失败：") + controller.errorMessage());
    require(!controller.acceptAiTask(taskId), QStringLiteral("重复采纳全剧结果创建了重复版本。"));
    controller.clearError();
    for (const QString& id : {firstEpisodeId, secondEpisodeId}) {
        require(controller.selectEpisode(id), controller.errorMessage());
        require(controller.scriptVersions().size() == 1
                    && controller.scriptVersions().first().toMap()
                           .value(QStringLiteral("versionType")).toString()
                           == QStringLiteral("generated_candidate")
                    && controller.scriptVersions().first().toMap()
                           .value(QStringLiteral("contentStatus")).toString()
                           == QStringLiteral("candidate")
                    && controller.scriptVersions().first().toMap()
                           .value(QStringLiteral("text")).toString().endsWith(QStringLiteral("方案二"))
                    && controller.scriptText() == (id == firstEpisodeId ? firstOriginal : secondOriginal),
                QStringLiteral("全剧采纳没有逐集新增选中候选版本，或改写了正式稿。"));
    }

    // 第二集候选失效时，第一集也不能留下半成品版本。
    const QString invalidTask = prepare(1);
    QJsonObject invalidRoot = QJsonDocument::fromJson(candidates.first().toUtf8()).object();
    QJsonArray invalidEpisodes = invalidRoot.value(QStringLiteral("episodes")).toArray();
    QJsonObject invalidSecond = invalidEpisodes.at(1).toObject();
    invalidSecond.insert(QStringLiteral("episodeId"), QStringLiteral("unknown-episode"));
    invalidEpisodes[1] = invalidSecond;
    invalidRoot.insert(QStringLiteral("episodes"), invalidEpisodes);
    require(controller.setAiTaskRunning(invalidTask), controller.errorMessage());
    require(controller.completeAiTask(invalidTask,
                QString::fromUtf8(QJsonDocument(invalidRoot).toJson(QJsonDocument::Compact)), {}),
            controller.errorMessage());
    require(!controller.acceptAiTask(invalidTask), QStringLiteral("全剧采纳接受了未知剧集身份。"));
    controller.clearError();
    require(controller.updateAiTaskCandidate(invalidTask, candidates.first()), controller.errorMessage());
    require(controller.renameEpisode(secondEpisodeId, QStringLiteral("改名后")), controller.errorMessage());
    require(!controller.acceptAiTask(invalidTask), QStringLiteral("剧集改名后仍采纳了过期全剧候选。"));
    controller.clearError();
    require(controller.renameEpisode(secondEpisodeId, QStringLiteral("第二集")), controller.errorMessage());
    require(controller.moveEpisode(secondEpisodeId, -1), controller.errorMessage());
    require(!controller.acceptAiTask(invalidTask), QStringLiteral("剧集重排后仍采纳了过期全剧候选。"));
    controller.clearError();
    require(controller.moveEpisode(secondEpisodeId, 1), controller.errorMessage());
    for (const QString& id : {firstEpisodeId, secondEpisodeId}) {
        require(controller.selectEpisode(id), controller.errorMessage());
        require(controller.scriptVersions().size() == 1,
                QStringLiteral("拒绝失效全剧候选后仍追加了部分版本。"));
    }
    require(controller.acceptAiTask(invalidTask), controller.errorMessage());
    require(controller.closeProject(), controller.errorMessage());
    require(controller.openProject(path), controller.errorMessage());
    require(controller.aiTask(taskId).value(QStringLiteral("candidateText")).toString() == encoded,
            QStringLiteral("全剧采纳丢失了未选中的候选及选择状态。"));
    for (const QString& id : {firstEpisodeId, secondEpisodeId}) {
        require(controller.selectEpisode(id), controller.errorMessage());
        require(controller.scriptVersions().size() == 2
                    && controller.scriptText() == (id == firstEpisodeId ? firstOriginal : secondOriginal),
                QStringLiteral("全剧单/多方案版本未持久化，或重开后正式稿改变。"));
    }

    require(controller.selectEpisode(secondEpisodeId), controller.errorMessage());
    const qsizetype previousVersionCount = controller.scriptVersions().size();
    const QByteArray previousHttpRequest = server.request();
    const QString continuationTaskId = generation.prepareTask(
        QStringLiteral("script_continue"), QStringLiteral("project"),
        controller.currentProjectId(), profileId, false, false, true,
        QStringLiteral("从全剧结尾继续剧情。"));
    require(!continuationTaskId.isEmpty(), generation.errorMessage());
    const QVariantMap continuationTask = controller.aiTask(continuationTaskId);
    const QString continuationRequest = continuationTask
        .value(QStringLiteral("requestText")).toString();
    require(continuationTask.value(QStringLiteral("targetTitle")).toString()
                    .contains(QStringLiteral("全剧末尾续写"))
                && continuationTask.value(QStringLiteral("systemInstruction")).toString()
                       .contains(QStringLiteral("最后一集现有剧本结尾之后"))
                && continuationRequest.contains(firstOriginal.trimmed())
                && continuationRequest.contains(secondOriginal)
                && continuationRequest.contains(QStringLiteral("从全剧结尾继续剧情"))
                && server.request() == previousHttpRequest,
            QStringLiteral("全剧续写预览未包含所有剧集上下文或误发请求。"));
    require(controller.setAiTaskRunning(continuationTaskId), controller.errorMessage());
    const QString continuationText = QStringLiteral("新一集：失踪的剧本终于有了下落。\n");
    require(controller.completeAiTask(continuationTaskId, continuationText, {}),
            controller.errorMessage());
    require(controller.scriptText() == secondOriginal,
            QStringLiteral("全剧续写候选返回时提前更改了正式剧本。"));
    require(controller.acceptAiTask(continuationTaskId), controller.errorMessage());
    require(controller.selectEpisode(firstEpisodeId), controller.errorMessage());
    require(controller.scriptText() == firstOriginal,
            QStringLiteral("全剧末尾续写修改了最后一集之前的内容。"));
    require(controller.selectEpisode(secondEpisodeId), controller.errorMessage());
    const QVariantList continuedVersions = controller.scriptVersions();
    require(controller.scriptText().contains(secondOriginal)
                && controller.scriptText().contains(continuationText.trimmed())
                && continuedVersions.size() == previousVersionCount + 1
                && continuedVersions.last().toMap().value(QStringLiteral("text")).toString()
                       == secondOriginal,
            QStringLiteral("确认全剧续写后没有只追加到最后一集，或未保存其原稿版本。"));
}

void verifyBatchScriptRewriteGenerationFlow()
{
    QTemporaryDir temporaryDirectory;
    require(temporaryDirectory.isValid(), QStringLiteral("无法创建批量改写测试目录。"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(temporaryDirectory.path()), controller.errorMessage());
    require(controller.createProject(QStringLiteral("批量剧本改写"), {},
                                     QStringLiteral("series")), controller.errorMessage());
    const QString firstEpisodeId = controller.currentEpisodeId();
    require(controller.createScene(), controller.errorMessage());
    const QString firstSceneId = controller.scenes().first().toMap()
                                     .value(QStringLiteral("id")).toString();
    const QString firstBlockId = controller.scriptBlocks().first().toMap()
                                     .value(QStringLiteral("id")).toString();
    require(controller.setScriptBlockScene(firstBlockId, firstSceneId), controller.errorMessage());
    const QString firstOriginal = QStringLiteral("第一集第一场动作原稿。");
    require(controller.updateScriptBlock(firstBlockId, QStringLiteral("action"), firstOriginal),
            controller.errorMessage());
    require(controller.createScriptBlock(QStringLiteral("dialogue"), firstSceneId),
            controller.errorMessage());
    const QString secondBlockId = controller.scriptBlocks().last().toMap()
                                      .value(QStringLiteral("id")).toString();
    const QString secondOriginal = QStringLiteral("第一集第一场对白原稿。");
    require(controller.updateScriptBlock(secondBlockId, QStringLiteral("dialogue"), secondOriginal),
            controller.errorMessage());

    require(controller.createEpisode(QStringLiteral("第二集")), controller.errorMessage());
    const QString secondEpisodeId = controller.currentEpisodeId();
    require(controller.createScene(), controller.errorMessage());
    const QString secondSceneId = controller.scenes().first().toMap()
                                      .value(QStringLiteral("id")).toString();
    const QString thirdBlockId = controller.scriptBlocks().first().toMap()
                                     .value(QStringLiteral("id")).toString();
    require(controller.setScriptBlockScene(thirdBlockId, secondSceneId), controller.errorMessage());
    const QString thirdOriginal = QStringLiteral("第二集第一场动作原稿。");
    require(controller.updateScriptBlock(thirdBlockId, QStringLiteral("action"), thirdOriginal),
            controller.errorMessage());

    const QVariantMap context = controller.aiContextSnapshot(
        QStringLiteral("project"), controller.currentProjectId(), false, false, true);
    require(context.value(QStringLiteral("valid")).toBool(),
            context.value(QStringLiteral("error")).toString());
    const QJsonObject manifest = QJsonDocument::fromJson(
        context.value(QStringLiteral("contextManifestJson")).toString().toUtf8()).object();
    const QJsonArray targets = manifest.value(QStringLiteral("targetScriptBlocks")).toArray();
    require(targets.size() == 3,
            QStringLiteral("全剧批量改写上下文没有列出每集的目标段落。"));

    QJsonArray choices;
    QStringList expectedCandidates;
    for (int candidateIndex = 0; candidateIndex < 3; ++candidateIndex) {
        QJsonArray blocks;
        for (const QJsonValue& value : targets) {
            const QString blockId = value.toObject().value(
                QStringLiteral("blockId")).toString();
            blocks.append(QJsonObject{
                {QStringLiteral("blockId"), blockId},
                {QStringLiteral("text"), QStringLiteral("方案%1：%2")
                     .arg(candidateIndex + 1).arg(blockId)}
            });
        }
        const QString candidate = QString::fromUtf8(QJsonDocument(QJsonObject{
            {QStringLiteral("format"), QStringLiteral("sceneforge.script-block-rewrites.v1")},
            {QStringLiteral("blocks"), blocks}
        }).toJson(QJsonDocument::Compact));
        expectedCandidates.append(candidate);
        choices.append(QJsonObject{{QStringLiteral("message"), QJsonObject{
            {QStringLiteral("content"), candidate}}}});
    }
    MockHttpServer server(200, QJsonDocument(QJsonObject{
        {QStringLiteral("choices"), choices},
        {QStringLiteral("usage"), QJsonObject{
            {QStringLiteral("prompt_tokens"), 80},
            {QStringLiteral("completion_tokens"), 45}
        }}
    }).toJson(QJsonDocument::Compact));

    MemoryCredentialStore credentialStore;
    sceneforge::ai::AiSettingsController settings(&credentialStore);
    const QString profileId = settings.upsertProfile(profileFor(server).toVariantMap(false));
    require(!profileId.isEmpty(), settings.errorMessage());
    require(settings.saveCredential(profileId, QStringLiteral("test-only-secret")),
            settings.errorMessage());
    sceneforge::ai::AiGenerationController generation(&controller, &settings, &credentialStore);
    const QString taskId = generation.prepareTask(
        QStringLiteral("script_rewrite"), QStringLiteral("project"),
        controller.currentProjectId(), profileId, false, false, true,
        QStringLiteral("保持全部剧情事实，分别润色全剧的每个段落。"), 3);
    require(!taskId.isEmpty(), generation.errorMessage());

    const QVariantMap taskBeforeSend = controller.aiTask(taskId);
    const QString requestPreview = taskBeforeSend.value(QStringLiteral("requestText")).toString();
    require(server.request().isEmpty()
                && requestPreview.contains(firstOriginal)
                && requestPreview.contains(secondOriginal)
                && requestPreview.contains(thirdOriginal)
                && taskBeforeSend.value(QStringLiteral("systemInstruction")).toString()
                       .contains(QStringLiteral("sceneforge.script-block-rewrites.v1")),
            QStringLiteral("批量改写发送预览未包含全剧目标正文或稳定 ID 结构要求。"));

    bool taskFinished = false;
    QEventLoop wait;
    QObject::connect(&controller,
                     &sceneforge::project::ProjectController::aiGenerationTasksChanged,
                     &wait, [&] {
        if (controller.aiTask(taskId).value(QStringLiteral("status")).toString()
            == QStringLiteral("succeeded")) {
            taskFinished = true;
            wait.quit();
        }
    });
    require(generation.submitPreparedTask(taskId), generation.errorMessage());
    if (!taskFinished) {
        QTimer::singleShot(5000, &wait, &QEventLoop::quit);
        wait.exec();
    }
    const QByteArray request = server.request();
    const qsizetype bodyStart = request.indexOf("\r\n\r\n") + 4;
    const QJsonObject requestBody = QJsonDocument::fromJson(request.mid(bodyStart)).object();
    require(taskFinished && requestBody.value(QStringLiteral("n")).toInt() == 3
                && requestPreview.contains(firstBlockId)
                && requestPreview.contains(secondBlockId)
                && requestPreview.contains(thirdBlockId),
            QStringLiteral("批量改写服务请求缺少 1/3 候选数量或跨集稳定段落清单。"));

    QStringList returnedCandidates;
    int selectedIndex = -1;
    require(sceneforge::project::decodeScriptCandidates(
                controller.aiTask(taskId).value(QStringLiteral("candidateText")).toString(),
                &returnedCandidates, &selectedIndex)
                && returnedCandidates == expectedCandidates && selectedIndex == 0,
            QStringLiteral("全剧批量改写没有完整保存服务返回的多方案。"));
    QString selectedCandidate;
    require(sceneforge::project::encodeScriptCandidates(
                returnedCandidates, 1, &selectedCandidate),
            QStringLiteral("无法选择全剧第二个批量改写方案。"));
    require(controller.updateAiTaskCandidate(taskId, selectedCandidate),
            controller.errorMessage());
    require(controller.acceptAiTask(taskId), controller.errorMessage());
    require(controller.selectEpisode(firstEpisodeId), controller.errorMessage());
    require(controller.scriptText().contains(QStringLiteral("方案2："))
                && controller.scriptVersions().size() == 1
                && controller.scriptVersions().first().toMap()
                       .value(QStringLiteral("text")).toString().contains(firstOriginal),
            QStringLiteral("批量采纳没有为第一集写入所选方案并保留原稿。"));
    require(controller.selectEpisode(secondEpisodeId), controller.errorMessage());
    require(controller.scriptText().contains(QStringLiteral("方案2："))
                && controller.scriptVersions().size() == 1
                && controller.scriptVersions().first().toMap()
                       .value(QStringLiteral("text")).toString().contains(thirdOriginal),
            QStringLiteral("批量采纳没有为第二集写入所选方案并保留原稿。"));
}

void verifySingleCandidateSceneReviewEnvelope()
{
    const QByteArray responseBody = R"({
        "choices": [{"message": {"role": "assistant", "content": "候选场景草稿。"}}],
        "usage": {"prompt_tokens": 12, "completion_tokens": 5}
    })";
    MockHttpServer server(200, responseBody);
    QTemporaryDir temporaryDirectory;
    require(temporaryDirectory.isValid(), QStringLiteral("无法创建场景生成测试目录。"));

    MemoryCredentialStore credentialStore;
    sceneforge::ai::AiSettingsController settingsController(&credentialStore);
    const sceneforge::ai::AiServiceProfile profile = profileFor(server);
    const QString profileId = settingsController.upsertProfile(profile.toVariantMap(false));
    require(!profileId.isEmpty(), QStringLiteral("场景生成服务配置失败：")
                                      + settingsController.errorMessage());
    require(settingsController.saveCredential(profileId, QStringLiteral("test-only-secret")),
            QStringLiteral("场景生成凭据保存失败：") + settingsController.errorMessage());

    sceneforge::project::ProjectController projectController;
    require(projectController.setLibraryRoot(
                QDir(temporaryDirectory.path()).filePath(QStringLiteral("作品库"))),
            QStringLiteral("设置场景生成作品库失败：") + projectController.errorMessage());
    require(projectController.createProject(QStringLiteral("单候选场景测试"), QString()),
            QStringLiteral("创建场景生成作品失败：") + projectController.errorMessage());
    require(projectController.createScene(),
            QStringLiteral("创建测试场次失败：") + projectController.errorMessage());
    const QString sceneId = projectController.scenes().first().toMap()
                                .value(QStringLiteral("id")).toString();
    sceneforge::ai::AiGenerationController generationController(
        &projectController, &settingsController, &credentialStore);
    const QString taskId = generationController.prepareTask(
        QStringLiteral("script_generate"), QStringLiteral("scene"), sceneId,
        profileId, false, false, false, QStringLiteral("生成场景。"));
    require(!taskId.isEmpty(), generationController.errorMessage());
    QEventLoop wait;
    bool taskFinished = false;
    QObject::connect(&projectController,
                     &sceneforge::project::ProjectController::aiGenerationTasksChanged,
                     &wait, [&] {
        if (projectController.aiTask(taskId).value(QStringLiteral("status")).toString()
            == QStringLiteral("succeeded")) {
            taskFinished = true;
            wait.quit();
        }
    });
    require(generationController.submitPreparedTask(taskId), generationController.errorMessage());
    if (!taskFinished) {
        QTimer::singleShot(5000, &wait, &QEventLoop::quit);
        wait.exec();
    }
    const QVariantMap task = projectController.aiTask(taskId);
    QStringList candidates;
    int selectedIndex = -1;
    require(taskFinished
                && sceneforge::project::decodeScriptCandidates(
                    task.value(QStringLiteral("candidateText")).toString(),
                    &candidates, &selectedIndex)
                && candidates == QStringList{QStringLiteral("候选场景草稿。")}
                && selectedIndex == 0
                && task.value(QStringLiteral("contextManifestJson")).toString()
                       .contains(QStringLiteral("\"candidateCount\":1"))
                && task.value(QStringLiteral("systemInstruction")).toString()
                       .contains(QStringLiteral("角色目标"))
                && projectController.scriptText().isEmpty(),
            QStringLiteral("单候选场景没有保留审核状态所需的候选信封，或提前改写了剧本。"));
}

void verifyScriptBlockGenerationReplacesSelectedTarget()
{
    MockHttpServer server(200, QByteArrayLiteral("{}"));
    QTemporaryDir temporaryDirectory;
    require(temporaryDirectory.isValid(), QStringLiteral("无法创建段落生成测试目录。"));

    MemoryCredentialStore credentialStore;
    sceneforge::ai::AiSettingsController settingsController(&credentialStore);
    const sceneforge::ai::AiServiceProfile profile = profileFor(server);
    const QString profileId = settingsController.upsertProfile(profile.toVariantMap(false));
    require(!profileId.isEmpty(), settingsController.errorMessage());
    require(settingsController.saveCredential(profileId, QStringLiteral("test-only-secret")),
            settingsController.errorMessage());

    sceneforge::project::ProjectController projectController;
    require(projectController.setLibraryRoot(temporaryDirectory.path()),
            projectController.errorMessage());
    require(projectController.createProject(QStringLiteral("段落生成目标测试"), {}),
            projectController.errorMessage());
    const QVariantList initialBlocks = projectController.scriptBlocks();
    require(!initialBlocks.isEmpty(), QStringLiteral("测试作品没有初始剧本段落。"));
    const QString blockId = initialBlocks.first().toMap()
                                .value(QStringLiteral("id")).toString();
    const QString originalText = QStringLiteral("顾遥：不要走。你还有话没说完。\n");
    require(projectController.updateScriptBlock(
                blockId, QStringLiteral("dialogue"), originalText),
            projectController.errorMessage());

    sceneforge::ai::AiGenerationController generationController(
        &projectController, &settingsController, &credentialStore);
    const QString taskId = generationController.prepareTask(
        QStringLiteral("script_generate"), QStringLiteral("script_block"), blockId,
        profileId, false, false, true, QStringLiteral("重写这段对白。"));
    require(!taskId.isEmpty(), generationController.errorMessage());
    const QVariantMap preparedTask = projectController.aiTask(taskId);
    require(preparedTask.value(QStringLiteral("requestText")).toString()
                    .contains(originalText.trimmed())
                && projectController.scriptBlocks().first().toMap()
                       .value(QStringLiteral("text")).toString() == originalText
                && server.request().isEmpty(),
            QStringLiteral("段落生成预览未包含目标原文，或准备时改写/发送了正式内容。"));
    require(projectController.setAiTaskRunning(taskId), projectController.errorMessage());
    const QString generatedText = QStringLiteral("顾遥：留下来。我们一起把故事说完。\n");
    require(projectController.completeAiTask(taskId, generatedText, {}),
            projectController.errorMessage());
    require(projectController.scriptBlocks().first().toMap()
                .value(QStringLiteral("text")).toString() == originalText,
            QStringLiteral("生成候选返回时提前改写了所选正式段落。"));
    require(projectController.acceptAiTask(taskId), projectController.errorMessage());

    const QVariantList acceptedBlocks = projectController.scriptBlocks();
    require(acceptedBlocks.size() == initialBlocks.size()
                && acceptedBlocks.first().toMap().value(QStringLiteral("id")).toString()
                       == blockId
                && acceptedBlocks.first().toMap().value(QStringLiteral("blockType")).toString()
                       == QStringLiteral("dialogue")
                && acceptedBlocks.first().toMap().value(QStringLiteral("text")).toString()
                       == generatedText
                && projectController.scriptVersions().size() == 1
                && projectController.scriptVersions().first().toMap()
                       .value(QStringLiteral("text")).toString() == originalText,
            QStringLiteral("确认段落生成后没有替换指定段落、保留其语义类型和原稿版本。"));
}

void verifyGenerationConfirmationAndCandidateFlow()
{
    const QByteArray responseBody = R"({
        "choices": [
            {"message": {"role": "assistant", "content": "剧本候选第一版。"}},
            {"message": {"role": "assistant", "content": "剧本候选第二版。"}},
            {"message": {"role": "assistant", "content": "剧本候选第三版。"}}
        ],
        "usage": {"prompt_tokens": 20, "completion_tokens": 7}
    })";
    MockHttpServer server(200, responseBody);
    QTemporaryDir temporaryDirectory;
    require(temporaryDirectory.isValid(), QStringLiteral("无法创建 AI 生成测试目录。"));

    MemoryCredentialStore credentialStore;
    sceneforge::ai::AiSettingsController settingsController(&credentialStore);
    const sceneforge::ai::AiServiceProfile profile = profileFor(server);
    const QString profileId = settingsController.upsertProfile(profile.toVariantMap(false));
    require(!profileId.isEmpty(), QStringLiteral("服务配置失败：")
                                      + settingsController.errorMessage());
    require(settingsController.saveCredential(profileId, QStringLiteral("test-only-secret")),
            QStringLiteral("凭据保存失败：") + settingsController.errorMessage());

    sceneforge::project::ProjectController projectController;
    require(projectController.setLibraryRoot(
                QDir(temporaryDirectory.path()).filePath(QStringLiteral("作品库"))),
            QStringLiteral("设置测试作品库失败：") + projectController.errorMessage());
    require(projectController.createProject(QStringLiteral("生成候选测试"),
                                            QStringLiteral("不可发送的私密简报。")),
            QStringLiteral("创建测试作品失败：") + projectController.errorMessage());
    const QString episodeId = projectController.currentEpisodeId();
    sceneforge::ai::AiGenerationController generationController(
        &projectController, &settingsController, &credentialStore);

    const QString taskId = generationController.prepareTask(
        QStringLiteral("script_generate"), QStringLiteral("episode"), episodeId,
        profileId, false, false, false, QStringLiteral("只写一场短剧。"), 3);
    require(!taskId.isEmpty(), generationController.errorMessage());
    require(server.request().isEmpty(),
            QStringLiteral("仅准备发送预览时不应提交 HTTP 请求。"));
    QVariantMap task = projectController.aiTask(taskId);
    require(task.value(QStringLiteral("status")).toString()
                == QStringLiteral("awaiting_confirmation")
                && task.value(QStringLiteral("requestText")).toString()
                       .contains(QStringLiteral("只写一场短剧。")),
            QStringLiteral("待发送任务没有持久化用户可见请求预览。"));

    const QString confirmedContent = QStringLiteral("这段用户修改后的确切发送内容；不发送简报。");
    require(generationController.updatePreparedRequest(
                taskId, task.value(QStringLiteral("systemInstruction")).toString(),
                confirmedContent), generationController.errorMessage());
    bool taskFinished = false;
    QEventLoop wait;
    QObject::connect(&projectController,
                     &sceneforge::project::ProjectController::aiGenerationTasksChanged,
                     &wait, [&] {
        if (projectController.aiTask(taskId).value(QStringLiteral("status")).toString()
            == QStringLiteral("succeeded")) {
            taskFinished = true;
            wait.quit();
        }
    });
    require(generationController.submitPreparedTask(taskId),
            generationController.errorMessage());
    if (!taskFinished) {
        QTimer::singleShot(5000, &wait, &QEventLoop::quit);
        wait.exec();
    }
    require(taskFinished, QStringLiteral("AI 生成控制器没有保存模拟服务返回的候选。"));
    const QByteArray request = server.request();
    const qsizetype bodyStart = request.indexOf("\r\n\r\n") + 4;
    const QJsonObject requestBodyObject = QJsonDocument::fromJson(request.mid(bodyStart)).object();
    const QString sentContent = requestBodyObject.value(QStringLiteral("messages")).toArray()
                                    .at(1).toObject()
                                    .value(QStringLiteral("content")).toString();
    require(sentContent == confirmedContent,
            QStringLiteral("实际发送内容与用户确认的请求预览不一致。"));
    require(!sentContent.contains(QStringLiteral("不可发送的私密简报")),
            QStringLiteral("用户移除的简报内容仍被发送。"));
    require(requestBodyObject.value(QStringLiteral("n")).toInt() == 3,
            QStringLiteral("生成任务没有发送确认的三个候选数量。"));
    require(projectController.scriptText().isEmpty(),
            QStringLiteral("AI 返回候选时提前改写了正式剧本。"));
    task = projectController.aiTask(taskId);
    QStringList savedCandidates;
    int selectedCandidateIndex = -1;
    require(sceneforge::project::decodeScriptCandidates(
                task.value(QStringLiteral("candidateText")).toString(),
                &savedCandidates, &selectedCandidateIndex)
                && savedCandidates.size() == 3 && selectedCandidateIndex == 0
                && task.value(QStringLiteral("status")).toString()
                       == QStringLiteral("succeeded"),
            QStringLiteral("生成候选或任务状态没有保存。"));
    require(projectController.saveAiTaskCandidateAsScriptVersion(taskId),
            QStringLiteral("独立候选版本保存失败：") + projectController.errorMessage());
    require(projectController.scriptText().isEmpty()
                && projectController.scriptVersions().size() == 1
                && projectController.scriptVersions().first().toMap()
                       .value(QStringLiteral("versionType")).toString()
                       == QStringLiteral("generated_candidate")
                && projectController.scriptVersions().first().toMap()
                       .value(QStringLiteral("text")).toString()
                       == QStringLiteral("剧本候选第一版。"),
            QStringLiteral("另存 AI 候选为独立版本时改变了正式稿，或版本内容错误。"));
    require(projectController.saveAiTaskCandidatesAsScriptVersions(taskId),
            QStringLiteral("批量另存其余候选版本失败：") + projectController.errorMessage());
    QSet<QString> savedCandidateTexts;
    for (const QVariant& version : projectController.scriptVersions()) {
        const QVariantMap item = version.toMap();
        if (item.value(QStringLiteral("versionType")).toString()
            == QStringLiteral("generated_candidate")) {
            savedCandidateTexts.insert(item.value(QStringLiteral("text")).toString());
        }
    }
    QSet<QString> expectedCandidateTexts;
    for (const QString& candidate : savedCandidates) {
        expectedCandidateTexts.insert(candidate);
    }
    require(savedCandidateTexts == expectedCandidateTexts
                && projectController.scriptVersions().size() == 3
                && projectController.scriptText().isEmpty(),
            QStringLiteral("批量另存候选没有创建全部独立版本，或改写了正式剧本。"));
    require(!projectController.saveAiTaskCandidatesAsScriptVersions(taskId)
                && projectController.scriptVersions().size() == 3,
            QStringLiteral("重复批量另存生成了重复版本。"));
    projectController.clearError();
    const QByteArray taskJson = QJsonDocument::fromVariant(
        projectController.aiGenerationTasks()).toJson();
    require(!taskJson.contains("test-only-secret"),
            QStringLiteral("服务凭据泄漏到了作品任务记录。"));
    const QString cancelledTaskId = generationController.prepareTask(
        QStringLiteral("script_generate"), QStringLiteral("episode"), episodeId,
        profileId, false, false, false, QStringLiteral("取消测试。"));
    require(!cancelledTaskId.isEmpty(), generationController.errorMessage());
    const QByteArray requestBeforeCancel = server.request();
    require(generationController.cancelTask(cancelledTaskId), generationController.errorMessage());
    require(server.request() == requestBeforeCancel
                && projectController.aiTask(cancelledTaskId)
                       .value(QStringLiteral("status")).toString()
                       == QStringLiteral("cancelled"),
            QStringLiteral("发送确认前取消仍发送了请求，或未记录取消状态。"));

    const QString projectPath = projectController.currentProjectPath();
    require(projectController.closeProject(), projectController.errorMessage());
    require(projectController.openProject(projectPath), projectController.errorMessage());
    task = projectController.aiTask(taskId);
    QStringList restoredCandidates;
    int restoredSelection = -1;
    require(sceneforge::project::decodeScriptCandidates(
                task.value(QStringLiteral("candidateText")).toString(),
                &restoredCandidates, &restoredSelection)
                && restoredCandidates == savedCandidates && restoredSelection == 0
                && task.value(QStringLiteral("status")).toString()
                       == QStringLiteral("succeeded")
                && projectController.scriptText().isEmpty()
                && projectController.scriptVersions().size() == 3
                && projectController.scriptVersions().first().toMap()
                       .value(QStringLiteral("text")).toString()
                       == QStringLiteral("剧本候选第一版。")
                && QJsonDocument::fromJson(task.value(
                       QStringLiteral("contextManifestJson")).toString().toUtf8())
                       .object().value(QStringLiteral("savedCandidateVersions")).toArray()
                       .size() == 3
                && projectController.aiTask(cancelledTaskId)
                       .value(QStringLiteral("status")).toString()
                       == QStringLiteral("cancelled"),
            QStringLiteral("重新打开作品后候选没有恢复，或尚未确认的候选写入了正式剧本。"));
    const QString invalidCandidatesJson = QStringLiteral(
        "{\"format\":\"sceneforge.script-candidates.v1\",\"selectedIndex\":4,"
        "\"items\":[\"无效选择\"]}");
    require(projectController.updateAiTaskCandidate(taskId, invalidCandidatesJson),
            projectController.errorMessage());
    require(!projectController.acceptAiTask(taskId) && projectController.scriptText().isEmpty(),
            QStringLiteral("无效的候选选择被采纳，或错误修改正式剧本。"));
    QString selectedCandidatesJson;
    require(sceneforge::project::encodeScriptCandidates(savedCandidates, 1,
                                                         &selectedCandidatesJson),
            QStringLiteral("无法保存选中的第二个剧本候选。"));
    require(projectController.updateAiTaskCandidate(taskId, selectedCandidatesJson),
            projectController.errorMessage());
    require(projectController.acceptAiTask(taskId), projectController.errorMessage());
    require(projectController.scriptText() == QStringLiteral("剧本候选第二版。")
                && projectController.aiTask(taskId).value(QStringLiteral("status")).toString()
                       == QStringLiteral("accepted"),
            QStringLiteral("用户确认后未将候选写入剧本并记录确认状态。"));
    const QString acceptedPath = projectController.currentProjectPath();
    require(projectController.closeProject(), projectController.errorMessage());
    require(projectController.openProject(acceptedPath), projectController.errorMessage());
    require(projectController.scriptText() == QStringLiteral("剧本候选第二版。")
                && projectController.aiTask(taskId).value(QStringLiteral("status")).toString()
                       == QStringLiteral("accepted"),
            QStringLiteral("已确认的剧本和 AI 任务状态没有在重开后恢复。"));
    QString blockId;
    for (const QVariant& value : projectController.scriptBlocks()) {
        const QVariantMap block = value.toMap();
        if (block.value(QStringLiteral("text")).toString()
            == QStringLiteral("剧本候选第二版。")) {
            blockId = block.value(QStringLiteral("id")).toString();
            break;
        }
    }
    require(!blockId.isEmpty(), QStringLiteral("无法定位用于改写保护测试的剧本段落。"));
    require(projectController.updateScriptBlock(
                blockId, QStringLiteral("dialogue"), QStringLiteral("剧本候选第二版。")),
            projectController.errorMessage());
    const QStringList rewriteOperations{
        QStringLiteral("script_compress"), QStringLiteral("script_expand"),
        QStringLiteral("script_polish"), QStringLiteral("script_style_adjust")};
    QString compressionTaskId;
    for (const QString& operation : rewriteOperations) {
        const QString transformTaskId = generationController.prepareTask(
            operation, QStringLiteral("script_block"), blockId, profileId,
            false, false, true, QStringLiteral("保留既有剧情事实。"));
        require(!transformTaskId.isEmpty(), generationController.errorMessage());
        const QVariantMap transformTask = projectController.aiTask(transformTaskId);
        require(transformTask.value(QStringLiteral("operation")).toString() == operation
                    && transformTask.value(QStringLiteral("status")).toString()
                           == QStringLiteral("awaiting_confirmation")
                    && !transformTask.value(QStringLiteral("systemInstruction")).toString().isEmpty(),
                QStringLiteral("段落改写操作没有生成待确认的专用任务。"));
        if (operation == QStringLiteral("script_compress")) {
            compressionTaskId = transformTaskId;
        } else {
            require(generationController.cancelTask(transformTaskId),
                    generationController.errorMessage());
        }
    }
    require(projectController.setAiTaskRunning(compressionTaskId),
            projectController.errorMessage());
    require(projectController.completeAiTask(compressionTaskId,
                                            QStringLiteral("压缩后的段落候选。"), {}),
            projectController.errorMessage());
    require(projectController.acceptAiTask(compressionTaskId),
            projectController.errorMessage());
    require(projectController.scriptText() == QStringLiteral("压缩后的段落候选。"),
            QStringLiteral("确认段落压缩候选后没有替换指定段落。"));
    const QVariantList preservedVersions = projectController.scriptVersions();
    require(!preservedVersions.isEmpty()
                && preservedVersions.last().toMap().value(QStringLiteral("versionType")).toString()
                       == QStringLiteral("user_edit")
                && preservedVersions.last().toMap().value(QStringLiteral("text")).toString()
                       == QStringLiteral("剧本候选第二版。"),
            QStringLiteral("采纳覆盖式 AI 改写时没有保存原剧本版本。"));
    const QString multiRewriteTaskId = generationController.prepareTask(
        QStringLiteral("script_rewrite"), QStringLiteral("script_block"), blockId,
        profileId, false, false, true, QStringLiteral("改写这句对白，保持事实。"), 3);
    require(!multiRewriteTaskId.isEmpty(), generationController.errorMessage());
    require(projectController.setAiTaskRunning(multiRewriteTaskId),
            projectController.errorMessage());
    const QStringList rewriteCandidates{
        QStringLiteral("改写候选第一版。"), QStringLiteral("改写候选第二版。"),
        QStringLiteral("改写候选第三版。")};
    QString rewriteCandidatesJson;
    require(sceneforge::project::encodeScriptCandidates(rewriteCandidates, 0,
                                                         &rewriteCandidatesJson),
            QStringLiteral("无法编码剧本改写候选。"));
    require(projectController.completeAiTask(multiRewriteTaskId, rewriteCandidatesJson, {}),
            projectController.errorMessage());
    require(sceneforge::project::encodeScriptCandidates(rewriteCandidates, 2,
                                                         &rewriteCandidatesJson),
            QStringLiteral("无法选择第三个剧本改写候选。"));
    require(projectController.updateAiTaskCandidate(multiRewriteTaskId,
                                                     rewriteCandidatesJson),
            projectController.errorMessage());
    require(projectController.acceptAiTask(multiRewriteTaskId),
            projectController.errorMessage());
    require(projectController.scriptText() == QStringLiteral("改写候选第三版。")
                && projectController.scriptVersions().last().toMap()
                       .value(QStringLiteral("text")).toString()
                       == QStringLiteral("压缩后的段落候选。"),
            QStringLiteral("选定的对白改写候选没有被应用，或原稿未保留为独立版本。"));
    const QString combinedTaskId = generationController.prepareTask(
        QStringLiteral("script_rewrite"), QStringLiteral("script_block"), blockId,
        profileId, false, false, true, QStringLiteral("组合两个段落。"), 3);
    require(!combinedTaskId.isEmpty(), generationController.errorMessage());
    require(projectController.setAiTaskRunning(combinedTaskId),
            projectController.errorMessage());
    const QStringList combinedCandidates{
        QStringLiteral("甲一\n\n甲二"), QStringLiteral("乙一\n\n乙二"),
        QStringLiteral("丙一\n\n丙二")};
    QString combinedCandidateJson;
    require(sceneforge::project::encodeScriptCandidates(combinedCandidates, 0,
                                                         &combinedCandidateJson),
            QStringLiteral("无法编码段落组合候选。"));
    require(projectController.completeAiTask(combinedTaskId, combinedCandidateJson, {}),
            projectController.errorMessage());
    require(sceneforge::project::encodeScriptCandidates(
                combinedCandidates, 2, &combinedCandidateJson, nullptr,
                QStringLiteral("丙一\n\n乙二")),
            QStringLiteral("无法编码用户组合后的采用版本。"));
    require(projectController.updateAiTaskCandidate(combinedTaskId, combinedCandidateJson),
            projectController.errorMessage());
    const QString preAcceptScript = projectController.scriptText();
    require(preAcceptScript == QStringLiteral("改写候选第三版。")
                && projectController.acceptAiTask(combinedTaskId)
                && projectController.scriptText() == QStringLiteral("丙一\n\n乙二"),
            QStringLiteral("合并候选在确认前修改了正式稿，或确认后没有应用组合稿。"));
    const QString combinedProjectPath = projectController.currentProjectPath();
    require(projectController.closeProject(), projectController.errorMessage());
    require(projectController.openProject(combinedProjectPath), projectController.errorMessage());
    QStringList restoredCombinedCandidates;
    int restoredCombinedSelection = -1;
    QString restoredAcceptedText;
    require(sceneforge::project::decodeScriptCandidates(
                projectController.aiTask(combinedTaskId)
                    .value(QStringLiteral("candidateText")).toString(),
                &restoredCombinedCandidates, &restoredCombinedSelection, nullptr,
                &restoredAcceptedText)
                && restoredAcceptedText == QStringLiteral("丙一\n\n乙二")
                && restoredCombinedCandidates == combinedCandidates
                && restoredCombinedSelection == 2
                && projectController.scriptText() == QStringLiteral("丙一\n\n乙二"),
            QStringLiteral("重开作品后多候选组合稿、原候选或选择没有恢复。"));
    const QVariantMap rewriteContext = projectController.aiContextSnapshot(
        QStringLiteral("script_block"), blockId, false, false, true);
    const QVariantMap rewriteTaskValues{
        {QStringLiteral("operation"), QStringLiteral("script_rewrite")},
        {QStringLiteral("targetType"), QStringLiteral("script_block")},
        {QStringLiteral("targetId"), blockId},
        {QStringLiteral("targetTitle"), QStringLiteral("剧本段落")},
        {QStringLiteral("serviceId"), profileId},
        {QStringLiteral("serviceName"), profile.name},
        {QStringLiteral("endpoint"), profile.endpoint},
        {QStringLiteral("model"), profile.model},
        {QStringLiteral("contextManifestJson"), rewriteContext.value(
             QStringLiteral("contextManifestJson"))},
        {QStringLiteral("systemInstruction"), QStringLiteral("只返回改写段落。")},
        {QStringLiteral("requestText"), QStringLiteral("改写选中段落。")}
    };
    const QString rewriteTaskId = projectController.prepareAiTask(rewriteTaskValues);
    require(!rewriteTaskId.isEmpty(), projectController.errorMessage());
    require(projectController.setAiTaskRunning(rewriteTaskId), projectController.errorMessage());
    require(projectController.completeAiTask(rewriteTaskId, QStringLiteral("AI 改写版。"), {}),
            projectController.errorMessage());
    require(projectController.updateScriptBlock(blockId, QStringLiteral("unstructured_text"),
                                                QStringLiteral("用户后来修改的正式段落。")),
            projectController.errorMessage());
    const int versionCountBeforeStaleSave = projectController.scriptVersions().size();
    require(!projectController.saveAiTaskCandidateAsScriptVersion(rewriteTaskId)
                && projectController.scriptVersions().size() == versionCountBeforeStaleSave,
            QStringLiteral("目标已变化后，过期的改写候选仍被另存为独立版本。"));
    require(!projectController.acceptAiTask(rewriteTaskId)
                && projectController.scriptText() == QStringLiteral("用户后来修改的正式段落。"),
            QStringLiteral("过期的 AI 改写候选覆盖了任务准备后更新的正式段落。"));
    projectController.retrySave();
    QEventLoop saveWait;
    QTimer::singleShot(1200, &saveWait, &QEventLoop::quit);
    saveWait.exec();
    require(projectController.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("改写保护测试的正式内容未能保存。"));
    const QString interruptedTaskId = generationController.prepareTask(
        QStringLiteral("script_generate"), QStringLiteral("episode"), episodeId,
        profileId, false, false, false, QStringLiteral("中断恢复测试。"));
    require(!interruptedTaskId.isEmpty(), generationController.errorMessage());
    require(projectController.setAiTaskRunning(interruptedTaskId),
            projectController.errorMessage());
    require(projectController.closeProject(), projectController.errorMessage());
    require(projectController.openProject(acceptedPath), projectController.errorMessage());
    require(projectController.aiTask(interruptedTaskId)
                .value(QStringLiteral("status")).toString() == QStringLiteral("interrupted")
                && projectController.scriptText()
                       == QStringLiteral("用户后来修改的正式段落。"),
            QStringLiteral("重开时没有把运行中断的任务标记为结果未知，或影响了正式剧本。"));
    const QString retryTaskId = generationController.prepareRetry(interruptedTaskId);
    require(!retryTaskId.isEmpty()
                && retryTaskId != interruptedTaskId
                && projectController.aiTask(retryTaskId)
                       .value(QStringLiteral("status")).toString()
                       == QStringLiteral("awaiting_confirmation"),
            QStringLiteral("中断任务没有保留原记录并创建需重新确认的新尝试。"));
    require(generationController.cancelTask(retryTaskId), generationController.errorMessage());
    const QVariantMap projectContext = projectController.aiContextSnapshot(
        QStringLiteral("project"), projectController.currentProjectId(), false, false, false);
    const QVariantMap storyTaskValues{
        {QStringLiteral("operation"), QStringLiteral("story_development")},
        {QStringLiteral("targetType"), QStringLiteral("project")},
        {QStringLiteral("targetId"), projectController.currentProjectId()},
        {QStringLiteral("targetTitle"), projectController.currentProjectName()},
        {QStringLiteral("serviceId"), profileId},
        {QStringLiteral("serviceName"), profile.name},
        {QStringLiteral("endpoint"), profile.endpoint},
        {QStringLiteral("model"), profile.model},
        {QStringLiteral("contextManifestJson"), projectContext.value(
             QStringLiteral("contextManifestJson"))},
        {QStringLiteral("systemInstruction"), QStringLiteral("只返回指定字段的 JSON。")},
        {QStringLiteral("requestText"), QStringLiteral("整理全剧故事发展字段。")}
    };
    const QString storyTaskId = projectController.prepareAiTask(storyTaskValues);
    require(!storyTaskId.isEmpty(), projectController.errorMessage());
    require(projectController.setAiTaskRunning(storyTaskId), projectController.errorMessage());
    require(projectController.completeAiTask(
                storyTaskId,
                QStringLiteral("{\"theme\":\"创作需要勇气。\",\"centralConflict\":\"主角必须在午夜前找到失踪的剧本。\"}"),
                {}), projectController.errorMessage());
    require(projectController.currentProjectStory()
                .value(QStringLiteral("centralConflict")).toString().isEmpty()
                && projectController.currentProjectStory()
                       .value(QStringLiteral("theme")).toString().isEmpty(),
            QStringLiteral("故事发展候选在用户确认前改写了正式创作简报。"));
    const QString selectedStoryFields = QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("values"), QJsonObject{
             {QStringLiteral("theme"), QStringLiteral("创作需要勇气。")},
             {QStringLiteral("centralConflict"),
              QStringLiteral("主角必须在午夜前找到失踪的剧本。")}}},
        {QStringLiteral("selected"), QJsonArray{QStringLiteral("theme")}}
    }).toJson(QJsonDocument::Compact));
    require(projectController.updateAiTaskCandidate(storyTaskId, selectedStoryFields),
            projectController.errorMessage());
    require(projectController.acceptAiTask(storyTaskId), projectController.errorMessage());
    require(projectController.currentProjectStory()
                .value(QStringLiteral("centralConflict")).toString().isEmpty()
                && projectController.currentProjectStory()
                       .value(QStringLiteral("theme")).toString()
                       == QStringLiteral("创作需要勇气。"),
            QStringLiteral("确认故事字段候选时没有只写入用户勾选的字段。"));
    const QString projectId = projectController.currentProjectId();
    const QString bibleTaskId = generationController.prepareTask(
        QStringLiteral("bible_suggest"), QStringLiteral("project"), projectId,
        profileId, true, true, false, QStringLiteral("建议关键人物和地点。"));
    require(!bibleTaskId.isEmpty(), projectController.errorMessage());
    require(projectController.aiTask(bibleTaskId)
                .value(QStringLiteral("systemInstruction")).toString()
                .contains(QStringLiteral("certaintyStatus"))
                && projectController.aiTask(bibleTaskId)
                       .value(QStringLiteral("requestText")).toString()
                       .contains(QStringLiteral("建议关键人物和地点。")),
            QStringLiteral("创作圣经建议没有构建用户可查看的结构化候选请求。"));
    require(projectController.setAiTaskRunning(bibleTaskId), projectController.errorMessage());
    const QJsonArray bibleItems{
        QJsonObject{
            {QStringLiteral("selected"), true},
            {QStringLiteral("type"), QStringLiteral("character")},
            {QStringLiteral("name"), QStringLiteral("林岚")},
            {QStringLiteral("description"), QStringLiteral("试图找回被删掉的故事。")},
            {QStringLiteral("certaintyStatus"), QStringLiteral("provisional")},
            {QStringLiteral("details"), QJsonObject{
                 {QStringLiteral("goal"), QStringLiteral("找回故事原稿。")}}}
        },
        QJsonObject{
            {QStringLiteral("selected"), false},
            {QStringLiteral("type"), QStringLiteral("location")},
            {QStringLiteral("name"), QStringLiteral("深夜编辑室")},
            {QStringLiteral("description"), QStringLiteral("主要工作地点。")},
            {QStringLiteral("certaintyStatus"), QStringLiteral("undecided")},
            {QStringLiteral("details"), QJsonObject{
                 {QStringLiteral("function"), QStringLiteral("故事发生地点。")}}}
        }
    };
    const QString bibleCandidateJson = QString::fromUtf8(QJsonDocument(
        QJsonObject{{QStringLiteral("items"), bibleItems}}).toJson(QJsonDocument::Compact));
    const qsizetype bibleCountBefore = projectController.bibleItems().size();
    require(projectController.completeAiTask(bibleTaskId, bibleCandidateJson, {}),
            projectController.errorMessage());
    require(projectController.bibleItems().size() == bibleCountBefore,
            QStringLiteral("创作圣经候选在人工确认前写入了正式设定。"));
    require(!projectController.acceptAiTask(bibleTaskId)
                && projectController.bibleItems().size() == bibleCountBefore,
            QStringLiteral("创作圣经候选绕过逐项选择直接写入。"));
    QJsonArray noSelectionItems = bibleItems;
    for (qsizetype index = 0; index < noSelectionItems.size(); ++index) {
        QJsonObject item = noSelectionItems[index].toObject();
        item.insert(QStringLiteral("selected"), false);
        noSelectionItems.replace(index, item);
    }
    const QString noSelectionJson = QString::fromUtf8(QJsonDocument(
        QJsonObject{{QStringLiteral("items"), noSelectionItems}})
                                                   .toJson(QJsonDocument::Compact));
    require(!projectController.acceptAiBibleCandidates(bibleTaskId, noSelectionJson)
                && projectController.bibleItems().size() == bibleCountBefore,
            QStringLiteral("没有勾选候选时仍修改了正式创作圣经。"));
    require(projectController.acceptAiBibleCandidates(bibleTaskId, bibleCandidateJson),
            projectController.errorMessage());
    const QVariantList acceptedBibleItems = projectController.bibleItems();
    require(acceptedBibleItems.size() == bibleCountBefore + 1
                && acceptedBibleItems.last().toMap().value(QStringLiteral("name")).toString()
                       == QStringLiteral("林岚")
                && acceptedBibleItems.last().toMap().value(QStringLiteral("certaintyStatus")).toString()
                       == QStringLiteral("provisional")
                && acceptedBibleItems.last().toMap().value(QStringLiteral("details")).toMap()
                       .value(QStringLiteral("goal")).toString()
                       == QStringLiteral("找回故事原稿。")
                && projectController.aiTask(bibleTaskId).value(QStringLiteral("status")).toString()
                       == QStringLiteral("accepted"),
            QStringLiteral("逐项确认没有只采纳选定设定，或丢失暂定状态和详情。"));
    const QString bibleSavedPath = projectController.currentProjectPath();
    require(projectController.closeProject(), projectController.errorMessage());
    require(projectController.openProject(bibleSavedPath), projectController.errorMessage());
    require(projectController.bibleItems().size() == bibleCountBefore + 1
                && projectController.aiTask(bibleTaskId).value(QStringLiteral("status")).toString()
                       == QStringLiteral("accepted"),
            QStringLiteral("已采纳的创作圣经候选重开后未恢复。"));

    QVariantMap targetBibleItem = projectController.bibleItems().last().toMap();
    const QString targetBibleItemId = targetBibleItem.value(QStringLiteral("id")).toString();
    require(projectController.setBibleItemCertaintyStatus(targetBibleItemId,
                                                          QStringLiteral("confirmed")),
            projectController.errorMessage());
    require(projectController.addBibleItemReference(
                targetBibleItemId, QStringLiteral("人物原型笔记"), QStringLiteral("来源")),
            projectController.errorMessage());
    require(projectController.setEpisodeBibleReference(targetBibleItemId, true),
            projectController.errorMessage());
    targetBibleItem = projectController.bibleItems().last().toMap();
    const QString originalBibleName = targetBibleItem.value(QStringLiteral("name")).toString();
    const QString originalBibleDescription =
        targetBibleItem.value(QStringLiteral("description")).toString();
    const QString originalBibleGoal = targetBibleItem.value(QStringLiteral("details")).toMap()
                                          .value(QStringLiteral("goal")).toString();
    const QVariantList originalBibleReferences =
        targetBibleItem.value(QStringLiteral("references")).toList();

    const QVariantMap bibleUpdateContext = projectController.aiContextSnapshot(
        QStringLiteral("bible_item"), targetBibleItemId, false, true, false);
    require(bibleUpdateContext.value(QStringLiteral("valid")).toBool(),
            bibleUpdateContext.value(QStringLiteral("error")).toString());
    const QJsonObject bibleUpdateManifest = QJsonDocument::fromJson(
        bibleUpdateContext.value(QStringLiteral("contextManifestJson")).toString().toUtf8())
                                                .object();
    require(bibleUpdateManifest.value(QStringLiteral("sources")).toArray().size() == 1
                && bibleUpdateManifest.value(QStringLiteral("targetBibleItemSnapshot"))
                       .toObject().value(QStringLiteral("id")).toString()
                       == targetBibleItemId,
            QStringLiteral("修改创作圣经的上下文未限制为所选条目或未保存原始快照。"));
    require(bibleUpdateManifest.value(QStringLiteral("targetBibleItemSha256")).toString()
                == bibleFingerprintFromMap(targetBibleItem),
            QStringLiteral("创作圣经任务快照与当前条目指纹不一致。"));

    const QString rejectedUpdateTaskId = generationController.prepareTask(
        QStringLiteral("bible_update_suggest"), QStringLiteral("bible_item"),
        targetBibleItemId, profileId, false, true, false,
        QStringLiteral("建议调整角色动机。"), 1,
        QStringLiteral("用户补充：她曾经在寻找姐姐时被误导。"));
    require(!rejectedUpdateTaskId.isEmpty(), generationController.errorMessage());
    const QVariantMap rejectedUpdateTask = projectController.aiTask(rejectedUpdateTaskId);
    require(rejectedUpdateTask.value(QStringLiteral("systemInstruction")).toString()
                .contains(QStringLiteral("items"))
                && rejectedUpdateTask.value(QStringLiteral("requestText")).toString()
                       .contains(originalBibleName)
                && rejectedUpdateTask.value(QStringLiteral("requestText")).toString()
                       .contains(QStringLiteral("用户补充上下文：\n用户补充：她曾经在寻找姐姐时被误导。"))
                && QJsonDocument::fromJson(rejectedUpdateTask.value(
                       QStringLiteral("contextManifestJson")).toString().toUtf8())
                       .object().value(QStringLiteral("sources")).toArray().last().toObject()
                       .value(QStringLiteral("type")).toString()
                       == QStringLiteral("user_supplied_context"),
            QStringLiteral("既有设定修改操作没有生成结构化请求或目标上下文。"));
    require(projectController.setAiTaskRunning(rejectedUpdateTaskId),
            projectController.errorMessage());
    const QString rawBibleUpdateCandidate = QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("items"), QJsonArray{QJsonObject{
             {QStringLiteral("name"), QStringLiteral("未确认的新名字")},
             {QStringLiteral("description"), QStringLiteral("未确认的新说明")},
             {QStringLiteral("details"), QJsonObject{
                  {QStringLiteral("goal"), QStringLiteral("未确认的新目标")}}}}}}
    }).toJson(QJsonDocument::Compact));
    require(projectController.completeAiTask(rejectedUpdateTaskId,
                                             rawBibleUpdateCandidate, {}),
            projectController.errorMessage());
    const QString noSelectedBibleFields = QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("items"), QJsonArray{QJsonObject{
             {QStringLiteral("targetId"), targetBibleItemId},
             {QStringLiteral("selected"), true},
             {QStringLiteral("name"), QStringLiteral("未确认的新名字")},
             {QStringLiteral("selectedFields"), QJsonArray{}}}}}
    }).toJson(QJsonDocument::Compact));
    require(!projectController.acceptAiBibleUpdateCandidates(rejectedUpdateTaskId,
                                                              noSelectedBibleFields)
                && projectController.bibleItems().last().toMap()
                       .value(QStringLiteral("name")).toString() == originalBibleName,
            QStringLiteral("没有勾选字段时创作圣经条目被修改。"));
    require(!projectController.acceptAiTask(rejectedUpdateTaskId)
                && projectController.bibleItems().last().toMap()
                       .value(QStringLiteral("description")).toString()
                       == originalBibleDescription,
            QStringLiteral("既有设定修改绕过了专用候选确认流程。"));
    require(projectController.rejectAiTask(rejectedUpdateTaskId),
            projectController.errorMessage());
    require(projectController.bibleItems().last().toMap()
                .value(QStringLiteral("details")).toMap()
                .value(QStringLiteral("goal")).toString() == originalBibleGoal,
            QStringLiteral("拒绝修改候选后正式创作圣经发生变化。"));
    const QString acceptedUpdateTaskId = generationController.prepareTask(
        QStringLiteral("bible_update_suggest"), QStringLiteral("bible_item"),
        targetBibleItemId, profileId, false, true, false,
        QStringLiteral("调整角色说明和目标。"));
    require(!acceptedUpdateTaskId.isEmpty(), generationController.errorMessage());
    require(projectController.setAiTaskRunning(acceptedUpdateTaskId),
            projectController.errorMessage());
    require(projectController.completeAiTask(acceptedUpdateTaskId,
                                             rawBibleUpdateCandidate, {}),
            projectController.errorMessage());
    const QString selectedBibleUpdate = QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("items"), QJsonArray{QJsonObject{
             {QStringLiteral("targetId"), targetBibleItemId},
             {QStringLiteral("selected"), true},
             {QStringLiteral("name"), QStringLiteral("用户未选择的新名字")},
             {QStringLiteral("description"), QStringLiteral("用户确认的新说明")},
             {QStringLiteral("details"), QJsonObject{
                  {QStringLiteral("goal"), QStringLiteral("用户确认的新目标")}}},
             {QStringLiteral("selectedFields"), QJsonArray{
                  QStringLiteral("description"), QStringLiteral("details.goal")}}}}}
    }).toJson(QJsonDocument::Compact));
    require(projectController.updateAiTaskCandidate(acceptedUpdateTaskId,
                                                     selectedBibleUpdate),
            projectController.errorMessage());
    const qsizetype bibleHistoryBeforeUpdate =
        projectController.bibleItemHistory(targetBibleItemId).size();
    require(projectController.acceptAiBibleUpdateCandidates(acceptedUpdateTaskId,
                                                             selectedBibleUpdate),
            projectController.errorMessage());
    targetBibleItem = projectController.bibleItems().last().toMap();
    require(targetBibleItem.value(QStringLiteral("id")).toString() == targetBibleItemId
                && targetBibleItem.value(QStringLiteral("name")).toString() == originalBibleName
                && targetBibleItem.value(QStringLiteral("description")).toString()
                       == QStringLiteral("用户确认的新说明")
                && targetBibleItem.value(QStringLiteral("details")).toMap()
                       .value(QStringLiteral("goal")).toString()
                       == QStringLiteral("用户确认的新目标")
                && targetBibleItem.value(QStringLiteral("certaintyStatus")).toString()
                       == QStringLiteral("confirmed")
                && targetBibleItem.value(QStringLiteral("references")).toList()
                       == originalBibleReferences
                && targetBibleItem.value(QStringLiteral("referenced")).toBool()
                && projectController.bibleItemHistory(targetBibleItemId).size()
                       > bibleHistoryBeforeUpdate
                && projectController.aiTask(acceptedUpdateTaskId)
                       .value(QStringLiteral("status")).toString()
                       == QStringLiteral("accepted"),
            QStringLiteral("确认修改未按选择更新字段，或丢失身份、状态、引用与历史。"));

    const QString staleUpdateTaskId = generationController.prepareTask(
        QStringLiteral("bible_update_suggest"), QStringLiteral("bible_item"),
        targetBibleItemId, profileId, false, true, false,
        QStringLiteral("检查设定更新期间变化保护。"));
    require(!staleUpdateTaskId.isEmpty(), generationController.errorMessage());
    require(projectController.setAiTaskRunning(staleUpdateTaskId),
            projectController.errorMessage());
    require(projectController.completeAiTask(staleUpdateTaskId,
                                             rawBibleUpdateCandidate, {}),
            projectController.errorMessage());
    const QString staleBibleDescription = QStringLiteral("生成后用户修改的说明。");
    require(projectController.updateBibleItem(targetBibleItemId, originalBibleName,
                                              staleBibleDescription),
            projectController.errorMessage());
    require(!projectController.acceptAiBibleUpdateCandidates(staleUpdateTaskId,
                                                              selectedBibleUpdate)
                && projectController.bibleItems().last().toMap()
                       .value(QStringLiteral("description")).toString()
                       == staleBibleDescription,
            QStringLiteral("过期创作圣经修改候选覆盖了生成后用户所做的修改。"));

    const QString immutableUpdateTaskId = generationController.prepareTask(
        QStringLiteral("bible_update_suggest"), QStringLiteral("bible_item"),
        targetBibleItemId, profileId, false, true, false,
        QStringLiteral("验证禁止改变状态保护。"));
    require(!immutableUpdateTaskId.isEmpty(), generationController.errorMessage());
    require(projectController.setAiTaskRunning(immutableUpdateTaskId),
            projectController.errorMessage());
    require(projectController.completeAiTask(immutableUpdateTaskId,
                                             rawBibleUpdateCandidate, {}),
            projectController.errorMessage());
    require(projectController.setBibleItemCertaintyStatus(targetBibleItemId,
                                                          QStringLiteral("immutable")),
            projectController.errorMessage());
    require(!projectController.acceptAiBibleUpdateCandidates(immutableUpdateTaskId,
                                                              selectedBibleUpdate)
                && projectController.bibleItems().last().toMap()
                       .value(QStringLiteral("description")).toString()
                       == staleBibleDescription,
            QStringLiteral("禁止改变的设定仍能被 AI 修改候选覆盖。"));
    const QString acceptedUpdateCandidatePath = projectController.currentProjectPath();
    projectController.retrySave();
    QEventLoop bibleSaveWait;
    QObject::connect(&projectController,
                     &sceneforge::project::ProjectController::saveStatusChanged,
                     &bibleSaveWait, [&] {
        if (projectController.saveStatus() == QStringLiteral("saved")) {
            bibleSaveWait.quit();
        }
    });
    if (projectController.saveStatus() != QStringLiteral("saved")) {
        QTimer::singleShot(3000, &bibleSaveWait, &QEventLoop::quit);
        bibleSaveWait.exec();
    }
    require(projectController.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("保存创作圣经修改保护测试数据失败：")
                + projectController.errorMessage());
    require(projectController.closeProject(), projectController.errorMessage());
    require(projectController.openProject(acceptedUpdateCandidatePath),
            projectController.errorMessage());
    const QVariantMap restoredUpdateTask = projectController.aiTask(acceptedUpdateTaskId);
    const QJsonObject restoredUpdateCandidate = QJsonDocument::fromJson(
        restoredUpdateTask.value(QStringLiteral("candidateText")).toString().toUtf8())
                                                  .object();
    require(restoredUpdateTask.value(QStringLiteral("status")).toString()
                == QStringLiteral("accepted")
                && restoredUpdateCandidate.value(QStringLiteral("items")).toArray().first()
                       .toObject().value(QStringLiteral("selectedFields")).toArray().size() == 2
                && projectController.bibleItems().last().toMap()
                       .value(QStringLiteral("description")).toString()
                       == staleBibleDescription
                && projectController.bibleItems().last().toMap()
                       .value(QStringLiteral("certaintyStatus")).toString()
                       == QStringLiteral("immutable"),
            QStringLiteral("重开后已采纳候选、字段选择或创作圣经状态未恢复。"));
    require(projectController.closeProject(), projectController.errorMessage());
}

void verifyBibleUpdateMultipleCandidateFlow()
{
    const auto candidateContent = [](const QString& description, const QString& goal) {
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {QStringLiteral("items"), QJsonArray{QJsonObject{
                {QStringLiteral("description"), description},
                {QStringLiteral("details"), QJsonObject{
                    {QStringLiteral("goal"), goal}}}}}}
        }).toJson(QJsonDocument::Compact));
    };
    QJsonArray choices;
    for (const auto& [description, goal] : {
             std::pair{QStringLiteral("第一方案说明"), QStringLiteral("第一方案目标。")},
             std::pair{QStringLiteral("第二方案说明"), QStringLiteral("第二方案目标。")},
             std::pair{QStringLiteral("第三方案说明"), QStringLiteral("第三方案目标。")}}) {
        choices.append(QJsonObject{{QStringLiteral("message"), QJsonObject{
            {QStringLiteral("role"), QStringLiteral("assistant")},
            {QStringLiteral("content"), candidateContent(description, goal)}}}});
    }
    MockHttpServer server(200, QJsonDocument(QJsonObject{
        {QStringLiteral("choices"), choices}
    }).toJson(QJsonDocument::Compact));
    QTemporaryDir temporaryDirectory;
    require(temporaryDirectory.isValid(), QStringLiteral("无法创建多候选测试目录。"));

    MemoryCredentialStore credentialStore;
    sceneforge::ai::AiSettingsController settingsController(&credentialStore);
    const sceneforge::ai::AiServiceProfile profile = profileFor(server);
    const QString profileId = settingsController.upsertProfile(profile.toVariantMap(false));
    require(!profileId.isEmpty(), QStringLiteral("多候选服务配置失败：")
                                      + settingsController.errorMessage());
    require(settingsController.saveCredential(profileId, QStringLiteral("test-only-secret")),
            QStringLiteral("多候选凭据保存失败：") + settingsController.errorMessage());

    sceneforge::project::ProjectController projectController;
    require(projectController.setLibraryRoot(
                QDir(temporaryDirectory.path()).filePath(QStringLiteral("作品库"))),
            QStringLiteral("设置多候选测试作品库失败：") + projectController.errorMessage());
    require(projectController.createProject(QStringLiteral("创作圣经多候选测试"), QString()),
            QStringLiteral("创建多候选测试作品失败：") + projectController.errorMessage());
    require(projectController.createBibleItem(QStringLiteral("character"),
                                              QStringLiteral("沈遥"),
                                              QStringLiteral("寻找失踪的姐姐。")),
            QStringLiteral("创建待修改设定失败：") + projectController.errorMessage());
    const QVariantMap original = projectController.bibleItems().first().toMap();
    const QString itemId = original.value(QStringLiteral("id")).toString();
    require(projectController.updateBibleItemDetail(itemId, QStringLiteral("goal"),
                                                    QStringLiteral("找到姐姐。")),
            QStringLiteral("设置原始角色目标失败：") + projectController.errorMessage());

    sceneforge::ai::AiGenerationController generationController(
        &projectController, &settingsController, &credentialStore);
    const QString taskId = generationController.prepareTask(
        QStringLiteral("bible_update_suggest"), QStringLiteral("bible_item"), itemId,
        profileId, false, true, false, QStringLiteral("给出不同的角色动机方案。"), 3);
    require(!taskId.isEmpty(), generationController.errorMessage());
    const QVariantMap preparedTask = projectController.aiTask(taskId);
    require(preparedTask.value(QStringLiteral("contextManifestJson")).toString()
                .contains(QStringLiteral("\"candidateCount\":3"))
                && preparedTask.value(QStringLiteral("systemInstruction")).toString()
                       .contains(QStringLiteral("独立方案"))
                && server.request().isEmpty(),
            QStringLiteral("既有设定多候选请求预览未记录方案数，或预览时提前发送。"));

    bool taskFinished = false;
    QEventLoop wait;
    QObject::connect(&projectController,
                     &sceneforge::project::ProjectController::aiGenerationTasksChanged,
                     &wait, [&] {
        if (projectController.aiTask(taskId).value(QStringLiteral("status")).toString()
            == QStringLiteral("succeeded")) {
            taskFinished = true;
            wait.quit();
        }
    });
    require(generationController.submitPreparedTask(taskId), generationController.errorMessage());
    if (!taskFinished) {
        QTimer::singleShot(5000, &wait, &QEventLoop::quit);
        wait.exec();
    }
    const QByteArray request = server.request();
    const qsizetype bodyStart = request.indexOf("\r\n\r\n") + 4;
    const QJsonObject requestBody = QJsonDocument::fromJson(request.mid(bodyStart)).object();
    const QJsonObject result = QJsonDocument::fromJson(
        projectController.aiTask(taskId).value(QStringLiteral("candidateText")).toString()
            .toUtf8()).object();
    const QJsonArray returnedCandidates = result.value(QStringLiteral("candidates")).toArray();
    require(taskFinished && requestBody.value(QStringLiteral("n")).toInt() == 3
                && result.value(QStringLiteral("format")).toString()
                       == QStringLiteral("sceneforge.bible-update-candidates.v1")
                && returnedCandidates.size() == 3
                && returnedCandidates.at(1).toString().contains(QStringLiteral("第二方案说明"))
                && projectController.bibleItems().first().toMap()
                       .value(QStringLiteral("description")).toString()
                       == QStringLiteral("寻找失踪的姐姐。"),
            QStringLiteral("既有设定多候选未被封装保存，或服务返回候选时修改了正式设定。"));

    const QString selectedCandidate = QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("items"), QJsonArray{QJsonObject{
            {QStringLiteral("targetId"), itemId},
            {QStringLiteral("selected"), true},
            {QStringLiteral("description"), QStringLiteral("第二方案说明")},
            {QStringLiteral("details"), QJsonObject{
                {QStringLiteral("goal"), QStringLiteral("第二方案目标。")}}},
            {QStringLiteral("selectedFields"), QJsonArray{
                QStringLiteral("description"), QStringLiteral("details.goal")}}}}}
    }).toJson(QJsonDocument::Compact));
    require(projectController.updateAiTaskCandidate(taskId, selectedCandidate),
            projectController.errorMessage());
    require(projectController.acceptAiBibleUpdateCandidates(taskId, selectedCandidate),
            projectController.errorMessage());
    const QVariantMap accepted = projectController.bibleItems().first().toMap();
    require(accepted.value(QStringLiteral("id")).toString() == itemId
                && accepted.value(QStringLiteral("name")).toString()
                       == original.value(QStringLiteral("name")).toString()
                && accepted.value(QStringLiteral("description")).toString()
                       == QStringLiteral("第二方案说明")
                && accepted.value(QStringLiteral("details")).toMap()
                       .value(QStringLiteral("goal")).toString()
                       == QStringLiteral("第二方案目标。"),
            QStringLiteral("确认第二个方案后没有只更新该方案选中的字段。"));

    const QString projectPath = projectController.currentProjectPath();
    projectController.retrySave();
    if (projectController.saveStatus() != QStringLiteral("saved")) {
        QEventLoop saveWait;
        QObject::connect(&projectController,
                         &sceneforge::project::ProjectController::saveStatusChanged,
                         &saveWait, [&] {
            if (projectController.saveStatus() == QStringLiteral("saved")) {
                saveWait.quit();
            }
        });
        QTimer::singleShot(3000, &saveWait, &QEventLoop::quit);
        saveWait.exec();
    }
    require(projectController.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("保存多候选采纳结果失败：") + projectController.errorMessage());
    require(projectController.closeProject(), projectController.errorMessage());
    require(projectController.openProject(projectPath), projectController.errorMessage());
    const QVariantMap restoredTask = projectController.aiTask(taskId);
    require(restoredTask.value(QStringLiteral("status")).toString()
                == QStringLiteral("accepted")
                && restoredTask.value(QStringLiteral("candidateText")).toString()
                       == selectedCandidate
                && projectController.bibleItems().first().toMap()
                       .value(QStringLiteral("description")).toString()
                       == QStringLiteral("第二方案说明"),
            QStringLiteral("重开作品后没有恢复选定的设定候选。"));
    require(projectController.closeProject(), projectController.errorMessage());
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SceneForgeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("AiTextAdapterTests"));
    try {
        verifyStructuredResponseAndRequestScope();
        verifyMultipleCandidateResponse();
        verifyAuthenticationFailure();
        verifyCancellation();
        verifyStructuredScriptAnalysisPreview();
        verifyProjectScriptGenerationFlow();
        verifyBatchScriptRewriteGenerationFlow();
        verifySingleCandidateSceneReviewEnvelope();
        verifyScriptBlockGenerationReplacesSelectedTarget();
        verifyGenerationConfirmationAndCandidateFlow();
        verifyBibleUpdateMultipleCandidateFlow();
        std::cout << "PASS: OpenAI-compatible text requests, structured responses, errors, and cancellation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

#include "aitextadapter_tests.moc"
