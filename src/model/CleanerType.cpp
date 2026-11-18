#include "CleanerType.h"

#include <QCoreApplication>

QJsonObject CleanRule::toJson() const
{
    return QJsonObject{
        {QStringLiteral("pattern"), pattern},
        {QStringLiteral("markers"), QJsonArray::fromStringList(markers)},
    };
}

CleanRule CleanRule::fromJson(const QJsonObject &obj)
{
    CleanRule rule;
    rule.pattern = obj.value(QStringLiteral("pattern")).toString();
    for (const QJsonValue &v : obj.value(QStringLiteral("markers")).toArray()) {
        if (!v.toString().isEmpty())
            rule.markers.append(v.toString());
    }
    return rule;
}

QJsonObject CleanerType::toJson() const
{
    QJsonArray ruleArray;
    for (const CleanRule &r : rules)
        ruleArray.append(r.toJson());
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("description"), description},
        {QStringLiteral("mode"),
         mode == Mode::Contents ? QStringLiteral("contents") : QStringLiteral("rules")},
        {QStringLiteral("rules"), ruleArray},
    };
}

CleanerType CleanerType::fromJson(const QJsonObject &obj)
{
    CleanerType type;
    type.id = obj.value(QStringLiteral("id")).toString();
    type.name = obj.value(QStringLiteral("name")).toString();
    type.description = obj.value(QStringLiteral("description")).toString();
    type.mode = obj.value(QStringLiteral("mode")).toString() == QLatin1String("contents")
                    ? Mode::Contents
                    : Mode::Rules;
    for (const QJsonValue &v : obj.value(QStringLiteral("rules")).toArray()) {
        if (v.isObject()) {
            const CleanRule rule = CleanRule::fromJson(v.toObject());
            if (!rule.pattern.isEmpty())
                type.rules.append(rule);
        }
    }
    return type;
}

QJsonArray cleanerTypesToJson(const QList<CleanerType> &types)
{
    QJsonArray array;
    for (const CleanerType &t : types)
        array.append(t.toJson());
    return array;
}

QList<CleanerType> cleanerTypesFromJson(const QJsonArray &array)
{
    QList<CleanerType> types;
    for (const QJsonValue &v : array) {
        if (!v.isObject())
            continue;
        CleanerType t = CleanerType::fromJson(v.toObject());
        if (!t.id.isEmpty())
            types.append(t);
    }
    return types;
}

namespace BuiltInTypes {

QString contentsId()
{
    return QStringLiteral("contents");
}

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("BuiltInTypes", text);
}

CleanerType rulesType(const char *id, const QString &name, const QString &description,
                      QList<CleanRule> rules)
{
    CleanerType t;
    t.id = QString::fromLatin1(id);
    t.name = name;
    t.description = description;
    t.mode = CleanerType::Mode::Rules;
    t.rules = std::move(rules);
    t.builtIn = true;
    return t;
}

QList<CleanerType> build()
{
    const auto rule = [](const char *pattern, QStringList markers = {}) {
        return CleanRule{QString::fromLatin1(pattern), std::move(markers)};
    };
    const QStringList cmake{QStringLiteral("CMakeLists.txt")};
    const QStringList node{QStringLiteral("package.json")};
    const QStringList python{QStringLiteral("pyproject.toml"), QStringLiteral("setup.py"),
                             QStringLiteral("setup.cfg"), QStringLiteral("requirements*.txt"),
                             QStringLiteral("Pipfile")};
    const QStringList gradle{QStringLiteral("build.gradle"), QStringLiteral("build.gradle.kts"),
                             QStringLiteral("settings.gradle"),
                             QStringLiteral("settings.gradle.kts")};
    const QStringList dotnet{QStringLiteral("*.csproj"), QStringLiteral("*.fsproj"),
                             QStringLiteral("*.vbproj")};

    QList<CleanerType> types;

    CleanerType contents;
    contents.id = contentsId();
    contents.name = tr("Folder contents");
    contents.description = tr("Everything inside the folder. The folder itself stays.");
    contents.mode = CleanerType::Mode::Contents;
    contents.builtIn = true;
    types.append(contents);

    types.append(rulesType("rust", tr("Rust"), tr("Cargo's target folders."),
                           {rule("target", {QStringLiteral("Cargo.toml")})}));
    types.append(rulesType("cpp", tr("C and C++ (CMake)"),
                           tr("CMake build folders, and the ones IDEs create."),
                           {rule("build", cmake), rule("build-*", cmake),
                            rule("cmake-build-*", cmake), rule("out", cmake)}));
    types.append(rulesType("node", tr("JavaScript and TypeScript"),
                           tr("node_modules and framework build caches."),
                           {rule("node_modules", node), rule(".next", node), rule(".nuxt", node),
                            rule(".turbo", node), rule(".parcel-cache", node),
                            rule(".svelte-kit", node)}));
    types.append(rulesType("python", tr("Python"),
                           tr("Bytecode, tool caches and virtual environments."),
                           {rule("__pycache__"), rule(".pytest_cache"), rule(".mypy_cache"),
                            rule(".ruff_cache"), rule(".tox", python), rule(".venv", python),
                            rule("venv", python)}));
    types.append(rulesType("jvm", tr("Java and Kotlin (Gradle, Maven)"),
                           tr("Gradle and Maven build folders."),
                           {rule("build", gradle), rule(".gradle", gradle),
                            rule("target", {QStringLiteral("pom.xml")})}));
    types.append(rulesType("dotnet", tr(".NET"), tr("bin and obj folders of .NET projects."),
                           {rule("bin", dotnet), rule("obj", dotnet)}));
    types.append(rulesType("go", tr("Go"), tr("Build output folders of Go projects."),
                           {rule("bin", {QStringLiteral("go.mod")})}));
    types.append(rulesType("swift", tr("Swift"), tr("Swift Package Manager's .build folders."),
                           {rule(".build", {QStringLiteral("Package.swift")})}));
    types.append(rulesType("dart", tr("Dart and Flutter"), tr("build and .dart_tool folders."),
                           {rule("build", {QStringLiteral("pubspec.yaml")}),
                            rule(".dart_tool", {QStringLiteral("pubspec.yaml")})}));
    types.append(rulesType("zig", tr("Zig"), tr("Zig's caches and output folders."),
                           {rule(".zig-cache", {QStringLiteral("build.zig")}),
                            rule("zig-cache", {QStringLiteral("build.zig")}),
                            rule("zig-out", {QStringLiteral("build.zig")})}));
    types.append(rulesType("haskell", tr("Haskell"), tr("Stack and Cabal build folders."),
                           {rule(".stack-work", {QStringLiteral("stack.yaml")}),
                            rule("dist-newstyle", {QStringLiteral("*.cabal"),
                                                   QStringLiteral("cabal.project")})}));

    // Every rule above, for a folder with projects in several languages.
    QList<CleanRule> all;
    for (const CleanerType &t : std::as_const(types)) {
        for (const CleanRule &r : t.rules) {
            if (!all.contains(r))
                all.append(r);
        }
    }
    types.append(rulesType("all-languages", tr("All programming languages"),
                           tr("The rules of every built-in language type."), all));
    return types;
}

} // namespace

const QList<CleanerType> &all()
{
    static const QList<CleanerType> types = build();
    return types;
}

} // namespace BuiltInTypes
