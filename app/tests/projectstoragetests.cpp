#include "../src/project/projectcontroller.h"
#include "../src/project/projectrepository.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
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

void dropSchemaElevenTables(QSqlQuery& query)
{
    for (const QString& statement : {
             QStringLiteral("DROP TABLE visual_reference_bible_items"),
             QStringLiteral("DROP TABLE visual_reference_shots"),
             QStringLiteral("DROP TABLE visual_references"),
             QStringLiteral("DROP TABLE shot_bible_items"),
             QStringLiteral("DROP TABLE shot_script_blocks"),
             QStringLiteral("DROP TABLE shots"),
             QStringLiteral("DROP TABLE ai_generation_tasks")}) {
        require(query.exec(statement), query.lastError().text());
    }
}

bool bibleItemIsReferenced(const QVariantList& bibleItems, const QString& itemId)
{
    for (const QVariant& value : bibleItems) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("id")).toString() == itemId) {
            return item.value(QStringLiteral("referenced")).toBool();
        }
    }
    return false;
}

QStringList bibleItemOrder(const QVariantList& bibleItems)
{
    QStringList itemIds;
    for (const QVariant& value : bibleItems) {
        itemIds.append(value.toMap().value(QStringLiteral("id")).toString());
    }
    return itemIds;
}

QString bibleItemIdByName(const QVariantList& bibleItems, const QString& name)
{
    for (const QVariant& value : bibleItems) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("name")).toString() == name) {
            return item.value(QStringLiteral("id")).toString();
        }
    }
    return {};
}

QVariantMap bibleItemById(const QVariantList& bibleItems, const QString& id)
{
    for (const QVariant& value : bibleItems) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("id")).toString() == id) {
            return item;
        }
    }
    return {};
}

QVariantMap qualityFindingByCode(const QVariantMap& report, const QString& code)
{
    for (const QVariant& value : report.value(QStringLiteral("findings")).toList()) {
        const QVariantMap finding = value.toMap();
        if (finding.value(QStringLiteral("code")).toString() == code) {
            return finding;
        }
    }
    return {};
}

QVariantMap qualityFindingById(const QVariantMap& report, const QString& id)
{
    for (const QVariant& value : report.value(QStringLiteral("findings")).toList()) {
        const QVariantMap finding = value.toMap();
        if (finding.value(QStringLiteral("id")).toString() == id) {
            return finding;
        }
    }
    return {};
}

QVariantMap qualityFindingByCodeAndScope(const QVariantMap& report,
                                         const QString& code,
                                         const QString& scopeId)
{
    for (const QVariant& value : report.value(QStringLiteral("findings")).toList()) {
        const QVariantMap finding = value.toMap();
        if (finding.value(QStringLiteral("code")).toString() == code
            && finding.value(QStringLiteral("scopeId")).toString() == scopeId) {
            return finding;
        }
    }
    return {};
}

int createAclOpenFixture(const QString& libraryPath)
{
    sceneforge::project::ProjectRepository repository;
    QString error;
    if (!repository.setLibraryRoot(libraryPath, &error)) {
        std::cerr << "FAIL: " << error.toStdString() << '\n';
        return 1;
    }
    sceneforge::project::ProjectInfo project;
    if (!repository.createProject(QStringLiteral("只读 ACL 重开验证"),
                                  QStringLiteral("ACL 拒绝写入前保存的原始创意。"),
                                  QStringLiteral("single_short"), &project, &error)) {
        std::cerr << "FAIL: " << error.toStdString() << '\n';
        return 1;
    }
    repository.closeProject();
    std::cout << project.path.toStdString() << '\n';
    return 0;
}

int expectAclOpenFailure(const QString& libraryPath, const QString& projectPath)
{
    sceneforge::project::ProjectRepository repository;
    QString error;
    if (!repository.setLibraryRoot(libraryPath, &error)) {
        std::cerr << "FAIL: " << error.toStdString() << '\n';
        return 1;
    }
    sceneforge::project::ProjectInfo project;
    if (repository.openProject(projectPath, &project, &error)) {
        repository.closeProject();
        std::cerr << "FAIL: read-only ACL project unexpectedly opened for writing\n";
        return 1;
    }
    if (!error.contains(QStringLiteral("权限"))) {
        std::cerr << "FAIL: read-only ACL open failure did not identify the directory permission problem: "
                  << error.toStdString() << '\n';
        return 1;
    }
    std::cout << "PASS: read-only ACL open failed visibly: "
              << error.toStdString() << '\n';
    return 0;
}

int expectAclOpenSuccess(const QString& libraryPath, const QString& projectPath)
{
    sceneforge::project::ProjectRepository repository;
    QString error;
    if (!repository.setLibraryRoot(libraryPath, &error)) {
        std::cerr << "FAIL: " << error.toStdString() << '\n';
        return 1;
    }
    sceneforge::project::ProjectInfo project;
    sceneforge::project::ProjectDraft draft;
    if (!repository.openProject(projectPath, &project, &error)
        || !repository.loadDraft(&draft, &error)) {
        std::cerr << "FAIL: " << error.toStdString() << '\n';
        return 1;
    }
    if (draft.brief.description != QStringLiteral("ACL 拒绝写入前保存的原始创意。")) {
        std::cerr << "FAIL: saved content changed during read-only open failure\n";
        return 1;
    }
    repository.closeProject();
    std::cout << "PASS: original content reopened after ACL removal\n";
    return 0;
}

void createVersionOneProject(const QString& projectPath, const QString& id)
{
    require(QDir().mkpath(projectPath), QStringLiteral("无法创建旧版作品测试目录。"));
    const QString connectionName = QStringLiteral("legacy-fixture-")
                                   + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        database.setDatabaseName(QDir(projectPath).filePath(QStringLiteral("project.sqlite")));
        require(database.open(), database.lastError().text());
        QSqlQuery query(database);
        require(query.exec(QStringLiteral(
                    "CREATE TABLE project_info ("
                    "singleton INTEGER PRIMARY KEY CHECK(singleton = 1), "
                    "project_id TEXT NOT NULL UNIQUE, name TEXT NOT NULL, created_at TEXT NOT NULL)")),
                query.lastError().text());
        query.prepare(QStringLiteral(
            "INSERT INTO project_info(singleton, project_id, name, created_at) "
            "VALUES(1, ?, '旧版作品', '2026-11-27T12:00:00.000Z')"));
        query.addBindValue(id);
        require(query.exec(), query.lastError().text());
        require(query.exec(QStringLiteral("PRAGMA user_version=1")), query.lastError().text());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void createVersionTwoProject(const QString& projectPath, const QString& id)
{
    require(QDir().mkpath(projectPath), QStringLiteral("无法创建 schema v2 测试作品目录。"));
    const QString connectionName = QStringLiteral("schema-two-fixture-")
                                   + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        database.setDatabaseName(QDir(projectPath).filePath(QStringLiteral("project.sqlite")));
        require(database.open(), database.lastError().text());
        QSqlQuery query(database);
        require(query.exec(QStringLiteral(
                    "CREATE TABLE project_info ("
                    "singleton INTEGER PRIMARY KEY CHECK(singleton = 1), "
                    "project_id TEXT NOT NULL UNIQUE, name TEXT NOT NULL, "
                    "created_at TEXT NOT NULL, updated_at TEXT NOT NULL, "
                    "project_type TEXT NOT NULL DEFAULT 'single_short')")),
                query.lastError().text());
        query.prepare(QStringLiteral(
            "INSERT INTO project_info(singleton, project_id, name, created_at, updated_at, "
            "project_type) VALUES(1, ?, '迁移保留内容', '2026-11-27T12:00:00.000Z', "
            "'2026-11-27T12:15:00.000Z', 'single_short')"));
        query.addBindValue(id);
        require(query.exec(), query.lastError().text());
        require(query.exec(QStringLiteral(
                    "CREATE TABLE creative_brief ("
                    "singleton INTEGER PRIMARY KEY CHECK(singleton = 1), "
                    "description TEXT NOT NULL DEFAULT '', genre TEXT NOT NULL DEFAULT '', "
                    "theme TEXT NOT NULL DEFAULT '', target_audience TEXT NOT NULL DEFAULT '', "
                    "tone TEXT NOT NULL DEFAULT '', target_duration TEXT NOT NULL DEFAULT '', "
                    "language TEXT NOT NULL DEFAULT '中文', platform_direction TEXT NOT NULL DEFAULT '', "
                    "required_elements TEXT NOT NULL DEFAULT '', "
                    "forbidden_elements TEXT NOT NULL DEFAULT '')")),
                query.lastError().text());
        query.prepare(QStringLiteral(
            "INSERT INTO creative_brief(singleton, description, genre, language) "
            "VALUES(1, '旧版故事构想', '悬疑', '中文')"));
        require(query.exec(), query.lastError().text());
        require(query.exec(QStringLiteral(
                    "CREATE TABLE script_draft ("
                    "singleton INTEGER PRIMARY KEY CHECK(singleton = 1), "
                    "content TEXT NOT NULL DEFAULT '')")), query.lastError().text());
        query.prepare(QStringLiteral(
            "INSERT INTO script_draft(singleton, content) VALUES(1, ?)"));
        query.addBindValue(QStringLiteral("旧版剧本正文必须保留。"));
        require(query.exec(), query.lastError().text());
        require(query.exec(QStringLiteral("PRAGMA user_version=2")), query.lastError().text());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void verifyProjectLifecycle()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建测试资料库目录。"));

    const QString libraryRoot = QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"));
    sceneforge::project::ProjectRepository repository;
    QString error;
    require(repository.setLibraryRoot(libraryRoot, &error), error);
    require(repository.libraryRoot() == QFileInfo(libraryRoot).canonicalFilePath(),
            QStringLiteral("资料库路径未规范化。"));

    sceneforge::project::ProjectInfo created;
    require(!repository.createProject(QStringLiteral("  "), QString(),
                                      QStringLiteral("single_short"), &created, &error),
            QStringLiteral("空作品名称不应允许创建。"));
    require(repository.createProject(QStringLiteral("雨夜来信"),
                                    QStringLiteral("一封迟到多年的信改变了两个人的生活。"),
                                    QStringLiteral("single_short"),
                                    &created, &error), error);
    require(!QUuid::fromString(created.id).isNull(), QStringLiteral("作品 ID 不是稳定 UUID。"));
    require(created.lastSavedAt.isValid(), QStringLiteral("作品创建后缺少最近保存时间。"));
    require(created.projectType == QStringLiteral("single_short"),
            QStringLiteral("首切片新作品类型应为单集短片。"));
    require(QFileInfo(QDir(created.path).filePath(QStringLiteral("project.sqlite"))).isFile(),
            QStringLiteral("作品数据库没有创建。"));
    require(QFileInfo(QDir(created.path).filePath(QStringLiteral("media/originals"))).isDir(),
            QStringLiteral("作品媒体目录没有创建。"));

    QStringList issues;
    const auto listed = repository.listProjects(&issues);
    require(listed.size() == 1 && listed.front().id == created.id,
            QStringLiteral("作品资料库没有发现刚创建的作品。"));
    require(issues.isEmpty(), QStringLiteral("有效作品被报告为目录问题。"));
    require(!repository.createProject(QStringLiteral("第二部作品"), QString(),
                                      QStringLiteral("single_short"),
                                      &created, &error),
            QStringLiteral("打开作品期间不应创建另一个作品。"));

    const auto firstProject = repository.currentProject();
    sceneforge::project::ProjectDraft draft;
    require(repository.loadDraft(&draft, &error), error);
    require(draft.brief.description
                == QStringLiteral("一封迟到多年的信改变了两个人的生活。"),
            QStringLiteral("新作品没有保存初始创意。"));
    draft.brief.genre = QStringLiteral("悬疑");
    draft.episodes.first().scriptText = QStringLiteral("内景，旧车站 - 夜\n雨水顺着屋檐落下。");
    draft.episodes.first().scenes.append({
        QUuid::createUuid().toString(QUuid::WithoutBraces), 0,
        QStringLiteral("旧车站·夜"), QStringLiteral("draft")});
    const QString stableSceneId = draft.episodes.first().scenes.first().id;
    require(repository.saveDraft(draft, &error), error);

    sceneforge::project::ProjectRepository competingRepository;
    require(competingRepository.setLibraryRoot(libraryRoot, &error), error);
    require(!competingRepository.openProject(firstProject.path, &created, &error),
            QStringLiteral("同一作品不应被第二个写入进程同时打开。"));
    repository.closeProject();
    require(!repository.hasOpenProject(), QStringLiteral("关闭作品后仍报告作品已打开。"));

    require(competingRepository.openProject(firstProject.path, &created, &error), error);
    require(created.id == firstProject.id && created.name == firstProject.name,
            QStringLiteral("重新打开后作品身份或名称发生变化。"));
    require(competingRepository.loadDraft(&draft, &error), error);
    require(draft.brief.genre == QStringLiteral("悬疑")
                && draft.episodes.first().scriptText
                       == QStringLiteral("内景，旧车站 - 夜\n雨水顺着屋檐落下。")
                && draft.episodes.first().scenes.size() == 1
                && draft.episodes.first().scenes.first().id == stableSceneId,
            QStringLiteral("重新打开后没有恢复已保存的简报和剧本正文。"));
    repository.closeProject();
    competingRepository.closeProject();

    const QString invalidDirectory = QDir(libraryRoot).filePath(QStringLiteral("损坏目录"));
    require(QDir().mkpath(invalidDirectory), QStringLiteral("无法创建无效目录测试样例。"));
    const auto validProjects = repository.listProjects(&issues);
    require(validProjects.size() == 1 && issues.size() == 1,
            QStringLiteral("资料库没有报告无法识别的目录。"));
}

void verifySchemaMigration()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建迁移测试目录。"));
    const QString libraryRoot = QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"));
    const QString projectId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString projectPath = QDir(libraryRoot).filePath(projectId);
    createVersionOneProject(projectPath, projectId);

    sceneforge::project::ProjectRepository repository;
    QString error;
    require(repository.setLibraryRoot(libraryRoot, &error), error);
    sceneforge::project::ProjectInfo project;
    require(repository.openProject(projectPath, &project, &error), error);
    require(project.id == projectId && project.name == QStringLiteral("旧版作品"),
            QStringLiteral("迁移后没有保留旧作品身份。"));
    require(project.projectType == QStringLiteral("single_short"),
            QStringLiteral("旧作品迁移后没有设置单集短片类型。"));

    sceneforge::project::ProjectDraft draft;
    require(repository.loadDraft(&draft, &error), error);
    require(draft.brief.language == QStringLiteral("中文")
                && draft.brief.description.isEmpty()
                && draft.episodes.size() == 1
                && draft.episodes.first().scriptText.isEmpty(),
            QStringLiteral("旧作品迁移后没有获得可继续创作的空白简报和剧本。"));

    const QString migrationBackupDirectory = QDir(projectPath).filePath(
        QStringLiteral("backups/migrations"));
    const QFileInfoList backups = QDir(migrationBackupDirectory).entryInfoList(
        QStringList{QStringLiteral("*.sqlite")}, QDir::Files);
    require(backups.size() == 12, QStringLiteral("逐版本迁移前没有分别生成可校验备份。"));
    repository.closeProject();
}

