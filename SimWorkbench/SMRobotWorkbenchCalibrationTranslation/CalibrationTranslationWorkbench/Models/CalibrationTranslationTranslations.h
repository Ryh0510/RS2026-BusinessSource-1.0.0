#pragma once
#include <QString>
namespace smrobot::workbench::spray::translation {
class CalibrationTranslationTranslations final { public:
static QString canonicalLanguageCode(const QString& languageCode);
static QString text(const QString& languageCode, const char* key);
}; }
