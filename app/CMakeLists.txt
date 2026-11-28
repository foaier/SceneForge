cmake_minimum_required(VERSION 3.21)

project(SceneForge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Network Quick QuickControls2 Sql)

qt_add_executable(SceneForge WIN32
    src/main.cpp
)

add_library(sceneforge_core STATIC
    src/ai/aigenerationcontroller.cpp
    src/ai/aigenerationcontroller.h
    src/ai/aiserviceprofile.cpp
    src/ai/aiserviceprofile.h
    src/ai/aitextserviceadapter.cpp
    src/ai/aitextserviceadapter.h
    src/ai/aisettingscontroller.cpp
    src/ai/aisettingscontroller.h
    src/ai/aicredentialstore.h
    src/project/biblecatalog.cpp
    src/project/biblecatalog.h
    src/project/aibiblecandidateapplier.cpp
    src/project/aibiblecandidateapplier.h
    src/project/aibibleupdatecandidateapplier.cpp
    src/project/aibibleupdatecandidateapplier.h
    src/project/aibatchscriptcandidateapplier.cpp
    src/project/aibatchscriptcandidateapplier.h
    src/project/aiprojectscriptcandidateapplier.cpp
    src/project/aiprojectscriptcandidateapplier.h
    src/project/scriptcatalog.cpp
    src/project/scriptcatalog.h
    src/project/scriptcandidatecodec.cpp
    src/project/scriptcandidatecodec.h
    src/project/projectcontroller.cpp
    src/project/projectcontroller.h
    src/project/projectrepository.cpp
    src/project/projectrepository.h
    src/project/projectpackageservice.cpp
    src/project/projectpackageservice.h
    src/project/projectdraft.h
    src/project/projectinfo.h
    src/project/projectlifecycleservice.cpp
    src/project/projectlifecycleservice.h
    src/project/qualityanalyzer.cpp
    src/project/qualityanalyzer.h
    src/project/storyimpactanalyzer.cpp
    src/project/storyimpactanalyzer.h
    src/platform/librarypathvalidator.cpp
    src/platform/librarypathvalidator.h
    src/platform/windowscredentialstore.cpp
    src/platform/windowscredentialstore.h
)

target_link_libraries(sceneforge_core
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Sql
)

if(WIN32)
    target_link_libraries(sceneforge_core PUBLIC Advapi32)
endif()

qt_add_qml_module(SceneForge
    URI SceneForge
    VERSION 1.0
    QML_FILES
        qml/AiGenerationDialog.qml
        qml/AiServiceSettingsDialog.qml
        qml/Main.qml
)

target_link_libraries(SceneForge
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        sceneforge_core
)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS QuickTest)

    qt_add_executable(ProjectStorageTests
        tests/projectstoragetests.cpp
    )
    target_link_libraries(ProjectStorageTests
        PRIVATE
            sceneforge_core
            Qt6::Core
            Qt6::Sql
    )
    add_test(NAME ProjectStorageTests COMMAND ProjectStorageTests)

    qt_add_executable(AiSettingsTests
        tests/aisettings_tests.cpp
    )
    target_link_libraries(AiSettingsTests
        PRIVATE
            sceneforge_core
            Qt6::Core
    )
    add_test(NAME AiSettingsTests COMMAND AiSettingsTests)

    qt_add_executable(AiTextAdapterTests
        tests/aitextadapter_tests.cpp
    )
    target_link_libraries(AiTextAdapterTests
        PRIVATE
            sceneforge_core
            Qt6::Core
            Qt6::Network
    )
    add_test(NAME AiTextAdapterTests COMMAND AiTextAdapterTests)

    qt_add_executable(AiBatchScriptCandidateTests
        tests/aibatchscriptcandidate_tests.cpp
    )
    target_link_libraries(AiBatchScriptCandidateTests
        PRIVATE
            sceneforge_core
            Qt6::Core
            Qt6::Sql
    )
    add_test(NAME AiBatchScriptCandidateTests COMMAND AiBatchScriptCandidateTests)

    qt_add_executable(ProjectUiTests
        tests/projectuitests.cpp
    )
    target_link_libraries(ProjectUiTests
        PRIVATE
            sceneforge_core
            Qt6::Quick
            Qt6::QuickControls2
            Qt6::QuickTest
    )
    target_compile_definitions(ProjectUiTests
        PRIVATE
            QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/qml"
    )
    add_test(NAME ProjectUiTests COMMAND ProjectUiTests)
    set_tests_properties(ProjectUiTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()