void verifySchemaTwoMigration()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建 schema v2 迁移测试目录。"));
    const QString libraryRoot = QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"));
    const QString projectId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString projectPath = QDir(libraryRoot).filePath(projectId);
    createVersionTwoProject(projectPath, projectId);

    sceneforge::project::ProjectRepository repository;
    QString error;
    require(repository.setLibraryRoot(libraryRoot, &error), error);
    sceneforge::project::ProjectInfo project;
    require(repository.openProject(projectPath, &project, &error), error);
    sceneforge::project::ProjectDraft draft;
    require(repository.loadDraft(&draft, &error), error);
    require(draft.brief.description == QStringLiteral("旧版故事构想")
                && draft.brief.genre == QStringLiteral("悬疑")
                && draft.episodes.size() == 1
                && draft.episodes.first().scriptText
                       == QStringLiteral("旧版剧本正文必须保留。"),
            QStringLiteral("schema v2 迁移丢失了旧版简报或剧本内容。"));
    require(repository.loadHistory(QStringLiteral("project"), projectId, &error).size() == 1
                && repository.loadHistory(QStringLiteral("episode"),
                                          draft.episodes.first().id, &error).size() == 1,
            QStringLiteral("迁移后没有建立作品和剧集历史基线。"));
    repository.closeProject();

    const QString migrationBackupDirectory = QDir(projectPath).filePath(
        QStringLiteral("backups/migrations"));
    const QFileInfoList backups = QDir(migrationBackupDirectory).entryInfoList(
        QStringList{QStringLiteral("*.sqlite")}, QDir::Files);
    require(backups.size() == 11,
            QStringLiteral("schema v2 的逐版本迁移没有分别生成备份。"));

    const QString connectionName = QStringLiteral("schema-version-check-")
                                   + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        database.setDatabaseName(QDir(projectPath).filePath(QStringLiteral("project.sqlite")));
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        require(database.open(), database.lastError().text());
        QSqlQuery query(database);
        require(query.exec(QStringLiteral("PRAGMA user_version")) && query.next()
                && query.value(0).toInt() == 13,
                QStringLiteral("schema v2 作品未升级到当前 schema v13。"));
        require(query.exec(QStringLiteral(
                    "SELECT count(*) FROM sqlite_master WHERE type='table' "
                    "AND name='script_draft'"))
                    && query.next() && query.value(0).toInt() == 0,
                QStringLiteral("迁移后仍保留了已废弃的单作品剧本表。"));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void verifySchemaThreeMigration()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建 schema v3 迁移测试目录。"));
    const QString libraryRoot = QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"));
    sceneforge::project::ProjectRepository creator;
    QString error;
    require(creator.setLibraryRoot(libraryRoot, &error), error);
    sceneforge::project::ProjectInfo created;
    require(creator.createProject(QStringLiteral("v3 迁移作品"),
                                  QStringLiteral("迁移时保留的全剧构想。"),
                                  QStringLiteral("series"), &created, &error), error);
    sceneforge::project::ProjectDraft draft;
    require(creator.loadDraft(&draft, &error), error);
    draft.episodes.first().scriptText = QStringLiteral("迁移时保留的剧本。 ");
    require(creator.saveDraft(draft, &error), error);
    creator.closeProject();

    const QString connectionName = QStringLiteral("schema-three-fixture-")
                                   + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        database.setDatabaseName(QDir(created.path).filePath(QStringLiteral("project.sqlite")));
        require(database.open(), database.lastError().text());
        QSqlQuery query(database);
        dropSchemaElevenTables(query);
        require(query.exec(QStringLiteral("DROP TABLE episode_bible_items")),
                query.lastError().text());
        require(query.exec(QStringLiteral("DROP TABLE bible_items")), query.lastError().text());
        require(query.exec(QStringLiteral("DROP TABLE script_versions")), query.lastError().text());
        require(query.exec(QStringLiteral("DROP TABLE script_blocks")), query.lastError().text());
        require(query.exec(QStringLiteral("DROP TABLE quality_finding_reviews")),
                query.lastError().text());
        require(query.exec(QStringLiteral("DROP TABLE story_impact_findings")),
                query.lastError().text());
        require(query.exec(QStringLiteral("ALTER TABLE scenes DROP COLUMN target_duration_seconds")),
                query.lastError().text());
        require(query.exec(QStringLiteral("ALTER TABLE scenes DROP COLUMN conflict_or_turn")),
                query.lastError().text());
        require(query.exec(QStringLiteral("ALTER TABLE scenes DROP COLUMN goal")),
                query.lastError().text());
        require(query.exec(QStringLiteral(
                    "ALTER TABLE creative_brief DROP COLUMN story_development_json")),
                query.lastError().text());
        require(query.exec(QStringLiteral(
                    "ALTER TABLE episode_brief DROP COLUMN story_development_json")),
                query.lastError().text());
        require(query.exec(QStringLiteral("ALTER TABLE project_info DROP COLUMN archived")),
                query.lastError().text());
        require(query.exec(QStringLiteral("PRAGMA user_version=3")), query.lastError().text());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    sceneforge::project::ProjectRepository repository;
    require(repository.setLibraryRoot(libraryRoot, &error), error);
    sceneforge::project::ProjectInfo project;
    require(repository.openProject(created.path, &project, &error), error);
    require(repository.loadDraft(&draft, &error), error);
    require(draft.brief.description == QStringLiteral("迁移时保留的全剧构想。")
                && draft.episodes.first().scriptText == QStringLiteral("迁移时保留的剧本。 "),
            QStringLiteral("schema v3 到 v4 迁移改变了原有创作内容。"));
    repository.closeProject();

    const QString schemaCheckConnection = QStringLiteral("schema-four-check-")
                                          + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", schemaCheckConnection);
        database.setDatabaseName(QDir(created.path).filePath(QStringLiteral("project.sqlite")));
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        require(database.open(), database.lastError().text());
        QSqlQuery query(database);
        require(query.exec(QStringLiteral("PRAGMA user_version")) && query.next()
                && query.value(0).toInt() == 13,
                QStringLiteral("schema v3 作品没有升级到 schema v13。"));
        database.close();
    }
    QSqlDatabase::removeDatabase(schemaCheckConnection);

    const QFileInfoList backups = QDir(QDir(created.path).filePath(
            QStringLiteral("backups/migrations")))
                                      .entryInfoList(QStringList{QStringLiteral("*.sqlite")},
                                                     QDir::Files);
    require(backups.size() == 10,
            QStringLiteral("schema v3 迁移前没有生成可校验备份。"));
}

void verifySchemaFourBibleMigration()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建 schema v4 迁移测试目录。"));
    const QString libraryRoot = QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"));
    sceneforge::project::ProjectRepository creator;
    QString error;
    require(creator.setLibraryRoot(libraryRoot, &error), error);
    sceneforge::project::ProjectInfo created;
    require(creator.createProject(QStringLiteral("v4 创作圣经迁移作品"), QString(),
                                  QStringLiteral("series"), &created, &error), error);
    sceneforge::project::ProjectDraft draft;
    require(creator.loadDraft(&draft, &error), error);
    sceneforge::project::SceneDraft legacyScene;
    legacyScene.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    legacyScene.title = QStringLiteral("旧版场次");
    draft.episodes.first().scenes.append(legacyScene);
    draft.episodes.first().scriptText = QStringLiteral("v4 旧版自由剧本文本必须保留。");
    if (draft.episodes.first().scriptBlocks.isEmpty()) {
        sceneforge::project::ScriptBlockDraft legacyBlock;
        legacyBlock.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        draft.episodes.first().scriptBlocks.append(legacyBlock);
    }
    draft.episodes.first().scriptBlocks.first().text = draft.episodes.first().scriptText;
    sceneforge::project::BibleItemDraft character;
    character.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    character.itemType = QStringLiteral("character");
    character.name = QStringLiteral("旧版角色");
    character.description = QStringLiteral("迁移前的简介。 ");
    character.contentStatus = QStringLiteral("confirmed");
    draft.bibleItems.append(character);
    draft.episodes.first().bibleItemIds.append(character.id);
    require(creator.saveDraft(draft, &error), error);
    creator.closeProject();

    const QString connectionName = QStringLiteral("schema-four-fixture-")
                                   + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        database.setDatabaseName(QDir(created.path).filePath(QStringLiteral("project.sqlite")));
        require(database.open(), database.lastError().text());
        QSqlQuery query(database);
        require(query.exec(QStringLiteral("PRAGMA foreign_keys=OFF")), query.lastError().text());
        dropSchemaElevenTables(query);
        require(query.exec(QStringLiteral("DROP TABLE script_versions")), query.lastError().text());
        require(query.exec(QStringLiteral("DROP TABLE script_blocks")), query.lastError().text());
        require(query.exec(QStringLiteral("DROP TABLE quality_finding_reviews")),
                query.lastError().text());
        require(query.exec(QStringLiteral("DROP TABLE story_impact_findings")),
                query.lastError().text());
        require(query.exec(QStringLiteral("ALTER TABLE scenes DROP COLUMN target_duration_seconds")),
                query.lastError().text());
        require(query.exec(QStringLiteral("ALTER TABLE scenes DROP COLUMN conflict_or_turn")),
                query.lastError().text());
        require(query.exec(QStringLiteral("ALTER TABLE scenes DROP COLUMN goal")),
                query.lastError().text());
        require(query.exec(QStringLiteral(
                    "ALTER TABLE creative_brief DROP COLUMN story_development_json")),
                query.lastError().text());
        require(query.exec(QStringLiteral(
                    "ALTER TABLE episode_brief DROP COLUMN story_development_json")),
                query.lastError().text());
        require(query.exec(QStringLiteral("DROP INDEX bible_items_type_name")),
                query.lastError().text());
        require(query.exec(QStringLiteral(
                    "CREATE TABLE bible_items_v4 ("
                    "item_id TEXT PRIMARY KEY, item_type TEXT NOT NULL, name TEXT NOT NULL, "
                    "description TEXT NOT NULL DEFAULT '', content_status TEXT NOT NULL, "
                    "created_at TEXT NOT NULL, updated_at TEXT NOT NULL)")),
                query.lastError().text());
        require(query.exec(QStringLiteral(
                    "INSERT INTO bible_items_v4(item_id, item_type, name, description, "
                    "content_status, created_at, updated_at) "
                    "SELECT item_id, item_type, name, description, content_status, "
                    "created_at, updated_at FROM bible_items")),
                query.lastError().text());
        require(query.exec(QStringLiteral(
                    "CREATE TABLE episode_bible_items_v4 (episode_id TEXT NOT NULL, "
                    "item_id TEXT NOT NULL, created_at TEXT NOT NULL, "
                    "PRIMARY KEY(episode_id, item_id))")),
                query.lastError().text());
        require(query.exec(QStringLiteral(
                    "INSERT INTO episode_bible_items_v4(episode_id, item_id, created_at) "
                    "SELECT episode_id, item_id, created_at FROM episode_bible_items")),
                query.lastError().text());
        require(query.exec(QStringLiteral("DROP TABLE episode_bible_items")),
                query.lastError().text());
        require(query.exec(QStringLiteral("DROP TABLE bible_items")), query.lastError().text());
        require(query.exec(QStringLiteral("ALTER TABLE bible_items_v4 RENAME TO bible_items")),
                query.lastError().text());
        require(query.exec(QStringLiteral(
                    "ALTER TABLE episode_bible_items_v4 RENAME TO episode_bible_items")),
                query.lastError().text());
        require(query.exec(QStringLiteral(
                    "CREATE UNIQUE INDEX bible_items_type_name "
                    "ON bible_items(item_type, name COLLATE NOCASE)")),
                query.lastError().text());
        require(query.exec(QStringLiteral("ALTER TABLE project_info DROP COLUMN archived")),
                query.lastError().text());
        require(query.exec(QStringLiteral("PRAGMA user_version=4")), query.lastError().text());
        require(query.exec(QStringLiteral(
                    "DELETE FROM content_history WHERE scope_type='bible_item'")),
                query.lastError().text());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    sceneforge::project::ProjectRepository migrated;
    require(migrated.setLibraryRoot(libraryRoot, &error), error);
    sceneforge::project::ProjectInfo reopened;
    require(migrated.openProject(created.path, &reopened, &error), error);
    require(migrated.loadDraft(&draft, &error), error);
    require(draft.bibleItems.size() == 1
                && draft.bibleItems.first().id == character.id
                && draft.bibleItems.first().description == QStringLiteral("迁移前的简介。 ")
                && draft.bibleItems.first().certaintyStatus == QStringLiteral("confirmed")
                && draft.bibleItems.first().details.isEmpty()
                && draft.episodes.first().bibleItemIds.contains(character.id)
                && draft.episodes.first().scriptText
                       == QStringLiteral("v4 旧版自由剧本文本必须保留。")
                && draft.episodes.first().scriptBlocks.size() == 1
                && draft.episodes.first().scriptBlocks.first().blockType
                       == QStringLiteral("unstructured_text")
                && draft.episodes.first().scriptBlocks.first().text
                       == QStringLiteral("v4 旧版自由剧本文本必须保留。")
                && draft.episodes.first().scenes.size() == 1
                && draft.episodes.first().scenes.first().goal.isEmpty()
                && draft.episodes.first().scenes.first().conflictOrTurn.isEmpty()
                && draft.episodes.first().scenes.first().targetDurationSeconds == 0,
            QStringLiteral("schema v4 迁移丢失了既有设定、状态或剧集引用。"));
    require(migrated.loadQualityFindingStatuses(&error).isEmpty() && error.isEmpty(),
            QStringLiteral("旧作品迁移后没有创建可用的质量提示处理状态表。"));
    require(migrated.loadHistory(QStringLiteral("bible_item"), character.id, &error).size() == 1,
            QStringLiteral("schema v4 迁移没有为既有创作圣经条目建立历史基线。"));
    require(!migrated.loadHistory(QStringLiteral("scene"), legacyScene.id, &error).isEmpty()
                && !migrated.loadHistory(QStringLiteral("script_block"),
                                         draft.episodes.first().scriptBlocks.first().id,
                                         &error).isEmpty(),
            QStringLiteral("旧场次或迁移后的剧本段落没有独立修改历史。"));
    migrated.closeProject();
    const QString schemaCheckConnection = QStringLiteral("schema-six-check-")
                                          + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", schemaCheckConnection);
        database.setDatabaseName(QDir(created.path).filePath(QStringLiteral("project.sqlite")));
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        require(database.open(), database.lastError().text());
        QSqlQuery query(database);
        require(query.exec(QStringLiteral("PRAGMA user_version")) && query.next()
                    && query.value(0).toInt() == 13,
                QStringLiteral("schema v4 迁移没有继续完成 v5 到 v13。"));
        database.close();
    }
    QSqlDatabase::removeDatabase(schemaCheckConnection);
    const QFileInfoList backups = QDir(QDir(created.path).filePath(
                                           QStringLiteral("backups/migrations")))
                                      .entryInfoList(QStringList{QStringLiteral("*.sqlite")},
                                                     QDir::Files);
    require(backups.size() == 9, QStringLiteral("schema v4 迁移前没有生成数据库备份。"));
}

