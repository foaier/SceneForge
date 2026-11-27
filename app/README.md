# 镜造（SceneForge）

Qt 6、C++20 和 QML/Qt Quick 项目工程骨架。当前只提供可构建的空白应用窗口，不包含产品功能实现。

## 构建

需要 CMake、Ninja、C++20 编译器和包含 Qt Quick、Qt Quick Controls 的 Qt 6 安装。Qt 与 C++ 编译器必须使用相互匹配的工具链。

```powershell
cmake -S app -B app/build -G Ninja -DCMAKE_PREFIX_PATH="<Qt安装前缀>"
cmake --build app/build
```

也可以在 Qt Creator 中打开 `app/CMakeLists.txt`，选择与 Qt 安装相匹配的构建套件。
