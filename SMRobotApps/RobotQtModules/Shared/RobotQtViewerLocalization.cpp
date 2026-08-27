#include "RobotQtViewerLocalization.h"

#include "RobotQtViewerWorkbenchPackageRegistry.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBox>
#include <QTreeWidget>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace robot_qt_viewer
{
namespace
{
    constexpr const char* kCatalogSchema = "smrobot.localization.catalog";
    constexpr int kCatalogVersion = 1;
    constexpr const char* kDefaultLanguageId = "en-US";
    constexpr const char* kLanguageSettingsKey = "ui/language";
    constexpr const char* kApplicationServiceProperty =
        "smrobot.localization.service";

    struct Catalog
    {
        RobotQtViewerLanguageInfo info;
        QHash<QString, QString> messages;
        QHash<QString, QString> legacy;
    };

    struct TemplatePattern
    {
        QString aliasTemplate;
        QString sourceTemplate;
        QRegularExpression expression;
        QVector<int> capturePlaceholders;
    };

    QString normalizedLanguageId(const QString& languageId)
    {
        const QString trimmed = languageId.trimmed();
        if(trimmed.compare(QStringLiteral("English"), Qt::CaseInsensitive) == 0 ||
            trimmed.compare(QStringLiteral("en"), Qt::CaseInsensitive) == 0 ||
            trimmed.compare(QStringLiteral("en_US"), Qt::CaseInsensitive) == 0) {
            return QStringLiteral("en-US");
        }
        if(trimmed.compare(QStringLiteral("Chinese"), Qt::CaseInsensitive) == 0 ||
            trimmed.compare(QStringLiteral("zh"), Qt::CaseInsensitive) == 0 ||
            trimmed.compare(QStringLiteral("zh_CN"), Qt::CaseInsensitive) == 0) {
            return QStringLiteral("zh-CN");
        }
        return QString(trimmed).replace(QLatin1Char('_'), QLatin1Char('-'));
    }

    bool hasPlaceholder(const QString& text)
    {
        static const QRegularExpression placeholder(QStringLiteral("%[1-9]"));
        return text.contains(placeholder);
    }

    TemplatePattern makeTemplatePattern(
        const QString& aliasTemplate,
        const QString& sourceTemplate)
    {
        TemplatePattern result;
        result.aliasTemplate = aliasTemplate;
        result.sourceTemplate = sourceTemplate;

        QString pattern = QStringLiteral("^");
        QString literal;
        for(int index = 0; index < aliasTemplate.size(); ++index) {
            const QChar current = aliasTemplate.at(index);
            if(current == QLatin1Char('%') && index + 1 < aliasTemplate.size() &&
                aliasTemplate.at(index + 1).isDigit()) {
                if(!literal.isEmpty()) {
                    pattern += QRegularExpression::escape(literal);
                    literal.clear();
                }
                const int placeholder = aliasTemplate.at(index + 1).digitValue();
                if(placeholder >= 1 && placeholder <= 9) {
                    pattern += QStringLiteral("(.*?)");
                    result.capturePlaceholders.push_back(placeholder);
                    ++index;
                    continue;
                }
            }
            literal += current;
        }
        pattern += QRegularExpression::escape(literal);
        pattern += QLatin1Char('$');
        result.expression = QRegularExpression(pattern, QRegularExpression::DotMatchesEverythingOption);
        return result;
    }

    QString renderTemplate(const QString& textTemplate, const QHash<int, QString>& arguments)
    {
        QString result = textTemplate;
        for(int placeholder = 9; placeholder >= 1; --placeholder) {
            const auto it = arguments.constFind(placeholder);
            if(it != arguments.constEnd()) {
                result.replace(
                    QStringLiteral("%") + QString::number(placeholder),
                    it.value());
            }
        }
        return result;
    }

    template<typename Setter>
    void setTranslatedText(
        const QString& current,
        const RobotQtViewerLocalizationService& localization,
        Setter setter)
    {
        const QString translated = localization.translateLegacyText(current);
        if(translated != current) {
            setter(translated);
        }
    }

    void translateTreeItem(
        QTreeWidgetItem* item,
        int columnCount,
        const RobotQtViewerLocalizationService& localization)
    {
        if(item == nullptr) {
            return;
        }
        for(int column = 0; column < columnCount; ++column) {
            setTranslatedText(item->text(column), localization, [item, column](const QString& text) {
                item->setText(column, text);
            });
            setTranslatedText(item->toolTip(column), localization, [item, column](const QString& text) {
                item->setToolTip(column, text);
            });
        }
        for(int child = 0; child < item->childCount(); ++child) {
            translateTreeItem(item->child(child), columnCount, localization);
        }
    }

    void translateObject(
        QObject* object,
        const RobotQtViewerLocalizationService& localization)
    {
        if(object == nullptr) {
            return;
        }

        if(auto* action = qobject_cast<QAction*>(object)) {
            setTranslatedText(action->text(), localization, [action](const QString& text) {
                action->setText(text);
            });
            setTranslatedText(action->toolTip(), localization, [action](const QString& text) {
                action->setToolTip(text);
            });
            setTranslatedText(action->statusTip(), localization, [action](const QString& text) {
                action->setStatusTip(text);
            });
        }
        if(auto* widget = qobject_cast<QWidget*>(object)) {
            setTranslatedText(widget->windowTitle(), localization, [widget](const QString& text) {
                widget->setWindowTitle(text);
            });
            setTranslatedText(widget->toolTip(), localization, [widget](const QString& text) {
                widget->setToolTip(text);
            });
            setTranslatedText(widget->statusTip(), localization, [widget](const QString& text) {
                widget->setStatusTip(text);
            });
        }
        if(auto* menu = qobject_cast<QMenu*>(object)) {
            setTranslatedText(menu->title(), localization, [menu](const QString& text) {
                menu->setTitle(text);
            });
        }
        if(auto* dock = qobject_cast<QDockWidget*>(object)) {
            setTranslatedText(dock->windowTitle(), localization, [dock](const QString& text) {
                dock->setWindowTitle(text);
            });
        }
        if(auto* button = qobject_cast<QAbstractButton*>(object)) {
            setTranslatedText(button->text(), localization, [button](const QString& text) {
                button->setText(text);
            });
        }
        if(auto* label = qobject_cast<QLabel*>(object)) {
            setTranslatedText(label->text(), localization, [label](const QString& text) {
                label->setText(text);
            });
        }
        if(auto* group = qobject_cast<QGroupBox*>(object)) {
            setTranslatedText(group->title(), localization, [group](const QString& text) {
                group->setTitle(text);
            });
        }
        if(auto* edit = qobject_cast<QLineEdit*>(object)) {
            setTranslatedText(edit->placeholderText(), localization, [edit](const QString& text) {
                edit->setPlaceholderText(text);
            });
        }
        if(auto* spin = qobject_cast<QAbstractSpinBox*>(object)) {
            setTranslatedText(spin->toolTip(), localization, [spin](const QString& text) {
                spin->setToolTip(text);
            });
        }
        if(auto* combo = qobject_cast<QComboBox*>(object)) {
            for(int index = 0; index < combo->count(); ++index) {
                setTranslatedText(
                    combo->itemText(index),
                    localization,
                    [combo, index](const QString& text) { combo->setItemText(index, text); });
            }
        }
        if(auto* tabs = qobject_cast<QTabWidget*>(object)) {
            for(int index = 0; index < tabs->count(); ++index) {
                setTranslatedText(
                    tabs->tabText(index),
                    localization,
                    [tabs, index](const QString& text) { tabs->setTabText(index, text); });
                setTranslatedText(
                    tabs->tabToolTip(index),
                    localization,
                    [tabs, index](const QString& text) { tabs->setTabToolTip(index, text); });
            }
        }
        if(auto* toolBox = qobject_cast<QToolBox*>(object)) {
            for(int index = 0; index < toolBox->count(); ++index) {
                setTranslatedText(
                    toolBox->itemText(index),
                    localization,
                    [toolBox, index](const QString& text) { toolBox->setItemText(index, text); });
                setTranslatedText(
                    toolBox->itemToolTip(index),
                    localization,
                    [toolBox, index](const QString& text) { toolBox->setItemToolTip(index, text); });
            }
        }
        if(auto* tree = qobject_cast<QTreeWidget*>(object)) {
            translateTreeItem(tree->headerItem(), tree->columnCount(), localization);
            for(int index = 0; index < tree->topLevelItemCount(); ++index) {
                translateTreeItem(tree->topLevelItem(index), tree->columnCount(), localization);
            }
        }
        if(auto* table = qobject_cast<QTableWidget*>(object)) {
            for(int column = 0; column < table->columnCount(); ++column) {
                QTableWidgetItem* item = table->horizontalHeaderItem(column);
                if(item != nullptr) {
                    setTranslatedText(item->text(), localization, [item](const QString& text) {
                        item->setText(text);
                    });
                    setTranslatedText(item->toolTip(), localization, [item](const QString& text) {
                        item->setToolTip(text);
                    });
                }
            }
            for(int row = 0; row < table->rowCount(); ++row) {
                for(int column = 0; column < table->columnCount(); ++column) {
                    QTableWidgetItem* item = table->item(row, column);
                    if(item != nullptr) {
                        setTranslatedText(item->text(), localization, [item](const QString& text) {
                            item->setText(text);
                        });
                        setTranslatedText(item->toolTip(), localization, [item](const QString& text) {
                            item->setToolTip(text);
                        });
                    }
                }
            }
        }
        if(auto* list = qobject_cast<QListWidget*>(object)) {
            for(int index = 0; index < list->count(); ++index) {
                QListWidgetItem* item = list->item(index);
                setTranslatedText(item->text(), localization, [item](const QString& text) {
                    item->setText(text);
                });
                setTranslatedText(item->toolTip(), localization, [item](const QString& text) {
                    item->setToolTip(text);
                });
            }
        }
    }
}

class RobotQtViewerLocalizationService::Impl
{
public:
    explicit Impl(QString directory)
        : catalogDirectory(std::move(directory))
    {
    }

    QString canonicalLanguageId(const QString& requested) const
    {
        const QString normalized = normalizedLanguageId(requested);
        for(auto it = catalogs.constBegin(); it != catalogs.constEnd(); ++it) {
            if(it.key().compare(normalized, Qt::CaseInsensitive) == 0) {
                return it.key();
            }
        }
        return QString();
    }

    QString message(const QString& key, const QString& englishFallback) const
    {
        QString languageId = currentLanguageId;
        QSet<QString> visited;
        while(!languageId.isEmpty() && !visited.contains(languageId)) {
            visited.insert(languageId);
            const auto catalogIt = catalogs.constFind(languageId);
            if(catalogIt == catalogs.constEnd()) {
                break;
            }
            const auto messageIt = catalogIt->messages.constFind(key);
            if(messageIt != catalogIt->messages.constEnd() && !messageIt->isEmpty()) {
                return messageIt.value();
            }
            languageId = catalogIt->info.fallbackId;
        }

        const auto english = catalogs.constFind(QString::fromLatin1(kDefaultLanguageId));
        if(english != catalogs.constEnd()) {
            const auto messageIt = english->messages.constFind(key);
            if(messageIt != english->messages.constEnd() && !messageIt->isEmpty()) {
                return messageIt.value();
            }
        }
        return englishFallback.isEmpty() ? key : englishFallback;
    }

    QString targetLegacyText(const QString& sourceTemplate) const
    {
        QString languageId = currentLanguageId;
        QSet<QString> visited;
        while(!languageId.isEmpty() && !visited.contains(languageId)) {
            visited.insert(languageId);
            const auto catalogIt = catalogs.constFind(languageId);
            if(catalogIt == catalogs.constEnd()) {
                break;
            }
            const auto value = catalogIt->legacy.constFind(sourceTemplate);
            if(value != catalogIt->legacy.constEnd() && !value->isEmpty()) {
                return value.value();
            }
            languageId = catalogIt->info.fallbackId;
        }
        return sourceTemplate;
    }

    bool canonicalLegacyText(
        const QString& displayed,
        QString* sourceTemplate,
        QHash<int, QString>* arguments) const
    {
        const auto direct = directAliases.constFind(displayed);
        if(direct != directAliases.constEnd()) {
            *sourceTemplate = direct.value();
            arguments->clear();
            return true;
        }
        const auto caseFolded = directAliasesCaseFolded.constFind(displayed.toCaseFolded());
        if(caseFolded != directAliasesCaseFolded.constEnd()) {
            *sourceTemplate = caseFolded.value();
            arguments->clear();
            return true;
        }

        for(const TemplatePattern& pattern : templatePatterns) {
            const QRegularExpressionMatch match = pattern.expression.match(displayed);
            if(!match.hasMatch()) {
                continue;
            }
            arguments->clear();
            for(int index = 0; index < pattern.capturePlaceholders.size(); ++index) {
                arguments->insert(
                    pattern.capturePlaceholders.at(index),
                    match.captured(index + 1));
            }
            *sourceTemplate = pattern.sourceTemplate;
            return true;
        }
        return false;
    }

    void rebuildLegacyIndex()
    {
        directAliases.clear();
        directAliasesCaseFolded.clear();
        templatePatterns.clear();
        for(auto catalogIt = catalogs.constBegin(); catalogIt != catalogs.constEnd(); ++catalogIt) {
            for(auto it = catalogIt->legacy.constBegin(); it != catalogIt->legacy.constEnd(); ++it) {
                const QString source = it.key();
                const QString translation = it.value();
                for(const QString& alias : { source, translation }) {
                    if(alias.isEmpty()) {
                        continue;
                    }
                    if(hasPlaceholder(alias)) {
                        templatePatterns.push_back(makeTemplatePattern(alias, source));
                    } else if(!directAliases.contains(alias)) {
                        directAliases.insert(alias, source);
                        directAliasesCaseFolded.insert(alias.toCaseFolded(), source);
                    }
                }
            }
        }
        std::sort(
            templatePatterns.begin(),
            templatePatterns.end(),
            [](const TemplatePattern& left, const TemplatePattern& right) {
                return left.aliasTemplate.size() > right.aliasTemplate.size();
            });
    }

    QString catalogDirectory;
    QHash<QString, Catalog> catalogs;
    QHash<QString, QString> directAliases;
    QHash<QString, QString> directAliasesCaseFolded;
    QVector<TemplatePattern> templatePatterns;
    QString currentLanguageId = QString::fromLatin1(kDefaultLanguageId);
    QPointer<QApplication> application;
    bool retranslating = false;
};

RobotQtViewerLocalizationService::RobotQtViewerLocalizationService(
    const QString& catalogDirectory,
    QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>(catalogDirectory))
{
}

RobotQtViewerLocalizationService::~RobotQtViewerLocalizationService()
{
    if(m_impl->application == nullptr) {
        return;
    }
    m_impl->application->removeEventFilter(this);
    if(installedOnApplication(*m_impl->application) == this) {
        m_impl->application->setProperty(kApplicationServiceProperty, QVariant());
    }
}

bool RobotQtViewerLocalizationService::reloadCatalogs(QString* errorMessage)
{
    QDir directory(m_impl->catalogDirectory);
    const QStringList files = directory.entryList(
        { QStringLiteral("*.i18n.json") },
        QDir::Files,
        QDir::Name | QDir::IgnoreCase);
    if(files.isEmpty()) {
        if(errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No localization catalogs were found in %1.")
                .arg(directory.absolutePath());
        }
        return false;
    }

    QHash<QString, Catalog> loadedCatalogs;
    for(const QString& fileName : files) {
        QFile file(directory.filePath(fileName));
        if(!file.open(QIODevice::ReadOnly)) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Cannot open localization catalog: %1")
                    .arg(file.fileName());
            }
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if(parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Invalid localization JSON %1: %2")
                    .arg(file.fileName(), parseError.errorString());
            }
            return false;
        }

        const QJsonObject root = document.object();
        const QString schema = root.value(QStringLiteral("schema")).toString();
        const int version = root.value(QStringLiteral("version")).toInt(-1);
        const QString locale = normalizedLanguageId(
            root.value(QStringLiteral("locale")).toString());
        const QString nativeName = root.value(QStringLiteral("nativeName")).toString().trimmed();
        const QString fallback = normalizedLanguageId(
            root.value(QStringLiteral("fallbackLocale")).toString());
        if(schema != QString::fromLatin1(kCatalogSchema) || version != kCatalogVersion ||
            locale.isEmpty() || nativeName.isEmpty() ||
            !root.value(QStringLiteral("messages")).isObject() ||
            !root.value(QStringLiteral("legacy")).isObject()) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Localization catalog has an invalid schema: %1")
                    .arg(file.fileName());
            }
            return false;
        }

        Catalog& catalog = loadedCatalogs[locale];
        catalog.info.id = locale;
        catalog.info.nativeName = nativeName;
        catalog.info.fallbackId = fallback;
        const auto mergeObject = [](const QJsonObject& object, QHash<QString, QString>* values) {
            for(auto it = object.constBegin(); it != object.constEnd(); ++it) {
                if(it.value().isString()) {
                    values->insert(it.key(), it.value().toString());
                }
            }
        };
        mergeObject(root.value(QStringLiteral("messages")).toObject(), &catalog.messages);
        mergeObject(root.value(QStringLiteral("legacy")).toObject(), &catalog.legacy);
    }

    if(!loadedCatalogs.contains(QString::fromLatin1(kDefaultLanguageId))) {
        if(errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The required en-US localization catalog is missing.");
        }
        return false;
    }
    for(auto it = loadedCatalogs.constBegin(); it != loadedCatalogs.constEnd(); ++it) {
        if(!it->info.fallbackId.isEmpty() && !loadedCatalogs.contains(it->info.fallbackId)) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Localization fallback is unavailable: %1 -> %2")
                    .arg(it.key(), it->info.fallbackId);
            }
            return false;
        }
        QSet<QString> visited;
        QString current = it.key();
        while(!current.isEmpty()) {
            if(visited.contains(current)) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Localization fallback cycle contains: %1")
                        .arg(current);
                }
                return false;
            }
            visited.insert(current);
            current = loadedCatalogs.value(current).info.fallbackId;
        }
    }

    const QHash<QString, QString> englishMessages =
        loadedCatalogs.value(QString::fromLatin1(kDefaultLanguageId)).messages;
    for(auto message = englishMessages.constBegin();
        message != englishMessages.constEnd();
        ++message) {
        const QString sourceText = message.value();
        if(sourceText.isEmpty()) {
            continue;
        }
        for(auto catalog = loadedCatalogs.begin(); catalog != loadedCatalogs.end(); ++catalog) {
            const auto translated = catalog->messages.constFind(message.key());
            if(translated != catalog->messages.constEnd() &&
                !catalog->legacy.contains(sourceText)) {
                catalog->legacy.insert(sourceText, translated.value());
            }
        }
    }

    m_impl->catalogs = std::move(loadedCatalogs);
    m_impl->rebuildLegacyIndex();
    QString desired = m_impl->canonicalLanguageId(m_impl->currentLanguageId);
    if(desired.isEmpty()) {
        desired = QString::fromLatin1(kDefaultLanguageId);
    }
    m_impl->currentLanguageId = desired;
    if(errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

void RobotQtViewerLocalizationService::installOnApplication(QApplication& application)
{
    if(m_impl->application == &application &&
        installedOnApplication(application) == this) {
        return;
    }
    if(m_impl->application != nullptr) {
        m_impl->application->removeEventFilter(this);
        if(installedOnApplication(*m_impl->application) == this) {
            m_impl->application->setProperty(kApplicationServiceProperty, QVariant());
        }
    }
    m_impl->application = &application;
    application.setProperty(
        kApplicationServiceProperty,
        QVariant::fromValue(static_cast<QObject*>(this)));
    application.installEventFilter(this);
}

RobotQtViewerLocalizationService*
RobotQtViewerLocalizationService::installedOnApplication(
    const QApplication& application) noexcept
{
    QObject* object = application.property(kApplicationServiceProperty).value<QObject*>();
    return dynamic_cast<RobotQtViewerLocalizationService*>(object);
}

QVector<RobotQtViewerLanguageInfo> RobotQtViewerLocalizationService::availableLanguages() const
{
    QVector<RobotQtViewerLanguageInfo> result;
    result.reserve(m_impl->catalogs.size());
    for(auto it = m_impl->catalogs.constBegin(); it != m_impl->catalogs.constEnd(); ++it) {
        result.push_back(it->info);
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        const bool leftIsDefault = left.id == QString::fromLatin1(kDefaultLanguageId);
        const bool rightIsDefault = right.id == QString::fromLatin1(kDefaultLanguageId);
        if(leftIsDefault != rightIsDefault) {
            return leftIsDefault;
        }
        if(leftIsDefault) {
            return false;
        }
        return left.nativeName.localeAwareCompare(right.nativeName) < 0;
    });
    return result;
}

