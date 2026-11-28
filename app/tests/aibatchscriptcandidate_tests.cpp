#include "../src/project/projectcontroller.h"
#include "../src/project/scriptcandidatecodec.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>
#include <stdexcept>

namespace {

using sceneforge::project::ProjectController;

void require(bool condition, const QString& message)
{
    if (!condition) {
        throw std::runtime_error(message.toStdString());
    }
}

QJsonArray targetBlocks(const QVariantMap& snapshot)
{
    const QJsonObject manifest = QJsonDocument::fromJson(
        snapshot.value(QStringLiteral("contextManifestJson")).toString().toUtf8()).object();
    return manifest.value(QStringLiteral("targetScriptBlocks")).toArray();
}

QString rewriteCandidate(const QJsonArray& targets, const QString& prefix)
{
    QJsonArray blocks;
    for (const QJsonValue& value : targets) {
        const QJsonObject target = value.toObject();
        blocks.append(QJsonObject{
            {QStringLiteral("blockId"), target.value(QStringLiteral("blockId"))},
            {QStringLiteral("text"), prefix + target.value(
                 QStringLiteral("blockId")).toString()}
        });
    }
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("format"), QStringLiteral("sceneforge.script-block-rewrites.v1")},
        {QStringLiteral("blocks"), blocks}
    }).toJson(QJsonDocument::Compact));
}

QString prepareBatchTask(ProjectController& controller,
                         const QString& targetType,
                         const QString& targetId,
                         int candidateCount)
{
    const QVariantMap snapshot = controller.aiContextSnapshot(
        targetType, targetId, false, false, true);
    require(snapshot.value(QStringLiteral("valid")).toBool(),
            QStringLiteral("建立批量改写上下文失败：")
                + snapshot.value(QStringLiteral("error")).toString());
    QJsonObject manifest = QJsonDocument::fromJson(
        snapshot.value(QStringLiteral("contextManifestJson")).toString().toUtf8()).object();
    manifest.insert(QStringLiteral("candidateCount"), candidateCount);
    const QString taskId = controller.prepareAiTask({
        {QStringLiteral("operation"), QStringLiteral("script_rewrite")},
        {QStringLiteral("targetType"), targetType},
        {QStringLiteral("targetId"), targetId},
        {QStringLiteral("targetTitle"), snapshot.value(QStringLiteral("targetTitle"))},
        {QStringLiteral("serviceId"), QStringLiteral("test-service")},
        {QStringLiteral("serviceName"), QStringLiteral("测试服务")},
        {QStringLiteral("endpoint"), QStringLiteral("http://127.0.0.1:8080/v1")},
        {QStringLiteral("model"), QStringLiteral("test-model")},
        {QStringLiteral("contextManifestJson"), QString::fromUtf8(
             QJsonDocument(manifest).toJson(QJsonDocument::Compact))},
        {QStringLiteral("systemInstruction"), QStringLiteral("按段落清单改写。")},
        {QStringLiteral("requestText"), snapshot.value(QStringLiteral("contextText"))}
    });
    require(!taskId.isEmpty(), QStringLiteral("准备批量改写任务失败：")
                                 + controller.errorMessage());
    require(controller.setAiTaskRunning(taskId), controller.errorMessage());
    return taskId;
}

QString scriptTextFor(ProjectController& controller, const QString& episodeId)
{
    require(controller.selectEpisode(episodeId), controller.errorMessage());
    return controller.scriptText();
}

int scriptVersionCountFor(ProjectController& controller, const QString& episodeId)
{
    require(controller.selectEpisode(episodeId), controller.errorMessage());
    return controller.scriptVersions().size();
}