void verifyStructuredStoryAndScriptWorkflow()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建结构化剧本测试目录。"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"))),
            controller.errorMessage());
    require(controller.createProject(QStringLiteral("结构化剧本测试"),
                                     QStringLiteral("一封迟到的信改变了两个人的选择。"),
                                     QStringLiteral("series")),
            controller.errorMessage());
    require(controller.updateDraftField(QStringLiteral("storySynopsis"),
                                        QStringLiteral("主角循着旧信寻找多年未见的朋友。"))
                && controller.updateDraftField(QStringLiteral("centralConflict"),
                                               QStringLiteral("真相会修复关系，也可能结束关系。"))
                && controller.updateDraftField(QStringLiteral("endingDirection"),
                                               QStringLiteral("以一次主动道歉结束。"))
                && controller.updateEpisodeField(QStringLiteral("episodeOutline"),
                                                 QStringLiteral("第一集：收到来信。")),
            QStringLiteral("作品级或剧集级故事发展字段无法编辑。"));

    require(controller.createScene(), controller.errorMessage());
    QString sceneId = controller.scenes().first().toMap()
                          .value(QStringLiteral("id")).toString();
    require(controller.updateSceneTitle(sceneId, QStringLiteral("内景，旧车站候车室 - 夜")),
            controller.errorMessage());
    require(controller.updateSceneQualityFields(sceneId,
                                                QStringLiteral("找到寄出的信"),
                                                QStringLiteral("信件内容改变双方关系"),
                                                QStringLiteral("45")),
            controller.errorMessage());
    require(controller.createScriptBlock(QStringLiteral("scene_heading"), sceneId),
            controller.errorMessage());
    const QString headingId = controller.scriptBlocks().last().toMap()
                                 .value(QStringLiteral("id")).toString();
    require(controller.updateScriptBlock(headingId, QStringLiteral("scene_heading"),
                                         QStringLiteral("内景，旧车站候车室 - 夜")),
            controller.errorMessage());
    require(controller.setScriptBlockScene(headingId, QString())
                && controller.scriptBlocks().last().toMap()
                       .value(QStringLiteral("sceneId")).toString().isEmpty()
                && controller.setScriptBlockScene(headingId, sceneId),
            QStringLiteral("剧本段落无法解除或恢复场次关联。"));
    require(controller.createScriptBlock(QStringLiteral("character"), sceneId),
            controller.errorMessage());
    const QString characterId = controller.scriptBlocks().last().toMap()
                                    .value(QStringLiteral("id")).toString();
    require(controller.updateScriptBlock(characterId, QStringLiteral("character"),
                                         QStringLiteral("林岚")),
            controller.errorMessage());
    require(controller.duplicateScriptBlock(characterId)
                && controller.scriptBlocks().size() == 4
                && controller.scriptBlocks().at(3).toMap()
                       .value(QStringLiteral("contentStatus")).toString()
                       == QStringLiteral("draft"),
            QStringLiteral("复制剧本段落没有生成独立草稿。"));
    const QString temporaryBlockId = controller.scriptBlocks().at(3).toMap()
                                         .value(QStringLiteral("id")).toString();
    require(controller.removeScriptBlock(temporaryBlockId), controller.errorMessage());
    require(controller.createScriptBlock(QStringLiteral("dialogue"), sceneId),
            controller.errorMessage());
    const QString dialogueId = controller.scriptBlocks().last().toMap()
                                   .value(QStringLiteral("id")).toString();
    require(controller.updateScriptBlock(dialogueId, QStringLiteral("dialogue"),
                                         QStringLiteral("再见。")),
            controller.errorMessage());
    require(controller.setScriptBlockStatus(dialogueId, QStringLiteral("confirmed")),
            controller.errorMessage());
    require(controller.updateScriptBlock(dialogueId, QStringLiteral("dialogue"),
                                         QStringLiteral("等等。"))
                && controller.scriptBlocks().last().toMap()
                       .value(QStringLiteral("contentStatus")).toString()
                       == QStringLiteral("draft"),
            QStringLiteral("编辑已确认对白后没有退回草稿状态。"));
    require(controller.splitScriptBlock(dialogueId, 2), controller.errorMessage());
    require(controller.scriptBlocks().size() == 5
                && controller.scriptBlocks().at(3).toMap()
                       .value(QStringLiteral("text")).toString() == QStringLiteral("等等")
                && controller.scriptBlocks().at(4).toMap()
                       .value(QStringLiteral("text")).toString() == QStringLiteral("。"),
            QStringLiteral("对白拆分没有保留两段文本。"));
    const QString splitBlockId = controller.scriptBlocks().at(4).toMap()
                                     .value(QStringLiteral("id")).toString();
    require(controller.mergeScriptBlocks(dialogueId, splitBlockId),
            QStringLiteral("对白合并接口拒绝操作：") + controller.errorMessage());
    require(controller.scriptBlocks().size() == 4
                && controller.scriptBlocks().last().toMap()
                       .value(QStringLiteral("text")).toString() == QStringLiteral("等等。"),
            QStringLiteral("相邻对白合并后内容异常，段落数：")
                + QString::number(controller.scriptBlocks().size())
                + QStringLiteral("；末段：")
                + controller.scriptBlocks().last().toMap()
                      .value(QStringLiteral("text")).toString());

    require(controller.moveScriptBlock(dialogueId, -1), controller.errorMessage());
    QString scriptOrderAfterMove;
    for (const QVariant& block : controller.scriptBlocks()) {
        const QVariantMap item = block.toMap();
        scriptOrderAfterMove += item.value(QStringLiteral("id")).toString()
                                + QStringLiteral(":")
                                + item.value(QStringLiteral("blockType")).toString()
                                + QStringLiteral("/")
                                + QString::number(item.value(QStringLiteral("sortOrder")).toInt())
                                + QStringLiteral(" ");
    }
    require(controller.scriptBlocks().at(2).toMap()
                .value(QStringLiteral("id")).toString() == dialogueId,
            QStringLiteral("对白上移后没有调整到预期顺序：") + scriptOrderAfterMove
                + QStringLiteral("；目标：") + dialogueId);
    require(controller.moveScriptBlock(dialogueId, 1), controller.errorMessage());
    require(controller.scriptBlocks().last().toMap()
                .value(QStringLiteral("id")).toString() == dialogueId,
            QStringLiteral("对白下移后没有恢复预期顺序。"));
    require(controller.createScene(), controller.errorMessage());
    const QString temporarySceneId = controller.scenes().last().toMap()
                                         .value(QStringLiteral("id")).toString();
    require(controller.moveScene(sceneId, 1)
                && controller.scenes().first().toMap()
                       .value(QStringLiteral("id")).toString() == temporarySceneId
                && controller.moveScene(sceneId, -1)
                && controller.scenes().first().toMap()
                       .value(QStringLiteral("id")).toString() == sceneId
                && controller.removeScene(temporarySceneId),
            QStringLiteral("场次顺序调整或移除失败。"));

    require(controller.duplicateScene(sceneId), controller.errorMessage());
    QVariantList scenes = controller.scenes();
    require(scenes.size() == 2, QStringLiteral("场次复制没有创建独立场次。"));
    const QString copiedSceneId = scenes.at(1).toMap().value(QStringLiteral("id")).toString();
    require(copiedSceneId != sceneId
                && controller.scriptBlocks().size() == 7
                && controller.scriptBlocks().at(4).toMap()
                       .value(QStringLiteral("sceneId")).toString() == copiedSceneId,
            QStringLiteral("复制场次没有复制内容并重建关联身份。"));
    const QString copiedBlockId = controller.scriptBlocks().at(4).toMap()
                                      .value(QStringLiteral("id")).toString();
    require(controller.setScriptBlockStatus(copiedBlockId, QStringLiteral("confirmed")),
            controller.errorMessage());
    require(controller.duplicateEpisode(controller.currentEpisodeId()), controller.errorMessage());
    require(controller.episodes().size() == 2
                && controller.scenes().size() == 2
                && controller.scriptBlocks().size() == 7,
            QStringLiteral("复制剧集没有保留内容结构。"));
    const QString copiedEpisodeSceneId = controller.scenes().first().toMap()
                                             .value(QStringLiteral("id")).toString();
    const QString copiedEpisodeSecondSceneId = controller.scenes().at(1).toMap()
                                                   .value(QStringLiteral("id")).toString();
    const QString copiedEpisodeBlockId = controller.scriptBlocks().at(4).toMap()
                                             .value(QStringLiteral("id")).toString();
    require(copiedEpisodeSceneId != sceneId && copiedEpisodeBlockId != copiedBlockId
                && controller.scriptBlocks().at(1).toMap()
                       .value(QStringLiteral("sceneId")).toString() == copiedEpisodeSceneId
                && controller.scriptBlocks().at(4).toMap()
                       .value(QStringLiteral("sceneId")).toString() == copiedEpisodeSecondSceneId
                && controller.scriptBlocks().at(4).toMap()
                       .value(QStringLiteral("contentStatus")).toString()
                       == QStringLiteral("draft"),
            QStringLiteral("复制剧集没有重建内部身份或将副本设为草稿。"));

    QEventLoop saveWait;
    QTimer::singleShot(1200, &saveWait, &QEventLoop::quit);
    saveWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("结构化剧本和故事设定没有自动保存：") + controller.errorMessage());
    require(!controller.sceneHistory(copiedEpisodeSceneId).isEmpty()
                && !controller.scriptBlockHistory(copiedEpisodeBlockId).isEmpty(),
            QStringLiteral("场次或剧本段落没有保存独立历史。"));
    const QString projectPath = controller.currentProjectPath();
    require(controller.closeProject(), controller.errorMessage());
    require(controller.openProject(projectPath), controller.errorMessage());
    require(controller.currentProjectStory().value(QStringLiteral("storySynopsis")).toString()
                    == QStringLiteral("主角循着旧信寻找多年未见的朋友。")
                && controller.currentProjectStory().value(QStringLiteral("centralConflict")).toString()
                       == QStringLiteral("真相会修复关系，也可能结束关系。")
                && controller.currentEpisodeBrief().value(QStringLiteral("episodeOutline")).toString()
                       == QStringLiteral("第一集：收到来信。")
                && controller.scenes().first().toMap().value(QStringLiteral("goal")).toString()
                       == QStringLiteral("找到寄出的信")
                && controller.scenes().first().toMap()
                       .value(QStringLiteral("conflictOrTurn")).toString()
                       == QStringLiteral("信件内容改变双方关系")
                && controller.scenes().first().toMap()
                       .value(QStringLiteral("targetDurationSeconds")).toInt() == 45
                && controller.scriptBlocks().size() == 7,
            QStringLiteral("重开后没有恢复故事发展或结构化剧本。"));
    require(controller.closeProject(), controller.errorMessage());

    QSettings settings;
    settings.remove(QStringLiteral("library/root"));
    settings.sync();
}

