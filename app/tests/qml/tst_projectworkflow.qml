import QtQuick
import QtTest
import "../../qml" as SceneForgeApp

TestCase {
    name: "ProjectWorkflow"
    when: windowShown

    property var appWindow: null

    Component {
        id: applicationWindowComponent
        SceneForgeApp.Main {
            visible: true
            width: 1280
            height: 1800
        }
    }

    function init() {
        var runLibrary = projectController.libraryRoot + "/ui-" + Date.now()
                        + "-" + Math.floor(Math.random() * 1000000)
        verify(projectController.setLibraryRoot(runLibrary))
        appWindow = applicationWindowComponent.createObject(null)
        verify(appWindow !== null)
        appWindow.requestActivate()
        tryCompare(appWindow, "active", true)
        tryCompare(appWindow, "visible", true)
    }

    function cleanup() {
        if (projectController.hasOpenProject) {
            projectController.markSaved()
            verify(projectController.closeProject())
        }
        if (appWindow !== null) {
            appWindow.destroy()
            appWindow = null
        }
    }

    function findVisualChild(parentItem, targetName) {
        if (parentItem === null || parentItem === undefined)
            return null
        if (parentItem.objectName === targetName)
            return parentItem
        if (parentItem.children === undefined)
            return null
        for (var index = 0; index < parentItem.children.length; ++index) {
            var result = findVisualChild(parentItem.children[index], targetName)
            if (result !== null)
                return result
        }
        return null
    }

    function test_ideaKeyboardSaveCloseAndReopen() {
        var ideaField = findChild(appWindow, "initialIdeaField")
        var nameField = findChild(appWindow, "projectNameField")
        verify(ideaField !== null)
        verify(nameField !== null)

        ideaField.forceActiveFocus()
        keyClick("a")
        compare(ideaField.text, "a")
        ideaField.text = "暴雨夜，一位修表匠收到十年前寄出的信。"
        nameField.text = "迟到的信"
        nameField.accepted()

        tryCompare(projectController, "hasOpenProject", true)
        compare(projectController.currentProjectName, "迟到的信")
        compare(projectController.briefDescription, "暴雨夜，一位修表匠收到十年前寄出的信。")

        var projectPath = projectController.currentProjectPath
        var scriptEditor = findChild(appWindow, "scriptTextField")
        verify(scriptEditor !== null)
        scriptEditor.forceActiveFocus()
        scriptEditor.text = "修表匠打开信封，发现里面是昨天的报纸。"
        tryCompare(projectController, "scriptText",
                   "修表匠打开信封，发现里面是昨天的报纸。")
        tryCompare(projectController, "saveStatus", "saved", 4000)

        var originalProjectId = projectController.currentProjectId
        var originalProjectPath = projectController.currentProjectPath
        var nameEditor = findChild(appWindow, "currentProjectNameEditor")
        var renameButton = findChild(appWindow, "renameProjectButton")
        verify(nameEditor !== null)
        verify(renameButton !== null)
        nameEditor.text = "   "
        renameButton.clicked()
        compare(projectController.currentProjectName, "迟到的信")
        compare(nameEditor.text, "迟到的信")
        verify(projectController.errorMessage.length > 0)
        nameEditor.text = "改名后的迟到的信"
        renameButton.clicked()
        tryCompare(projectController, "currentProjectName", "改名后的迟到的信")
        compare(projectController.currentProjectId, originalProjectId)
        compare(projectController.currentProjectPath, originalProjectPath)

        var statusIndicator = findChild(appWindow, "saveStatusIndicator")
        compare(statusIndicator.text, "已保存")
        findChild(appWindow, "closeProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", false)

        var projectListPanel = findChild(appWindow, "projectListPanel")
        verify(projectListPanel !== null)
        tryVerify(function() { return projectListPanel.visible })
        var projectRepeater = findChild(appWindow, "projectListRepeater")
        tryVerify(function() {
            return projectRepeater !== null && projectRepeater.count > 0
        })
        var openButton = findVisualChild(projectRepeater.itemAt(0), "openProjectButton")
        verify(openButton !== null)
        openButton.clicked()
        tryCompare(projectController, "currentProjectPath", projectPath)
        compare(projectController.currentProjectName, "改名后的迟到的信")
        compare(projectController.currentProjectId, originalProjectId)
        compare(projectController.briefDescription, "暴雨夜，一位修表匠收到十年前寄出的信。")
        compare(projectController.scriptText, "修表匠打开信封，发现里面是昨天的报纸。")
        compare(findChild(appWindow, "scriptTextField").text,
                "修表匠打开信封，发现里面是昨天的报纸。")
    }

    function test_projectLifecycleControls() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)
        tryCompare(projectController, "saveStatus", "saved", 4000)
        var originalId = projectController.currentProjectId
        var originalPath = projectController.currentProjectPath
        findChild(appWindow, "closeProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", false)

        var repeater = findChild(appWindow, "projectListRepeater")
        tryVerify(function() { return repeater !== null && repeater.count === 1 })
        var archiveButton = findVisualChild(repeater.itemAt(0), "archiveProjectButton")
        verify(archiveButton !== null)
        archiveButton.clicked()
        tryVerify(function() { return projectController.projects[0].archived })

        var duplicateButton = findVisualChild(repeater.itemAt(0), "duplicateProjectButton")
        verify(duplicateButton !== null)
        duplicateButton.clicked()
        tryVerify(function() { return projectController.projects.length === 2 })
        verify(projectController.projects.some(function(project) {
            return project.id !== originalId && project.path !== originalPath
        }))

        var removeButton = findVisualChild(repeater.itemAt(0), "removeProjectButton")
        verify(removeButton !== null)
        removeButton.clicked()
        tryVerify(function() {
            return projectController.projectRemovalPreview.id.length > 0
        })
        projectController.cancelProjectRemoval()
        compare(projectController.projects.length, 2)
    }

    function test_blankProjectAllowsUndecidedFields() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()

        tryCompare(projectController, "hasOpenProject", true)
        compare(projectController.briefDescription, "")
        compare(projectController.briefGenre, "")
        compare(projectController.briefTargetDuration, "")
        verify(findChild(appWindow, "briefDescriptionField") !== null)
        compare(findChild(appWindow, "briefGenreField").placeholderText, "待决定")
        var undecidedHint = findChild(appWindow, "undecidedFieldsHint")
        verify(undecidedHint !== null)
        verify(undecidedHint.text.indexOf("空白字段表示“待决定”") >= 0)

        projectController.markSaveFailed("测试保存失败提示")
        tryCompare(projectController, "saveStatus", "failed")
        var errorMessage = findChild(appWindow, "errorMessageText")
        var retryButton = findChild(appWindow, "retrySaveButton")
        verify(errorMessage.visible)
        compare(errorMessage.text, "测试保存失败提示")
        verify(retryButton.visible)
        retryButton.clicked()
        tryCompare(projectController, "saveStatus", "saved", 4000)

        var blankProjectPath = projectController.currentProjectPath
        findChild(appWindow, "closeProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", false)
        verify(projectController.openProject(blankProjectPath))
        compare(projectController.briefDescription, "")
        compare(projectController.briefGenre, "")
        compare(projectController.briefTargetDuration, "")
        compare(projectController.currentProjectStory.storySynopsis, "")
        compare(findChild(appWindow, "briefGenreField").placeholderText, "待决定")
        verify(findChild(appWindow, "undecidedFieldsHint").text
               .indexOf("空白字段表示“待决定”") >= 0)
    }

    function test_bibleFieldsStatesReferencesAndHistory() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)

        var typeCombo = findChild(appWindow, "bibleTypeCombo")
        var nameField = findChild(appWindow, "bibleItemNameField")
        verify(typeCombo !== null)
        verify(nameField !== null)
        compare(typeCombo.count, 9)
        compare(appWindow.bibleCertaintyStatuses.length, 4)
        typeCombo.currentIndex = 3
        typeCombo.activated(3)
        compare(appWindow.newBibleItemType, "character")
        verify(projectController.createBibleItem("character", "林岚", "角色介绍"))
        tryVerify(function() { return projectController.bibleItems.length === 1 })

        compare(appWindow.bibleDetailFields("character").length, 10)
        compare(appWindow.bibleTypeLabel("character"), "角色")
        verify(projectController.updateBibleItemDetail(projectController.bibleItems[0].id,
                                                       "goal", "找回失踪的妹妹。"))
        tryVerify(function() {
            return projectController.bibleItems[0].details.goal === "找回失踪的妹妹。"
        })

        verify(projectController.addBibleItemReference(projectController.bibleItems[0].id,
                                                       "笔记/人物设定.md",
                                                       "人物动机来源"))
        tryVerify(function() { return projectController.bibleItems[0].references.length === 1 })
        verify(projectController.setBibleItemCertaintyStatus(projectController.bibleItems[0].id,
                                                             "confirmed"))
        tryVerify(function() {
            return projectController.bibleItems[0].certaintyStatus === "confirmed"
        })
        tryCompare(projectController, "saveStatus", "saved", 4000)
    }

    function test_storyImpactReviewAndIntentionalReason() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)
        verify(projectController.createBibleItem("character", "顾遥", "主角设定"))
        var characterId = projectController.bibleItems[0].id
        verify(projectController.setEpisodeBibleReference(characterId, true))
        verify(projectController.createScene())
        var sceneId = projectController.scenes[0].id
        var blockId = projectController.scriptBlocks[0].id
        verify(projectController.updateScriptBlock(blockId, "dialogue", "顾遥：我会回来。"))
        verify(projectController.setScriptBlockScene(blockId, sceneId))
        tryCompare(projectController, "saveStatus", "saved", 4000)

        verify(projectController.updateBibleItemDetail(characterId, "goal", "守住旧街。"))
        verify(projectController.setBibleItemCertaintyStatus(characterId, "confirmed"))
        tryVerify(function() {
            return projectController.storyImpacts.some(
                        function(impact) { return impact.targetType === "script_block" })
        })
        var impactIndex = projectController.storyImpacts.findIndex(
                    function(impact) { return impact.targetType === "script_block" })
        verify(impactIndex >= 0)
        var impactRepeater = findChild(appWindow, "storyImpactRepeater")
        verify(impactRepeater !== null)
        tryVerify(function() { return impactRepeater.itemAt(impactIndex) !== null })
        var impact = projectController.storyImpacts[impactIndex]
        var impactCard = impactRepeater.itemAt(impactIndex)
        var reasonField = findVisualChild(impactCard, "storyImpactReason_" + impact.id)
        var reviewButton = findVisualChild(impactCard,
                                           "storyImpactReviewButton_" + impact.id)
        var reviewedButton = findVisualChild(impactCard,
                                             "storyImpactReviewedButton_" + impact.id)
        verify(reasonField !== null)
        verify(reviewButton !== null)
        verify(reviewedButton !== null)

        reviewedButton.clicked()
        tryVerify(function() {
            return projectController.storyImpacts.some(function(entry) {
                return entry.id === impact.id && entry.reviewStatus === "reviewed"
            })
        })
        impactCard = impactRepeater.itemAt(impactIndex)
        reviewedButton = findVisualChild(impactCard,
                                         "storyImpactReviewedButton_" + impact.id)
        verify(reviewedButton !== null)
        reviewedButton.clicked()
        tryVerify(function() {
            return projectController.storyImpacts.some(function(entry) {
                return entry.id === impact.id && entry.reviewStatus === "open"
            })
        })

        impactCard = impactRepeater.itemAt(impactIndex)
        reasonField = findVisualChild(impactCard, "storyImpactReason_" + impact.id)
        reviewButton = findVisualChild(impactCard,
                                       "storyImpactReviewButton_" + impact.id)
        verify(reasonField !== null)
        verify(reviewButton !== null)
        verify(!reviewButton.enabled)
        reasonField.text = "这是有意安排的伏笔。"
        verify(reviewButton.enabled)
        reviewButton.clicked()
        tryVerify(function() {
            return projectController.storyImpacts.some(function(entry) {
                return entry.id === impact.id
                       && entry.reviewStatus === "intentional"
                       && entry.reason === "这是有意安排的伏笔。"
            })
        })
    }

    function test_storyPlanningAndStructuredScriptEditing() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)

        verify(appWindow.projectStoryFieldsView.count === 7)
        var synopsisField = findVisualChild(appWindow.projectStoryFieldsView.itemAt(1),
                                            "projectStoryField_storySynopsis")
        verify(synopsisField !== null)
        synopsisField.forceActiveFocus()
        synopsisField.text = "一封迟到的信让两位旧友重新相遇。"
        tryVerify(function() {
            return projectController.currentProjectStory.storySynopsis
                   === "一封迟到的信让两位旧友重新相遇。"
        })

        findChild(appWindow, "addSceneButton").clicked()
        tryVerify(function() { return projectController.scenes.length === 1 })
        appWindow.newScriptBlockType = "dialogue"
        appWindow.newScriptSceneId = projectController.scenes[0].id
        findChild(appWindow, "addScriptBlockButton").clicked()
        tryVerify(function() { return projectController.scriptBlocks.length === 2 })

        var dialogue = projectController.scriptBlocks[1]
        compare(dialogue.blockType, "dialogue")
        compare(dialogue.sceneId, projectController.scenes[0].id)
        var dialogueEditor = findVisualChild(appWindow.scriptBlocksView.itemAt(1),
                                             "scriptBlockEditor_" + dialogue.id)
        verify(dialogueEditor !== null)
        dialogueEditor.forceActiveFocus()
        dialogueEditor.text = "你终于回来了。"
        tryVerify(function() {
            return projectController.scriptBlocks[1].text === "你终于回来了。"
        })
        tryCompare(projectController, "saveStatus", "saved", 4000)
    }

    function test_translationVersionIsIndependentAndComparable() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)
        verify(projectController.updateDraftField("scriptText", "原稿对白：你好。"))

        var languageField = findChild(appWindow, "scriptVersionLanguageField")
        verify(languageField !== null)
        languageField.text = "English"
        findChild(appWindow, "createScriptVersionButton").clicked()
        tryVerify(function() { return projectController.scriptVersions.length === 2 })

        var version = projectController.scriptVersions[1]
        compare(version.language, "English")
        var versionEditor = findVisualChild(appWindow.scriptVersionsView.itemAt(1),
                                            "scriptVersionBlock_" + version.blocks[0].id)
        verify(versionEditor !== null)
        versionEditor.forceActiveFocus()
        versionEditor.text = "Hello."
        tryVerify(function() {
            return projectController.scriptVersions[1].blocks[0].text === "Hello."
        })
        compare(projectController.scriptText, "原稿对白：你好。")

        findVisualChild(appWindow.scriptVersionsView.itemAt(1),
                        "compareScriptVersionButton_" + version.id).clicked()
        tryVerify(function() { return findChild(appWindow, "versionComparisonDialog").visible })
        verify(appWindow.scriptVersionComparison.indexOf("- 原稿对白：你好。") >= 0)
        verify(appWindow.scriptVersionComparison.indexOf("+ Hello.") >= 0)
        findChild(appWindow, "versionComparisonDialog").close()
        findVisualChild(appWindow.scriptVersionsView.itemAt(1),
                        "restoreScriptVersionButton_" + version.id).clicked()
        tryVerify(function() { return findChild(appWindow, "restoreScriptVersionDialog").visible })
        verify(appWindow.scriptVersionComparison.indexOf("+ Hello.") >= 0)
        compare(projectController.scriptText, "原稿对白：你好。")
        tryCompare(projectController, "saveStatus", "saved", 4000)
    }

    function test_qualityCheckIsReviewOnlyAndSceneTargetsAreEditable() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)
        findChild(appWindow, "addSceneButton").clicked()
        tryVerify(function() { return projectController.scenes.length === 1 })

        var scene = projectController.scenes[0]
        tryVerify(function() {
            var sceneCard = appWindow.scenesView.itemAt(0)
            return sceneCard !== null
                   && findVisualChild(sceneCard, "sceneGoal_" + scene.id) !== null
                   && findVisualChild(sceneCard, "sceneConflictOrTurn_" + scene.id) !== null
                   && findVisualChild(sceneCard, "sceneTargetDuration_" + scene.id) !== null
        })
        var sceneCard = appWindow.scenesView.itemAt(0)
        var goalField = findVisualChild(sceneCard, "sceneGoal_" + scene.id)
        var conflictField = findVisualChild(sceneCard, "sceneConflictOrTurn_" + scene.id)
        var durationField = findVisualChild(sceneCard, "sceneTargetDuration_" + scene.id)
        verify(goalField !== null)
        verify(conflictField !== null)
        verify(durationField !== null)
        verify(projectController.updateSceneQualityFields(scene.id,
                                                         "拿到遗失的信件",
                                                         "",
                                                         "30"))
        tryVerify(function() {
            return projectController.scenes[0].goal === "拿到遗失的信件"
                   && projectController.scenes[0].targetDurationSeconds === 30
        })

        var originalScript = projectController.scriptText
        findChild(appWindow, "runQualityCheckButton").clicked()
        tryVerify(function() {
            return projectController.qualityReport.findings !== undefined
                   && projectController.qualityReport.findings.some(
                       function(finding) {
                           return finding.code === "scene_conflict_or_turn_missing"
                       })
        })
        compare(projectController.scriptText, originalScript)
        verify(projectController.qualityReport.findings[0].location.length > 0)
        tryCompare(projectController, "saveStatus", "saved", 4000)
    }

    function test_manualShotsAndTextReferencesCanBeEdited() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)
        findChild(appWindow, "addSceneButton").clicked()
        tryVerify(function() { return projectController.scenes.length === 1 })
        var sceneId = projectController.scenes[0].id
        verify(projectController.createBibleItem("character", "林岚", "主角"))
        var itemId = projectController.bibleItems[0].id
        appWindow.newScriptBlockType = "action"
        appWindow.newScriptSceneId = sceneId
        findChild(appWindow, "addScriptBlockButton").clicked()
        tryVerify(function() { return projectController.scriptBlocks.length === 2 })
        var blockId = projectController.scriptBlocks[1].id

        findChild(appWindow, "newShotSceneCombo").currentIndex = 0
        findChild(appWindow, "createShotButton").clicked()
        tryVerify(function() { return projectController.shots.length === 1 })
        var shotId = projectController.shots[0].id
        var shotTitle = findVisualChild(appWindow.shotsView.itemAt(0), "shotTitle_" + shotId)
        verify(shotTitle !== null)
        shotTitle.forceActiveFocus()
        shotTitle.text = "车站入口镜头"
        var scriptLink = findVisualChild(appWindow.shotsView.itemAt(0),
                                         "shotScriptBlockLink_" + shotId + "_" + blockId)
        var bibleLink = findVisualChild(appWindow.shotsView.itemAt(0),
                                        "shotBibleItemLink_" + shotId + "_" + itemId)
        verify(scriptLink !== null)
        verify(bibleLink !== null)
        scriptLink.checked = true
        scriptLink.toggled()
        bibleLink = findVisualChild(appWindow.shotsView.itemAt(0),
                                    "shotBibleItemLink_" + shotId + "_" + itemId)
        bibleLink.checked = true
        bibleLink.toggled()
        tryVerify(function() {
            return projectController.shots[0].title === "车站入口镜头"
        })
        tryVerify(function() {
            return projectController.shots[0].scriptBlockIds.includes(blockId)
        })
        tryVerify(function() {
            return projectController.shots[0].bibleItemIds.includes(itemId)
        })

        findChild(appWindow, "newReferenceSceneCombo").currentIndex = 0
        findChild(appWindow, "createVisualReferenceButton").clicked()
        tryVerify(function() { return projectController.visualReferences.length === 1 })
        var referenceId = projectController.visualReferences[0].id
        var referenceCard = appWindow.visualReferencesView.itemAt(0)
        var referenceTitle = findVisualChild(referenceCard,
                                             "visualReferenceTitle_" + referenceId)
        var shotLink = findVisualChild(referenceCard,
                                       "visualReferenceShotLink_" + referenceId + "_" + shotId)
        var referenceBibleLink = findVisualChild(
                    referenceCard, "visualReferenceBibleItemLink_" + referenceId + "_" + itemId)
        verify(referenceTitle !== null)
        verify(shotLink !== null)
        verify(referenceBibleLink !== null)
        referenceTitle.forceActiveFocus()
        referenceTitle.text = "林岚外观文字基准"
        shotLink = findVisualChild(appWindow.visualReferencesView.itemAt(0),
                                   "visualReferenceShotLink_" + referenceId + "_" + shotId)
        shotLink.checked = true
        shotLink.toggled()
        referenceBibleLink = findVisualChild(
                    appWindow.visualReferencesView.itemAt(0),
                    "visualReferenceBibleItemLink_" + referenceId + "_" + itemId)
        referenceBibleLink.checked = true
        referenceBibleLink.toggled()
        tryVerify(function() {
            return projectController.visualReferences[0].title === "林岚外观文字基准"
                   && projectController.visualReferences[0].shotIds.includes(shotId)
                   && projectController.visualReferences[0].bibleItemIds.includes(itemId)
        })
        compare(findVisualChild(referenceCard,
                                "visualReferenceDescription_" + referenceId).placeholderText,
                "记录希望保持的角色外观、色彩、光线或构图特点")
    }

    function test_aiServicesUseSeparateTextAndImageDefaults() {
        var openButton = findChild(appWindow, "openAiServiceSettingsButton")
        verify(openButton !== null)
        openButton.clicked()

        var dialog = findChild(appWindow, "aiServiceSettingsDialog")
        verify(dialog !== null)
        tryVerify(function() { return dialog.visible })

        var newButton = findChild(dialog, "newAiServiceProfileButton")
        var nameField = findChild(dialog, "aiServiceNameField")
        var capabilityBox = findChild(dialog, "aiServiceCapabilityBox")
        var endpointField = findChild(dialog, "aiServiceEndpointField")
        var modelField = findChild(dialog, "aiServiceModelField")
        var saveButton = findChild(dialog, "saveAiServiceProfileButton")
        var defaultButton = findChild(dialog, "setDefaultAiServiceButton")
        verify(newButton !== null && nameField !== null && capabilityBox !== null)
        verify(endpointField !== null && modelField !== null)
        verify(saveButton !== null && defaultButton !== null)

        nameField.text = "测试本地文本 " + Date.now()
        capabilityBox.currentIndex = 0
        endpointField.text = "http://127.0.0.1:11434/v1"
        modelField.text = "test-story-model"
        saveButton.clicked()
        var textServiceId = aiSettingsController.lastProfileId
        verify(textServiceId.length > 0)
        compare(aiSettingsController.profiles.find(function(profile) {
            return profile.id === textServiceId
        }).networkScope, "local")
        defaultButton.clicked()
        compare(aiSettingsController.defaultTextServiceId, textServiceId)

        newButton.clicked()
        nameField.text = "测试外部图片 " + Date.now()
        capabilityBox.currentIndex = 1
        endpointField.text = "https://images.example/v1"
        modelField.text = "test-image-model"
        saveButton.clicked()
        var imageServiceId = aiSettingsController.lastProfileId
        verify(imageServiceId.length > 0 && imageServiceId !== textServiceId)
        compare(aiSettingsController.profiles.find(function(profile) {
            return profile.id === imageServiceId
        }).networkScope, "external")
        defaultButton.clicked()
        compare(aiSettingsController.defaultImageServiceId, imageServiceId)
        verify(aiSettingsController.authorizeExternalService(imageServiceId))
        tryVerify(function() { return dialog.selectedProfile.authorized })
        var revokeButton = findChild(dialog, "revokeAiServiceAuthorizationButton")
        verify(revokeButton !== null && revokeButton.visible)
        revokeButton.clicked()
        tryVerify(function() { return !dialog.selectedProfile.authorized })

        verify(aiSettingsController.removeProfile(textServiceId))
        verify(aiSettingsController.removeProfile(imageServiceId))
        dialog.close()
    }

    function test_aiGenerationDialogPreviewsBeforeSending() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)

        var openButton = findChild(appWindow, "openAiGenerationButton")
        verify(openButton !== null)
        openButton.clicked()
        var dialog = findChild(appWindow, "aiGenerationDialog")
        tryVerify(function() { return dialog !== null && dialog.visible })
        var operationBox = findChild(dialog, "aiGenerationOperation")
        verify(operationBox !== null)
        compare(operationBox.model[1].value, "bible_suggest")
        compare(operationBox.model[2].value, "bible_update_suggest")
        compare(operationBox.model[6].value, "script_compress")
        compare(operationBox.model[9].value, "script_style_adjust")
        verify(findChild(dialog, "aiGenerationTarget") !== null)
        verify(findChild(dialog, "prepareAiGenerationButton") !== null)
        verify(findChild(dialog, "aiGenerationAdditionalContext") !== null)
        verify(findChild(dialog, "aiGenerationTaskList") !== null)
        var includeBrief = findChild(dialog, "aiIncludeBriefContext")
        var includeBible = findChild(dialog, "aiIncludeBibleContext")
        verify(includeBrief !== null)
        verify(includeBible !== null)
        operationBox.currentIndex = 10
        operationBox.activated(10)
        compare(dialog.selectedOperation, "script_analysis")
        verify(includeBrief.checked)
        verify(includeBible.checked)
        compare(projectController.aiGenerationTasks.length, 0)
        dialog.selectedOperation = "script_compress"
        verify(projectController.createScene())
        var rewriteTargets = dialog.targetOptions()
        verify(rewriteTargets.length > 0)
        verify(rewriteTargets.some(function(target) { return target.type === "project" }))
        verify(rewriteTargets.some(function(target) { return target.type === "episode" }))
        verify(rewriteTargets.some(function(target) { return target.type === "scene" }))
        verify(rewriteTargets.some(function(target) { return target.type === "script_block" }))
        dialog.selectedOperation = "script_analysis"
        verify(dialog.targetOptions().some(function(target) { return target.type === "project" }))
        compare(operationBox.model[4].value, "script_continue")
        dialog.selectedOperation = "script_continue"
        var continuationTargets = dialog.targetOptions()
        verify(continuationTargets.some(function(target) {
            return target.type === "project"
                   && target.label.indexOf("全剧末尾续写") >= 0
        }))
        dialog.selectedOperation = "bible_suggest"
        compare(dialog.targetOptions()[0].type, "project")
        compare(dialog.bibleDetailFields("character").length, 10)
        dialog.loadBibleCandidateReview({
            operation: "bible_suggest",
            status: "succeeded",
            candidateText: JSON.stringify({items: [
                {selected: true, type: "character", name: "候选角色",
                 description: "待审阅", details: {goal: "推动故事。"}}
            ]})
        })
        compare(dialog.bibleCandidateItems.length, 1)
        compare(dialog.bibleCandidateItems[0].name, "候选角色")
        compare(JSON.parse(dialog.bibleCandidatesJson()).items.length, 1)
        dialog.loadStoryCandidateReview({
            operation: "story_development",
            status: "succeeded",
            candidateText: JSON.stringify({
                theme: "创作需要勇气。",
                centralConflict: "午夜前找回剧本。"
            })
        })
        compare(dialog.storyCandidateFields.length, 2)
        dialog.updateStoryCandidate(0, "value", "创作和解需要勇气。")
        dialog.updateStoryCandidate(1, "selected", false)
        var reviewedStory = JSON.parse(dialog.storyCandidatesJson())
        compare(reviewedStory.values.theme, "创作和解需要勇气。")
        compare(reviewedStory.selected.length, 1)
        compare(reviewedStory.selected[0], "theme")

        dialog.selectedOperation = "bible_update_suggest"
        compare(dialog.targetOptions().length, 0)
        var updateTask = {
            id: "update-preview",
            targetId: "bible-item-1",
            operation: "bible_update_suggest",
            status: "succeeded",
            contextManifestJson: JSON.stringify({targetBibleItemSnapshot: {
                id: "bible-item-1", type: "character", name: "旧名",
                description: "旧说明", details: {goal: "旧目标"}
            }}),
            candidateText: JSON.stringify({items: [{
                name: "新名", description: "新说明", details: {goal: "新目标"}
            }]})
        }
        dialog.loadBibleUpdateCandidateReview(updateTask)
        compare(dialog.bibleUpdateCandidateItems.length, 1)
        compare(dialog.bibleUpdateCandidateItems[0].fields.length, 3)
        dialog.updateBibleUpdateField(0, 2, "selected", false)
        dialog.updateBibleUpdateField(0, 1, "value", "已编辑说明")
        var updateJson = JSON.parse(dialog.bibleUpdateCandidatesJson())
        compare(updateJson.items[0].targetId, "bible-item-1")
        compare(updateJson.items[0].description, "已编辑说明")
        compare(updateJson.items[0].selectedFields.length, 2)
        verify(updateJson.items[0].selectedFields.indexOf("details.goal") < 0)

        var firstProposal = JSON.stringify({items: [{
            description: "第一方案说明", details: {goal: "第一方案目标。"}
        }]})
        var secondProposal = JSON.stringify({items: [{
            description: "第二方案说明", details: {goal: "第二方案目标。"}
        }]})
        var alternativesTask = {
            id: "update-alternatives",
            targetId: "bible-item-1",
            operation: "bible_update_suggest",
            status: "succeeded",
            contextManifestJson: JSON.stringify({
                candidateCount: 3,
                targetBibleItemSnapshot: {
                    id: "bible-item-1", type: "character", name: "旧名",
                    description: "旧说明", details: {goal: "旧目标"}
                }
            }),
            candidateText: JSON.stringify({
                format: "sceneforge.bible-update-candidates.v1",
                selectedIndex: 0,
                candidates: [firstProposal, secondProposal]
            })
        }
        dialog.loadBibleUpdateCandidateReview(alternativesTask)
        verify(dialog.bibleUpdateAlternativesActive)
        compare(dialog.bibleUpdateCandidateItems.length, 2)
        compare(dialog.bibleUpdateCandidateItems[0].fields[0].value, "第一方案说明")
        compare(dialog.bibleUpdateCandidateItems[1].fields[0].value, "第二方案说明")
        dialog.updateBibleUpdateCandidate(1, "selected", true)
        verify(!dialog.bibleUpdateCandidateItems[0].selected)
        verify(dialog.bibleUpdateCandidateItems[1].selected)
        var savedAlternatives = JSON.parse(dialog.bibleUpdateReviewStateJson())
        compare(savedAlternatives.selectedIndex, 1)
        compare(savedAlternatives.candidates.length, 2)
        dialog.loadBibleUpdateCandidateReview({
            id: "update-alternatives-reopened",
            targetId: "bible-item-1",
            operation: "bible_update_suggest",
            status: "succeeded",
            contextManifestJson: alternativesTask.contextManifestJson,
            candidateText: JSON.stringify(savedAlternatives)
        })
        verify(dialog.bibleUpdateAlternativesActive)
        verify(!dialog.bibleUpdateCandidateItems[0].selected)
        verify(dialog.bibleUpdateCandidateItems[1].selected)
        var selectedAlternative = JSON.parse(dialog.bibleUpdateCandidatesJson())
        compare(selectedAlternative.items.length, 1)
        compare(selectedAlternative.items[0].description, "第二方案说明")
        compare(selectedAlternative.items[0].details.goal, "第二方案目标。")
        dialog.close()
    }

    function test_aiBibleUpdateCandidatesRequireFieldSelectionAndConfirmation() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)
        verify(projectController.createBibleItem("character", "沈遥", "寻找失踪的姐姐。"))
        var original = projectController.bibleItems[0]
        var itemId = original.id
        verify(projectController.updateBibleItemDetail(itemId, "goal", "找到姐姐。"))
        verify(projectController.setBibleItemCertaintyStatus(itemId, "confirmed"))
        verify(projectController.addBibleItemReference(itemId, "创作笔记", "人物原型来源"))

        findChild(appWindow, "openAiGenerationButton").clicked()
        var dialog = findChild(appWindow, "aiGenerationDialog")
        tryVerify(function() { return dialog !== null && dialog.visible })
        dialog.selectedOperation = "bible_update_suggest"
        var options = dialog.targetOptions()
        compare(options.length, 1)
        compare(options[0].type, "bible_item")
        compare(options[0].id, itemId)

        var context = projectController.aiContextSnapshot("bible_item", itemId, false, true, false)
        verify(context.valid)
        var taskId = projectController.prepareAiTask({
            operation: "bible_update_suggest",
            targetType: "bible_item",
            targetId: itemId,
            targetTitle: "沈遥",
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: context.contextManifestJson,
            systemInstruction: "只返回设定修改候选。",
            requestText: "建议让角色转而寻找真相。"
        })
        verify(taskId.length > 0)
        verify(projectController.setAiTaskRunning(taskId))
        verify(projectController.completeAiTask(taskId, JSON.stringify({items: [{
            name: "沈遥",
            description: "寻找姐姐失踪背后的真相。",
            details: {goal: "查明姐姐失踪的真相。"}
        }]}), {}))
        dialog.showTask(taskId)

        var candidateList = findChild(dialog, "aiBibleUpdateCandidateList")
        tryCompare(candidateList, "count", 1)
        verify(candidateList.visible)
        compare(projectController.bibleItems[0].description, "寻找失踪的姐姐。")
        compare(projectController.bibleItems[0].details.goal, "找到姐姐。")
        dialog.updateBibleUpdateField(0, 1, "selected", false)
        dialog.updateBibleUpdateField(0, 2, "value", "修改后的目标。")
        findChild(dialog, "acceptAiBibleCandidatesButton").clicked()

        compare(projectController.bibleItems[0].id, itemId)
        compare(projectController.bibleItems[0].name, "沈遥")
        compare(projectController.bibleItems[0].description, "寻找失踪的姐姐。")
        compare(projectController.bibleItems[0].details.goal, "修改后的目标。")
        compare(projectController.bibleItems[0].certaintyStatus, "confirmed")
        compare(projectController.bibleItems[0].references.length, 1)
        compare(projectController.aiTask(taskId).status, "accepted")
        dialog.close()
    }

    function test_aiBibleCandidateReviewPanel() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)
        findChild(appWindow, "openAiGenerationButton").clicked()
        var dialog = findChild(appWindow, "aiGenerationDialog")
        tryVerify(function() { return dialog !== null && dialog.visible })

        var projectId = projectController.currentProjectId
        var context = projectController.aiContextSnapshot(
                          "project", projectId, true, true, false)
        verify(context.valid)
        var taskId = projectController.prepareAiTask({
            operation: "bible_suggest",
            targetType: "project",
            targetId: projectId,
            targetTitle: projectController.currentProjectName,
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: context.contextManifestJson,
            systemInstruction: "测试候选",
            requestText: "提出未确认的角色和地点设定。"
        })
        verify(taskId.length > 0)
        verify(projectController.setAiTaskRunning(taskId))
        verify(projectController.completeAiTask(taskId, JSON.stringify({items: [
            {selected: true, type: "character", name: "林岚",
             description: "找回故事原稿。", certaintyStatus: "provisional",
             details: {goal: "找回原稿。"}},
            {selected: false, type: "location", name: "旧编辑室",
             description: "夜间工作的地方。", certaintyStatus: "undecided",
             details: {function: "发生关键对话。"}}
        ]}), {}))
        dialog.showTask(taskId)

        var candidateList = findChild(dialog, "aiBibleCandidateList")
        tryCompare(candidateList, "count", 2)
        verify(candidateList.visible)
        verify(findChild(candidateList, "aiBibleCandidateSelection") !== null)
        dialog.updateBibleCandidate(0, "name", "林岚（已审阅）")
        dialog.updateBibleCandidateDetail(0, "goal", "找回并修复原稿。")
        var acceptButton = findChild(dialog, "acceptAiBibleCandidatesButton")
        verify(acceptButton !== null && acceptButton.visible)
        acceptButton.clicked()
        compare(projectController.bibleItems.length, 1)
        compare(projectController.bibleItems[0].name, "林岚（已审阅）")
        compare(projectController.bibleItems[0].certaintyStatus, "provisional")
        compare(projectController.bibleItems[0].details.goal, "找回并修复原稿。")
        compare(projectController.aiTask(taskId).status, "accepted")
        dialog.close()
    }

    function test_aiScriptCandidatesCanBeComparedAndSelected() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)
        findChild(appWindow, "openAiGenerationButton").clicked()
        var dialog = findChild(appWindow, "aiGenerationDialog")
        tryVerify(function() { return dialog !== null && dialog.visible })

        var episodeId = projectController.currentEpisodeId
        var context = projectController.aiContextSnapshot(
                          "episode", episodeId, false, false, false)
        verify(context.valid)
        var manifest = JSON.parse(context.contextManifestJson)
        manifest.candidateCount = 3
        var candidates = [
            "第一版：修表匠发现一封迟到的信。",
            "第二版：修表匠从停摆的钟里找到线索。",
            "第三版：修表匠发现寄信人仍在门外。"
        ]
        var taskId = projectController.prepareAiTask({
            operation: "script_generate",
            targetType: "episode",
            targetId: episodeId,
            targetTitle: projectController.currentEpisodeTitle,
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: JSON.stringify(manifest),
            systemInstruction: "生成剧本候选",
            requestText: "生成三个版本。"
        })
        verify(taskId.length > 0)
        verify(projectController.setAiTaskRunning(taskId))
        verify(projectController.completeAiTask(taskId, JSON.stringify({
            format: "sceneforge.script-candidates.v1",
            selectedIndex: 0,
            items: candidates
        }), {}))

        dialog.showTask(taskId)
        var selector = findChild(dialog, "aiScriptCandidateSelector")
        verify(selector !== null && selector.visible)
        compare(selector.count, 3)
        var editor = findChild(dialog, "aiCandidateEditor")
        compare(editor.text, candidates[0])
        selector.currentIndex = 1
        selector.activated(1)
        compare(editor.text, candidates[1])
        editor.text = "第二版（已编辑）：修表匠从停摆的钟里找到线索。"
        editor.textEdited(editor.text)
        findChild(dialog, "saveAiCandidateAsVersionButton").clicked()
        compare(projectController.scriptText, "")
        compare(projectController.scriptVersions.length, 1)
        compare(projectController.scriptVersions[0].contentStatus, "candidate")
        compare(projectController.scriptVersions[0].text, editor.text)
        var saveAllCandidatesButton = findChild(
                    dialog, "saveAllAiCandidatesAsVersionsButton")
        verify(saveAllCandidatesButton !== null && saveAllCandidatesButton.visible)
        saveAllCandidatesButton.clicked()
        compare(projectController.scriptVersions.length, 3)
        compare(projectController.scriptText, "")
        findChild(dialog, "acceptAiBibleCandidatesButton").clicked()
        compare(projectController.scriptText,
                "第二版（已编辑）：修表匠从停摆的钟里找到线索。")
        compare(projectController.aiTask(taskId).status, "accepted")
        var saved = JSON.parse(projectController.aiTask(taskId).candidateText)
        compare(saved.selectedIndex, 1)
        compare(saved.items.length, 3)
        compare(saved.items[0], candidates[0])
        compare(saved.items[1], "第二版（已编辑）：修表匠从停摆的钟里找到线索。")

        var blockId = projectController.scriptBlocks[0].id
        verify(projectController.updateScriptBlock(
                   blockId, "dialogue", "第二版（已编辑）：修表匠从停摆的钟里找到线索。"))
        var rewriteContext = projectController.aiContextSnapshot(
                                 "script_block", blockId, false, false, true)
        verify(rewriteContext.valid)
        var rewriteManifest = JSON.parse(rewriteContext.contextManifestJson)
        rewriteManifest.candidateCount = 3
        var rewriteTaskId = projectController.prepareAiTask({
            operation: "script_rewrite",
            targetType: "script_block",
            targetId: blockId,
            targetTitle: "对白段落",
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: JSON.stringify(rewriteManifest),
            systemInstruction: "改写选中对白并返回多个版本",
            requestText: "生成三个改写版本。"
        })
        verify(rewriteTaskId.length > 0)
        verify(projectController.setAiTaskRunning(rewriteTaskId))
        var rewrites = [
            "改写一：钟声从墙后传来。\n\n门外有人敲门。",
            "改写二：停摆的钟突然敲响。\n\n修表匠点亮台灯。",
            "改写三：钟面映出寄信人的名字。\n\n门外传来脚步声。"
        ]
        verify(projectController.completeAiTask(rewriteTaskId, JSON.stringify({
            format: "sceneforge.script-candidates.v1",
            selectedIndex: 0,
            items: rewrites
        }), {}))
        dialog.showTask(rewriteTaskId)
        verify(selector.visible)
        compare(selector.count, 3)
        var partialToggle = findChild(dialog, "aiScriptPartialSelectionToggle")
        verify(partialToggle !== null && partialToggle.enabled)
        dialog.setScriptPartialSelection(true)
        tryCompare(partialToggle, "checked", true)
        var comparisonList = findChild(dialog, "aiScriptComparisonList")
        tryCompare(comparisonList, "count", 2)
        verify(comparisonList.visible)
        compare(dialog.scriptComparisonParagraphs[0].choices.length, 3)
        compare(dialog.scriptComparisonParagraphs[1].choices.length, 3)
        dialog.selectScriptParagraphCandidate(0, 2)
        dialog.selectScriptParagraphCandidate(1, 1)
        findChild(dialog, "acceptAiBibleCandidatesButton").clicked()
        var mergedText = "改写三：钟面映出寄信人的名字。\n\n修表匠点亮台灯。"
        compare(projectController.scriptText, mergedText)
        compare(projectController.scriptVersions.length, 4)
        compare(projectController.scriptVersions[3].versionType, "user_edit")
        compare(projectController.scriptVersions[3].text,
                "第二版（已编辑）：修表匠从停摆的钟里找到线索。")
        var mergedTask = JSON.parse(projectController.aiTask(rewriteTaskId).candidateText)
        compare(mergedTask.acceptedText, mergedText)
        var projectPath = projectController.currentProjectPath
        dialog.close()
        verify(projectController.closeProject())
        verify(projectController.openProject(projectPath))
        compare(projectController.scriptText, mergedText)
        compare(JSON.parse(projectController.aiTask(rewriteTaskId).candidateText).acceptedText,
                mergedText)
    }

    function test_aiProjectScriptCandidatesAreEditedAndReviewedPerEpisode() {
        appWindow.newProjectType = "series"
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)
        findChild(appWindow, "openAiGenerationButton").clicked()
        var dialog = findChild(appWindow, "aiGenerationDialog")
        tryVerify(function() { return dialog !== null && dialog.visible })

        var firstId = projectController.currentEpisodeId
        var firstOriginalId = projectController.scriptBlocks[0].id
        verify(projectController.updateScriptBlock(firstOriginalId, "action", "第一集正式内容。"))
        verify(projectController.createEpisode("第二集"))
        var secondId = projectController.currentEpisodeId
        var secondOriginalId = projectController.scriptBlocks[0].id
        verify(projectController.updateScriptBlock(secondOriginalId, "action", "第二集正式内容。"))

        var episodes = projectController.episodes
        var candidates = []
        for (var candidateIndex = 0; candidateIndex < 3; ++candidateIndex) {
            var candidateEpisodes = []
            for (var episodeIndex = 0; episodeIndex < episodes.length; ++episodeIndex) {
                candidateEpisodes.push({
                    episodeId: episodes[episodeIndex].id,
                    title: episodes[episodeIndex].title,
                    text: "候选" + (candidateIndex + 1) + " / "
                          + episodes[episodeIndex].title + "剧本。"
                })
            }
            candidates.push(JSON.stringify({
                format: "sceneforge.project-script.v1",
                episodes: candidateEpisodes
            }))
        }
        var context = projectController.aiContextSnapshot("project",
                                                          projectController.currentProjectId,
                                                          false, false, false)
        verify(context.valid)
        var manifest = JSON.parse(context.contextManifestJson)
        manifest.candidateCount = 3
        manifest.targetEpisodes = episodes.map(function(episode) {
            return {episodeId: episode.id, title: episode.title}
        })
        var taskId = projectController.prepareAiTask({
            operation: "script_generate",
            targetType: "project",
            targetId: projectController.currentProjectId,
            targetTitle: "全剧剧本",
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: JSON.stringify(manifest),
            systemInstruction: "返回各集结构化剧本候选",
            requestText: "生成三个全剧剧本候选。"
        })
        verify(taskId.length > 0)
        verify(projectController.setAiTaskRunning(taskId))
        verify(projectController.completeAiTask(taskId, JSON.stringify({
            format: "sceneforge.script-candidates.v1",
            selectedIndex: 0,
            items: candidates
        }), {}))

        dialog.showTask(taskId)
        var selector = findChild(dialog, "aiScriptCandidateSelector")
        verify(selector !== null && selector.visible)
        compare(selector.count, 3)
        var list = findChild(dialog, "aiProjectScriptEpisodeList")
        verify(list !== null && list.visible)
        compare(list.count, 2)
        var firstDelegate = list.itemAtIndex(0)
        var secondDelegate = list.itemAtIndex(1)
        tryVerify(function() {
            firstDelegate = list.itemAtIndex(0)
            secondDelegate = list.itemAtIndex(1)
            return firstDelegate !== null && secondDelegate !== null
        })
        var firstEditor = findVisualChild(firstDelegate, "aiProjectScriptEpisodeEditor_0")
        var secondEditor = findVisualChild(secondDelegate, "aiProjectScriptEpisodeEditor_1")
        tryVerify(function() {
            firstEditor = findVisualChild(firstDelegate, "aiProjectScriptEpisodeEditor_0")
            secondEditor = findVisualChild(secondDelegate, "aiProjectScriptEpisodeEditor_1")
            return firstEditor !== null && secondEditor !== null
        })
        verify(firstEditor !== null && secondEditor !== null)
        compare(firstEditor.text, "候选1 / 第 1 集剧本。")
        compare(secondEditor.text, "候选1 / 第二集剧本。")
        firstEditor.text = "候选1编辑 / 第 1 集剧本。"
        dialog.updateProjectScriptEpisode(0, firstEditor.text)
        selector.currentIndex = 1
        dialog.selectedScriptCandidateIndex = 1
        verify(dialog.loadProjectScriptCandidate(
                   dialog.scriptCandidateItems[1].text))
        compare(dialog.projectScriptEpisodes[0].text, "候选2 / 第 1 集剧本。")
        firstDelegate = list.itemAtIndex(0)
        secondDelegate = list.itemAtIndex(1)
        firstEditor = findVisualChild(firstDelegate, "aiProjectScriptEpisodeEditor_0")
        secondEditor = findVisualChild(secondDelegate, "aiProjectScriptEpisodeEditor_1")
        tryVerify(function() {
            firstDelegate = list.itemAtIndex(0)
            secondDelegate = list.itemAtIndex(1)
            firstEditor = findVisualChild(firstDelegate, "aiProjectScriptEpisodeEditor_0")
            secondEditor = findVisualChild(secondDelegate, "aiProjectScriptEpisodeEditor_1")
            return firstEditor !== null && secondEditor !== null
        })
        compare(firstEditor.text, "候选2 / 第 1 集剧本。")
        compare(secondEditor.text, "候选2 / 第二集剧本。")
        secondEditor.text = "候选2编辑 / 第二集剧本。"
        dialog.updateProjectScriptEpisode(1, secondEditor.text)
        tryVerify(function() { return dialog.projectScriptCandidateValid(dialog.taskById(taskId)) })
        findChild(dialog, "aiCandidateEditor").text = "原始 JSON 不可见"
        verify(!findChild(dialog, "aiCandidateEditor").visible)
        verify(!findChild(dialog, "saveAiCandidateAsVersionButton").visible)
        verify(!findChild(dialog, "saveAllAiCandidatesAsVersionsButton").visible)
        findChild(dialog, "saveAiCandidateChangesButton").clicked()
        var saved = JSON.parse(projectController.aiTask(taskId).candidateText)
        compare(saved.selectedIndex, 1)
        compare(JSON.parse(saved.items[0]).episodes[0].text, "候选1编辑 / 第 1 集剧本。")
        compare(JSON.parse(saved.items[1]).episodes[1].text, "候选2编辑 / 第二集剧本。")

        verify(projectController.acceptAiTask(taskId))
        compare(projectController.aiTask(taskId).status, "accepted")
        verify(projectController.selectEpisode(firstId))
        compare(projectController.scriptText, "第一集正式内容。")
        compare(projectController.scriptVersions.length, 1)
        compare(projectController.scriptVersions[0].versionType, "generated_candidate")
        compare(projectController.scriptVersions[0].text, "候选2 / 第 1 集剧本。")
        verify(projectController.selectEpisode(secondId))
        compare(projectController.scriptText, "第二集正式内容。")
        compare(projectController.scriptVersions.length, 1)
        compare(projectController.scriptVersions[0].versionType, "generated_candidate")
        compare(projectController.scriptVersions[0].text, "候选2编辑 / 第二集剧本。")

        var staleTaskId = projectController.prepareAiTask({
            operation: "script_generate",
            targetType: "project",
            targetId: projectController.currentProjectId,
            targetTitle: "全剧剧本",
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: JSON.stringify(manifest),
            systemInstruction: "返回各集结构化剧本候选",
            requestText: "检测过期候选。"
        })
        verify(projectController.setAiTaskRunning(staleTaskId))
        verify(projectController.completeAiTask(staleTaskId, candidates[0], {}))
        dialog.showTask(staleTaskId)
        verify(projectController.moveEpisode(secondId, -1))
        tryVerify(function() { return !dialog.projectScriptCandidateValid(dialog.taskById(staleTaskId)) })
        var acceptButton = findChild(dialog, "acceptAiBibleCandidatesButton")
        verify(acceptButton !== null && !acceptButton.enabled)
        verify(!projectController.acceptAiTask(staleTaskId))
        projectController.clearError()
        verify(projectController.moveEpisode(secondId, 1))
        verify(projectController.selectEpisode(firstId))
        compare(projectController.scriptVersions.length, 1)
        verify(projectController.selectEpisode(secondId))
        compare(projectController.scriptVersions.length, 1)
    }

    function test_aiScriptCandidatesWithDifferentParagraphBoundariesCanBeComposed() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)
        findChild(appWindow, "openAiGenerationButton").clicked()
        var dialog = findChild(appWindow, "aiGenerationDialog")
        tryVerify(function() { return dialog !== null && dialog.visible })

        var episodeId = projectController.currentEpisodeId
        var context = projectController.aiContextSnapshot(
                          "episode", episodeId, false, false, false)
        verify(context.valid)
        var manifest = JSON.parse(context.contextManifestJson)
        manifest.candidateCount = 3
        var taskId = projectController.prepareAiTask({
            operation: "script_generate",
            targetType: "episode",
            targetId: episodeId,
            targetTitle: projectController.currentEpisodeTitle,
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: JSON.stringify(manifest),
            systemInstruction: "生成不同段落边界的候选",
            requestText: "生成多个版本。"
        })
        verify(taskId.length > 0)
        verify(projectController.setAiTaskRunning(taskId))
        var candidates = [
            "甲一\n\n甲二",
            "乙一\n\n乙二\n\n乙三",
            "丙一"
        ]
        verify(projectController.completeAiTask(taskId, JSON.stringify({
            format: "sceneforge.script-candidates.v1",
            selectedIndex: 0,
            items: candidates
        }), {}))
        dialog.showTask(taskId)

        var partialToggle = findChild(dialog, "aiScriptPartialSelectionToggle")
        verify(partialToggle !== null && partialToggle.enabled)
        dialog.setScriptPartialSelection(true)
        compare(dialog.scriptCandidatesAligned, false)
        compare(dialog.scriptComparisonParagraphs.length, 2)
        compare(dialog.scriptComparisonParagraphs[0].choices.length, 6)

        dialog.selectScriptParagraphCandidate(0, 2)
        compare(dialog.scriptComparisonParagraphs[0].text, "乙一")

        var sourceSelector = findChild(dialog, "aiScriptSourceParagraphSelector")
        var addParagraphButton = findChild(dialog, "addAiScriptParagraphButton")
        verify(sourceSelector !== null && addParagraphButton !== null)
        sourceSelector.currentIndex = 4
        addParagraphButton.clicked()
        compare(dialog.scriptComparisonParagraphs.length, 3)
        compare(dialog.scriptComparisonParagraphs[2].text, "乙三")
        dialog.moveScriptParagraph(2, -1)
        compare(dialog.scriptComparisonParagraphs[1].text, "乙三")

        dialog.updateScriptParagraphText(2, "甲二\n\n尾声")
        dialog.splitScriptParagraph(2)
        compare(dialog.scriptComparisonParagraphs.length, 4)
        compare(dialog.scriptComparisonParagraphs[3].text, "尾声")
        dialog.mergeScriptParagraphWithPrevious(3)
        compare(dialog.scriptComparisonParagraphs.length, 3)
        compare(dialog.scriptComparisonParagraphs[2].text, "甲二\n尾声")
        dialog.removeScriptParagraph(1)
        compare(dialog.scriptComparisonParagraphs.length, 2)

        var acceptedText = "乙一\n\n甲二\n尾声"
        compare(dialog.mergedScriptText(), acceptedText)
        findChild(dialog, "acceptAiBibleCandidatesButton").clicked()
        compare(projectController.scriptText, acceptedText)
        var projectPath = projectController.currentProjectPath
        dialog.close()
        verify(projectController.closeProject())
        verify(projectController.openProject(projectPath))
        var savedCandidate = JSON.parse(projectController.aiTask(taskId).candidateText)
        compare(savedCandidate.acceptedText, acceptedText)
        compare(projectController.scriptText, acceptedText)
    }

    function test_aiScriptAnalysisRequiresAKnownSourceAndKeepsScriptReadOnly() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)

        var blockId = projectController.scriptBlocks[0].id
        var originalText = "顾遥把旧钥匙放回抽屉。"
        verify(projectController.updateScriptBlock(blockId, "action", originalText))
        var context = projectController.aiContextSnapshot(
                          "script_block", blockId, false, false, true)
        verify(context.valid)
        var manifest = JSON.parse(context.contextManifestJson)
        var source = manifest.sources.filter(function(item) {
            return item.type === "script_block" && item.id === blockId
        })[0]
        verify(source !== undefined)
        var legacyManifest = JSON.parse(JSON.stringify(manifest))
        manifest.analysisCategoryVersion = 1
        context.contextManifestJson = JSON.stringify(manifest)

        findChild(appWindow, "openAiGenerationButton").clicked()
        var dialog = findChild(appWindow, "aiGenerationDialog")
        tryVerify(function() { return dialog !== null && dialog.visible })
        var taskId = projectController.prepareAiTask({
            operation: "script_analysis",
            targetType: "script_block",
            targetId: blockId,
            targetTitle: "动作段落",
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: context.contextManifestJson,
            systemInstruction: "返回结构化分析建议。",
            requestText: "用户选择并可编辑的上下文：\n" + context.contextText
        })
        verify(taskId.length > 0)
        verify(projectController.setAiTaskRunning(taskId))
        var structuredAnalysis = {
            format: "sceneforge.script-analysis.v1",
            findings: [{
                sourceId: blockId,
                quote: "旧钥匙",
                category: "continuity",
                severity: "medium",
                observation: "钥匙的归还动作可能缺少前因。",
                reason: "上下文没有说明角色为何持有钥匙。",
                suggestion: "补充钥匙来源或保留为待确认问题。"
            }]
        }
        verify(projectController.completeAiTask(taskId,
                                               JSON.stringify(structuredAnalysis), {}))
        dialog.showTask(taskId)
        compare(dialog.scriptAnalysisReviewValid, true)
        compare(dialog.scriptAnalysisFindings.length, 1)
        compare(dialog.scriptAnalysisFindings[0].sourceId, blockId)
        compare(dialog.scriptAnalysisFindings[0].sourceTitle, source.title)
        compare(dialog.scriptAnalysisCategoryLabel(
                    dialog.scriptAnalysisFindings[0].category), "连续性")
        compare(dialog.scriptAnalysisCategoryLabel("target_duration"), "目标时长")
        verify(findChild(dialog, "aiScriptAnalysisList").visible)
        verify(!findChild(dialog, "aiCandidateEditor").visible)
        compare(projectController.scriptText, originalText)

        var invalidTaskId = projectController.prepareAiTask({
            operation: "script_analysis",
            targetType: "script_block",
            targetId: blockId,
            targetTitle: "动作段落",
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: context.contextManifestJson,
            systemInstruction: "返回结构化分析建议。",
            requestText: "用户选择并可编辑的上下文：\n" + context.contextText
        })
        verify(invalidTaskId.length > 0)
        verify(projectController.setAiTaskRunning(invalidTaskId))
        structuredAnalysis.findings[0].sourceId = "not-in-context"
        verify(projectController.completeAiTask(invalidTaskId,
                                               JSON.stringify(structuredAnalysis), {}))
        dialog.showTask(invalidTaskId)
        compare(dialog.scriptAnalysisReviewValid, false)
        verify(dialog.scriptAnalysisReviewError.length > 0)
        verify(findChild(dialog, "aiCandidateEditor").visible)
        compare(projectController.scriptText, originalText)

        var invalidCategoryTaskId = projectController.prepareAiTask({
            operation: "script_analysis",
            targetType: "script_block",
            targetId: blockId,
            targetTitle: "动作段落",
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: context.contextManifestJson,
            systemInstruction: "返回结构化分析建议。",
            requestText: "用户选择并可编辑的上下文：\n" + context.contextText
        })
        verify(invalidCategoryTaskId.length > 0)
        verify(projectController.setAiTaskRunning(invalidCategoryTaskId))
        structuredAnalysis.findings[0].sourceId = blockId
        structuredAnalysis.findings[0].category = "invented-category"
        verify(projectController.completeAiTask(invalidCategoryTaskId,
                                               JSON.stringify(structuredAnalysis), {}))
        dialog.showTask(invalidCategoryTaskId)
        compare(dialog.scriptAnalysisReviewValid, false)
        verify(dialog.scriptAnalysisReviewError.length > 0)
        compare(projectController.scriptText, originalText)

        var legacyTaskId = projectController.prepareAiTask({
            operation: "script_analysis",
            targetType: "script_block",
            targetId: blockId,
            targetTitle: "动作段落",
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: JSON.stringify(legacyManifest),
            systemInstruction: "返回结构化分析建议。",
            requestText: "用户选择并可编辑的上下文：\n" + context.contextText
        })
        verify(legacyTaskId.length > 0)
        verify(projectController.setAiTaskRunning(legacyTaskId))
        structuredAnalysis.findings[0].category = "连续性"
        verify(projectController.completeAiTask(legacyTaskId,
                                               JSON.stringify(structuredAnalysis), {}))
        dialog.showTask(legacyTaskId)
        compare(dialog.scriptAnalysisReviewValid, true)
        compare(dialog.scriptAnalysisCategoryLabel(
                    dialog.scriptAnalysisFindings[0].category), "连续性")
        compare(projectController.scriptText, originalText)
    }

    function test_sceneGenerationRequiresAndRestoresSettingReview() {
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)

        verify(projectController.createBibleItem("character", "林岚", "主要角色"))
        var characterId = projectController.bibleItems[0].id
        verify(projectController.updateBibleItemDetail(characterId, "goal", "找回寄出的旧信。"))
        verify(projectController.updateBibleItemDetail(characterId, "motivation", "弥补迟来的歉意。"))
        verify(projectController.updateBibleItemDetail(characterId, "conflict", "害怕面对收信人。"))
        verify(projectController.updateBibleItemDetail(characterId, "speechStyle", "短句，回避道歉。"))
        verify(projectController.setEpisodeBibleReference(characterId, true))

        verify(projectController.createBibleItem("location", "旧邮局", "暴雨夜的邮局"))
        var locationId = projectController.bibleItems.filter(function(item) {
            return item.name === "旧邮局"
        })[0].id
        verify(projectController.updateBibleItemDetail(locationId, "function", "信件分拣与暂存。"))
        verify(projectController.updateBibleItemDetail(locationId, "limitations", "停电时柜台无法照明。"))
        verify(projectController.setEpisodeBibleReference(locationId, true))

        verify(projectController.createBibleItem("story_rule", "旧信规则", "延迟送达规则"))
        var ruleId = projectController.bibleItems.filter(function(item) {
            return item.name === "旧信规则"
        })[0].id
        verify(projectController.updateBibleItemDetail(ruleId, "rule", "旧信只能在暴雨夜送达。"))
        verify(projectController.updateBibleItemDetail(ruleId, "consequence", "错过后无法再次投递。"))
        verify(projectController.setEpisodeBibleReference(ruleId, true))

        verify(projectController.createScene())
        var sceneId = projectController.scenes[0].id
        var blockId = projectController.scriptBlocks[0].id
        verify(projectController.setScriptBlockScene(blockId, sceneId))
        var context = projectController.aiContextSnapshot(
                          "scene", sceneId, true, true, false)
        verify(context.valid)
        var manifest = JSON.parse(context.contextManifestJson)
        manifest.candidateCount = 1

        findChild(appWindow, "openAiGenerationButton").clicked()
        var dialog = findChild(appWindow, "aiGenerationDialog")
        tryVerify(function() { return dialog !== null && dialog.visible })
        var candidateText = "林岚摸到停电的柜台，收回手。\n\n林岚：我只是想把信送到。"
        var taskId = projectController.prepareAiTask({
            operation: "script_generate",
            targetType: "scene",
            targetId: sceneId,
            targetTitle: projectController.scenes[0].title,
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: JSON.stringify(manifest),
            systemInstruction: "逐项体现已提供的场景设定。",
            requestText: "用户选择并可编辑的上下文：\n" + context.contextText
        })
        verify(taskId.length > 0)
        verify(projectController.setAiTaskRunning(taskId))
        verify(projectController.completeAiTask(taskId, JSON.stringify({
            format: "sceneforge.script-candidates.v1",
            selectedIndex: 0,
            items: [candidateText]
        }), {}))
        dialog.showTask(taskId)

        compare(dialog.scriptSceneRequirementItems.length, 4)
        verify(dialog.scriptSceneRequirementItems[0].details.indexOf("找回寄出的旧信") >= 0)
        verify(dialog.scriptSceneRequirementItems[1].details.indexOf("害怕面对收信人") >= 0)
        verify(dialog.scriptSceneRequirementItems[2].details.indexOf("回避道歉") >= 0)
        verify(dialog.scriptSceneRequirementItems[3].details.indexOf("无法照明") >= 0)
        verify(dialog.scriptSceneRequirementItems[3].details.indexOf("暴雨夜送达") >= 0)
        var acceptButton = findChild(dialog, "acceptAiBibleCandidatesButton")
        verify(acceptButton !== null && !acceptButton.enabled)

        dialog.updateScriptSceneRequirement(0, "satisfied")
        dialog.updateScriptSceneRequirement(1, "needs_revision")
        dialog.updateScriptSceneRequirement(2, "satisfied")
        dialog.updateScriptSceneRequirement(3, "satisfied")
        verify(!acceptButton.enabled)
        dialog.updateScriptSceneRequirement(1, "satisfied")
        verify(acceptButton.enabled)

        var currentTask = projectController.aiTask(taskId)
        verify(projectController.updateAiTaskCandidate(
                   taskId, dialog.candidateTextToSave(currentTask)))
        var projectPath = projectController.currentProjectPath
        dialog.close()
        verify(projectController.closeProject())
        verify(projectController.openProject(projectPath))
        dialog.showTask(taskId)
        compare(dialog.scriptSceneRequirementItems.length, 4)
        verify(dialog.sceneGenerationChecklistComplete())
        verify(acceptButton.enabled)

        acceptButton.clicked()
        compare(projectController.aiTask(taskId).status, "accepted")
        verify(projectController.scriptText.indexOf(candidateText) >= 0)
        var savedCharacter = projectController.bibleItems.filter(function(item) {
            return item.id === characterId
        })[0]
        compare(savedCharacter.details.goal, "找回寄出的旧信。")
        var savedCandidate = JSON.parse(projectController.aiTask(taskId).candidateText)
        compare(savedCandidate.sceneChecklistCandidateIndex, 0)
        compare(savedCandidate.sceneChecklist.length, 4)
    }

    function test_aiBatchScriptRewriteReviewsCandidatesAndRejectsStaleTargets() {
        appWindow.newProjectType = "series"
        findChild(appWindow, "blankEntryButton").clicked()
        findChild(appWindow, "createProjectButton").clicked()
        tryCompare(projectController, "hasOpenProject", true)
        var firstEpisodeId = projectController.currentEpisodeId
        verify(projectController.createScene())
        var firstSceneId = projectController.scenes[0].id
        var firstBlockId = projectController.scriptBlocks[0].id
        verify(projectController.setScriptBlockScene(firstBlockId, firstSceneId))
        verify(projectController.updateScriptBlock(
                   firstBlockId, "action", "第一集第一场原稿。"))
        verify(projectController.createScriptBlock("dialogue", firstSceneId))
        var secondBlockId = projectController.scriptBlocks[1].id
        verify(projectController.updateScriptBlock(
                   secondBlockId, "dialogue", "第一集第一场对白原稿。"))
        var firstEpisodeOriginal = projectController.scriptText

        verify(projectController.createEpisode("第二集"))
        var secondEpisodeId = projectController.currentEpisodeId
        verify(projectController.createScene())
        var secondSceneId = projectController.scenes[0].id
        var thirdBlockId = projectController.scriptBlocks[0].id
        verify(projectController.setScriptBlockScene(thirdBlockId, secondSceneId))
        verify(projectController.updateScriptBlock(
                   thirdBlockId, "action", "第二集第一场原稿。"))

        var context = projectController.aiContextSnapshot(
                          "project", projectController.currentProjectId,
                          false, false, true)
        verify(context.valid)
        var manifest = JSON.parse(context.contextManifestJson)
        var targets = manifest.targetScriptBlocks
        compare(targets.length, 3)
        manifest.candidateCount = 3
        var candidates = []
        for (var candidateIndex = 0; candidateIndex < 3; ++candidateIndex) {
            var blocks = targets.map(function(target) {
                return {blockId: target.blockId,
                        text: "候选" + (candidateIndex + 1) + "：" + target.blockId}
            })
            candidates.push(JSON.stringify({
                format: "sceneforge.script-block-rewrites.v1",
                blocks: blocks
            }))
        }
        var taskId = projectController.prepareAiTask({
            operation: "script_rewrite",
            targetType: "project",
            targetId: projectController.currentProjectId,
            targetTitle: projectController.currentProjectName,
            serviceId: "test-text-service",
            serviceName: "测试文本服务",
            endpoint: "http://127.0.0.1:8080/v1",
            model: "test-model",
            contextManifestJson: JSON.stringify(manifest),
            systemInstruction: "返回有稳定段落 ID 的结构化批量改写。",
            requestText: context.contextText
        })
        verify(taskId.length > 0)
        verify(projectController.setAiTaskRunning(taskId))
        verify(projectController.completeAiTask(taskId, JSON.stringify({
            format: "sceneforge.script-candidates.v1",
            selectedIndex: 0,
            items: candidates
        }), {}))

        findChild(appWindow, "openAiGenerationButton").clicked()
        var dialog = findChild(appWindow, "aiGenerationDialog")
        tryVerify(function() { return dialog !== null && dialog.visible })
        dialog.showTask(taskId)
        var list = findChild(dialog, "aiBatchRewriteBlockList")
        tryCompare(list, "count", 3)
        tryVerify(function() { return list.itemAtIndex(0) !== null })
        var firstRow = list.itemAtIndex(0)
        var editor = firstRow.textEditor
        verify(editor !== null && !editor.readOnly)
        var selector = findChild(dialog, "aiScriptCandidateSelector")
        verify(selector !== null && selector.visible)
        compare(selector.count, 3)
        selector.currentIndex = 2
        selector.activated(2)
        compare(dialog.batchRewriteBlocks[0].text, "候选3：" + targets[0].blockId)
        firstRow = list.itemAtIndex(0)
        editor = firstRow.textEditor
        var sourceSelector = firstRow.sourceSelector
        verify(sourceSelector !== null && sourceSelector.visible)
        compare(sourceSelector.count, 3)
        sourceSelector.currentIndex = 1
        sourceSelector.activated(1)
        compare(dialog.batchRewriteBlocks[0].text, "候选2：" + targets[0].blockId)
        dialog.updateBatchRewriteBlock(1, "人工修订的第二段。")
        var savedReview = JSON.parse(dialog.candidateTextToSave(
                                         projectController.aiTask(taskId)))
        var combinedBlocks = JSON.parse(savedReview.acceptedText).blocks
        compare(combinedBlocks[0].text, "候选2：" + targets[0].blockId)
        compare(combinedBlocks[1].text, "人工修订的第二段。")

        var acceptButton = findChild(dialog, "acceptAiBibleCandidatesButton")
        verify(acceptButton !== null && acceptButton.enabled)
        verify(projectController.selectEpisode(firstEpisodeId))
        verify(projectController.updateScriptBlock(
                   firstBlockId, "action", "生成后用户更新的第一集原稿。"))
        tryCompare(acceptButton, "enabled", false)
        acceptButton.clicked()
        compare(projectController.aiTask(taskId).status, "succeeded")
        compare(projectController.scriptVersions.length, 0)
        verify(firstEpisodeOriginal.indexOf("第一集第一场原稿。") >= 0)
        verify(projectController.scriptText.indexOf("生成后用户更新的第一集原稿。") >= 0)
        verify(projectController.scriptText.indexOf("第一集第一场对白原稿。") >= 0)
        verify(projectController.scriptText.indexOf("第一集第一场原稿。") < 0)
        verify(projectController.selectEpisode(secondEpisodeId))
        verify(projectController.scriptText.indexOf("第二集第一场原稿。") >= 0)
    }

}
