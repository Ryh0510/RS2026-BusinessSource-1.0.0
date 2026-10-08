#pragma once

class QWidget;
class QString;
// Opt-in diagnostic driver: exercises the application's real workbench/widgets.
void startPlaybackWindowProfile(QWidget& window, const QString& trajectoryFile,
    const QString& reportFile);