void verifyQualityWorkflow()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建质量检查测试目录。"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"))),
            controller.errorMessage());
    require(controller.createProject(QStringLiteral("质量检查测试"), QString(),
                                     QStringLiteral("single_short")),
            controller.errorMessage());
    require(controller.updateDraftField(QStringLiteral("targetDuration"),
                                        QStringLiteral("1 秒")),
            controller.errorMessage());
    require(controller.updateEpisodeField(QStringLiteral("targetDuration"),
                                          QStringLiteral("1 秒")),
            controller.errorMessage());
    require(controller.createScene(), controller.errorMessage());
    const QString sceneId = controller.scenes().first().toMap()
                                .value(QStringLiteral("id")).toString();
    require(controller.updateSceneTitle(sceneId, QStringLiteral("旧车站重逢")),
            controller.errorMessage());
    require(controller.updateSceneQualityFields(sceneId, QString(), QString(),
                                                QStringLiteral("1")), controller.errorMessage());

    QVariantMap firstBlock = controller.scriptBlocks().first().toMap();
    const QString speakerBlockId = firstBlock.value(QStringLiteral("id")).toString();
    require(controller.updateScriptBlock(speakerBlockId, QStringLiteral("character"),
                                         QStringLiteral("林岚"))
                && controller.setScriptBlockScene(speakerBlockId, sceneId),
            controller.errorMessage());
    require(controller.createScriptBlock(QStringLiteral("dialogue"), sceneId),
            controller.errorMessage());
    const QString dialogueText = QStringLiteral(
        "我们今天必须找到那封信，否则就再也没有机会问清当年的真相。你准备好了吗？");
    require(controller.updateScriptBlock(controller.scriptBlocks().last().toMap()
                                             .value(QStringLiteral("id")).toString(),
                                         QStringLiteral("dialogue"), dialogueText),
            controller.errorMessage());
    require(controller.createScriptBlock(QStringLiteral("character"), sceneId)
                && controller.updateScriptBlock(controller.scriptBlocks().last().toMap()
                                                     .value(QStringLiteral("id")).toString(),
                                                 QStringLiteral("character"),
                                                 QStringLiteral("周宁"))
                && controller.createScriptBlock(QStringLiteral("dialogue"), sceneId)
                && controller.updateScriptBlock(controller.scriptBlocks().last().toMap()
                                                     .value(QStringLiteral("id")).toString(),
                                                 QStringLiteral("dialogue"), dialogueText),
            controller.errorMessage());
    for (const QString& title : {QStringLiteral("售票窗口"), QStringLiteral("站台出口")}) {
        require(controller.createScene(), controller.errorMessage());
        const QString pacingSceneId = controller.scenes().last().toMap()
                                          .value(QStringLiteral("id")).toString();
        require(controller.updateSceneTitle(pacingSceneId, title)
                    && controller.createScriptBlock(QStringLiteral("action"), pacingSceneId),
                controller.errorMessage());
        require(controller.updateScriptBlock(controller.scriptBlocks().last().toMap()
                                                 .value(QStringLiteral("id")).toString(),
                                             QStringLiteral("action"),
                                             QStringLiteral("门开了。")),
                controller.errorMessage());
    }
    require(controller.createBibleItem(QStringLiteral("character"), QStringLiteral("林岚"),
                                       QStringLiteral("主角。")),
            controller.errorMessage());
    const QString originalScript = controller.scriptText();

    controller.runQualityCheck();
    QVariantMap report = controller.qualityReport();
    const QVariantMap durationFinding = qualityFindingByCode(
        report, QStringLiteral("episode_duration_mismatch"));
    const QVariantMap sceneFinding = qualityFindingByCode(
        report, QStringLiteral("scene_goal_missing"));
    require(report.value(QStringLiteral("projectEstimate")).toMap()
                    .value(QStringLiteral("available")).toBool()
                && report.value(QStringLiteral("projectEstimate")).toMap()
                       .value(QStringLiteral("seconds")).toInt() > 1
                && !durationFinding.isEmpty()
                && durationFinding.value(QStringLiteral("reason")).toString()
                       .contains(QStringLiteral("旧车站重逢"))
                && !sceneFinding.isEmpty()
                && !qualityFindingByCode(report,
                                         QStringLiteral("scene_conflict_or_turn_missing")).isEmpty()
                && !qualityFindingByCode(report,
                                         QStringLiteral("scene_duration_mismatch")).isEmpty()
                && !qualityFindingByCode(report,
                                         QStringLiteral("project_duration_mismatch")).isEmpty()
                && !qualityFindingByCode(report,
                                         QStringLiteral("scene_pacing_outlier")).isEmpty()
                && !qualityFindingByCode(report,
                                         QStringLiteral("character_motivation_unclear")).isEmpty()
                && !qualityFindingByCode(report,
                                         QStringLiteral("character_voice_duplicate_line")).isEmpty()
                && !qualityFindingByCode(report,
                                         QStringLiteral("repeated_script_information")).isEmpty(),
            QStringLiteral("质量检查没有给出可定位的时长或剧本质量提示。"));
    require(controller.scriptText() == originalScript,
            QStringLiteral("质量检查自动修改了剧本正文。"));
    const QString goalFindingId = sceneFinding.value(QStringLiteral("id")).toString();
    require(controller.setQualityFindingStatus(goalFindingId, QStringLiteral("ignored")),
            controller.errorMessage());
    require(qualityFindingById(controller.qualityReport(), goalFindingId)
                    .value(QStringLiteral("status")).toString() == QStringLiteral("ignored"),
            QStringLiteral("忽略质量提示后没有更新当前检查结果。"));

    QEventLoop saveWait;
    QTimer::singleShot(1200, &saveWait, &QEventLoop::quit);
    saveWait.exec();
    const QString projectPath = controller.currentProjectPath();
    require(controller.closeProject(), controller.errorMessage());
    require(controller.openProject(projectPath), controller.errorMessage());
    controller.runQualityCheck();
    require(qualityFindingById(controller.qualityReport(), goalFindingId)
                    .value(QStringLiteral("status")).toString() == QStringLiteral("ignored"),
            QStringLiteral("重新打开作品后没有恢复质量提示处理状态。"));

    require(controller.updateSceneQualityFields(sceneId, QStringLiteral("找到迟来的信"),
                                                QStringLiteral("信件内容引发误会"),
                                                QStringLiteral("1")), controller.errorMessage());
    controller.runQualityCheck();
    require(qualityFindingByCodeAndScope(controller.qualityReport(),
                                         QStringLiteral("scene_goal_missing"), sceneId).isEmpty()
                && qualityFindingByCodeAndScope(controller.qualityReport(),
                                                QStringLiteral("scene_conflict_or_turn_missing"),
                                                sceneId).isEmpty(),
            QStringLiteral("补充场次目标和冲突后仍显示已解决提示。"));
    require(!controller.updateSceneQualityFields(sceneId, QStringLiteral("目标"),
                                                 QStringLiteral("冲突"),
                                                 QStringLiteral("86401")),
            QStringLiteral("超出上限的场次目标时长应被拒绝。"));
    QEventLoop finalSaveWait;
    QTimer::singleShot(1200, &finalSaveWait, &QEventLoop::quit);
    finalSaveWait.exec();
    require(controller.closeProject(), controller.errorMessage());

    QSettings settings;
    settings.remove(QStringLiteral("library/root"));
    settings.sync();
}

void verifyScriptVersionWorkflow()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建剧本版本测试目录。"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"))),
            controller.errorMessage());
    require(controller.createProject(QStringLiteral("剧本版本测试"), QString(),
                                     QStringLiteral("single_short")),
            controller.errorMessage());
    require(controller.createScene(), controller.errorMessage());
    const QString sceneId = controller.scenes().first().toMap()
                                .value(QStringLiteral("id")).toString();
    QVariantMap activeBlock = controller.scriptBlocks().first().toMap();
    const QString activeBlockId = activeBlock.value(QStringLiteral("id")).toString();
    require(controller.updateScriptBlock(activeBlockId, QStringLiteral("dialogue"),
                                         QStringLiteral("你好，世界。"))
                && controller.setScriptBlockScene(activeBlockId, sceneId),
            controller.errorMessage());

    require(controller.createScriptVersion(QStringLiteral("English"),
                                           QStringLiteral("translation")),
            controller.errorMessage());
    QVariantMap englishVersion;
    QVariantMap originalVersion;
    for (const QVariant& item : controller.scriptVersions()) {
        const QVariantMap version = item.toMap();
        if (version.value(QStringLiteral("language")).toString() == QStringLiteral("English")) {
            englishVersion = version;
        } else if (version.value(QStringLiteral("versionType")).toString()
                   == QStringLiteral("original")) {
            originalVersion = version;
        }
    }
    require(!englishVersion.isEmpty() && !originalVersion.isEmpty()
                && englishVersion.value(QStringLiteral("sourceVersionId")).toString()
                       == originalVersion.value(QStringLiteral("id")).toString(),
            QStringLiteral("译稿创建时没有建立独立的原稿来源版本。"));
    const QString englishVersionId = englishVersion.value(QStringLiteral("id")).toString();
    QVariantMap englishBlock = englishVersion.value(QStringLiteral("blocks")).toList()
                                   .first().toMap();
    const QString englishBlockId = englishBlock.value(QStringLiteral("id")).toString();
    require(englishBlock.value(QStringLiteral("sceneId")).toString() == sceneId
                && englishBlock.value(QStringLiteral("blockType")).toString()
                       == QStringLiteral("dialogue"),
            QStringLiteral("新语言版本没有保留语义类型或场次关系。"));
    require(controller.updateScriptVersionBlock(englishVersionId, englishBlockId,
                                                QStringLiteral("Hello, world.")),
            controller.errorMessage());
    require(controller.scriptText() == QStringLiteral("你好，世界。")
                && originalVersion.value(QStringLiteral("blocks")).toList()
                       .first().toMap().value(QStringLiteral("text")).toString()
                       == QStringLiteral("你好，世界。"),
            QStringLiteral("编辑独立译稿改动了原始剧本。"));
    const QString difference = controller.compareScriptVersions(QString(), englishVersionId);
    require(difference.contains(QStringLiteral("- 你好，世界。"))
                && difference.contains(QStringLiteral("+ Hello, world.")),
            QStringLiteral("版本差异没有定位到译文的增删内容。"));

    require(controller.createScriptVersion(QStringLiteral("French"),
                                           QStringLiteral("translation"),
                                           englishVersionId),
            controller.errorMessage());
    QVariantMap frenchVersion;
    for (const QVariant& item : controller.scriptVersions()) {
        if (item.toMap().value(QStringLiteral("language")).toString()
            == QStringLiteral("French")) {
            frenchVersion = item.toMap();
            break;
        }
    }
    require(!frenchVersion.isEmpty(), QStringLiteral("没有找到新建的法语版本。"));
    const QString frenchVersionId = frenchVersion.value(QStringLiteral("id")).toString();
    const QString frenchBlockId = frenchVersion.value(QStringLiteral("blocks")).toList()
                                      .first().toMap().value(QStringLiteral("id")).toString();
    require(frenchVersion.value(QStringLiteral("sourceVersionId")).toString()
                == englishVersionId
                && controller.updateScriptVersionBlock(frenchVersionId, frenchBlockId,
                                                      QStringLiteral("Bonjour, le monde."))
                && controller.setScriptVersionStatus(frenchVersionId,
                                                     QStringLiteral("confirmed")),
            QStringLiteral("派生语言版本无法编辑或确认。"));
    require(controller.setScriptVersionStatus(englishVersionId, QStringLiteral("archived"))
                && !controller.updateScriptVersionBlock(englishVersionId, englishBlockId,
                                                        QStringLiteral("不应写入")),
            QStringLiteral("归档版本仍允许编辑。"));
    require(controller.restoreScriptVersion(frenchVersionId), controller.errorMessage());
    require(controller.scriptText() == QStringLiteral("Bonjour, le monde.")
                && controller.currentEpisodeBrief().value(QStringLiteral("language")).toString()
                       == QStringLiteral("French"),
            QStringLiteral("确认恢复没有应用所选版本内容和语言。"));

    QEventLoop saveWait;
    QTimer::singleShot(1200, &saveWait, &QEventLoop::quit);
    saveWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("剧本版本没有自动保存：") + controller.errorMessage());
    require(!controller.scriptVersionHistory(frenchVersionId).isEmpty(),
            QStringLiteral("剧本版本没有记录独立修改历史。"));
    int preservedCurrentSnapshots = 0;
    for (const QVariant& value : controller.scriptVersions()) {
        const QVariantMap version = value.toMap();
        if (version.value(QStringLiteral("versionType")).toString()
                == QStringLiteral("user_edit")
            && version.value(QStringLiteral("contentStatus")).toString()
                   == QStringLiteral("confirmed")
            && version.value(QStringLiteral("text")).toString()
                   == QStringLiteral("你好，世界。")) {
            ++preservedCurrentSnapshots;
        }
    }
    require(preservedCurrentSnapshots == 1,
            QStringLiteral("恢复前的当前剧本没有先保留为可追溯版本。"));

    const QString projectPath = controller.currentProjectPath();
    require(controller.closeProject(), controller.errorMessage());
    require(controller.openProject(projectPath), controller.errorMessage());
    QVariantMap restoredEnglish;
    QVariantMap restoredFrench;
    for (const QVariant& item : controller.scriptVersions()) {
        const QVariantMap version = item.toMap();
        if (version.value(QStringLiteral("language")).toString() == QStringLiteral("English")) {
            restoredEnglish = version;
        } else if (version.value(QStringLiteral("language")).toString()
                   == QStringLiteral("French")) {
            restoredFrench = version;
        }
    }
    require(controller.scriptText() == QStringLiteral("Bonjour, le monde.")
                && controller.currentEpisodeBrief().value(QStringLiteral("language")).toString()
                       == QStringLiteral("French")
                && controller.scriptVersions().size() == 4
                && restoredEnglish.value(QStringLiteral("contentStatus")).toString()
                       == QStringLiteral("archived")
                && restoredFrench.value(QStringLiteral("sourceVersionId")).toString()
                       == englishVersionId
                && restoredFrench.value(QStringLiteral("contentStatus")).toString()
                       == QStringLiteral("confirmed")
                && !restoredEnglish.isEmpty() && !restoredFrench.isEmpty(),
            QStringLiteral("关闭重开后语言版本、来源链或恢复内容丢失。"));
    require(controller.closeProject(), controller.errorMessage());

    QSettings settings;
    settings.remove(QStringLiteral("library/root"));
    settings.sync();
}