void verifyBatchRewriteScopesAndAtomicity()
{
    QTemporaryDir temporaryDirectory;
    require(temporaryDirectory.isValid(), QStringLiteral("无法创建批量改写测试目录。"));

    ProjectController controller;
    require(controller.setLibraryRoot(QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("作品库"))), controller.errorMessage());
    require(controller.createProject(QStringLiteral("多集批量改写"), {},
                                     QStringLiteral("series")), controller.errorMessage());
    const QString firstEpisodeId = controller.currentEpisodeId();

    require(controller.createScene(), controller.errorMessage());
    const QString firstSceneId = controller.scenes().first().toMap()
                                     .value(QStringLiteral("id")).toString();
    QVariantList firstEpisodeBlocks = controller.scriptBlocks();
    const QString firstBlockId = firstEpisodeBlocks.first().toMap()
                                     .value(QStringLiteral("id")).toString();
    require(controller.setScriptBlockScene(firstBlockId, firstSceneId), controller.errorMessage());
    require(controller.updateScriptBlock(firstBlockId, QStringLiteral("action"),
                                         QStringLiteral("第一集场景动作原稿。")),
            controller.errorMessage());
    require(controller.createScriptBlock(QStringLiteral("dialogue"), firstSceneId),
            controller.errorMessage());
    firstEpisodeBlocks = controller.scriptBlocks();
    const QString secondBlockId = firstEpisodeBlocks.last().toMap()
                                      .value(QStringLiteral("id")).toString();
    require(controller.updateScriptBlock(secondBlockId, QStringLiteral("dialogue"),
                                         QStringLiteral("第一集场景对白原稿。")),
            controller.errorMessage());
    const QString firstEpisodeOriginal = controller.scriptText();

    require(controller.createEpisode(QStringLiteral("第二集")), controller.errorMessage());
    const QString secondEpisodeId = controller.currentEpisodeId();
    require(controller.createScene(), controller.errorMessage());
    const QString secondSceneId = controller.scenes().first().toMap()
                                      .value(QStringLiteral("id")).toString();
    const QString thirdBlockId = controller.scriptBlocks().first().toMap()
                                     .value(QStringLiteral("id")).toString();
    require(controller.setScriptBlockScene(thirdBlockId, secondSceneId), controller.errorMessage());
    require(controller.updateScriptBlock(thirdBlockId, QStringLiteral("action"),
                                         QStringLiteral("第二集场景动作原稿。")),
            controller.errorMessage());
    const QString secondEpisodeOriginal = controller.scriptText();

    // 任一目标在生成后变化时，整批候选都必须拒绝，不能先写入其他段落。
    const QString staleTaskId = prepareBatchTask(
        controller, QStringLiteral("project"), controller.currentProjectId(), 1);
    const QJsonArray staleTargets = targetBlocks(controller.aiTask(staleTaskId));
    require(staleTargets.size() == 3,
            QStringLiteral("全剧上下文没有列出全部非空剧本段落。"));
    require(controller.completeAiTask(staleTaskId,
                                      rewriteCandidate(staleTargets, QStringLiteral("过期改写：")),
                                      {}), controller.errorMessage());
    require(controller.updateScriptBlock(thirdBlockId, QStringLiteral("action"),
                                         QStringLiteral("生成后用户更新的第二集原稿。")),
            controller.errorMessage());
    const QString secondEpisodeUpdatedOriginal = controller.scriptText();
    require(!controller.acceptAiTask(staleTaskId),
            QStringLiteral("全剧批量候选未因目标段落变化而失效。"));
    require(scriptTextFor(controller, firstEpisodeId) == firstEpisodeOriginal
                && scriptTextFor(controller, secondEpisodeId) == secondEpisodeUpdatedOriginal
                && scriptVersionCountFor(controller, firstEpisodeId) == 0
                && scriptVersionCountFor(controller, secondEpisodeId) == 0,
            QStringLiteral("拒绝过期全剧候选时产生了部分写入或备份版本。"));
    controller.clearError();

    // 多候选全剧改写只写入所选方案，并为每集保留独立原稿备份。
    const QString projectTaskId = prepareBatchTask(
        controller, QStringLiteral("project"), controller.currentProjectId(), 3);
    const QJsonArray projectTargets = targetBlocks(controller.aiTask(projectTaskId));
    const QStringList projectChoices{
        rewriteCandidate(projectTargets, QStringLiteral("全剧方案一：")),
        rewriteCandidate(projectTargets, QStringLiteral("全剧方案二：")),
        rewriteCandidate(projectTargets, QStringLiteral("全剧方案三："))
    };
    QJsonArray mixedBlocks;
    for (qsizetype index = 0; index < projectTargets.size(); ++index) {
        const QJsonArray preferred = QJsonDocument::fromJson(
            projectChoices.at(index == 0 ? 0 : 1).toUtf8()).object()
                                        .value(QStringLiteral("blocks")).toArray();
        mixedBlocks.append(preferred.at(index));
    }
    const QString mixedCandidate = QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("format"), QStringLiteral("sceneforge.script-block-rewrites.v1")},
        {QStringLiteral("blocks"), mixedBlocks}
    }).toJson(QJsonDocument::Compact));
    QString selectedProjectChoice;
    require(sceneforge::project::encodeScriptCandidates(
                projectChoices, 1, &selectedProjectChoice, nullptr, mixedCandidate),
            QStringLiteral("无法编码逐段组合的全剧候选。"));
    require(controller.completeAiTask(projectTaskId, selectedProjectChoice, {}),
            controller.errorMessage());
    require(controller.acceptAiTask(projectTaskId), controller.errorMessage());
    require(scriptVersionCountFor(controller, firstEpisodeId) == 1
                && scriptVersionCountFor(controller, secondEpisodeId) == 1,
            QStringLiteral("全剧批量改写没有逐集保留原稿版本。"));
    const QString rewrittenProjectFirst = scriptTextFor(controller, firstEpisodeId);
    require(rewrittenProjectFirst.contains(QStringLiteral("全剧方案一："))
                && rewrittenProjectFirst.contains(QStringLiteral("全剧方案二：")),
            QStringLiteral("全剧采纳没有应用逐段混合的候选方案。"));
    const QString rewrittenProjectSecond = scriptTextFor(controller, secondEpisodeId);
    require(rewrittenProjectSecond.contains(QStringLiteral("全剧方案二："))
                && !rewrittenProjectSecond.contains(QStringLiteral("全剧方案一：")),
            QStringLiteral("第二集没有保留其所选候选段落来源。"));

    // 整集改写只影响目标剧集。
    const QString episodeTaskId = prepareBatchTask(
        controller, QStringLiteral("episode"), firstEpisodeId, 1);
    const QJsonArray episodeTargets = targetBlocks(controller.aiTask(episodeTaskId));
    require(episodeTargets.size() == 2, QStringLiteral("整集目标没有覆盖该集的两段内容。"));
    require(controller.completeAiTask(episodeTaskId,
                                      rewriteCandidate(episodeTargets, QStringLiteral("整集改写：")),
                                      {}), controller.errorMessage());
    require(controller.acceptAiTask(episodeTaskId), controller.errorMessage());
    require(scriptTextFor(controller, firstEpisodeId).contains(QStringLiteral("整集改写："))
                && scriptTextFor(controller, secondEpisodeId) == rewrittenProjectSecond
                && scriptVersionCountFor(controller, firstEpisodeId) == 2
                && scriptVersionCountFor(controller, secondEpisodeId) == 1,
            QStringLiteral("整集改写超出目标剧集，或未保留原稿。"));

    // 整场改写只替换该场的段落，不碰其他场和剧集。
    const QString sceneTaskId = prepareBatchTask(
        controller, QStringLiteral("scene"), firstSceneId, 1);
    const QJsonArray sceneTargets = targetBlocks(controller.aiTask(sceneTaskId));
    require(sceneTargets.size() == 2, QStringLiteral("整场目标没有覆盖该场的全部段落。"));
    require(controller.completeAiTask(sceneTaskId,
                                      rewriteCandidate(sceneTargets, QStringLiteral("整场改写：")),
                                      {}), controller.errorMessage());
    require(controller.acceptAiTask(sceneTaskId), controller.errorMessage());
    require(scriptTextFor(controller, firstEpisodeId).contains(QStringLiteral("整场改写："))
                && scriptTextFor(controller, secondEpisodeId) == rewrittenProjectSecond
                && scriptVersionCountFor(controller, firstEpisodeId) == 3
                && scriptVersionCountFor(controller, secondEpisodeId) == 1,
            QStringLiteral("整场改写超出场次范围，或未保留原稿。"));
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SceneForgeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("AiBatchScriptCandidateTests"));
    try {
        verifyBatchRewriteScopesAndAtomicity();
        std::cout << "PASS: batch script rewrite scope, candidate selection, stale-target rejection, and rollback\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
