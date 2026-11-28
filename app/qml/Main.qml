import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    visible: true
    width: 1280
    height: 820
    minimumWidth: 900
    minimumHeight: 620
    title: qsTr("镜造")
    color: "#1E2023"

    readonly property color panelColor: "#27292D"
    readonly property color raisedColor: "#303338"
    readonly property color borderColor: "#3A3D42"
    readonly property color primaryText: "#ECEDEF"
    readonly property color secondaryText: "#A5A8AE"
    readonly property color accentColor: "#C49A57"
    readonly property var projectControllerReference: projectController
    readonly property var qualityProjectEstimate: projectController.qualityReport.projectEstimate || ({})
    property bool ideaMode: true
    property bool templateMode: false
    property string newProjectType: "single_short"
    property string pendingEpisodeRemovalId: ""
    property string pendingSceneRemovalId: ""
    property string pendingBibleItemId: ""
    property string pendingBibleItemName: ""
    property string newBibleItemType: "character"
    property string newScriptBlockType: "scene_heading"
    property string newScriptSceneId: ""
    property string scriptVersionLanguage: "English"
    property string scriptVersionType: "translation"
    property string scriptVersionSourceId: ""
    property string scriptVersionCompareSourceId: ""
    property string newShotSceneId: ""
    property string newVisualReferenceSceneId: ""
    property string pendingScriptVersionRestoreId: ""
    property string scriptVersionComparison: ""
    property alias projectStoryFieldsView: projectStoryFieldsRepeater
    property alias scenesView: scenesRepeater
    property alias scriptBlocksView: scriptBlockRepeater
    property alias shotsView: shotsRepeater
    property alias visualReferencesView: visualReferencesRepeater
    property alias scriptVersionsView: scriptVersionsRepeater
    readonly property var bibleTypes: [
        { key: "project_position", label: qsTr("作品定位") },
        { key: "story_rule", label: qsTr("故事规则") },
        { key: "era", label: qsTr("时代") },
        { key: "character", label: qsTr("角色") },
        { key: "location", label: qsTr("地点") },
        { key: "relationship", label: qsTr("人物关系") },
        { key: "prop", label: qsTr("道具") },
        { key: "visual_tone", label: qsTr("视觉基调") },
        { key: "continuity_fact", label: qsTr("连续性事实") }
    ]
    readonly property var bibleCertaintyStatuses: [
        { key: "confirmed", label: qsTr("确定") },
        { key: "provisional", label: qsTr("暂定") },
        { key: "undecided", label: qsTr("待决定") },
        { key: "immutable", label: qsTr("禁止改变") }
    ]
    readonly property var storyDevelopmentFields: [
        { key: "theme", label: qsTr("主题"), hint: qsTr("故事希望表达的核心问题或情感是什么？") },
        { key: "storySynopsis", label: qsTr("故事梗概"), hint: qsTr("用几句话说明故事从开始到结局的主要发展。") },
        { key: "centralConflict", label: qsTr("核心冲突"), hint: qsTr("主角想达成什么，主要阻力是什么？") },
        { key: "characterRelationships", label: qsTr("人物关系"), hint: qsTr("记录关键人物之间的关系和变化。") },
        { key: "endingDirection", label: qsTr("结局方向"), hint: qsTr("描述希望抵达的结局，不必现在定稿。") },
        { key: "episodeOutline", label: qsTr("分集大纲"), hint: qsTr("多集作品可按集记录主要事件；单集作品可留空。") },
        { key: "sceneOutline", label: qsTr("场次大纲"), hint: qsTr("按顺序写下每场的目的、冲突或转折。") }
    ]
    readonly property var scriptBlockTypes: [
        { key: "unstructured_text", label: qsTr("自由文本") },
        { key: "scene_heading", label: qsTr("场景标题") },
        { key: "scene_description", label: qsTr("场景描述") },
        { key: "action", label: qsTr("动作") },
        { key: "character", label: qsTr("角色") },
        { key: "dialogue", label: qsTr("对白") },
        { key: "parenthetical", label: qsTr("括号提示") },
        { key: "narration", label: qsTr("旁白") },
        { key: "voice_over", label: qsTr("画外音") },
        { key: "sound_effect", label: qsTr("音效") },
        { key: "music", label: qsTr("音乐") },
        { key: "transition", label: qsTr("转场") }
    ]
    readonly property var contentStatuses: [
        { key: "draft", label: qsTr("草稿") },
        { key: "confirmed", label: qsTr("已确认") },
        { key: "suggestion", label: qsTr("建议，待确认") }
    ]
    readonly property var scriptVersionTypes: [
        { key: "original", label: qsTr("原稿") },
        { key: "translation", label: qsTr("译稿") },
        { key: "generated_candidate", label: qsTr("生成候选") },
        { key: "user_edit", label: qsTr("用户编辑稿") },
        { key: "confirmed", label: qsTr("已确认稿") },
        { key: "archived", label: qsTr("归档稿") }
    ]
    readonly property var scriptVersionStatuses: [
        { key: "draft", label: qsTr("草稿") },
        { key: "candidate", label: qsTr("候选") },
        { key: "confirmed", label: qsTr("已确认") },
        { key: "archived", label: qsTr("归档，只读") }
    ]
    readonly property var visualReferenceUsages: [
        { key: "character_appearance", label: qsTr("角色外观") },
        { key: "location", label: qsTr("地点") },
        { key: "composition", label: qsTr("构图") },
        { key: "costume", label: qsTr("服装") },
        { key: "prop", label: qsTr("道具") },
        { key: "emotion", label: qsTr("情绪") },
        { key: "lighting", label: qsTr("光线") },
        { key: "color", label: qsTr("色彩") },
        { key: "style", label: qsTr("风格") }
    ]

    readonly property var scriptSceneOptions: {
        const options = [{ id: "", title: qsTr("未关联场次") }]
        for (const scene of projectController.scenes) {
            options.push({ id: scene.id, title: qsTr("场次 %1 · %2")
                          .arg(scene.sortOrder + 1).arg(scene.title || qsTr("待命名")) })
        }
        return options
    }

    readonly property var scriptVersionSourceOptions: {
        const options = [{ id: "", title: qsTr("当前剧本") }]
        for (const version of projectController.scriptVersions) {
            options.push({ id: version.id,
                          title: version.language + " · "
                                 + window.scriptVersionTypeLabel(version.versionType) })
        }
        return options
    }

    readonly property var storyboardSceneOptions: {
        const options = []
        for (const scene of projectController.scenes) {
            options.push({ id: scene.id,
                          title: qsTr("场次 %1 · %2")
                                 .arg(scene.sortOrder + 1)
                                 .arg(scene.title || qsTr("待命名")) })
        }
        return options
    }

    function bibleTypeLabel(type) {
        const item = bibleTypes.find(entry => entry.key === type)
        return item ? item.label : type
    }

    function bibleDetailFields(type) {
        const fields = {
            project_position: [
                { key: "corePromise", label: qsTr("核心承诺") },
                { key: "intendedAudience", label: qsTr("目标受众") },
                { key: "appeal", label: qsTr("主要看点") },
                { key: "overallTone", label: qsTr("整体基调") }
            ],
            story_rule: [
                { key: "rule", label: qsTr("规则") },
                { key: "consequence", label: qsTr("违反后果") },
                { key: "exceptions", label: qsTr("例外") }
            ],
            era: [
                { key: "timePeriod", label: qsTr("时间范围") },
                { key: "socialContext", label: qsTr("社会背景") },
                { key: "technologyLevel", label: qsTr("技术水平") }
            ],
            character: [
                { key: "identity", label: qsTr("身份") },
                { key: "goal", label: qsTr("目标") },
                { key: "motivation", label: qsTr("动机") },
                { key: "conflict", label: qsTr("冲突") },
                { key: "experience", label: qsTr("经历") },
                { key: "personality", label: qsTr("性格") },
                { key: "speechStyle", label: qsTr("说话方式") },
                { key: "appearance", label: qsTr("外观") },
                { key: "relationships", label: qsTr("关系") },
                { key: "growth", label: qsTr("成长变化") }
            ],
            location: [
                { key: "function", label: qsTr("地点功能") },
                { key: "timeConditions", label: qsTr("时间条件") },
                { key: "visualFeatures", label: qsTr("视觉特征") },
                { key: "availableProps", label: qsTr("可用道具") },
                { key: "limitations", label: qsTr("限制") }
            ],
            relationship: [
                { key: "parties", label: qsTr("关系双方") },
                { key: "relationshipType", label: qsTr("关系类型") },
                { key: "history", label: qsTr("关系经历") },
                { key: "tension", label: qsTr("关系冲突") },
                { key: "change", label: qsTr("变化") }
            ],
            prop: [
                { key: "function", label: qsTr("道具功能") },
                { key: "appearance", label: qsTr("外观") },
                { key: "owner", label: qsTr("持有人") },
                { key: "constraints", label: qsTr("使用限制") }
            ],
            visual_tone: [
                { key: "palette", label: qsTr("色彩") },
                { key: "style", label: qsTr("视觉风格") },
                { key: "lighting", label: qsTr("光线") },
                { key: "camera", label: qsTr("镜头感") },
                { key: "references", label: qsTr("风格说明") }
            ],
            continuity_fact: [
                { key: "fact", label: qsTr("事实") },
                { key: "scope", label: qsTr("适用范围") },
                { key: "verification", label: qsTr("校验方式") },
                { key: "relatedItems", label: qsTr("关联设定") }
            ]
        }
        return fields[type] || []
    }

    function bibleCertaintyStatusIndex(status) {
        return Math.max(0, bibleCertaintyStatuses.findIndex(entry => entry.key === status))
    }

    function storyDevelopmentFieldIndex(key) {
        return Math.max(0, storyDevelopmentFields.findIndex(entry => entry.key === key))
    }

    function scriptBlockTypeLabel(type) {
        const item = scriptBlockTypes.find(entry => entry.key === type)
        return item ? item.label : type
    }

    function scriptBlockTypeIndex(type) {
        return Math.max(0, scriptBlockTypes.findIndex(entry => entry.key === type))
    }

    function contentStatusIndex(status) {
        return Math.max(0, contentStatuses.findIndex(entry => entry.key === status))
    }

    function scriptSceneIndex(sceneId) {
        return Math.max(0, scriptSceneOptions.findIndex(entry => entry.id === sceneId))
    }

    function sceneLabel(sceneId) {
        if (!sceneId)
            return qsTr("未关联场次")
        const scene = projectController.scenes.find(entry => entry.id === sceneId)
        return scene ? qsTr("场次 %1 · %2").arg(scene.sortOrder + 1).arg(scene.title)
                     : qsTr("场次已移除")
    }

    function scriptVersionTypeLabel(type) {
        const item = scriptVersionTypes.find(entry => entry.key === type)
        return item ? item.label : type
    }

    function scriptVersionTypeIndex(type) {
        return Math.max(0, scriptVersionTypes.findIndex(entry => entry.key === type))
    }

    function scriptVersionStatusIndex(status) {
        return Math.max(0, scriptVersionStatuses.findIndex(entry => entry.key === status))
    }

    function visualReferenceUsageLabel(usage) {
        const item = visualReferenceUsages.find(entry => entry.key === usage)
        return item ? item.label : usage
    }

    function visualReferenceUsageIndex(usage) {
        return Math.max(0, visualReferenceUsages.findIndex(entry => entry.key === usage))
    }

    function formatFileSize(byteCount) {
        if (byteCount < 1024)
            return qsTr("%1 B").arg(byteCount)
        if (byteCount < 1024 * 1024)
            return qsTr("%1 KB").arg((byteCount / 1024).toFixed(1))
        if (byteCount < 1024 * 1024 * 1024)
            return qsTr("%1 MB").arg((byteCount / 1048576).toFixed(1))
        return qsTr("%1 GB").arg((byteCount / 1073741824).toFixed(2))
    }

    function scriptVersionSourceIndex(versionId) {
        return Math.max(0, scriptVersionSourceOptions.findIndex(entry => entry.id === versionId))
    }

    onClosing: function(close) {
        if (projectController.hasOpenProject
                && projectController.saveStatus !== "saved") {
            close.accepted = false
            projectController.retrySave()
        }
    }

    FolderDialog {
        id: libraryFolderDialog
        title: qsTr("选择作品资料库目录")
        onAccepted: {
            if (projectController.setLibraryRootUrl(selectedFolder)) {
                libraryPathField.text = projectController.libraryRoot
            }
        }
    }

    FolderDialog {
        id: packageDestinationDialog
        title: qsTr("选择资料包保存位置")
        onAccepted: projectController.createProjectPackage(selectedFolder.toLocalFile(), true)
    }

    FolderDialog {
        id: packageSourceDialog
        title: qsTr("选择要恢复的资料包目录")
        onAccepted: projectController.restoreProjectPackage(selectedFolder.toLocalFile())
    }

    FolderDialog {
        id: removalBackupDialog
        title: qsTr("选择移除前资料包的保存位置")
        onAccepted: projectController.backupProjectBeforeRemoval(selectedFolder.toLocalFile())
    }

    Dialog {
        id: removeProjectDialog
        objectName: "removeProjectDialog"
        title: qsTr("移到系统回收站")
        modal: true
        width: Math.min(600, window.width - 40)
        standardButtons: DialogButtonBox.NoButton
        onRejected: projectController.cancelProjectRemoval()

        contentItem: ColumnLayout {
            spacing: 12
            Text {
                Layout.fillWidth: true
                text: qsTr("将作品“%1”及其全部本地文件移到系统回收站。此操作会移除 %2 个文件（含 %3 个素材文件），约 %4。")
                          .arg(projectController.projectRemovalPreview.name || "")
                          .arg(projectController.projectRemovalPreview.fileCount || 0)
                          .arg(projectController.projectRemovalPreview.mediaCount || 0)
                          .arg(window.formatFileSize(projectController.projectRemovalPreview.totalBytes || 0))
                color: primaryText
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                visible: projectController.errorMessage.length > 0
                text: projectController.errorMessage
                color: "#F0C4C1"
                wrapMode: Text.WordWrap
            }
            Button {
                visible: projectController.errorMessage.length > 0
                text: qsTr("重新检查范围")
                onClicked: {
                    var path = projectController.projectRemovalPreview.path
                    if (projectController.previewProjectRemoval(path)) {
                        removeProjectDialog.close()
                        removeProjectDialog.open()
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                visible: (projectController.projectRemovalPreview.backupPath || "").length > 0
                text: qsTr("已创建移除前资料包：%1").arg(projectController.projectRemovalPreview.backupPath || "")
                color: "#B9D4BF"
                wrapMode: Text.WrapAnywhere
            }
            Button {
                text: qsTr("先创建移除前资料包")
                onClicked: removalBackupDialog.open()
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button {
                    text: qsTr("取消")
                    onClicked: {
                        projectController.cancelProjectRemoval()
                        removeProjectDialog.close()
                    }
                }
                Button {
                    text: qsTr("移到系统回收站")
                    highlighted: true
                    onClicked: {
                        if (projectController.confirmProjectRemoval())
                            removeProjectDialog.close()
                    }
                }
            }
        }
    }

    Dialog {
        id: removeEpisodeDialog
        title: qsTr("移除剧集")
        modal: true
        width: Math.min(520, window.width - 40)
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: projectController.removeEpisode(window.pendingEpisodeRemovalId)
        contentItem: Text {
            text: qsTr("移除后，该集及其内容将从当前作品结构中删除。确定继续吗？")
            color: primaryText
            wrapMode: Text.WordWrap
            padding: 18
        }
    }

    Dialog {
        id: removeSceneDialog
        title: qsTr("移除场次")
        modal: true
        width: Math.min(520, window.width - 40)
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: projectController.removeScene(window.pendingSceneRemovalId)
        contentItem: Text {
            text: qsTr("此场次会从当前剧集结构中移除。确定继续吗？")
            color: primaryText
            wrapMode: Text.WordWrap
            padding: 18
        }
    }

    Dialog {
        id: removeBibleItemDialog
        title: qsTr("删除全剧角色或地点")
        modal: true
        width: Math.min(520, window.width - 40)
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: projectController.removeBibleItem(window.pendingBibleItemId)
        contentItem: Text {
            text: qsTr("删除“%1”会解除它在所有剧集中的引用。确定继续吗？")
                  .arg(window.pendingBibleItemName)
            color: window.primaryText
            wrapMode: Text.WordWrap
            padding: 18
        }
    }

    Dialog {
        id: historyDialog
        objectName: "historyDialog"
        title: qsTr("保存的内容快照")
        modal: true
        width: Math.min(700, window.width - 40)
        standardButtons: Dialog.Close
        property string snapshotText: ""
        contentItem: TextArea {
            readOnly: true
            wrapMode: TextEdit.Wrap
            text: historyDialog.snapshotText
            color: primaryText
            selectByMouse: true
            implicitWidth: 640
            implicitHeight: 420
        }
    }

    Dialog {
        id: versionComparisonDialog
        objectName: "versionComparisonDialog"
        title: qsTr("剧本版本差异")
        modal: true
        width: Math.min(820, window.width - 40)
        standardButtons: Dialog.Close
        contentItem: TextArea {
            readOnly: true
            wrapMode: TextEdit.NoWrap
            text: window.scriptVersionComparison
            color: primaryText
            selectByMouse: true
            implicitWidth: 760
            implicitHeight: 480
            font.family: "Consolas"
        }
    }

    Dialog {
        id: restoreScriptVersionDialog
        objectName: "restoreScriptVersionDialog"
        title: qsTr("恢复剧本版本")
        modal: true
        width: Math.min(820, window.width - 40)
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: {
            if (projectController.restoreScriptVersion(
                        window.pendingScriptVersionRestoreId)) {
                window.pendingScriptVersionRestoreId = ""
            }
        }
        contentItem: ColumnLayout {
            spacing: 10
            Text {
                Layout.fillWidth: true
                text: qsTr("影响范围：当前剧集的剧本段落和该集语言字段。其他剧集、场次结构与创作圣经不变。确认后，当前剧本会先保存为独立版本；恢复后的段落会作为草稿。")
                color: primaryText
                wrapMode: Text.WordWrap
            }
            TextArea {
                Layout.fillWidth: true
                Layout.preferredHeight: 420
                readOnly: true
                wrapMode: TextEdit.NoWrap
                text: window.scriptVersionComparison
                color: primaryText
                selectByMouse: true
                font.family: "Consolas"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 66
            color: "#17191B"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    radius: 6
                    color: accentColor

                    Text {
                        anchors.centerIn: parent
                        text: "镜"
                        color: "#17191B"
                        font.pixelSize: 17
                        font.bold: true
                    }
                }

                Text {
                    text: qsTr("镜造")
                    color: primaryText
                    font.pixelSize: 19
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 22
                    color: borderColor
                }

                Text {
                    text: projectController.hasOpenProject
                          ? projectController.currentProjectName
                          : qsTr("作品资料库")
                    color: secondaryText
                    font.pixelSize: 14
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Button {
                    objectName: "openAiServiceSettingsButton"
                    text: qsTr("AI 服务")
                    onClicked: aiServiceSettingsDialog.open()
                }

                Button {
                    objectName: "openAiGenerationButton"
                    text: qsTr("AI 创作")
                    visible: projectController.hasOpenProject
                    highlighted: true
                    onClicked: aiGenerationDialog.open()
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                    radius: 12
                    color: projectController.saveStatus === "failed"
                           ? "#572F31"
                           : projectController.saveStatus === "unsaved"
                             ? "#57452A"
                             : projectController.saveStatus === "saving"
                               ? "#35414A"
                               : "#303C35"
                    implicitWidth: saveStatusText.implicitWidth + 22
                    implicitHeight: 30

                    Text {
                        id: saveStatusText
                    objectName: "saveStatusIndicator"
                        anchors.centerIn: parent
                        text: projectController.saveStatusLabel
                        color: projectController.saveStatus === "failed" ? "#F3B5B3" : primaryText
                        font.pixelSize: 12
                    }
                }

                Button {
                    visible: projectController.hasOpenProject
                             && projectController.saveStatus === "failed"
                    objectName: "retrySaveButton"
                    text: qsTr("重试保存")
                    highlighted: true
                    onClicked: projectController.retrySave()
                }

                Button {
                    visible: projectController.hasOpenProject
                    objectName: "createProjectPackageButton"
                    text: qsTr("导出资料包")
                    onClicked: packageDestinationDialog.open()
                }

                Button {
                    visible: projectController.hasOpenProject
                    objectName: "closeProjectButton"
                    text: qsTr("关闭作品")
                    onClicked: projectController.closeProject()
                }
            }
        }

        Rectangle {
            visible: projectController.errorMessage.length > 0
            Layout.fillWidth: true
            implicitHeight: errorTopText.implicitHeight + 22
            color: "#3D3030"

            Text {
                id: errorTopText
                objectName: "errorMessageText"
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                anchors.topMargin: 11
                anchors.bottomMargin: 11
                text: projectController.errorMessage
                color: "#F0C4C1"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }

        Rectangle {
            visible: projectController.operationMessage.length > 0
            Layout.fillWidth: true
            implicitHeight: operationMessageText.implicitHeight + 22
            color: "#303C35"

            Text {
                id: operationMessageText
                objectName: "operationMessageText"
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                anchors.topMargin: 11
                anchors.bottomMargin: 11
                text: projectController.operationMessage
                color: primaryText
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }

        ScrollView {
            id: pageScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: Math.min(pageScroll.availableWidth - 64, 920)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 46
                spacing: 22

                ColumnLayout {
                    visible: !projectController.hasOpenProject
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: qsTr("从一个故事开始")
                        color: primaryText
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: qsTr("作品保存在你选择的本地资料库中。")
                        color: secondaryText
                        font.pixelSize: 14
                    }
                }

                Rectangle {
                    visible: !projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: libraryContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: libraryContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 12

                        Text {
                            text: qsTr("作品资料库位置")
                            color: primaryText
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("首阶段支持本机 NTFS 固定磁盘目录；网络盘、同步目录和可移动磁盘暂不支持。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            TextField {
                                id: libraryPathField
                                Layout.fillWidth: true
                                text: projectController.libraryRoot
                                placeholderText: qsTr("例如：D:/SceneForgeProjects")
                                selectByMouse: true
                                onAccepted: projectController.setLibraryRoot(text)
                            }

                            Button {
                                text: qsTr("浏览…")
                                onClicked: libraryFolderDialog.open()
                            }

                            Button {
                                text: qsTr("使用此目录")
                                highlighted: true
                                onClicked: projectController.setLibraryRoot(libraryPathField.text)
                            }
                        }

                    }
                }

                Rectangle {
                    visible: !projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: createContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: createContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 14

                        Text {
                            text: qsTr("新建作品")
                            color: primaryText
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                text: qsTr("一句创意")
                                objectName: "ideaEntryButton"
                                checkable: true
                                checked: window.ideaMode
                                onClicked: {
                                    window.ideaMode = true
                                    window.templateMode = false
                                }
                            }

                            Button {
                                text: qsTr("空白作品")
                                objectName: "blankEntryButton"
                                checkable: true
                                checked: !window.ideaMode && !window.templateMode
                                onClicked: {
                                    window.ideaMode = false
                                    window.templateMode = false
                                }
                            }

                            Button {
                                text: qsTr("基础结构模板")
                                checkable: true
                                checked: window.templateMode
                                onClicked: {
                                    window.ideaMode = false
                                    window.templateMode = true
                                }
                            }
                        }

                        Text {
                            visible: window.templateMode
                            Layout.fillWidth: true
                            text: qsTr("模板只提供可编辑的结构建议，不会把示例内容当成已确认的故事事实。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: qsTr("作品类型")
                                color: secondaryText
                                font.pixelSize: 13
                            }

                            Button {
                                text: qsTr("单集短片")
                                checkable: true
                                checked: window.newProjectType === "single_short"
                                onClicked: window.newProjectType = "single_short"
                            }

                            Button {
                                text: qsTr("多集短剧")
                                checkable: true
                                checked: window.newProjectType === "series"
                                onClicked: window.newProjectType = "series"
                            }
                        }

                        TextArea {
                            id: initialIdeaField
                            objectName: "initialIdeaField"
                            visible: window.ideaMode
                            Layout.fillWidth: true
                            Layout.preferredHeight: 96
                            placeholderText: qsTr("用一句话描述你的故事想法；没有结局或目标也可以继续。")
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            TextField {
                                id: projectNameField
                                objectName: "projectNameField"
                                Layout.fillWidth: true
                                placeholderText: qsTr("作品名称（可留空）")
                                selectByMouse: true
                                onAccepted: {
                                    if (projectController.createProject(
                                                text,
                                                window.ideaMode ? initialIdeaField.text : "",
                                                window.newProjectType,
                                                window.templateMode ? "basic_structure" : "")) {
                                        text = ""
                                        initialIdeaField.text = ""
                                    }
                                }
                            }

                            Button {
                                objectName: "createProjectButton"
                                text: qsTr("创建作品")
                                highlighted: true
                                enabled: projectController.libraryRoot.length > 0
                                onClicked: {
                                    if (projectController.createProject(
                                                projectNameField.text,
                                                window.ideaMode ? initialIdeaField.text : "",
                                                window.newProjectType,
                                                window.templateMode ? "basic_structure" : "")) {
                                        projectNameField.text = ""
                                        initialIdeaField.text = ""
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: !projectController.hasOpenProject
                             && projectController.projects.length > 0
                    objectName: "projectListPanel"
                    Layout.fillWidth: true
                    implicitHeight: projectListContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: projectListContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: qsTr("作品列表")
                                color: primaryText
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                            }

                            Button {
                                text: qsTr("刷新")
                                onClicked: projectController.refreshProjects()
                            }

                            Button {
                                visible: !projectController.hasOpenProject
                                objectName: "restoreProjectPackageButton"
                                text: qsTr("恢复资料包")
                                onClicked: packageSourceDialog.open()
                            }
                        }

                        Repeater {
                            objectName: "projectListRepeater"
                            model: projectController.projects

                            delegate: Rectangle {
                                required property var modelData
                                objectName: "projectRow_" + modelData.id
                                Layout.fillWidth: true
                                Layout.preferredHeight: 104
                                radius: 7
                                color: raisedColor

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 12
                                    spacing: 12

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3

                                        Text {
                                            text: modelData.name
                                            color: primaryText
                                            font.pixelSize: 14
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            visible: modelData.archived
                                            text: qsTr("已归档 · 仍可恢复")
                                            color: accentColor
                                            font.pixelSize: 11
                                        }

                                        Text {
                                            text: modelData.id
                                            color: secondaryText
                                            font.pixelSize: 11
                                            elide: Text.ElideMiddle
                                        }
                                    }

                                    Button {
                                        objectName: "openProjectButton"
                                        text: qsTr("打开")
                                        onClicked: projectController.openProject(modelData.path)
                                    }
                                    Button {
                                        objectName: "duplicateProjectButton"
                                        text: qsTr("复制")
                                        onClicked: projectController.duplicateProject(modelData.path)
                                    }
                                    Button {
                                        objectName: "archiveProjectButton"
                                        text: modelData.archived ? qsTr("取消归档") : qsTr("归档")
                                        onClicked: projectController.setProjectArchived(modelData.path,
                                                                                       !modelData.archived)
                                    }
                                    Button {
                                        objectName: "removeProjectButton"
                                        text: qsTr("移到回收站…")
                                        onClicked: {
                                            if (projectController.previewProjectRemoval(modelData.path))
                                                removeProjectDialog.open()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: !projectController.hasOpenProject
                             && projectController.libraryIssues.length > 0
                    Layout.fillWidth: true
                    implicitHeight: issueContent.implicitHeight + 28
                    radius: 8
                    color: "#3D3030"
                    border.color: "#745151"

                    ColumnLayout {
                        id: issueContent
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 6

                        Text {
                            text: qsTr("有目录未能识别为作品")
                            color: "#F0C4C1"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: projectController.libraryIssues.join("\n")
                            color: "#D3B7B5"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Rectangle {
                    visible: !projectController.hasOpenProject
                             && projectController.libraryRoot.length > 0
                             && projectController.projects.length === 0
                             && projectController.libraryIssues.length === 0
                    Layout.fillWidth: true
                    implicitHeight: 84
                    radius: 8
                    color: "transparent"
                    border.color: borderColor

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("这里还没有作品。创建一个作品后，它会显示在这里。")
                        color: secondaryText
                        font.pixelSize: 13
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: workspaceContent.implicitHeight + 48
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: workspaceContent
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: 14

                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                id: currentProjectNameEditor
                                objectName: "currentProjectNameEditor"
                                Layout.fillWidth: true
                                Layout.maximumWidth: 520
                                text: projectController.currentProjectName
                                placeholderText: qsTr("作品名称")
                                selectByMouse: true
                                onAccepted: {
                                    if (!projectController.renameProject(text))
                                        text = projectController.currentProjectName
                                }
                            }
                            Button {
                                objectName: "renameProjectButton"
                                text: qsTr("保存名称")
                                onClicked: {
                                    if (!projectController.renameProject(
                                                currentProjectNameEditor.text)) {
                                        currentProjectNameEditor.text =
                                                projectController.currentProjectName
                                    }
                                }
                            }
                        }

                        Text {
                            text: qsTr("作品 ID：") + projectController.currentProjectId
                            color: secondaryText
                            font.pixelSize: 12
                        }

                        Text {
                            text: qsTr("作品类型：")
                                  + (projectController.currentProjectType === "single_short"
                                     ? qsTr("单集短片") : qsTr("多集短剧"))
                            color: secondaryText
                            font.pixelSize: 12
                        }

                        Text {
                            text: qsTr("作品确认状态：")
                                  + (projectController.currentProjectContentStatus === "confirmed"
                                     ? qsTr("已确认") : qsTr("草稿，待确认"))
                            color: secondaryText
                            font.pixelSize: 12
                        }

                        Button {
                            text: projectController.currentProjectContentStatus === "confirmed"
                                  ? qsTr("改回草稿") : qsTr("标记全剧内容已确认")
                            onClicked: projectController.setProjectContentConfirmed(
                                           projectController.currentProjectContentStatus !== "confirmed")
                        }

                        Text {
                            text: qsTr("作品位置：") + projectController.currentProjectPath
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WrapAnywhere
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: borderColor
                        }

                        Text {
                            text: qsTr("作品已打开，可以安全关闭并在资料库中重新打开。")
                            color: primaryText
                            font.pixelSize: 14
                        }

                        Text {
                            text: qsTr("最近保存：") + projectController.lastSavedAt
                            color: secondaryText
                            font.pixelSize: 12
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                             && projectController.currentProjectType === "series"
                    Layout.fillWidth: true
                    implicitHeight: episodeListContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: episodeListContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("剧集结构")
                                color: primaryText
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                            }
                            Button {
                                text: qsTr("新增一集")
                                onClicked: projectController.createEpisode("")
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("作品简报描述全剧方向；每集可以分别维护目标和剧本。集列表顺序可调整。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            model: projectController.episodes
                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 54
                                radius: 6
                                color: modelData.selected ? "#3A3832" : raisedColor
                                border.color: modelData.selected ? accentColor : borderColor

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    Button {
                                        text: modelData.title
                                        highlighted: modelData.selected
                                        onClicked: projectController.selectEpisode(modelData.id)
                                    }

                                    TextField {
                                        Layout.fillWidth: true
                                        text: modelData.title
                                        placeholderText: qsTr("剧集名称")
                                        selectByMouse: true
                                        onEditingFinished: projectController.renameEpisode(
                                                               modelData.id, text)
                                    }

                                    Text {
                                        text: (modelData.contentStatus === "suggestion"
                                               ? qsTr("结构建议 · ")
                                               : modelData.contentStatus === "confirmed"
                                                 ? qsTr("已确认 · ") : qsTr("草稿 · "))
                                              + modelData.sceneCount + qsTr(" 场")
                                        color: secondaryText
                                        font.pixelSize: 11
                                    }

                                    Button {
                                        text: "↑"
                                        onClicked: projectController.moveEpisode(modelData.id, -1)
                                    }
                                    Button {
                                        text: "↓"
                                        onClicked: projectController.moveEpisode(modelData.id, 1)
                                    }
                                    Button {
                                        text: qsTr("复制")
                                        onClicked: projectController.duplicateEpisode(modelData.id)
                                    }
                                    Button {
                                        text: qsTr("移除")
                                        onClicked: {
                                            window.pendingEpisodeRemovalId = modelData.id
                                            removeEpisodeDialog.open()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: bibleContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: bibleContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 10

                        Text {
                            text: qsTr("创作圣经")
                            color: primaryText
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("集中维护作品定位、世界规则、时代、角色、地点、关系、道具、视觉基调和连续性事实。设定可关联到多集，并记录状态、参考资料及修改历史。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ComboBox {
                                id: bibleTypeCombo
                                objectName: "bibleTypeCombo"
                                model: window.bibleTypes
                                textRole: "label"
                                currentIndex: Math.max(0, window.bibleTypes.findIndex(
                                                        item => item.key === window.newBibleItemType))
                                onActivated: window.newBibleItemType = window.bibleTypes[currentIndex].key
                            }

                            TextField {
                                id: bibleItemNameField
                                objectName: "bibleItemNameField"
                                Layout.fillWidth: true
                                placeholderText: qsTr("设定名称")
                                selectByMouse: true
                            }

                            TextField {
                                id: bibleItemDescriptionField
                                Layout.fillWidth: true
                                placeholderText: qsTr("简要描述（可留空）")
                                selectByMouse: true
                            }

                            Button {
                                objectName: "createBibleItemButton"
                                text: qsTr("添加")
                                highlighted: true
                                onClicked: {
                                    if (projectController.createBibleItem(
                                                window.newBibleItemType,
                                                bibleItemNameField.text,
                                                bibleItemDescriptionField.text)) {
                                        bibleItemNameField.text = ""
                                        bibleItemDescriptionField.text = ""
                                    }
                                }
                            }
                        }

                        Repeater {
                            objectName: "bibleItemRepeater"
                            model: projectController.bibleItems
                            delegate: Rectangle {
                                objectName: "bibleItemDelegate"
                                required property var modelData
                                property var bibleItem: modelData
                                Layout.fillWidth: true
                                implicitHeight: bibleItemContent.implicitHeight + 20
                                radius: 7
                                color: raisedColor

                                ColumnLayout {
                                    id: bibleItemContent
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: window.bibleTypeLabel(bibleItem.itemType)
                                            color: accentColor
                                            font.pixelSize: 12
                                        }
                                        TextField {
                                            Layout.fillWidth: true
                                            text: bibleItem.name
                                            placeholderText: qsTr("名称")
                                            selectByMouse: true
                                            enabled: bibleItem.certaintyStatus !== "immutable"
                                            onEditingFinished: projectController.updateBibleItem(
                                                                   bibleItem.id, text,
                                                                   bibleItem.description)
                                        }
                                        ComboBox {
                                            objectName: "bibleCertaintyStatusCombo"
                                            model: window.bibleCertaintyStatuses
                                            textRole: "label"
                                            currentIndex: window.bibleCertaintyStatusIndex(
                                                              bibleItem.certaintyStatus)
                                            onActivated: projectController.setBibleItemCertaintyStatus(
                                                             bibleItem.id,
                                                             window.bibleCertaintyStatuses[currentIndex].key)
                                        }
                                        Button {
                                            objectName: "bibleItemHistoryButton"
                                            text: qsTr("历史")
                                            onClicked: {
                                                const history = projectController.bibleItemHistory(bibleItem.id)
                                                historyDialog.snapshotText = JSON.stringify(history, null, 2)
                                                historyDialog.open()
                                            }
                                        }
                                        Button {
                                            text: qsTr("删除")
                                            enabled: bibleItem.certaintyStatus !== "immutable"
                                            onClicked: {
                                                window.pendingBibleItemId = bibleItem.id
                                                window.pendingBibleItemName = bibleItem.name
                                                removeBibleItemDialog.open()
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        TextField {
                                            Layout.fillWidth: true
                                            text: bibleItem.description
                                            placeholderText: qsTr("简介")
                                            selectByMouse: true
                                            enabled: bibleItem.certaintyStatus !== "immutable"
                                            onEditingFinished: projectController.updateBibleItem(
                                                                   bibleItem.id, bibleItem.name,
                                                                   text)
                                        }
                                        CheckBox {
                                            text: qsTr("用于本集：") + projectController.currentEpisodeTitle
                                            checked: bibleItem.referenced
                                            onClicked: projectController.setEpisodeBibleReference(
                                                           bibleItem.id, checked)
                                        }
                                    }

                                    Repeater {
                                        objectName: "bibleDetailFieldRepeater"
                                        model: window.bibleDetailFields(bibleItem.itemType)
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Text {
                                                Layout.preferredWidth: 100
                                                text: modelData.label
                                                color: secondaryText
                                                font.pixelSize: 12
                                            }
                                            TextField {
                                                objectName: "bibleDetail_" + modelData.key
                                                Layout.fillWidth: true
                                                text: bibleItem.details[modelData.key] || ""
                                                selectByMouse: true
                                                enabled: bibleItem.certaintyStatus !== "immutable"
                                                onEditingFinished: projectController.updateBibleItemDetail(
                                                                       bibleItem.id,
                                                                       modelData.key,
                                                                       text)
                                            }
                                        }
                                    }

                                    Repeater {
                                        model: bibleItem.references
                                        delegate: RowLayout {
                                            required property var modelData
                                            required property int index
                                            Layout.fillWidth: true
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.source + " — " + modelData.purpose
                                                color: secondaryText
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                            }
                                            Button {
                                                text: qsTr("移除资料")
                                                enabled: bibleItem.certaintyStatus !== "immutable"
                                                onClicked: projectController.removeBibleItemReference(
                                                               bibleItem.id, index)
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        TextField {
                                            id: referenceSourceField
                                            objectName: "bibleReferenceSourceField"
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("参考资料路径或来源")
                                            selectByMouse: true
                                            enabled: bibleItem.certaintyStatus !== "immutable"
                                        }
                                        TextField {
                                            id: referencePurposeField
                                            objectName: "bibleReferencePurposeField"
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("参考用途，例如外观、年代考据")
                                            selectByMouse: true
                                            enabled: bibleItem.certaintyStatus !== "immutable"
                                        }
                                        Button {
                                            objectName: "addBibleReferenceButton"
                                            text: qsTr("添加资料")
                                            enabled: bibleItem.certaintyStatus !== "immutable"
                                            onClicked: {
                                                if (projectController.addBibleItemReference(
                                                            bibleItem.id,
                                                            referenceSourceField.text,
                                                            referencePurposeField.text)) {
                                                    referenceSourceField.text = ""
                                                    referencePurposeField.text = ""
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            visible: projectController.bibleItems.length === 0
                            text: qsTr("创作圣经还没有条目。可以先添加角色、地点、规则或其它设定；信息暂不完整也可以留空并标记待决定。")
                            color: secondaryText
                            font.pixelSize: 12
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                             && projectController.currentProjectType === "series"
                    Layout.fillWidth: true
                    implicitHeight: episodeBriefContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: episodeBriefContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 10

                        Text {
                            text: qsTr("单集创作简报 · ") + projectController.currentEpisodeTitle
                            color: primaryText
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: qsTr("单集状态：")
                                  + (projectController.currentEpisodeContentStatus === "confirmed"
                                     ? qsTr("已确认")
                                     : projectController.currentEpisodeContentStatus === "suggestion"
                                       ? qsTr("模板建议，待采纳") : qsTr("草稿，待确认"))
                            color: secondaryText
                            font.pixelSize: 12
                        }

                        Button {
                            visible: projectController.currentEpisodeContentStatus === "suggestion"
                            text: qsTr("将结构建议作为草稿开始编辑")
                            onClicked: projectController.acceptEpisodeSuggestions()
                        }

                        Button {
                            visible: projectController.currentEpisodeContentStatus !== "suggestion"
                            text: projectController.currentEpisodeContentStatus === "confirmed"
                                  ? qsTr("改回单集草稿") : qsTr("标记本集内容已确认")
                            onClicked: projectController.setEpisodeContentConfirmed(
                                           projectController.currentEpisodeContentStatus !== "confirmed")
                        }

                        TextArea {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            text: projectController.currentEpisodeBrief.description || ""
                            placeholderText: qsTr("本集想讲什么？可暂时留空。")
                            wrapMode: TextEdit.Wrap
                            onTextChanged: if (activeFocus)
                                               projectController.updateEpisodeField("description", text)
                        }

                        Repeater {
                            model: window.storyDevelopmentFields
                            delegate: ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Text {
                                    text: modelData.label
                                    color: secondaryText
                                    font.pixelSize: 12
                                }
                                TextArea {
                                    objectName: "episodeStoryField_" + modelData.key
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 72
                                    text: projectController.currentEpisodeBrief[modelData.key] || ""
                                    placeholderText: modelData.hint
                                    wrapMode: TextEdit.Wrap
                                    selectByMouse: true
                                    onTextChanged: if (activeFocus)
                                                       projectController.updateEpisodeField(
                                                           modelData.key, text)
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 14
                            rowSpacing: 8

                            Repeater {
                                model: [
                                    {label: qsTr("题材"), key: "genre"},
                                    {label: qsTr("主题"), key: "theme"},
                                    {label: qsTr("目标受众"), key: "targetAudience"},
                                    {label: qsTr("情绪基调"), key: "tone"},
                                    {label: qsTr("目标时长"), key: "targetDuration"},
                                    {label: qsTr("语言"), key: "language"},
                                    {label: qsTr("发布方向"), key: "platformDirection"}
                                ]
                                delegate: ColumnLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Text {
                                        text: modelData.label
                                        color: secondaryText
                                        font.pixelSize: 12
                                    }
                                    TextField {
                                        Layout.fillWidth: true
                                        text: projectController.currentEpisodeBrief[modelData.key] || ""
                                        placeholderText: qsTr("待决定")
                                        selectByMouse: true
                                        onTextEdited: projectController.updateEpisodeField(
                                                          modelData.key, text)
                                    }
                                }
                            }
                        }

                        TextArea {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                            text: projectController.currentEpisodeBrief.requiredElements || ""
                            placeholderText: qsTr("本集必须包含；可留空")
                            wrapMode: TextEdit.Wrap
                            onTextChanged: if (activeFocus)
                                               projectController.updateEpisodeField("requiredElements", text)
                        }

                        TextArea {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                            text: projectController.currentEpisodeBrief.forbiddenElements || ""
                            placeholderText: qsTr("本集不要包含；可留空")
                            wrapMode: TextEdit.Wrap
                            onTextChanged: if (activeFocus)
                                               projectController.updateEpisodeField("forbiddenElements", text)
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: briefContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: briefContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 12

                        Text {
                            text: projectController.currentProjectType === "series"
                                  ? qsTr("全剧创作简报") : qsTr("创作简报")
                            color: primaryText
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            objectName: "undecidedFieldsHint"
                            text: qsTr("自由创意和结构化信息可以同时填写。空白字段表示“待决定”，可跳过，之后再补充。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            text: qsTr("故事想法")
                            color: primaryText
                            font.pixelSize: 13
                        }

                        TextArea {
                            id: briefDescriptionField
                            objectName: "briefDescriptionField"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120
                            text: projectController.briefDescription
                            placeholderText: qsTr("描述你想讲的故事，也可以暂时留空。")
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            onTextChanged: {
                                if (activeFocus) {
                                    projectController.updateDraftField("description", text)
                                }
                            }
                        }

                        Repeater {
                            id: projectStoryFieldsRepeater
                            objectName: "projectStoryFieldsRepeater"
                            model: window.storyDevelopmentFields
                            delegate: ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Text {
                                    text: modelData.label
                                    color: secondaryText
                                    font.pixelSize: 12
                                }
                                TextArea {
                                    objectName: "projectStoryField_" + modelData.key
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: modelData.key === "episodeOutline"
                                                            || modelData.key === "sceneOutline"
                                                            ? 110 : 84
                                    text: projectController.currentProjectStory[modelData.key] || ""
                                    placeholderText: modelData.hint
                                    wrapMode: TextEdit.Wrap
                                    selectByMouse: true
                                    onTextChanged: if (activeFocus)
                                                       projectController.updateDraftField(
                                                           modelData.key, text)
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 16
                            rowSpacing: 10

                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("题材"); color: secondaryText; font.pixelSize: 12 }
                                TextField {
                                    objectName: "briefGenreField"
                                    Layout.fillWidth: true
                                    text: projectController.briefGenre
                                    placeholderText: qsTr("待决定")
                                    selectByMouse: true
                                    onTextEdited: projectController.updateDraftField("genre", text)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("主题"); color: secondaryText; font.pixelSize: 12 }
                                TextField {
                                    Layout.fillWidth: true
                                    text: projectController.briefTheme
                                    placeholderText: qsTr("待决定")
                                    selectByMouse: true
                                    onTextEdited: projectController.updateDraftField("theme", text)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("目标受众"); color: secondaryText; font.pixelSize: 12 }
                                TextField {
                                    Layout.fillWidth: true
                                    text: projectController.briefTargetAudience
                                    placeholderText: qsTr("待决定")
                                    selectByMouse: true
                                    onTextEdited: projectController.updateDraftField("targetAudience", text)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("情绪基调"); color: secondaryText; font.pixelSize: 12 }
                                TextField {
                                    Layout.fillWidth: true
                                    text: projectController.briefTone
                                    placeholderText: qsTr("待决定")
                                    selectByMouse: true
                                    onTextEdited: projectController.updateDraftField("tone", text)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("目标时长"); color: secondaryText; font.pixelSize: 12 }
                                TextField {
                                    Layout.fillWidth: true
                                    text: projectController.briefTargetDuration
                                    placeholderText: qsTr("如：约 3 分钟；待决定")
                                    selectByMouse: true
                                    onTextEdited: projectController.updateDraftField("targetDuration", text)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("创作语言"); color: secondaryText; font.pixelSize: 12 }
                                TextField {
                                    Layout.fillWidth: true
                                    text: projectController.briefLanguage
                                    placeholderText: qsTr("中文")
                                    selectByMouse: true
                                    onTextEdited: projectController.updateDraftField("language", text)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("平台方向"); color: secondaryText; font.pixelSize: 12 }
                                TextField {
                                    Layout.fillWidth: true
                                    text: projectController.briefPlatformDirection
                                    placeholderText: qsTr("可留空，待决定")
                                    selectByMouse: true
                                    onTextEdited: projectController.updateDraftField("platformDirection", text)
                                }
                            }
                        }

                        Text {
                            text: qsTr("必须包含")
                            color: secondaryText
                            font.pixelSize: 12
                        }

                        TextArea {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 72
                            text: projectController.briefRequiredElements
                            placeholderText: qsTr("没有特别要求可留空。")
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            onTextChanged: {
                                if (activeFocus) {
                                    projectController.updateDraftField("requiredElements", text)
                                }
                            }
                        }

                        Text {
                            text: qsTr("不要包含")
                            color: secondaryText
                            font.pixelSize: 12
                        }

                        TextArea {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 72
                            text: projectController.briefForbiddenElements
                            placeholderText: qsTr("没有特别限制可留空。")
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            onTextChanged: {
                                if (activeFocus) {
                                    projectController.updateDraftField("forbiddenElements", text)
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: sceneContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: sceneContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("场次身份 · ") + projectController.currentEpisodeTitle
                                color: primaryText
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                            }
                            Button {
                                objectName: "addSceneButton"
                                text: qsTr("新增场次")
                                onClicked: projectController.createScene()
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("按场次组织剧本内容。删除场次会将其段落保留为未关联场次，不会丢弃正文。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            id: scenesRepeater
                            model: projectController.scenes
                            delegate: ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 7
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: qsTr("场次 %1").arg(modelData.sortOrder + 1)
                                        color: secondaryText
                                        font.pixelSize: 12
                                    }
                                    TextField {
                                        Layout.fillWidth: true
                                        text: modelData.title
                                        placeholderText: qsTr("场次标题（可待决定）")
                                        selectByMouse: true
                                        onEditingFinished: projectController.updateSceneTitle(
                                                               modelData.id, text)
                                    }
                                    Text {
                                        text: modelData.contentStatus === "suggestion"
                                              ? qsTr("建议，待确认")
                                              : window.contentStatuses[window.contentStatusIndex(
                                                    modelData.contentStatus)].label
                                        color: secondaryText
                                        font.pixelSize: 11
                                    }
                                    ComboBox {
                                        objectName: "sceneStatusCombo_" + modelData.id
                                        model: window.contentStatuses
                                        textRole: "label"
                                        currentIndex: window.contentStatusIndex(modelData.contentStatus)
                                        onActivated: projectController.setSceneContentStatus(
                                                         modelData.id,
                                                         window.contentStatuses[currentIndex].key)
                                    }
                                    Button {
                                        text: qsTr("历史")
                                        onClicked: {
                                            const history = projectController.sceneHistory(modelData.id)
                                            historyDialog.snapshotText = JSON.stringify(history, null, 2)
                                            historyDialog.open()
                                        }
                                    }
                                    Button {
                                        text: qsTr("复制场次")
                                        onClicked: projectController.duplicateScene(modelData.id)
                                    }
                                    Button {
                                        text: "↑"
                                        onClicked: projectController.moveScene(modelData.id, -1)
                                    }
                                    Button {
                                        text: "↓"
                                        onClicked: projectController.moveScene(modelData.id, 1)
                                    }
                                    Button {
                                        text: qsTr("移除")
                                        onClicked: {
                                            window.pendingSceneRemovalId = modelData.id
                                            removeSceneDialog.open()
                                        }
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    TextField {
                                        id: sceneGoalInput
                                        objectName: "sceneGoal_" + modelData.id
                                        Layout.fillWidth: true
                                        text: modelData.goal
                                        placeholderText: qsTr("本场目标（可留空）")
                                        selectByMouse: true
                                        onEditingFinished: projectController.updateSceneQualityFields(
                                                               modelData.id, text,
                                                               sceneConflictInput.text,
                                                               sceneDurationInput.text)
                                    }
                                    TextField {
                                        id: sceneConflictInput
                                        objectName: "sceneConflictOrTurn_" + modelData.id
                                        Layout.fillWidth: true
                                        text: modelData.conflictOrTurn
                                        placeholderText: qsTr("阻碍 / 冲突 / 转折（可留空）")
                                        selectByMouse: true
                                        onEditingFinished: projectController.updateSceneQualityFields(
                                                               modelData.id, sceneGoalInput.text,
                                                               text, sceneDurationInput.text)
                                    }
                                    TextField {
                                        id: sceneDurationInput
                                        objectName: "sceneTargetDuration_" + modelData.id
                                        Layout.preferredWidth: 150
                                        text: modelData.targetDurationSeconds > 0
                                              ? String(modelData.targetDurationSeconds) : ""
                                        placeholderText: qsTr("目标秒数（可选）")
                                        validator: IntValidator { bottom: 1; top: 86400 }
                                        selectByMouse: true
                                        onEditingFinished: projectController.updateSceneQualityFields(
                                                               modelData.id, sceneGoalInput.text,
                                                               sceneConflictInput.text, text)
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: scriptContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: scriptContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 12

                        Text {
                            text: qsTr("故事 / 剧本正文")
                                  + (projectController.currentProjectType === "series"
                                     ? qsTr(" · ") + projectController.currentEpisodeTitle
                                     : "")
                            color: primaryText
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: projectController.scriptTextEditable
                                  ? qsTr("可继续使用自由文本，也可以添加语义段落，把剧本整理为场景标题、动作、角色和对白等结构。")
                                  : qsTr("每段内容都可独立编辑、排序、复制、拆分、合并并查看历史。变更会先作为草稿保存。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            ComboBox {
                                id: newScriptBlockTypeCombo
                                objectName: "newScriptBlockTypeCombo"
                                Layout.preferredWidth: 160
                                model: window.scriptBlockTypes
                                textRole: "label"
                                currentIndex: window.scriptBlockTypeIndex(window.newScriptBlockType)
                                onActivated: window.newScriptBlockType =
                                                 window.scriptBlockTypes[currentIndex].key
                            }
                            ComboBox {
                                id: newScriptSceneCombo
                                objectName: "newScriptSceneCombo"
                                Layout.fillWidth: true
                                model: window.scriptSceneOptions
                                textRole: "title"
                                currentIndex: window.scriptSceneIndex(window.newScriptSceneId)
                                onActivated: window.newScriptSceneId =
                                                 window.scriptSceneOptions[currentIndex].id
                            }
                            Button {
                                objectName: "addScriptBlockButton"
                                text: qsTr("添加剧本段落")
                                highlighted: true
                                onClicked: projectController.createScriptBlock(
                                               window.newScriptBlockType,
                                               window.scriptSceneOptions[
                                                   newScriptSceneCombo.currentIndex].id)
                            }
                        }

                        TextArea {
                            id: scriptTextField
                            objectName: "scriptTextField"
                            visible: projectController.scriptTextEditable
                            Layout.fillWidth: true
                            Layout.preferredHeight: 360
                            text: projectController.scriptText
                            placeholderText: qsTr("输入剧本正文；或添加语义段落开始结构化创作。")
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            onTextChanged: {
                                if (activeFocus) {
                                    projectController.updateDraftField("scriptText", text)
                                }
                            }
                        }

                        Repeater {
                            id: scriptBlockRepeater
                            objectName: "scriptBlockRepeater"
                            visible: !projectController.scriptTextEditable
                            model: projectController.scriptBlocks
                            delegate: Rectangle {
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                implicitHeight: blockContent.implicitHeight + 18
                                radius: 7
                                color: raisedColor
                                border.color: borderColor

                                ColumnLayout {
                                    id: blockContent
                                    anchors.fill: parent
                                    anchors.margins: 9
                                    spacing: 7

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("段落 %1 · %2")
                                                  .arg(modelData.sortOrder + 1)
                                                  .arg(window.scriptBlockTypeLabel(modelData.blockType))
                                            color: accentColor
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }
                                        ComboBox {
                                            objectName: "scriptBlockType_" + modelData.id
                                            model: window.scriptBlockTypes
                                            textRole: "label"
                                            currentIndex: window.scriptBlockTypeIndex(
                                                              modelData.blockType)
                                            onActivated: projectController.updateScriptBlock(
                                                             modelData.id,
                                                             window.scriptBlockTypes[currentIndex].key,
                                                             blockEditor.text)
                                        }
                                        ComboBox {
                                            objectName: "scriptBlockStatus_" + modelData.id
                                            model: window.contentStatuses
                                            textRole: "label"
                                            currentIndex: window.contentStatusIndex(
                                                              modelData.contentStatus)
                                            onActivated: projectController.setScriptBlockStatus(
                                                             modelData.id,
                                                             window.contentStatuses[currentIndex].key)
                                        }
                                        Button {
                                            text: qsTr("历史")
                                            onClicked: {
                                                const history = projectController.scriptBlockHistory(
                                                                    modelData.id)
                                                historyDialog.snapshotText = JSON.stringify(
                                                            history, null, 2)
                                                historyDialog.open()
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: qsTr("所属场次")
                                            color: secondaryText
                                            font.pixelSize: 12
                                        }
                                        ComboBox {
                                            objectName: "scriptBlockScene_" + modelData.id
                                            Layout.fillWidth: true
                                            model: window.scriptSceneOptions
                                            textRole: "title"
                                            currentIndex: window.scriptSceneIndex(modelData.sceneId)
                                            onActivated: projectController.setScriptBlockScene(
                                                             modelData.id,
                                                             window.scriptSceneOptions[currentIndex].id)
                                        }
                                        Button {
                                            text: "↑"
                                            enabled: modelData.sortOrder > 0
                                            onClicked: projectController.moveScriptBlock(
                                                           modelData.id, -1)
                                        }
                                        Button {
                                            text: "↓"
                                            enabled: modelData.sortOrder
                                                     < projectController.scriptBlocks.length - 1
                                            onClicked: projectController.moveScriptBlock(
                                                           modelData.id, 1)
                                        }
                                        Button {
                                            text: qsTr("复制")
                                            onClicked: projectController.duplicateScriptBlock(
                                                           modelData.id)
                                        }
                                        Button {
                                            text: qsTr("拆分")
                                            onClicked: projectController.splitScriptBlock(
                                                           modelData.id, blockEditor.cursorPosition)
                                        }
                                    }

                                    TextArea {
                                        id: blockEditor
                                        objectName: "scriptBlockEditor_" + modelData.id
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: Math.max(72,
                                                                        Math.min(180,
                                                                                 contentHeight + 24))
                                        text: modelData.text
                                        placeholderText: qsTr("输入这段内容；拆分时先把光标放在要拆分的位置。")
                                        wrapMode: TextEdit.Wrap
                                        selectByMouse: true
                                        onTextChanged: if (activeFocus)
                                                           projectController.updateScriptBlock(
                                                               modelData.id,
                                                               modelData.blockType, text)
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Button {
                                            text: qsTr("合并到上一段")
                                            enabled: index > 0
                                                     && projectController.scriptBlocks[index - 1].sceneId
                                                        === modelData.sceneId
                                                     && projectController.scriptBlocks[index - 1].blockType
                                                        === modelData.blockType
                                                     && projectController.scriptBlocks[index - 1].contentStatus
                                                        === modelData.contentStatus
                                            onClicked: projectController.mergeScriptBlocks(
                                                           projectController.scriptBlocks[index - 1].id,
                                                           modelData.id)
                                        }
                                        Button {
                                            text: qsTr("合并下一段")
                                            enabled: index + 1 < projectController.scriptBlocks.length
                                                     && projectController.scriptBlocks[index + 1].sceneId
                                                        === modelData.sceneId
                                                     && projectController.scriptBlocks[index + 1].blockType
                                                        === modelData.blockType
                                                     && projectController.scriptBlocks[index + 1].contentStatus
                                                        === modelData.contentStatus
                                            onClicked: projectController.mergeScriptBlocks(
                                                           modelData.id,
                                                           projectController.scriptBlocks[index + 1].id)
                                        }
                                        Item { Layout.fillWidth: true }
                                        Button {
                                            text: qsTr("删除段落")
                                            onClicked: projectController.removeScriptBlock(modelData.id)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: storyboardContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: storyboardContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 10

                        Text {
                            text: qsTr("镜头与视觉参考基础")
                            color: primaryText
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("当前支持手工记录镜头描述、剧本段落/设定关联，以及可复用的文字参考说明。这里不导入图片文件，也不调用图片生成服务。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            ComboBox {
                                id: newShotSceneCombo
                                objectName: "newShotSceneCombo"
                                Layout.fillWidth: true
                                model: window.storyboardSceneOptions
                                textRole: "title"
                                currentIndex: Math.max(0, window.storyboardSceneOptions.findIndex(
                                    entry => entry.id === window.newShotSceneId))
                                onActivated: window.newShotSceneId =
                                                 window.storyboardSceneOptions[currentIndex].id
                            }
                            Button {
                                objectName: "createShotButton"
                                text: qsTr("添加镜头")
                                enabled: window.storyboardSceneOptions.length > 0
                                onClicked: projectController.createShot(
                                               window.storyboardSceneOptions[newShotSceneCombo.currentIndex].id)
                            }
                            ComboBox {
                                id: newReferenceSceneCombo
                                objectName: "newReferenceSceneCombo"
                                Layout.fillWidth: true
                                model: window.storyboardSceneOptions
                                textRole: "title"
                                currentIndex: Math.max(0, window.storyboardSceneOptions.findIndex(
                                    entry => entry.id === window.newVisualReferenceSceneId))
                                onActivated: window.newVisualReferenceSceneId =
                                                 window.storyboardSceneOptions[currentIndex].id
                            }
                            Button {
                                objectName: "createVisualReferenceButton"
                                text: qsTr("添加文字参考")
                                enabled: window.storyboardSceneOptions.length > 0
                                onClicked: projectController.createVisualReference(
                                               window.storyboardSceneOptions[
                                                   newReferenceSceneCombo.currentIndex].id)
                            }
                        }

                        Repeater {
                            id: shotsRepeater
                            objectName: "shotsRepeater"
                            model: projectController.shots
                            delegate: Rectangle {
                                id: shotCard
                                required property var modelData
                                property var shotInfo: modelData
                                Layout.fillWidth: true
                                implicitHeight: shotContent.implicitHeight + 18
                                radius: 7
                                color: raisedColor
                                border.color: borderColor

                                ColumnLayout {
                                    id: shotContent
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 7
                                    RowLayout {
                                        Layout.fillWidth: true
                                        TextField {
                                            id: shotTitleField
                                            objectName: "shotTitle_" + shotCard.shotInfo.id
                                            Layout.fillWidth: true
                                            text: shotCard.shotInfo.title
                                            placeholderText: qsTr("镜头标题")
                                            onTextChanged: if (activeFocus)
                                                projectController.updateShot(
                                                    shotCard.shotInfo.id, text,
                                                    shotDescriptionField.text)
                                        }
                                        Text {
                                            text: window.sceneLabel(shotCard.shotInfo.sceneId)
                                            color: secondaryText
                                            font.pixelSize: 12
                                        }
                                        ComboBox {
                                            objectName: "shotStatus_" + shotCard.shotInfo.id
                                            model: window.contentStatuses
                                            textRole: "label"
                                            currentIndex: window.contentStatusIndex(
                                                shotCard.shotInfo.contentStatus)
                                            onActivated: projectController.setShotContentStatus(
                                                shotCard.shotInfo.id,
                                                window.contentStatuses[currentIndex].key)
                                        }
                                        Button {
                                            text: qsTr("删除镜头")
                                            onClicked: projectController.removeShot(
                                                shotCard.shotInfo.id)
                                        }
                                    }
                                    TextArea {
                                        id: shotDescriptionField
                                        objectName: "shotDescription_" + shotCard.shotInfo.id
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 70
                                        text: shotCard.shotInfo.description
                                        placeholderText: qsTr("画面主体、动作、构图或镜头运动描述")
                                        wrapMode: TextEdit.Wrap
                                        onTextChanged: if (activeFocus)
                                            projectController.updateShot(
                                                shotCard.shotInfo.id, shotTitleField.text, text)
                                    }
                                    Text {
                                        text: qsTr("关联剧本段落")
                                        color: secondaryText
                                        font.pixelSize: 12
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        Repeater {
                                            model: projectController.scriptBlocks
                                            delegate: CheckBox {
                                                required property var modelData
                                                objectName: "shotScriptBlockLink_"
                                                             + shotCard.shotInfo.id + "_"
                                                             + modelData.id
                                                text: qsTr("%1 · %2").arg(modelData.sortOrder + 1)
                                                         .arg(window.scriptBlockTypeLabel(
                                                             modelData.blockType))
                                                checked: shotCard.shotInfo.scriptBlockIds.includes(
                                                             modelData.id)
                                                onToggled: projectController.setShotScriptBlock(
                                                    shotCard.shotInfo.id, modelData.id, checked)
                                            }
                                        }
                                    }
                                    Text {
                                        text: qsTr("关联创作圣经条目")
                                        color: secondaryText
                                        font.pixelSize: 12
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        Repeater {
                                            model: projectController.bibleItems
                                            delegate: CheckBox {
                                                required property var modelData
                                                objectName: "shotBibleItemLink_"
                                                             + shotCard.shotInfo.id + "_"
                                                             + modelData.id
                                                text: modelData.name
                                                checked: shotCard.shotInfo.bibleItemIds.includes(
                                                             modelData.id)
                                                onToggled: projectController.setShotBibleReference(
                                                    shotCard.shotInfo.id, modelData.id, checked)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Repeater {
                            id: visualReferencesRepeater
                            objectName: "visualReferencesRepeater"
                            model: projectController.visualReferences
                            delegate: Rectangle {
                                id: visualReferenceCard
                                required property var modelData
                                property var referenceInfo: modelData
                                Layout.fillWidth: true
                                implicitHeight: referenceContent.implicitHeight + 18
                                radius: 7
                                color: raisedColor
                                border.color: borderColor

                                ColumnLayout {
                                    id: referenceContent
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 7
                                    RowLayout {
                                        Layout.fillWidth: true
                                        TextField {
                                            id: referenceTitleField
                                            objectName: "visualReferenceTitle_"
                                                            + visualReferenceCard.referenceInfo.id
                                            Layout.fillWidth: true
                                            text: visualReferenceCard.referenceInfo.title
                                            onTextChanged: if (activeFocus)
                                                projectController.updateVisualReference(
                                                    visualReferenceCard.referenceInfo.id, text,
                                                    referenceDescriptionField.text,
                                                    window.visualReferenceUsages[
                                                        referenceUsageCombo.currentIndex].key)
                                        }
                                        Text {
                                            text: window.sceneLabel(
                                                visualReferenceCard.referenceInfo.sceneId)
                                            color: secondaryText
                                            font.pixelSize: 12
                                        }
                                        ComboBox {
                                            id: referenceUsageCombo
                                            objectName: "visualReferenceUsage_"
                                                            + visualReferenceCard.referenceInfo.id
                                            model: window.visualReferenceUsages
                                            textRole: "label"
                                            currentIndex: window.visualReferenceUsageIndex(
                                                visualReferenceCard.referenceInfo.usage)
                                            onActivated: projectController.updateVisualReference(
                                                visualReferenceCard.referenceInfo.id,
                                                referenceTitleField.text,
                                                referenceDescriptionField.text,
                                                window.visualReferenceUsages[currentIndex].key)
                                        }
                                        Button {
                                            text: qsTr("删除参考")
                                            onClicked: projectController.removeVisualReference(
                                                visualReferenceCard.referenceInfo.id)
                                        }
                                    }
                                    TextArea {
                                        id: referenceDescriptionField
                                        objectName: "visualReferenceDescription_"
                                                        + visualReferenceCard.referenceInfo.id
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 70
                                        text: visualReferenceCard.referenceInfo.description
                                        placeholderText: qsTr("记录希望保持的角色外观、色彩、光线或构图特点")
                                        wrapMode: TextEdit.Wrap
                                        onTextChanged: if (activeFocus)
                                            projectController.updateVisualReference(
                                                visualReferenceCard.referenceInfo.id,
                                                referenceTitleField.text, text,
                                                window.visualReferenceUsages[
                                                    referenceUsageCombo.currentIndex].key)
                                    }
                                    Text {
                                        text: qsTr("复用于镜头")
                                        color: secondaryText
                                        font.pixelSize: 12
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        Repeater {
                                            model: projectController.shots
                                            delegate: CheckBox {
                                                required property var modelData
                                                objectName: "visualReferenceShotLink_"
                                                             + visualReferenceCard.referenceInfo.id + "_"
                                                             + modelData.id
                                                text: modelData.title
                                                checked: visualReferenceCard.referenceInfo.shotIds.includes(
                                                             modelData.id)
                                                onToggled: projectController.setVisualReferenceShot(
                                                    visualReferenceCard.referenceInfo.id,
                                                    modelData.id, checked)
                                            }
                                        }
                                    }
                                    Text {
                                        text: qsTr("关联创作圣经条目")
                                        color: secondaryText
                                        font.pixelSize: 12
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        Repeater {
                                            model: projectController.bibleItems
                                            delegate: CheckBox {
                                                required property var modelData
                                                objectName: "visualReferenceBibleItemLink_"
                                                             + visualReferenceCard.referenceInfo.id + "_"
                                                             + modelData.id
                                                text: modelData.name
                                                checked: visualReferenceCard.referenceInfo.bibleItemIds.includes(
                                                             modelData.id)
                                                onToggled: projectController.setVisualReferenceBibleItem(
                                                    visualReferenceCard.referenceInfo.id,
                                                    modelData.id, checked)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        Text {
                            visible: projectController.shots.length === 0
                                     && projectController.visualReferences.length === 0
                            text: qsTr("尚未记录镜头或视觉参考。")
                            color: secondaryText
                            font.pixelSize: 12
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: scriptVersionsContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: scriptVersionsContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 10

                        Text {
                            text: qsTr("剧本版本与语言")
                            color: primaryText
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("版本相互独立。译稿从当前剧本或所选版本复制结构与段落，修改译稿不会改动原稿。恢复前会显示差异，并自动保留恢复前的当前剧本。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                id: scriptVersionLanguageField
                                objectName: "scriptVersionLanguageField"
                                Layout.preferredWidth: 150
                                text: window.scriptVersionLanguage
                                placeholderText: qsTr("版本语言")
                                selectByMouse: true
                                onTextEdited: window.scriptVersionLanguage = text
                            }
                            ComboBox {
                                id: scriptVersionTypeCombo
                                objectName: "scriptVersionTypeCombo"
                                model: window.scriptVersionTypes
                                textRole: "label"
                                currentIndex: window.scriptVersionTypeIndex(
                                                  window.scriptVersionType)
                                onActivated: window.scriptVersionType =
                                                 window.scriptVersionTypes[currentIndex].key
                            }
                            ComboBox {
                                id: scriptVersionSourceCombo
                                objectName: "scriptVersionSourceCombo"
                                Layout.fillWidth: true
                                model: window.scriptVersionSourceOptions
                                textRole: "title"
                                currentIndex: window.scriptVersionSourceIndex(
                                                  window.scriptVersionSourceId)
                                onActivated: window.scriptVersionSourceId =
                                                 window.scriptVersionSourceOptions[currentIndex].id
                            }
                            Button {
                                objectName: "createScriptVersionButton"
                                text: qsTr("创建独立版本")
                                highlighted: true
                                onClicked: projectController.createScriptVersion(
                                               scriptVersionLanguageField.text,
                                               window.scriptVersionType,
                                               scriptVersionSourceCombo.currentIndex === 0
                                                   ? ""
                                                   : window.scriptVersionSourceOptions[
                                                         scriptVersionSourceCombo.currentIndex].id)
                            }
                        }

                        Repeater {
                            id: scriptVersionsRepeater
                            objectName: "scriptVersionsRepeater"
                            model: projectController.scriptVersions
                            delegate: Rectangle {
                                id: scriptVersionCard
                                required property var modelData
                                property var versionInfo: modelData
                                Layout.fillWidth: true
                                implicitHeight: versionContent.implicitHeight + 18
                                radius: 7
                                color: raisedColor
                                border.color: borderColor

                                ColumnLayout {
                                    id: versionContent
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.language + " · "
                                                  + window.scriptVersionTypeLabel(
                                                        modelData.versionType)
                                            color: accentColor
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }
                                        ComboBox {
                                            objectName: "scriptVersionStatus_" + modelData.id
                                            model: window.scriptVersionStatuses
                                            textRole: "label"
                                            currentIndex: window.scriptVersionStatusIndex(
                                                              modelData.contentStatus)
                                            onActivated: projectController.setScriptVersionStatus(
                                                             modelData.id,
                                                             window.scriptVersionStatuses[currentIndex].key)
                                        }
                                        Button {
                                            text: qsTr("历史")
                                            onClicked: {
                                                const history = projectController.scriptVersionHistory(
                                                                    modelData.id)
                                                historyDialog.snapshotText = JSON.stringify(
                                                            history, null, 2)
                                                historyDialog.open()
                                            }
                                        }
                                    }

                                    Repeater {
                                        model: scriptVersionCard.versionInfo.blocks
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Text {
                                                    text: qsTr("%1 原文 · %2 · %3 · %4")
                                                          .arg(modelData.sourceLanguage)
                                                          .arg(modelData.sortOrder + 1)
                                                          .arg(window.scriptBlockTypeLabel(
                                                                   modelData.blockType))
                                                          .arg(window.sceneLabel(modelData.sceneId))
                                                    color: secondaryText
                                                    font.pixelSize: 11
                                                }
                                                TextArea {
                                                    objectName: "scriptVersionSourceBlock_"
                                                                 + modelData.id
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: Math.max(64,
                                                                                    Math.min(150,
                                                                                             contentHeight + 20))
                                                    text: modelData.sourceText
                                                    readOnly: true
                                                    placeholderText: qsTr("原文暂无对应段落")
                                                    wrapMode: TextEdit.Wrap
                                                    selectByMouse: true
                                                }
                                            }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Text {
                                                    text: qsTr("%1 版本")
                                                          .arg(scriptVersionCard.versionInfo.language)
                                                    color: secondaryText
                                                    font.pixelSize: 11
                                                }
                                                TextArea {
                                                    objectName: "scriptVersionBlock_" + modelData.id
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: Math.max(64,
                                                                                    Math.min(150,
                                                                                             contentHeight + 20))
                                                    text: modelData.text
                                                    readOnly: scriptVersionCard.versionInfo.contentStatus
                                                              === "archived"
                                                              || scriptVersionCard.versionInfo.contentStatus
                                                                 === "confirmed"
                                                    placeholderText: qsTr("编辑此版本段落")
                                                    wrapMode: TextEdit.Wrap
                                                    selectByMouse: true
                                                    onTextChanged: if (activeFocus)
                                                                       projectController.updateScriptVersionBlock(
                                                                           scriptVersionCard.versionInfo.id,
                                                                           modelData.id, text)
                                                }
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        ComboBox {
                                            objectName: "scriptVersionCompareSource_" + modelData.id
                                            Layout.fillWidth: true
                                            model: window.scriptVersionSourceOptions
                                            textRole: "title"
                                            currentIndex: window.scriptVersionSourceIndex(
                                                              window.scriptVersionCompareSourceId)
                                            onActivated: window.scriptVersionCompareSourceId =
                                                             window.scriptVersionSourceOptions[
                                                                 currentIndex].id
                                        }
                                        Button {
                                            objectName: "compareScriptVersionButton_" + modelData.id
                                            text: qsTr("比较差异")
                                            onClicked: {
                                                window.scriptVersionComparison =
                                                        projectController.compareScriptVersions(
                                                            window.scriptVersionCompareSourceId,
                                                            modelData.id)
                                                versionComparisonDialog.open()
                                            }
                                        }
                                        Button {
                                            objectName: "restoreScriptVersionButton_" + modelData.id
                                            text: qsTr("恢复到当前剧本…")
                                            onClicked: {
                                                window.pendingScriptVersionRestoreId = modelData.id
                                                window.scriptVersionComparison =
                                                        projectController.compareScriptVersions(
                                                            "", modelData.id)
                                                restoreScriptVersionDialog.open()
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            visible: projectController.scriptVersions.length === 0
                            text: qsTr("还没有独立版本。可以从当前剧本创建原稿快照、译稿或用户编辑稿。")
                            color: secondaryText
                            font.pixelSize: 12
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: qualityContent.implicitHeight + 40
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: qualityContent
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("时长估算与剧本检查")
                                color: primaryText
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                            }
                            Button {
                                objectName: "runQualityCheckButton"
                                text: projectController.qualityReport.findings === undefined
                                      ? qsTr("运行检查") : qsTr("重新检查")
                                highlighted: true
                                onClicked: projectController.runQualityCheck()
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("估算根据对白与动作文字长度生成，只用于创作规划，不代表成片时长；规则提示需人工判断，不会修改剧本。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            visible: window.qualityProjectEstimate.available === true
                            Layout.fillWidth: true
                            text: qsTr("全剧约 %1（估算范围 %2）")
                                  .arg(window.qualityProjectEstimate.display || "")
                                  .arg(window.qualityProjectEstimate.rangeDisplay || "")
                            color: accentColor
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        Text {
                            visible: window.qualityProjectEstimate.basis !== undefined
                            Layout.fillWidth: true
                            text: (window.qualityProjectEstimate.basis || "") + " "
                                  + (window.qualityProjectEstimate.uncertainty || "")
                            color: secondaryText
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            model: projectController.qualityReport.episodes || []
                            delegate: Text {
                                required property var modelData
                                Layout.fillWidth: true
                                text: qsTr("%1：%2（范围 %3）")
                                      .arg(modelData.title)
                                      .arg(modelData.estimate.display)
                                      .arg(modelData.estimate.rangeDisplay)
                                color: secondaryText
                                font.pixelSize: 12
                            }
                        }

                        Repeater {
                            model: projectController.qualityReport.findings || []
                            delegate: Rectangle {
                                required property var modelData
                                objectName: "qualityFinding_" + modelData.id
                                Layout.fillWidth: true
                                implicitHeight: findingContent.implicitHeight + 18
                                radius: 7
                                color: raisedColor
                                border.color: borderColor

                                ColumnLayout {
                                    id: findingContent
                                    anchors.fill: parent
                                    anchors.margins: 9
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.location + " · " + modelData.title
                                            color: modelData.status === ""
                                                   ? accentColor : secondaryText
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                            wrapMode: Text.WordWrap
                                        }
                                        Text {
                                            text: modelData.status === "addressed"
                                                  ? qsTr("已处理")
                                                  : modelData.status === "ignored"
                                                    ? qsTr("已忽略") : qsTr("待复核")
                                            color: secondaryText
                                            font.pixelSize: 11
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.reason
                                        color: primaryText
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.suggestion
                                        color: secondaryText
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Item { Layout.fillWidth: true }
                                        Button {
                                            text: qsTr("标记已处理")
                                            enabled: modelData.status !== "addressed"
                                            onClicked: projectController.setQualityFindingStatus(
                                                           modelData.id, "addressed")
                                        }
                                        Button {
                                            text: qsTr("忽略")
                                            enabled: modelData.status !== "ignored"
                                            onClicked: projectController.setQualityFindingStatus(
                                                           modelData.id, "ignored")
                                        }
                                        Button {
                                            visible: modelData.status !== ""
                                            text: qsTr("恢复待复核")
                                            onClicked: projectController.setQualityFindingStatus(
                                                           modelData.id, "open")
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            visible: projectController.qualityReport.findings !== undefined
                                     && projectController.qualityReport.findings.length === 0
                            text: qsTr("当前检查规则没有发现提示。结果不代表剧本已经通过质量评审。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: storyImpactContent.implicitHeight + 36
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: storyImpactContent
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 8

                        Text {
                            text: qsTr("设定变化影响")
                            color: primaryText
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("剧本段落通过文字匹配定位；手工建立的镜头和文字参考通过稳定关联定位。命中只代表待复核线索；没有命中不代表没有影响。这里不包含图片文件导入、图片生成或完整连续性分析。")
                            color: secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            objectName: "storyImpactRepeater"
                            model: projectController.storyImpacts
                            delegate: Rectangle {
                                required property var modelData
                                objectName: "storyImpact_" + modelData.id
                                Layout.fillWidth: true
                                implicitHeight: impactContent.implicitHeight + 18
                                radius: 7
                                color: raisedColor
                                border.color: borderColor

                                ColumnLayout {
                                    id: impactContent
                                    anchors.fill: parent
                                    anchors.margins: 9
                                    spacing: 6

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.sourceItemName + " · " + modelData.changeSummary
                                        color: primaryText
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        wrapMode: Text.WordWrap
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.targetType === "script_block"
                                              ? qsTr("可能相关：%1%2").arg(modelData.episodeTitle)
                                                    .arg(modelData.sceneTitle.length > 0
                                                         ? " · " + modelData.sceneTitle : "")
                                              : modelData.targetType === "shot"
                                                ? qsTr("待复核镜头：%1 · %2").arg(modelData.sceneTitle)
                                                      .arg(modelData.shotTitle)
                                                : modelData.targetType === "visual_reference"
                                                  ? qsTr("待复核视觉参考：%1 · %2")
                                                        .arg(modelData.sceneTitle)
                                                        .arg(modelData.visualReferenceTitle)
                                              : modelData.targetType === "episode"
                                                ? qsTr("%1：已关联设定，但未定位到剧本段落，影响范围未能判定")
                                                      .arg(modelData.episodeTitle)
                                                : qsTr("其他未追踪资产或关联范围：当前未能判定")
                                        color: modelData.assessmentStatus === "located"
                                               ? accentColor : secondaryText
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }

                                    Text {
                                        visible: modelData.evidence.length > 0
                                        Layout.fillWidth: true
                                        text: qsTr("匹配文本：%1").arg(modelData.evidence)
                                        color: secondaryText
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }

                                    Text {
                                        visible: modelData.reviewStatus === "intentional"
                                        Layout.fillWidth: true
                                        text: qsTr("已记录为有意例外：%1").arg(modelData.reason)
                                        color: accentColor
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }

                                    Text {
                                        visible: modelData.reviewStatus === "reviewed"
                                        Layout.fillWidth: true
                                        text: qsTr("已人工复核，保留当前内容。")
                                        color: accentColor
                                        font.pixelSize: 12
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        TextField {
                                            id: impactReasonField
                                            objectName: "storyImpactReason_" + modelData.id
                                            visible: modelData.assessmentStatus === "located"
                                                     && modelData.reviewStatus !== "reviewed"
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("若这是有意的情节例外，请填写理由")
                                            text: modelData.reviewStatus === "intentional"
                                                  ? modelData.reason : ""
                                            selectByMouse: true
                                            enabled: modelData.reviewStatus !== "intentional"
                                        }
                                        Button {
                                            objectName: "storyImpactReviewButton_" + modelData.id
                                            visible: modelData.assessmentStatus === "located"
                                            enabled: modelData.reviewStatus === "intentional"
                                                     || impactReasonField.text.trim().length > 0
                                            text: modelData.reviewStatus === "intentional"
                                                  ? qsTr("撤销有意例外") : qsTr("记录有意例外")
                                            onClicked: projectController.setStoryImpactIntentional(
                                                           modelData.id,
                                                           modelData.reviewStatus !== "intentional",
                                                           impactReasonField.text)
                                        }
                                        Button {
                                            objectName: "storyImpactReviewedButton_" + modelData.id
                                            visible: modelData.assessmentStatus === "located"
                                                     && modelData.reviewStatus !== "intentional"
                                            text: modelData.reviewStatus === "reviewed"
                                                  ? qsTr("撤销复核") : qsTr("标记已复核")
                                            onClicked: projectController.setStoryImpactReviewed(
                                                           modelData.id,
                                                           modelData.reviewStatus !== "reviewed")
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            visible: projectController.storyImpacts.length === 0
                            text: qsTr("暂无已记录的设定变化影响。设定修改并保存后，结果会显示在这里。")
                            color: secondaryText
                            font.pixelSize: 12
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                    Layout.fillWidth: true
                    implicitHeight: projectHistoryContent.implicitHeight + 36
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: projectHistoryContent
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 8

                        Text {
                            text: qsTr("作品修改历史")
                            color: primaryText
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }

                        Repeater {
                            model: projectController.projectHistory
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.createdAt + " · " + modelData.summary
                                    color: secondaryText
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                                Button {
                                    text: qsTr("查看快照")
                                    onClicked: {
                                        historyDialog.snapshotText = modelData.snapshot
                                        historyDialog.open()
                                    }
                                }
                            }
                        }

                        Text {
                            visible: projectController.projectHistory.length === 0
                            text: qsTr("尚无保存历史")
                            color: secondaryText
                            font.pixelSize: 12
                        }
                    }
                }

                Rectangle {
                    visible: projectController.hasOpenProject
                             && projectController.episodes.length > 0
                    Layout.fillWidth: true
                    implicitHeight: episodeHistoryContent.implicitHeight + 36
                    radius: 10
                    color: panelColor
                    border.color: borderColor

                    ColumnLayout {
                        id: episodeHistoryContent
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 8

                        Text {
                            text: qsTr("当前内容单元修改历史 · ") + projectController.currentEpisodeTitle
                            color: primaryText
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }

                        Repeater {
                            model: projectController.episodeHistory
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.createdAt + " · " + modelData.summary
                                    color: secondaryText
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                                Button {
                                    text: qsTr("查看快照")
                                    onClicked: {
                                        historyDialog.snapshotText = modelData.snapshot
                                        historyDialog.open()
                                    }
                                }
                            }
                        }

                        Text {
                            visible: projectController.episodeHistory.length === 0
                            text: qsTr("尚无保存历史")
                            color: secondaryText
                            font.pixelSize: 12
                        }
                    }
                }

            }
        }
    }

    AiServiceSettingsDialog {
        id: aiServiceSettingsDialog
        settingsController: aiSettingsController
    }

    AiGenerationDialog {
        id: aiGenerationDialog
        generationController: aiGenerationController
        projectController: window.projectControllerReference
        settingsController: aiSettingsController
    }
}