void verifySaveStatusTransitions()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建状态测试目录。"));

    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"))),
            controller.errorMessage());
    require(controller.createProject(QStringLiteral("保存状态测试"),
                                     QStringLiteral("两个多年未见的朋友在车站重逢。")),
            controller.errorMessage());
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("新作品状态应为已保存。"));
    require(controller.briefDescription()
                == QStringLiteral("两个多年未见的朋友在车站重逢。"),
            QStringLiteral("创建作品时没有保留一句创意。"));

    const QString projectPath = controller.currentProjectPath();
    require(controller.updateDraftField(QStringLiteral("genre"), QStringLiteral("剧情悬疑")),
            QStringLiteral("无法更新创作简报字段。"));
    require(controller.updateDraftField(QStringLiteral("scriptText"),
                                        QStringLiteral("外景，站台 - 夜\n两人隔着人群认出彼此。")),
            QStringLiteral("无法更新剧本正文。"));
    require(controller.saveStatus() == QStringLiteral("unsaved"),
            QStringLiteral("编辑后没有显示未保存状态。"));

    QEventLoop saveWait;
    QTimer::singleShot(1100, &saveWait, &QEventLoop::quit);
    saveWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("离线自动保存没有完成。"));
    require(controller.lastSavedAt() != QStringLiteral("暂无保存记录"),
            QStringLiteral("自动保存后没有更新最近保存状态。"));
    require(controller.closeProject(), controller.errorMessage());
    require(controller.openProject(projectPath), controller.errorMessage());
    require(controller.briefGenre() == QStringLiteral("剧情悬疑")
                && controller.scriptText()
                       == QStringLiteral("外景，站台 - 夜\n两人隔着人群认出彼此。"),
            QStringLiteral("作品重新打开后没有恢复自动保存的内容。"));

    const QString externalConnectionName = QStringLiteral("save-failure-lock-")
                                           + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase externalWriter = QSqlDatabase::addDatabase(
        "QSQLITE", externalConnectionName);
    externalWriter.setDatabaseName(QDir(projectPath).filePath(QStringLiteral("project.sqlite")));
    externalWriter.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=0"));
    require(externalWriter.open(), externalWriter.lastError().text());
    {
        QSqlQuery acquireWriteLock(externalWriter);
        require(acquireWriteLock.exec(QStringLiteral("BEGIN IMMEDIATE")),
                acquireWriteLock.lastError().text());
    }
    const QString unsavedText = QStringLiteral("保存锁定期间仍留在内存中的剧本草稿。");
    require(controller.updateDraftField(QStringLiteral("scriptText"), unsavedText),
            QStringLiteral("无法准备保存失败测试内容。"));
    QEventLoop failedSaveWait;
    QTimer::singleShot(2400, &failedSaveWait, &QEventLoop::quit);
    failedSaveWait.exec();
    require(controller.saveStatus() == QStringLiteral("failed")
                && controller.scriptText() == unsavedText
                && !controller.errorMessage().isEmpty(),
            QStringLiteral("保存被锁阻塞后没有保留草稿并报告失败。"));
    require(!controller.closeProject(),
            QStringLiteral("保存失败后不应允许关闭并丢弃草稿。"));
    {
        QSqlQuery releaseWriteLock(externalWriter);
        require(releaseWriteLock.exec(QStringLiteral("ROLLBACK")),
                releaseWriteLock.lastError().text());
    }
    externalWriter.close();
    externalWriter = QSqlDatabase();
    QSqlDatabase::removeDatabase(externalConnectionName);

    controller.retrySave();
    QEventLoop retryWait;
    QTimer::singleShot(1200, &retryWait, &QEventLoop::quit);
    retryWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("释放写锁后不能重试保存。"));

    const QString capacityConnectionName = QStringLiteral("save-failure-simulation-")
                                           + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase capacityConnection = QSqlDatabase::addDatabase(
        "QSQLITE", capacityConnectionName);
    capacityConnection.setDatabaseName(QDir(projectPath).filePath(QStringLiteral("project.sqlite")));
    require(capacityConnection.open(), capacityConnection.lastError().text());
    {
        QSqlQuery simulateCapacityError(capacityConnection);
        require(simulateCapacityError.exec(QStringLiteral(
                    "CREATE TRIGGER simulate_disk_full BEFORE UPDATE ON episode_script_draft "
                    "BEGIN SELECT RAISE(ABORT, 'database or disk is full'); END")),
                simulateCapacityError.lastError().text());
    }
    const QString capacityLimitedText = QStringLiteral("磁盘空间不足时仍需保留的剧本草稿。");
    require(controller.updateDraftField(QStringLiteral("scriptText"), capacityLimitedText),
            QStringLiteral("无法准备容量不足时的保存失败测试内容。"));
    QEventLoop capacityFailureWait;
    QTimer::singleShot(2400, &capacityFailureWait, &QEventLoop::quit);
    capacityFailureWait.exec();
    require(controller.saveStatus() == QStringLiteral("failed")
                && controller.scriptText() == capacityLimitedText
                && controller.errorMessage().contains(QStringLiteral("full"), Qt::CaseInsensitive),
            QStringLiteral("模拟 SQLite 空间不足错误时未保留草稿或未报告保存错误：")
                + controller.errorMessage());
    {
        QSqlQuery removeCapacitySimulation(capacityConnection);
        require(removeCapacitySimulation.exec(QStringLiteral(
                    "DROP TRIGGER simulate_disk_full")),
                removeCapacitySimulation.lastError().text());
    }
    capacityConnection.close();
    capacityConnection = QSqlDatabase();
    QSqlDatabase::removeDatabase(capacityConnectionName);
    controller.retrySave();
    QEventLoop capacityRetryWait;
    QTimer::singleShot(2400, &capacityRetryWait, &QEventLoop::quit);
    capacityRetryWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("解除模拟空间限制后无法保存保留的草稿：")
                + controller.errorMessage());

    controller.markUnsaved();
    require(controller.saveStatus() == QStringLiteral("unsaved"),
            QStringLiteral("未保存状态转换失败。"));
    require(!controller.closeProject() && controller.hasOpenProject(),
            QStringLiteral("存在未保存内容时不应关闭作品。"));

    controller.markSaving();
    require(controller.saveStatus() == QStringLiteral("saving"),
            QStringLiteral("保存中状态转换失败。"));
    controller.markSaveFailed(QStringLiteral("测试保存失败"));
    require(controller.saveStatus() == QStringLiteral("failed"),
            QStringLiteral("保存失败状态转换失败。"));
    require(!controller.closeProject(), QStringLiteral("保存失败后不应静默关闭作品。"));

    controller.markSaved();
    require(controller.closeProject(), controller.errorMessage());
    require(controller.saveStatus() == QStringLiteral("closed"),
            QStringLiteral("关闭作品后的状态错误。"));
    require(controller.openProject(projectPath), controller.errorMessage());
    require(controller.scriptText() == capacityLimitedText,
            QStringLiteral("容量错误解除并重试后，作品重开未恢复已保存的草稿。"));
    require(controller.closeProject(), controller.errorMessage());

    QSettings settings;
    settings.remove(QStringLiteral("library/root"));
    settings.sync();
}

void verifySeriesWorkflow()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建多集作品测试目录。"));

    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"))),
            controller.errorMessage());
    require(controller.createProject(QStringLiteral("海边来信"),
                                     QStringLiteral("一段跨越数年的故事。"),
                                     QStringLiteral("series")),
            controller.errorMessage());
    require(controller.currentProjectType() == QStringLiteral("series")
                && controller.episodes().size() == 1,
            QStringLiteral("多集短剧没有建立首集结构。"));

    const QString firstEpisodeId = controller.currentEpisodeId();
    require(controller.createBibleItem(QStringLiteral("character"),
                                       QStringLiteral("林岚"),
                                       QStringLiteral("全剧主角，始终保留这段简介。")),
            controller.errorMessage());
    const QString characterId = bibleItemIdByName(controller.bibleItems(), QStringLiteral("林岚"));
    require(controller.updateBibleItemDetail(characterId, QStringLiteral("goal"),
                                             QStringLiteral("找回失踪的妹妹。"))
                && controller.updateBibleItemDetail(characterId, QStringLiteral("speechStyle"),
                                                    QStringLiteral("短句，克制。"))
                && controller.addBibleItemReference(characterId,
                                                    QStringLiteral("笔记/人物设定.md"),
                                                    QStringLiteral("人物动机来源"))
                && controller.setBibleItemCertaintyStatus(characterId,
                                                          QStringLiteral("confirmed")),
            QStringLiteral("角色的详细设定、参考资料或状态无法维护。"));
    require(!controller.updateBibleItemDetail(characterId, QStringLiteral("timePeriod"),
                                              QStringLiteral("不适用于角色的字段。")),
            QStringLiteral("不匹配设定类型的创作圣经字段不应被保存。"));
    require(!controller.addBibleItemReference(characterId, QStringLiteral("  "),
                                              QStringLiteral("缺少来源")),
            QStringLiteral("缺少来源的参考资料不应被保存。"));
    require(controller.createBibleItem(QStringLiteral("location"),
                                       QStringLiteral("旧海港"),
                                       QStringLiteral("全剧共享地点。")),
            controller.errorMessage());
    const QString locationId = bibleItemIdByName(controller.bibleItems(), QStringLiteral("旧海港"));
    require(controller.setEpisodeBibleReference(characterId, true)
                && controller.setEpisodeBibleReference(locationId, true),
            QStringLiteral("无法将全剧角色和地点关联到首集。"));
    require(controller.updateEpisodeField(QStringLiteral("genre"), QStringLiteral("成长")),
            controller.errorMessage());
    require(controller.updateDraftField(QStringLiteral("scriptText"),
                                        QStringLiteral("第一集的故事正文。")),
            controller.errorMessage());
    require(controller.createScene(), controller.errorMessage());
    const QString firstSceneId = controller.scenes().first().toMap()
                                     .value(QStringLiteral("id")).toString();

    require(controller.createEpisode(QStringLiteral("第二集")), controller.errorMessage());
    const QString secondEpisodeId = controller.currentEpisodeId();
    require(controller.setEpisodeBibleReference(characterId, true),
            QStringLiteral("同一全剧角色无法关联到第二集。"));
    require(controller.updateEpisodeField(QStringLiteral("theme"), QStringLiteral("重新选择")),
            controller.errorMessage());
    require(controller.updateDraftField(QStringLiteral("scriptText"),
                                        QStringLiteral("第二集的故事正文。")),
            controller.errorMessage());
    require(controller.createScene(), controller.errorMessage());
    require(controller.moveEpisode(secondEpisodeId, -1), controller.errorMessage());
    require(controller.renameEpisode(secondEpisodeId, QStringLiteral("第 2 集：重逢")),
            controller.errorMessage());
    const QStringList bibleOrderBeforeReopen = bibleItemOrder(controller.bibleItems());

    QEventLoop saveWait;
    QTimer::singleShot(1100, &saveWait, &QEventLoop::quit);
    saveWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("多集结构的自动保存没有完成。"));
    const QVariantMap characterItem = bibleItemById(controller.bibleItems(), characterId);
    require(characterItem.value(QStringLiteral("certaintyStatus")).toString()
                    == QStringLiteral("confirmed")
                && characterItem.value(QStringLiteral("details")).toMap()
                       .value(QStringLiteral("goal")).toString()
                       == QStringLiteral("找回失踪的妹妹。")
                && characterItem.value(QStringLiteral("references")).toList().size() == 1,
            QStringLiteral("创作圣经详细设定、状态或资料没有保存。"));
    require(!controller.bibleItemHistory(characterId).isEmpty(),
            QStringLiteral("创作圣经设定没有生成修改历史。"));
    require(controller.episodes().first().toMap().value(QStringLiteral("id")).toString()
                == secondEpisodeId,
            QStringLiteral("集顺序调整没有反映在剧集列表中。"));
    require(!controller.projectHistory().isEmpty() && !controller.episodeHistory().isEmpty(),
            QStringLiteral("作品或当前剧集没有保存修改历史。"));

    require(controller.setProjectContentConfirmed(true)
                && controller.setEpisodeContentConfirmed(true),
            QStringLiteral("不能标记作品和剧集内容已确认。"));
    QEventLoop confirmationWait;
    QTimer::singleShot(900, &confirmationWait, &QEventLoop::quit);
    confirmationWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved")
                && controller.currentProjectContentStatus() == QStringLiteral("confirmed")
                && controller.currentEpisodeContentStatus() == QStringLiteral("confirmed"),
            QStringLiteral("作品和剧集确认状态没有独立保存。"));
    require(controller.updateDraftField(QStringLiteral("scriptText"),
                                        QStringLiteral("修改后的剧本内容。")),
            controller.errorMessage());
    require(controller.currentProjectContentStatus() == QStringLiteral("draft")
                && controller.currentEpisodeContentStatus() == QStringLiteral("draft"),
            QStringLiteral("确认后修改内容没有自动退回草稿状态。"));
    QEventLoop editedSaveWait;
    QTimer::singleShot(900, &editedSaveWait, &QEventLoop::quit);
    editedSaveWait.exec();

    const QString projectPath = controller.currentProjectPath();
    require(controller.selectEpisode(firstEpisodeId), controller.errorMessage());
    require(controller.scriptText() == QStringLiteral("第一集的故事正文。")
                && controller.currentEpisodeBrief().value(QStringLiteral("genre")).toString()
                       == QStringLiteral("成长")
                && controller.scenes().first().toMap().value(QStringLiteral("id")).toString()
                       == firstSceneId
                && bibleItemIsReferenced(controller.bibleItems(), characterId)
                && bibleItemIsReferenced(controller.bibleItems(), locationId),
            QStringLiteral("切换剧集时剧本、单集简报或场次身份发生串集。"));
    require(controller.closeProject(), controller.errorMessage());
    require(controller.openProject(projectPath), controller.errorMessage());
    require(controller.episodes().size() == 2
                && controller.currentProjectContentStatus() == QStringLiteral("draft"),
            QStringLiteral("重新打开后没有恢复多集结构。"));
    require(bibleItemOrder(controller.bibleItems()) == bibleOrderBeforeReopen,
            QStringLiteral("重开作品后全剧角色和地点列表顺序发生变化。"));
    const QVariantMap restoredCharacter = bibleItemById(controller.bibleItems(), characterId);
    require(restoredCharacter.value(QStringLiteral("certaintyStatus")).toString()
                    == QStringLiteral("confirmed")
                && restoredCharacter.value(QStringLiteral("details")).toMap()
                       .value(QStringLiteral("speechStyle")).toString()
                       == QStringLiteral("短句，克制。")
                && restoredCharacter.value(QStringLiteral("references")).toList().size() == 1,
            QStringLiteral("重新打开后创作圣经的状态、详情或参考资料丢失。"));
    require(!controller.setBibleItemCertaintyStatus(characterId,
                                                    QStringLiteral("unknown")),
            QStringLiteral("无效的创作圣经状态不应被接受。"));
    require(controller.setBibleItemCertaintyStatus(characterId,
                                                  QStringLiteral("immutable"))
                && !controller.updateBibleItemDetail(characterId, QStringLiteral("goal"),
                                                      QStringLiteral("禁止改变后的内容"))
                && !controller.removeBibleItem(characterId),
            QStringLiteral("禁止改变状态没有阻止直接修改或删除设定。"));
    require(controller.setBibleItemCertaintyStatus(characterId,
                                                  QStringLiteral("confirmed")),
            QStringLiteral("无法将创作圣经设定从禁止改变恢复为确定。"));
    require(controller.selectEpisode(secondEpisodeId), controller.errorMessage());
    require(controller.currentEpisodeTitle() == QStringLiteral("第 2 集：重逢"),
            QStringLiteral("重新打开后剧集标题没有保留。"));
    require(controller.scriptText() == QStringLiteral("修改后的剧本内容。"),
            QStringLiteral("重新打开后单集剧本没有独立保存。"));
    require(controller.currentEpisodeBrief().value(QStringLiteral("theme")).toString()
                == QStringLiteral("重新选择"),
            QStringLiteral("重新打开后单集简报没有独立保存。"));
    require(controller.currentEpisodeContentStatus() == QStringLiteral("draft"),
            QStringLiteral("重新打开后剧集草稿状态没有保留。"));
    require(bibleItemIsReferenced(controller.bibleItems(), characterId)
                && !bibleItemIsReferenced(controller.bibleItems(), locationId),
            QStringLiteral("重新打开后全剧设定与剧集的关联不正确。"));

    require(controller.removeBibleItem(characterId),
            QStringLiteral("无法删除全剧角色引用。"));
    require(controller.bibleItems().size() == 1
                && !controller.bibleItems().first().toMap()
                        .value(QStringLiteral("referenced")).toBool(),
            QStringLiteral("删除全剧角色后，当前集仍显示失效引用。"));
    QEventLoop referenceRemovalWait;
    QTimer::singleShot(1000, &referenceRemovalWait, &QEventLoop::quit);
    referenceRemovalWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("删除全剧角色及跨集引用没有保存：") + controller.errorMessage());
    require(controller.closeProject(), controller.errorMessage());
    require(controller.openProject(projectPath), controller.errorMessage());
    require(controller.bibleItems().size() == 1
                && controller.bibleItems().first().toMap()
                       .value(QStringLiteral("id")).toString() == locationId,
            QStringLiteral("重新打开后全剧地点未保留或已删除角色被恢复。"));
    require(controller.selectEpisode(firstEpisodeId), controller.errorMessage());
    require(controller.bibleItems().first().toMap()
                .value(QStringLiteral("referenced")).toBool(),
            QStringLiteral("重新打开后全剧地点与首集的关联丢失。"));
    require(controller.selectEpisode(secondEpisodeId), controller.errorMessage());
    require(!controller.bibleItems().first().toMap()
                 .value(QStringLiteral("referenced")).toBool(),
            QStringLiteral("全剧地点被意外关联到未选择的剧集。"));
    require(controller.removeEpisode(firstEpisodeId), controller.errorMessage());
    require(controller.episodes().size() == 1
                && controller.currentEpisodeId() == secondEpisodeId,
            QStringLiteral("移除剧集后作品结构或当前选择异常。"));
    require(controller.closeProject() == false,
            QStringLiteral("移除剧集后未保存状态应阻止关闭。"));
    controller.retrySave();
    QEventLoop deletionSaveWait;
    QTimer::singleShot(1200, &deletionSaveWait, &QEventLoop::quit);
    deletionSaveWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("移除剧集后的结构没有保存。"));
    require(controller.closeProject(), controller.errorMessage());

    QSettings settings;
    settings.remove(QStringLiteral("library/root"));
    settings.sync();
}

