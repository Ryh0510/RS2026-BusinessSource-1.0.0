#pragma once

class QWidget;
class QString;
// Opt-in diagnostic driver: exercises the application's real workbench/widgets.
void startPlaybackWindowProfile(QWidget& window, const QString& trajectoryFile,
    const QString& reportFile);
void startMoveBaseWindowProfile(QWidget& window, const QString& reportFile);

void startStlImportWindowProfile(QWidget& window, const QString& meshFile, const QString& reportFile);