QString RobotQtViewerLocalizationService::currentLanguageId() const
{
    return m_impl->currentLanguageId;
}

QString RobotQtViewerLocalizationService::savedLanguageId() const
{
    QSettings settings;
    const QString requested = settings.value(
        QString::fromLatin1(kLanguageSettingsKey),
        QString::fromLatin1(kDefaultLanguageId)).toString();
    const QString canonical = m_impl->canonicalLanguageId(requested);
    return canonical.isEmpty() ? QString::fromLatin1(kDefaultLanguageId) : canonical;
}

QString RobotQtViewerLocalizationService::languageNativeName(const QString& languageId) const
{
    const QString canonical = m_impl->canonicalLanguageId(languageId);
    const auto it = m_impl->catalogs.constFind(canonical);
    return it == m_impl->catalogs.constEnd() ? languageId : it->info.nativeName;
}

bool RobotQtViewerLocalizationService::setLanguage(
    const QString& languageId,
    bool persist,
    QString* errorMessage)
{
    const QString canonical = m_impl->canonicalLanguageId(languageId);
    if(canonical.isEmpty()) {
        if(errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Localization catalog is unavailable: %1")
                .arg(languageId);
        }
        return false;
    }
    m_impl->currentLanguageId = canonical;
    if(persist) {
        QSettings settings;
        settings.setValue(QString::fromLatin1(kLanguageSettingsKey), canonical);
    }
    if(errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

QString RobotQtViewerLocalizationService::text(
    const QString& key,
    const QString& englishFallback) const
{
    return m_impl->message(key, englishFallback);
}

QString RobotQtViewerLocalizationService::translateLegacyText(const QString& text) const
{
    if(text.isEmpty()) {
        return text;
    }
    QString sourceTemplate;
    QHash<int, QString> arguments;
    if(!m_impl->canonicalLegacyText(text, &sourceTemplate, &arguments)) {
        return text;
    }
    return renderTemplate(m_impl->targetLegacyText(sourceTemplate), arguments);
}

void RobotQtViewerLocalizationService::retranslateObjectTree(QObject* root) const
{
    if(root == nullptr || m_impl->retranslating) {
        return;
    }
    m_impl->retranslating = true;
    translateObject(root, *this);
    const auto children = root->findChildren<QObject*>();
    for(QObject* child : children) {
        translateObject(child, *this);
    }
    m_impl->retranslating = false;
}

void RobotQtViewerLocalizationService::retranslateApplication() const
{
    if(m_impl->application == nullptr) {
        return;
    }
    for(QWidget* widget : m_impl->application->topLevelWidgets()) {
        QEvent languageChange(QEvent::LanguageChange);
        QApplication::sendEvent(widget, &languageChange);
        retranslateObjectTree(widget);
    }
}

bool RobotQtViewerLocalizationService::eventFilter(QObject* watched, QEvent* event)
{
    if(!m_impl->retranslating && event != nullptr &&
        (event->type() == QEvent::Show ||
         event->type() == QEvent::WindowActivate ||
         event->type() == QEvent::ToolTip ||
         event->type() == QEvent::LayoutRequest)) {
        retranslateObjectTree(watched);
    }
    return QObject::eventFilter(watched, event);
}

RobotQtViewerWidgetLanguageParticipant::RobotQtViewerWidgetLanguageParticipant(QObject& root)
{
    addRoot(root);
}

void RobotQtViewerWidgetLanguageParticipant::addRoot(QObject& root)
{
    if(!m_roots.contains(&root)) {
        m_roots.push_back(&root);
    }
}

void RobotQtViewerWidgetLanguageParticipant::retranslateUi(
    const RobotQtViewerLocalizationService& localization) noexcept
{
    for(const QPointer<QObject>& root : m_roots) {
        localization.retranslateObjectTree(root.data());
    }
}

void RobotQtViewerNoOpLanguageParticipant::retranslateUi(
    const RobotQtViewerLocalizationService&) noexcept
{
}

RobotQtViewerLanguageCoordinator::RobotQtViewerLanguageCoordinator(
    RobotQtViewerLocalizationService& localization,
    RobotQtViewerWorkbenchPackageRegistry& registry)
    : m_localization(localization)
    , m_registry(registry)
{
}

void RobotQtViewerLanguageCoordinator::registerShellParticipant(
    IRobotQtViewerLanguageParticipant& participant)
{
    if(!m_shellParticipants.contains(&participant)) {
        m_shellParticipants.push_back(&participant);
    }
}

bool RobotQtViewerLanguageCoordinator::switchLanguage(
    const QString& languageId,
    QString* errorMessage)
{
    if(!m_registry.validateEnabledModeLanguages(errorMessage)) {
        return false;
    }
    if(!m_localization.setLanguage(languageId, false, errorMessage)) {
        return false;
    }
    if(!dispatch(errorMessage)) {
        return false;
    }
    return m_localization.setLanguage(languageId, true, errorMessage);
}

bool RobotQtViewerLanguageCoordinator::retranslateCurrentLanguage(QString* errorMessage)
{
    if(!m_registry.validateEnabledModeLanguages(errorMessage)) {
        return false;
    }
    return dispatch(errorMessage);
}

bool RobotQtViewerLanguageCoordinator::dispatch(QString* errorMessage)
{
    for(IRobotQtViewerLanguageParticipant* participant : m_shellParticipants) {
        if(participant != nullptr) {
            participant->retranslateUi(m_localization);
        }
    }
    for(const RobotQtViewerWorkbenchModeDesc& mode : m_registry.modes()) {
        if(mode.enabled && mode.languageParticipant != nullptr) {
            mode.languageParticipant->retranslateUi(m_localization);
        }
    }
    m_localization.retranslateApplication();
    if(errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}
}