void verifyStoryImpactWorkflow()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建设定影响测试目录。"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"))),
            controller.errorMessage());
    require(controller.createProject(QStringLiteral("回声之后"), QString()),
            controller.errorMessage());

    const QString characterName = QStringLiteral("林岚");
    require(controller.createBibleItem(QStringLiteral("character"), characterName,
                                       QStringLiteral("暂定主角设定。")),
            controller.errorMessage());
    const QString characterId = bibleItemIdByName(controller.bibleItems(), characterName);
    require(controller.createBibleItem(QStringLiteral("character"), QStringLiteral("周远"),
                                       QStringLiteral("剧本中尚未提及的角色。")),
            controller.errorMessage());
    const QString unmentionedCharacterId = bibleItemIdByName(controller.bibleItems(),
                                                               QStringLiteral("周远"));
    require(controller.setEpisodeBibleReference(characterId, true),
            QStringLiteral("角色设定无法关联到当前剧集。"));
    require(controller.setEpisodeBibleReference(unmentionedCharacterId, true),
            QStringLiteral("未出现在剧本中的角色设定无法关联到当前剧集。"));
    require(controller.updateBibleItemDetail(characterId, QStringLiteral("goal"),
                                             QStringLiteral("找回妹妹。")),
            QStringLiteral("无法建立角色设定基线。"));
    require(controller.createScene(), controller.errorMessage());
    const QString sceneId = controller.scenes().first().toMap()
                                .value(QStringLiteral("id")).toString();
    const QVariantList initialBlocks = controller.scriptBlocks();
    require(!initialBlocks.isEmpty(), QStringLiteral("新作品缺少初始剧本段落。"));
    const QString blockId = initialBlocks.first().toMap()
                                .value(QStringLiteral("id")).toString();
    require(controller.updateScriptBlock(blockId, QStringLiteral("dialogue"),
                                         QStringLiteral("林岚：我会找回妹妹。"))
                && controller.setScriptBlockScene(blockId, sceneId),
            QStringLiteral("无法建立可追踪的角色对白。"));
    QEventLoop initialSaveWait;
    QTimer::singleShot(1100, &initialSaveWait, &QEventLoop::quit);
    initialSaveWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("设定影响测试的基线内容没有保存。"));

    require(controller.updateBibleItemDetail(characterId, QStringLiteral("goal"),
                                             QStringLiteral("守护港口的孩子。"))
                && controller.setBibleItemCertaintyStatus(characterId,
                                                          QStringLiteral("confirmed"))
                && controller.updateBibleItemDetail(unmentionedCharacterId,
                                                    QStringLiteral("goal"),
                                                    QStringLiteral("寻找失联的哥哥。"))
                && controller.setBibleItemCertaintyStatus(unmentionedCharacterId,
                                                          QStringLiteral("confirmed")),
            QStringLiteral("角色设定无法从暂定变为确定。"));
    QEventLoop impactSaveWait;
    QTimer::singleShot(1100, &impactSaveWait, &QEventLoop::quit);
    impactSaveWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("设定变化和影响记录没有保存：") + controller.errorMessage());

    QVariantMap locatedImpact;
    QVariantMap unknownCoverage;
    bool linkedButUnlocated = false;
    for (const QVariant& value : controller.storyImpacts()) {
        const QVariantMap impact = value.toMap();
        if (impact.value(QStringLiteral("targetType")).toString()
            == QStringLiteral("script_block")) {
            locatedImpact = impact;
        } else if (impact.value(QStringLiteral("targetType")).toString()
                   == QStringLiteral("unsupported_asset")) {
            unknownCoverage = impact;
        } else if (impact.value(QStringLiteral("targetType")).toString()
                       == QStringLiteral("episode")
                   && impact.value(QStringLiteral("sourceItemId")).toString()
                          == unmentionedCharacterId
                   && impact.value(QStringLiteral("assessmentStatus")).toString()
                          == QStringLiteral("unassessed")) {
            linkedButUnlocated = true;
        }
    }
    require(!locatedImpact.isEmpty()
                && locatedImpact.value(QStringLiteral("sourceItemId")).toString() == characterId
                && locatedImpact.value(QStringLiteral("episodeId")).toString()
                       == controller.currentEpisodeId()
                && locatedImpact.value(QStringLiteral("sceneId")).toString() == sceneId
                && locatedImpact.value(QStringLiteral("scriptBlockId")).toString() == blockId
                && locatedImpact.value(QStringLiteral("assessmentStatus")).toString()
                       == QStringLiteral("located")
                && locatedImpact.value(QStringLiteral("changeSummary")).toString().contains(
                       QStringLiteral("可能与新设定不一致")),
            QStringLiteral("角色设定变化没有定位到稳定的剧集、场次和剧本段落身份。"));
    require(!unknownCoverage.isEmpty()
                && unknownCoverage.value(QStringLiteral("assessmentStatus")).toString()
                       == QStringLiteral("unassessed"),
            QStringLiteral("尚未支持的分镜和视觉资产范围没有明确标为未能判定。"));
    require(linkedButUnlocated,
            QStringLiteral("已关联角色但未匹配到字面文本时，未将影响范围标为未能判定。"));
    const QString unassessedImpactId = unknownCoverage.value(QStringLiteral("id")).toString();
    require(!controller.setStoryImpactReviewed(unassessedImpactId, true),
            QStringLiteral("未能判定的影响不应标记为已复核。"));
    require(!controller.setStoryImpactIntentional(
                unassessedImpactId, true,
                QStringLiteral("尚未定位，不能确认例外。")),
            QStringLiteral("未能判定的影响不应标记为有意例外。"));
    const QString impactId = locatedImpact.value(QStringLiteral("id")).toString();
    require(controller.setStoryImpactReviewed(impactId, true), controller.errorMessage());
    const QVariantList reviewedImpacts = controller.storyImpacts();
    require(std::any_of(reviewedImpacts.cbegin(), reviewedImpacts.cend(),
                [&impactId](const QVariant& value) {
                    const QVariantMap impact = value.toMap();
                    return impact.value(QStringLiteral("id")).toString() == impactId
                           && impact.value(QStringLiteral("reviewStatus")).toString()
                                  == QStringLiteral("reviewed");
                }),
            QStringLiteral("普通影响提示无法标记为已复核。"));
    require(controller.setStoryImpactReviewed(impactId, false), controller.errorMessage());
    require(!controller.setStoryImpactIntentional(
                impactId, true, QString()),
            QStringLiteral("未填写理由时不应确认有意例外。"));
    require(controller.setStoryImpactIntentional(
                impactId, true,
                QStringLiteral("此处故意让角色隐瞒真实目标。")),
            controller.errorMessage());

    const QString projectPath = controller.currentProjectPath();
    require(controller.closeProject(), controller.errorMessage());
    require(controller.openProject(projectPath), controller.errorMessage());
    bool restoredIntentionalReview = false;
    for (const QVariant& value : controller.storyImpacts()) {
        const QVariantMap impact = value.toMap();
        if (impact.value(QStringLiteral("id")).toString() == impactId
            && impact.value(QStringLiteral("reviewStatus")).toString()
                   == QStringLiteral("intentional")
            && impact.value(QStringLiteral("reason")).toString()
                   == QStringLiteral("此处故意让角色隐瞒真实目标。")) {
            restoredIntentionalReview = true;
        }
    }
    require(restoredIntentionalReview,
            QStringLiteral("有意例外及其理由没有在重开后保留。"));
    require(controller.closeProject(), controller.errorMessage());

    QSettings settings;
    settings.remove(QStringLiteral("library/root"));
    settings.sync();
}

void verifyShotAndVisualReferenceWorkflow()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建镜头与视觉参考测试目录。"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"))),
            controller.errorMessage());
    require(controller.createProject(QStringLiteral("镜头关联测试"), QString()),
            controller.errorMessage());
    require(controller.createBibleItem(QStringLiteral("character"), QStringLiteral("林岚"),
                                       QStringLiteral("主角")),
            controller.errorMessage());
    const QString itemId = bibleItemIdByName(controller.bibleItems(), QStringLiteral("林岚"));
    require(controller.updateBibleItemDetail(itemId, QStringLiteral("appearance"),
                                             QStringLiteral("蓝色外套")),
            controller.errorMessage());
    require(controller.createScene(), controller.errorMessage());
    const QString sceneId = controller.scenes().first().toMap()
                                .value(QStringLiteral("id")).toString();
    const QString blockId = controller.scriptBlocks().first().toMap()
                                .value(QStringLiteral("id")).toString();
    require(controller.updateScriptBlock(blockId, QStringLiteral("action"),
                                         QStringLiteral("林岚走进旧车站。"))
                && controller.setScriptBlockScene(blockId, sceneId),
            QStringLiteral("无法建立镜头来源剧本段落。"));
    require(controller.createShot(sceneId), controller.errorMessage());
    const QVariantMap shot = controller.shots().first().toMap();
    const QString shotId = shot.value(QStringLiteral("id")).toString();
    require(controller.updateShot(shotId, QStringLiteral("车站入口"),
                                  QStringLiteral("中景，人物逆光进入画面。"))
                && controller.setShotScriptBlock(shotId, blockId, true)
                && controller.setShotBibleReference(shotId, itemId, true),
            QStringLiteral("无法建立镜头关联和镜头描述。"));
    require(controller.createVisualReference(sceneId), controller.errorMessage());
    const QString referenceId = controller.visualReferences().first().toMap()
                                    .value(QStringLiteral("id")).toString();
    require(controller.updateVisualReference(referenceId, QStringLiteral("林岚外观基准"),
                                             QStringLiteral("保持蓝色外套与短发。"),
                                             QStringLiteral("character_appearance"))
                && controller.setVisualReferenceShot(referenceId, shotId, true)
                && controller.setVisualReferenceBibleItem(referenceId, itemId, true),
            QStringLiteral("无法建立视觉参考和多镜头复用关系。"));
    QEventLoop firstSaveWait;
    QTimer::singleShot(1100, &firstSaveWait, &QEventLoop::quit);
    firstSaveWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("镜头与视觉参考没有保存：") + controller.errorMessage());
    const QString projectPath = controller.currentProjectPath();
    require(controller.closeProject(), controller.errorMessage());
    require(controller.openProject(projectPath), controller.errorMessage());
    require(controller.shots().size() == 1
                && controller.shots().first().toMap().value(QStringLiteral("id")).toString()
                       == shotId
                && controller.shots().first().toMap()
                       .value(QStringLiteral("scriptBlockIds")).toStringList().contains(blockId)
                && controller.shots().first().toMap()
                       .value(QStringLiteral("bibleItemIds")).toStringList().contains(itemId)
                && controller.visualReferences().size() == 1
                && controller.visualReferences().first().toMap()
                       .value(QStringLiteral("shotIds")).toStringList().contains(shotId)
                && controller.visualReferences().first().toMap()
                       .value(QStringLiteral("bibleItemIds")).toStringList().contains(itemId),
            QStringLiteral("重开作品后镜头、文字参考或稳定关联丢失。"));

    require(controller.updateScriptBlock(blockId, QStringLiteral("action"),
                                         QStringLiteral("林岚脱下蓝色外套，走进旧车站。")),
            controller.errorMessage());
    QEventLoop scriptImpactWait;
    QTimer::singleShot(1100, &scriptImpactWait, &QEventLoop::quit);
    scriptImpactWait.exec();
    bool scriptShotImpact = false;
    bool scriptReferenceImpact = false;
    for (const QVariant& value : controller.storyImpacts()) {
        const QVariantMap impact = value.toMap();
        if (impact.value(QStringLiteral("sourceItemId")).toString() != blockId) {
            continue;
        }
        scriptShotImpact = scriptShotImpact
            || (impact.value(QStringLiteral("targetType")).toString() == QStringLiteral("shot")
                && impact.value(QStringLiteral("shotId")).toString() == shotId);
        scriptReferenceImpact = scriptReferenceImpact
            || (impact.value(QStringLiteral("targetType")).toString()
                    == QStringLiteral("visual_reference")
                && impact.value(QStringLiteral("visualReferenceId")).toString()
                       == referenceId);
    }
    require(scriptShotImpact && scriptReferenceImpact,
            QStringLiteral("剧本段落变化未定位关联镜头和文字参考。"));

    require(controller.updateBibleItemDetail(itemId, QStringLiteral("appearance"),
                                             QStringLiteral("红色风衣")),
            controller.errorMessage());
    QEventLoop bibleImpactWait;
    QTimer::singleShot(1100, &bibleImpactWait, &QEventLoop::quit);
    bibleImpactWait.exec();
    bool bibleShotImpact = false;
    bool bibleReferenceImpact = false;
    for (const QVariant& value : controller.storyImpacts()) {
        const QVariantMap impact = value.toMap();
        if (impact.value(QStringLiteral("sourceItemId")).toString() != itemId) {
            continue;
        }
        bibleShotImpact = bibleShotImpact
            || (impact.value(QStringLiteral("targetType")).toString() == QStringLiteral("shot")
                && impact.value(QStringLiteral("shotId")).toString() == shotId);
        bibleReferenceImpact = bibleReferenceImpact
            || (impact.value(QStringLiteral("targetType")).toString()
                    == QStringLiteral("visual_reference")
                && impact.value(QStringLiteral("visualReferenceId")).toString()
                       == referenceId);
    }
    require(bibleShotImpact && bibleReferenceImpact,
            QStringLiteral("创作圣经变化未定位关联镜头和文字参考。"));
}

void verifySchemaEightToNineMigration()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建 schema v8 迁移测试目录。"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"))),
            controller.errorMessage());
    require(controller.createProject(QStringLiteral("迁移验证"), QString()),
            controller.errorMessage());
    const QString projectPath = controller.currentProjectPath();
    require(controller.closeProject(), controller.errorMessage());

    const QString connectionName = QStringLiteral("schema-eight-fixture-")
                                   + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        database.setDatabaseName(QDir(projectPath).filePath(QStringLiteral("project.sqlite")));
        require(database.open(), QStringLiteral("无法打开 schema v8 测试数据库。"));
        QSqlQuery query(database);
        dropSchemaElevenTables(query);
        require(query.exec(QStringLiteral("DROP TABLE story_impact_findings"))
                    && query.exec(QStringLiteral("ALTER TABLE project_info DROP COLUMN archived"))
                    && query.exec(QStringLiteral("PRAGMA user_version=8")),
                QStringLiteral("无法准备 schema v8 测试数据库。"));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    require(controller.openProject(projectPath), controller.errorMessage());
    require(controller.storyImpacts().isEmpty(),
            QStringLiteral("schema v8 迁移后影响记录表不能正常读取。"));
    require(controller.closeProject(), controller.errorMessage());

    const QString checkConnection = QStringLiteral("schema-nine-check-")
                                   + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", checkConnection);
        database.setDatabaseName(QDir(projectPath).filePath(QStringLiteral("project.sqlite")));
        require(database.open(), QStringLiteral("无法校验迁移后的作品数据库。"));
        QSqlQuery query(database);
        require(query.exec(QStringLiteral("PRAGMA user_version")) && query.next()
                    && query.value(0).toInt() == 13,
                QStringLiteral("schema v8 作品没有迁移到 schema v13。"));
        database.close();
    }
    QSqlDatabase::removeDatabase(checkConnection);
    const QFileInfoList backups = QDir(QDir(projectPath).filePath(
                                          QStringLiteral("backups/migrations")))
                                      .entryInfoList(
        {QStringLiteral("before-schema-9-*.sqlite")},
        QDir::Files, QDir::Name);
    require(!backups.isEmpty(),
            QStringLiteral("schema v8 到 v9 迁移前没有生成可校验备份。"));

    QSettings settings;
    settings.remove(QStringLiteral("library/root"));
    settings.sync();
}

void verifyProjectPackageWorkflow()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建资料包测试目录。"));
    const QString libraryRoot = QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(libraryRoot),
            QStringLiteral("资料包测试无法设置作品资料库：") + controller.errorMessage());
    require(controller.createProject(QStringLiteral("雾中来信"), QString()),
            QStringLiteral("资料包测试无法创建作品：") + controller.errorMessage());
    const QString sourceId = controller.currentProjectId();
    const QString sourcePath = controller.currentProjectPath();
    require(controller.updateDraftField(QStringLiteral("scriptText"),
                                        QStringLiteral("林岚在旧车站找到一封信。"))
                && controller.createBibleItem(QStringLiteral("character"),
                                              QStringLiteral("林岚"),
                                              QStringLiteral("主角资料。")),
            controller.errorMessage());
    const QString mediaPath = QDir(sourcePath).filePath(
        QStringLiteral("media/originals/测试素材.bin"));
    QFile mediaFile(mediaPath);
    require(mediaFile.open(QIODevice::WriteOnly)
                && mediaFile.write(QByteArray::fromHex("001122334455667788")) == 9,
            QStringLiteral("无法创建资料包媒体样例。"));
    mediaFile.close();

    QEventLoop saveWait;
    QTimer::singleShot(1200, &saveWait, &QEventLoop::quit);
    saveWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("资料包源作品没有保存。"));
    const bool packageCreated = controller.createProjectPackage(temporaryRoot.path(), true);
    require(packageCreated,
            QStringLiteral("资料包创建失败：") + controller.errorMessage()
                + QStringLiteral("（保存状态：") + controller.saveStatus() + QStringLiteral("）"));
    const QString packageMessage = controller.operationMessage();
    const qsizetype packageSeparator = packageMessage.indexOf(QStringLiteral("："));
    require(packageSeparator >= 0,
            QStringLiteral("资料包创建成功后没有返回可见位置。"));
    const QString packagePath = packageMessage.mid(packageSeparator + 1).trimmed();
    require(QFileInfo(QDir(packagePath).filePath(QStringLiteral("manifest.json"))).isFile()
                && QFileInfo(QDir(packagePath).filePath(QStringLiteral(
                       "media/originals/测试素材.bin"))).isFile(),
            QStringLiteral("资料包没有清单或用户媒体文件。"));
    require(controller.closeProject(), controller.errorMessage());

    const bool packageRestored = controller.restoreProjectPackage(packagePath);
    require(packageRestored,
            QStringLiteral("资料包恢复失败：") + controller.errorMessage());
    require(controller.projects().size() == 2,
            QStringLiteral("资料包恢复没有建立独立作品副本。"));
    QVariantMap restoredInfo;
    QVariantMap sourceInfo;
    for (const QVariant& value : controller.projects()) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("id")).toString() == sourceId) {
            sourceInfo = item;
        } else if (item.value(QStringLiteral("name")).toString()
                   == QStringLiteral("雾中来信（恢复副本）")) {
            restoredInfo = item;
        }
    }
    require(!sourceInfo.isEmpty() && !restoredInfo.isEmpty()
                && restoredInfo.value(QStringLiteral("id")).toString() != sourceId
                && QFileInfo(QDir(sourceInfo.value(QStringLiteral("path")).toString())
                                 .filePath(QStringLiteral("project.sqlite"))).isFile(),
            QStringLiteral("恢复后源作品不可访问或副本身份未重新分配。"));
    require(controller.openProject(restoredInfo.value(QStringLiteral("path")).toString()),
            controller.errorMessage());
    require(controller.scriptText() == QStringLiteral("林岚在旧车站找到一封信。")
                && bibleItemIdByName(controller.bibleItems(), QStringLiteral("林岚")).size() > 0
                && QFileInfo(QDir(controller.currentProjectPath()).filePath(
                       QStringLiteral("media/originals/测试素材.bin"))).isFile(),
            QStringLiteral("恢复副本没有还原剧本、创作圣经或媒体素材。"));
    require(controller.closeProject(), controller.errorMessage());

    QFile corruptedMedia(QDir(packagePath).filePath(
        QStringLiteral("media/originals/测试素材.bin")));
    require(corruptedMedia.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && corruptedMedia.write(QByteArrayLiteral("corrupt")) == 7,
            QStringLiteral("无法准备资料包校验失败样例。"));
    corruptedMedia.close();
    require(!controller.restoreProjectPackage(packagePath)
                && controller.projects().size() == 2,
            QStringLiteral("资料包校验失败后仍创建了部分恢复作品。"));
    require(controller.openProject(sourceInfo.value(QStringLiteral("path")).toString()),
            QStringLiteral("资料包恢复或校验失败后原作品不能重新打开。"));
    require(controller.closeProject(), controller.errorMessage());

    QSettings settings;
    settings.remove(QStringLiteral("library/root"));
    settings.sync();
}

void verifyProjectLifecycleWorkflow()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建作品生命周期测试目录。"));
    const QString libraryRoot = QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(libraryRoot), controller.errorMessage());
    require(controller.createProject(QStringLiteral("雾中来信"), QStringLiteral("主创意")),
            controller.errorMessage());
    const QString sourceId = controller.currentProjectId();
    const QString sourcePath = controller.currentProjectPath();
    require(controller.updateDraftField(QStringLiteral("scriptText"), QStringLiteral("第一稿正文")),
            controller.errorMessage());
    const QString exportPath = QDir(sourcePath).filePath(QStringLiteral("exports/交付说明.txt"));
    QFile exportFile(exportPath);
    require(exportFile.open(QIODevice::WriteOnly)
                && exportFile.write(QStringLiteral("应随作品复制的持久文件").toUtf8()) > 0,
            QStringLiteral("无法创建作品生命周期持久文件样例。"));
    exportFile.close();
    const QString mediaPath = QDir(sourcePath).filePath(QStringLiteral("media/originals/镜头素材.bin"));
    QFile mediaFile(mediaPath);
    require(mediaFile.open(QIODevice::WriteOnly)
                && mediaFile.write(QByteArray::fromHex("AABBCCDDEEFF")) == 6,
            QStringLiteral("无法创建作品生命周期媒体样例。"));
    mediaFile.close();
    QEventLoop saveWait;
    QTimer::singleShot(1200, &saveWait, &QEventLoop::quit);
    saveWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"), controller.errorMessage());
    require(controller.closeProject(), controller.errorMessage());

    require(controller.setProjectArchived(sourcePath, true), controller.errorMessage());
    bool sourceArchived = false;
    for (const QVariant& value : controller.projects()) {
        const QVariantMap project = value.toMap();
        if (project.value(QStringLiteral("id")).toString() == sourceId) {
            sourceArchived = project.value(QStringLiteral("archived")).toBool();
        }
    }
    require(sourceArchived, QStringLiteral("作品归档状态未持久保存到作品列表。"));

    require(controller.duplicateProject(sourcePath), controller.errorMessage());
    require(controller.projects().size() == 2, QStringLiteral("复制作品没有创建新作品。"));
    QVariantMap duplicate;
    for (const QVariant& value : controller.projects()) {
        const QVariantMap project = value.toMap();
        if (project.value(QStringLiteral("id")).toString() != sourceId) {
            duplicate = project;
        }
    }
    require(!duplicate.isEmpty() && duplicate.value(QStringLiteral("name")).toString()
                                      == QStringLiteral("雾中来信（副本）"),
            QStringLiteral("作品副本身份或名称不正确。"));
    require(QFileInfo(QDir(duplicate.value(QStringLiteral("path")).toString())
                          .filePath(QStringLiteral("exports/交付说明.txt"))).isFile()
                && QFileInfo(QDir(duplicate.value(QStringLiteral("path")).toString())
                                 .filePath(QStringLiteral("media/originals/镜头素材.bin"))).isFile(),
            QStringLiteral("复制作品没有保留持久文件或媒体文件。"));
    require(controller.openProject(duplicate.value(QStringLiteral("path")).toString()),
            controller.errorMessage());
    require(controller.currentProjectId() != sourceId
                && controller.briefDescription() == QStringLiteral("主创意")
                && controller.scriptText() == QStringLiteral("第一稿正文"),
            QStringLiteral("作品副本身份与内容没有正确独立恢复。"));
    require(controller.closeProject(), controller.errorMessage());

    require(controller.previewProjectRemoval(sourcePath),
            QStringLiteral("移除预览失败：") + controller.errorMessage());
    const QVariantMap preview = controller.projectRemovalPreview();
    require(preview.value(QStringLiteral("id")).toString() == sourceId
                && preview.value(QStringLiteral("fileCount")).toInt() >= 3
                && preview.value(QStringLiteral("mediaCount")).toInt() == 1,
            QStringLiteral("移除预览没有展示作品文件和媒体范围。"));
    const bool backupCreated = controller.backupProjectBeforeRemoval(temporaryRoot.path());
    require(backupCreated,
            QStringLiteral("移除前备份失败：") + controller.errorMessage());
    const QString backupPath = controller.projectRemovalPreview()
                                   .value(QStringLiteral("backupPath")).toString();
    require(QFileInfo(QDir(backupPath).filePath(QStringLiteral("manifest.json"))).isFile()
                && QFileInfo(QDir(backupPath).filePath(QStringLiteral("exports/交付说明.txt"))).isFile(),
            QStringLiteral("移除前备份没有收录作品的持久文件。"));
    controller.cancelProjectRemoval();
    require(QFileInfo(sourcePath).isDir(), QStringLiteral("取消移除后源作品没有保留。"));

    require(controller.previewProjectRemoval(sourcePath),
            QStringLiteral("移除范围变更测试预览失败：") + controller.errorMessage());
    const QString changedFile = QDir(sourcePath).filePath(QStringLiteral("exports/新文件.txt"));
    QFile unexpectedFile(changedFile);
    require(unexpectedFile.open(QIODevice::WriteOnly)
                && unexpectedFile.write("changed") > 0,
            QStringLiteral("无法创建移除范围变化测试文件。"));
    unexpectedFile.close();
    require(!controller.confirmProjectRemoval() && QFileInfo(sourcePath).isDir(),
            QStringLiteral("移除前作品范围变化时未阻止确认。"));
    require(QFile::remove(changedFile), QStringLiteral("无法清理生命周期测试文件。"));
    require(controller.previewProjectRemoval(sourcePath),
            QStringLiteral("最终移除预览失败：") + controller.errorMessage());
    const bool projectRemoved = controller.confirmProjectRemoval();
    require(projectRemoved, controller.errorMessage());
    require(controller.projects().size() == 1
                && QFileInfo(sourcePath).isDir() == false,
            QStringLiteral("确认移除后源作品仍留在资料库中。"));

    require(controller.restoreProjectPackage(backupPath), controller.errorMessage());
    require(controller.projects().size() == 2,
            QStringLiteral("移除前资料包不能恢复为独立作品。"));

    QSettings settings;
    settings.remove(QStringLiteral("library/root"));
    settings.sync();
}

void verifySchemaNineToElevenMigration()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建 schema v9 迁移测试目录。"));
    const QString libraryRoot = QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(libraryRoot), controller.errorMessage());
    require(controller.createProject(QStringLiteral("v9 作品"), QStringLiteral("保留内容")),
            controller.errorMessage());
    const QString projectPath = controller.currentProjectPath();
    require(controller.closeProject(), controller.errorMessage());

    const QString connectionName = QStringLiteral("schema-nine-fixture-")
                                   + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        database.setDatabaseName(QDir(projectPath).filePath(QStringLiteral("project.sqlite")));
        require(database.open(), database.lastError().text());
        QSqlQuery query(database);
        require(query.exec(QStringLiteral("DROP TABLE visual_reference_bible_items"))
                    && query.exec(QStringLiteral("DROP TABLE visual_reference_shots"))
                    && query.exec(QStringLiteral("DROP TABLE visual_references"))
                    && query.exec(QStringLiteral("DROP TABLE shot_bible_items"))
                    && query.exec(QStringLiteral("DROP TABLE shot_script_blocks"))
                    && query.exec(QStringLiteral("DROP TABLE shots"))
                    && query.exec(QStringLiteral("DROP INDEX story_impact_source_created"))
                    && query.exec(QStringLiteral("DROP TABLE story_impact_findings"))
                    && query.exec(QStringLiteral(
                        "CREATE TABLE story_impact_findings ("
                        "impact_id TEXT PRIMARY KEY, source_item_id TEXT NOT NULL, "
                        "source_item_name TEXT NOT NULL, change_summary TEXT NOT NULL, "
                        "target_type TEXT NOT NULL CHECK(target_type IN "
                        "('script_block', 'episode', 'unassessed', 'unsupported_asset')), "
                        "episode_id TEXT NOT NULL DEFAULT '', episode_title TEXT NOT NULL DEFAULT '', "
                        "scene_id TEXT NOT NULL DEFAULT '', scene_title TEXT NOT NULL DEFAULT '', "
                        "script_block_id TEXT NOT NULL DEFAULT '', block_type TEXT NOT NULL DEFAULT '', "
                        "evidence TEXT NOT NULL DEFAULT '', "
                        "assessment_status TEXT NOT NULL CHECK(assessment_status IN "
                        "('located', 'unassessed')), "
                        "review_status TEXT NOT NULL CHECK(review_status IN ('open', 'intentional')), "
                        "reason TEXT NOT NULL DEFAULT '', created_at TEXT NOT NULL, "
                        "updated_at TEXT NOT NULL)"))
                    && query.exec(QStringLiteral(
                        "CREATE INDEX story_impact_source_created "
                        "ON story_impact_findings(source_item_id, created_at)"))
                    && query.exec(QStringLiteral(
                        "INSERT INTO story_impact_findings(impact_id, source_item_id, "
                        "source_item_name, change_summary, target_type, episode_id, "
                        "episode_title, scene_id, scene_title, script_block_id, block_type, "
                        "evidence, assessment_status, review_status, reason, created_at, "
                        "updated_at) VALUES('legacy-impact', 'legacy-source', '旧设定', "
                        "'内容变化', 'script_block', '', '', '', '', '', '', "
                        "'旧影响证据', 'located', 'intentional', '既有复核理由', "
                        "'2026-11-27T12:00:00.000Z', "
                        "'2026-11-27T12:00:00.000Z')"))
                    && query.exec(QStringLiteral("DROP TABLE ai_generation_tasks"))
                    && query.exec(QStringLiteral("ALTER TABLE project_info DROP COLUMN archived"))
                    && query.exec(QStringLiteral("PRAGMA user_version=9")),
                QStringLiteral("无法准备 schema v9 作品样例：") + query.lastError().text());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    require(controller.openProject(projectPath), controller.errorMessage());
    require(controller.storyImpacts().size() == 1
                && controller.storyImpacts().first().toMap()
                       .value(QStringLiteral("id")).toString()
                       == QStringLiteral("legacy-impact")
                && controller.storyImpacts().first().toMap()
                       .value(QStringLiteral("evidence")).toString()
                       == QStringLiteral("旧影响证据"),
            QStringLiteral("schema v9→v12 逐步迁移丢失了既有影响记录字段。"));
    require(controller.closeProject(), controller.errorMessage());
    sceneforge::project::ProjectRepository repository;
    QString error;
    require(repository.setLibraryRoot(libraryRoot, &error), error);
    sceneforge::project::ProjectInfo project;
    require(repository.openProject(projectPath, &project, &error), error);
    require(!project.archived, QStringLiteral("旧作品迁移后应默认为未归档。"));
    repository.closeProject();

    const QString checkConnection = QStringLiteral("schema-eleven-check-")
                                    + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", checkConnection);
        database.setDatabaseName(QDir(projectPath).filePath(QStringLiteral("project.sqlite")));
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        require(database.open(), database.lastError().text());
        QSqlQuery query(database);
        require(query.exec(QStringLiteral("PRAGMA user_version")) && query.next()
                    && query.value(0).toInt() == 13,
                QStringLiteral("schema v9 作品没有迁移到 schema v13。"));
        require(query.exec(QStringLiteral(
                    "SELECT count(*) FROM sqlite_master WHERE type='table' "
                    "AND name IN ('shots', 'visual_references', 'shot_script_blocks', "
                    "'visual_reference_shots')"))
                    && query.next() && query.value(0).toInt() == 4,
                QStringLiteral("schema v9 迁移没有创建镜头和视觉参考关联表。"));
        require(query.exec(QStringLiteral(
                    "SELECT review_status, reason, evidence FROM story_impact_findings "
                    "WHERE impact_id='legacy-impact'"))
                    && query.next()
                    && query.value(0).toString() == QStringLiteral("intentional")
                    && query.value(1).toString() == QStringLiteral("既有复核理由")
                    && query.value(2).toString() == QStringLiteral("旧影响证据"),
                QStringLiteral("v9→v12 迁移没有保留影响记录的复核状态和证据。"));
        database.close();
    }
    QSqlDatabase::removeDatabase(checkConnection);
    const QFileInfoList backups = QDir(QDir(projectPath).filePath(
                                           QStringLiteral("backups/migrations")))
                                      .entryInfoList({QStringLiteral("before-schema-10-*.sqlite"),
                                                      QStringLiteral("before-schema-11-*.sqlite"),
                                                      QStringLiteral("before-schema-12-*.sqlite"),
                                                      QStringLiteral("before-schema-13-*.sqlite")},
                                                     QDir::Files);
    require(backups.size() == 4,
            QStringLiteral("schema v9→v13 逐版本迁移没有分别生成迁移前备份。"));
}

void verifyTemplateWorkflow()
{
    QTemporaryDir temporaryRoot;
    require(temporaryRoot.isValid(), QStringLiteral("无法创建模板作品测试目录。"));
    sceneforge::project::ProjectController controller;
    require(controller.setLibraryRoot(QDir(temporaryRoot.path()).filePath(QStringLiteral("作品库"))),
            controller.errorMessage());
    require(controller.createProject(QStringLiteral("结构模板示例"), QString(),
                                     QStringLiteral("series"),
                                     QStringLiteral("basic_structure")),
            controller.errorMessage());
    require(controller.episodes().size() == 3
                && controller.episodes().first().toMap()
                       .value(QStringLiteral("contentStatus")).toString()
                       == QStringLiteral("suggestion")
                && controller.scenes().size() == 4
                && controller.scenes().first().toMap()
                       .value(QStringLiteral("contentStatus")).toString()
                       == QStringLiteral("suggestion")
                && controller.scriptText().isEmpty(),
            QStringLiteral("模板建议没有保持为空白故事内容或没有明确标记为建议。"));
    const QString sceneId = controller.scenes().first().toMap()
                                .value(QStringLiteral("id")).toString();
    require(controller.removeScene(sceneId) && controller.scenes().size() == 3,
            QStringLiteral("模板场次建议不能编辑或移除。"));
    require(!controller.closeProject(),
            QStringLiteral("模板编辑未保存时不应静默关闭作品。"));
    controller.retrySave();
    QEventLoop saveWait;
    QTimer::singleShot(1200, &saveWait, &QEventLoop::quit);
    saveWait.exec();
    require(controller.saveStatus() == QStringLiteral("saved"),
            QStringLiteral("模板变更没有成功保存：") + controller.saveStatus()
                + QStringLiteral("；") + controller.errorMessage());
    require(controller.closeProject(), controller.errorMessage());

    const QString projectPath = controller.projects().first().toMap()
                                    .value(QStringLiteral("path")).toString();
    require(controller.openProject(projectPath), controller.errorMessage());
    require(controller.episodes().size() == 3 && controller.scenes().size() == 3
                && controller.currentProjectContentStatus() == QStringLiteral("draft"),
            QStringLiteral("模板建议修改和重开后丢失或被误记为正式確認。"));
    require(controller.closeProject(), controller.errorMessage());

    QSettings settings;
    settings.remove(QStringLiteral("library/root"));
    settings.sync();
}

} // 命名空间

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SceneForge"));
    QCoreApplication::setApplicationName(QStringLiteral("ProjectStorageTests"));

    if (argc == 3
        && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--create-acl-fixture")) {
        return createAclOpenFixture(QString::fromLocal8Bit(argv[2]));
    }
    if (argc == 4
        && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--expect-open-failure")) {
        return expectAclOpenFailure(QString::fromLocal8Bit(argv[2]),
                                    QString::fromLocal8Bit(argv[3]));
    }
    if (argc == 4
        && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--expect-open-success")) {
        return expectAclOpenSuccess(QString::fromLocal8Bit(argv[2]),
                                    QString::fromLocal8Bit(argv[3]));
    }
    if (argc == 3
        && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--expect-create-failure")) {
        sceneforge::project::ProjectRepository repository;
        QString error;
        if (!repository.setLibraryRoot(QString::fromLocal8Bit(argv[2]), &error)) {
            std::cerr << "FAIL: library selection failed before the write probe: "
                      << error.toStdString() << '\n';
            return 1;
        }
        sceneforge::project::ProjectInfo project;
        if (repository.createProject(QStringLiteral("只读目录验证"), QString(),
                                     QStringLiteral("single_short"),
                                     &project, &error)) {
            std::cerr << "FAIL: project creation unexpectedly succeeded in a write-denied directory\n";
            return 1;
        }
        std::cout << "PASS: project creation failed visibly: " << error.toStdString() << '\n';
        return 0;
    }

    try {
        verifyProjectLifecycle();
        verifySchemaMigration();
        verifySchemaTwoMigration();
        verifySchemaThreeMigration();
        verifySchemaFourBibleMigration();
        verifyStructuredStoryAndScriptWorkflow();
        verifyQualityWorkflow();
        verifyScriptVersionWorkflow();
        verifySaveStatusTransitions();
        verifySeriesWorkflow();
        verifyStoryImpactWorkflow();
        verifyShotAndVisualReferenceWorkflow();
        verifySchemaEightToNineMigration();
        verifySchemaNineToElevenMigration();
        verifyProjectPackageWorkflow();
        verifyProjectLifecycleWorkflow();
        verifyTemplateWorkflow();
        std::cout << "PASS: project lifecycle, stable identity, lock exclusion, and save status\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
